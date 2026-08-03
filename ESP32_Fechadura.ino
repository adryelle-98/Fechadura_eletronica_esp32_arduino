/*
  ================================================================
  FECHADURA ELETRÔNICA - CONTROLADOR PRINCIPAL (ESP32)
  ================================================================
  Responsabilidades desta placa:
    - É a ÚNICA autoridade sobre a senha (fonte única da verdade).
    - Hospeda uma página web para digitação remota da senha.
    - Recebe tentativas de senha vindas do Arduino Uno (via Serial2)
      digitadas no LCD Keypad Shield.
    - Valida qualquer tentativa (seja da web ou do Uno) usando a
      MESMA função, garantindo senha sempre sincronizada.
    - Controla o servo motor (simula a fechadura), os LEDs
      verde/vermelho, o buzzer de alarme e lê o sensor Hall
      (detecta se a porta está fechada).
    - Salva a senha em memória não-volátil, não tem o risco de perder se cair a energia.
*/

#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Servo.h>
#include <Preferences.h>

const char* WIFI_SSID = "Nome da rede";
const char* WIFI_PASS = "senha da rede";
 
#define servo 18
#define hall 27
#define ledVerde 25
#define ledVermelho 26
#define buzzer 13
#define RXD2 33   
#define TXD2 32   

WebServer server(80);
Servo servoFechadura;
Preferences preferencias;

// SENHA
String senhaCorreta = "1234";       
const uint8_t TAM_SENHA = 4;

//Tentativas
uint8_t tentativasErradas = 0;
const uint8_t MAX_TENTATIVAS = 3;

bool bloqueado = false;
unsigned long inicioBloqueio = 0;
const unsigned long TEMPO_BLOQUEIO = 15000; // 15s de bloqueio

//servo
const int ANGULO_TRANCADO      = 0;
const int ANGULO_DESTRANCADO   = 90;

bool portaDestrancada = false;       // true entre o momento que destranca e a porta ser fechada de novo
unsigned long momentoDestravou = 0;
// Se o usuário destrancar mas nunca abrir a porta, tranca sozinho depois disso (segurança):
const unsigned long TEMPO_MAX_DESTRANCADA_SEM_ABRIR = 15000; // 15s

//  MÁQUINA DE ESTADOS DA PORTA (sensor Hall) 
// Ímã detectado = porta fechada
// Ímã ausente = porta aberta
bool portaAberta = false;
unsigned long momentoAbriuPorta = 0;   // instante em que a porta foi aberta

const unsigned long TEMPO_ALERTA_PORTA = 5000;  // espera 5s antes de começar a alarmar
const unsigned long INTERVALO_BIP      = 3000;  // um bipe a cada 3s enquanto aberta
const unsigned long DURACAO_BIP        = 150;   // duração de cada bipe (ms)

bool alarmePortaAberta = false;  // true quando já passou do tempo de tolerância
bool bipTocando        = false;  // true durante a janela de DURACAO_BIP em que o buzzer está ligado
unsigned long ultimoBip    = 0;
unsigned long inicioBip    = 0;

// Dados vindos do Uno
String bufferSerialUno = "";

//-----
void setup() {
  Serial.begin(115200); 
  Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2); // Comunicação com o Uno

  pinMode(ledVerde, OUTPUT);
  pinMode(ledVermelho, OUTPUT);
  pinMode(buzzer, OUTPUT);
  digitalWrite(ledVerde, LOW);
  digitalWrite(ledVermelho, LOW);
  digitalWrite(buzzer, LOW);
  pinMode(hall, INPUT_PULLUP);

  // Servo
  ESP32PWM::allocateTimer(0);
  servoFechadura.setPeriodHertz(50);
  servoFechadura.attach(servo, 500, 2400);
  servoFechadura.write(ANGULO_TRANCADO);

  // Salva na mémoria
  preferencias.begin("fechadura", false);
  senhaCorreta = preferencias.getString("senha", senhaCorreta);
  preferencias.end();

  conectarWiFi();
  server.on("/", HTTP_GET, handleRoot);
  server.on("/destrancar", HTTP_GET, handleDestrancar);
  server.on("/trocarsenha", HTTP_GET, handleTrocarSenha);
  server.on("/status", HTTP_GET, handleStatus);
  server.begin();
  Serial.println("Sistema pronto.");
}

void loop() {
  server.handleClient();
  processarSerialUno();
  atualizarBloqueio();
  monitorarPorta();   // lê o Hall, controla o alarme e o relock automático
}
// Conferir senha
uint8_t validarSenha(String tentativa, String origem) {
  tentativa.trim();
  if (bloqueado) {
    Serial.println("Tentativa recusada (" + origem + "): sistema bloqueado.");
    return 2;
  }
  if (portaAberta) {
    Serial.println("Tentativa ignorada: porta já está aberta.");
    return 3;
  }
  if (tentativa == senhaCorreta) {
    tentativasErradas = 0;
    Serial.println("Acesso liberado via " + origem);
    acessoLiberado();
    return 0;
  } 
  else {
    tentativasErradas++;
    Serial.println("Senha incorreta via " + origem + " (" + String(tentativasErradas) + "/" + String(MAX_TENTATIVAS) + ")");
    acessoNegado();
    if (tentativasErradas >= MAX_TENTATIVAS) {
      ativarBloqueioEAlarme();
      return 2;
    }
    return 1;
  }
}


// Hardware
'void acessoLiberado() {
  digitalWrite(ledVermelho, LOW);
  digitalWrite(ledVerde, HIGH);
  servoFechadura.write(ANGULO_DESTRANCADO);
  portaDestrancada = true;
  momentoDestravou = millis();'
}

void acessoNegado() {
  digitalWrite(ledVerde, LOW);
  for (uint8_t i = 0; i < 3; i++) {
    digitalWrite(ledVermelho, HIGH);
    delay(150);
    digitalWrite(ledVermelho, LOW);
    delay(150);
  }
}

void ativarBloqueioEAlarme() {
  bloqueado = true;
  inicioBloqueio = millis();
  Serial.println(">>> ALARME: 3 tentativas incorretas. Sistema bloqueado.");
  digitalWrite(ledVermelho, HIGH);
  for (uint8_t i = 0; i < 6; i++) {
    digitalWrite(buzzer, HIGH);
    delay(200);
    digitalWrite(buzzer, LOW);
    delay(200);
  }
}

void atualizarBloqueio() {
  if (bloqueado && (millis() - inicioBloqueio >= TEMPO_BLOQUEIO)) {
    bloqueado = false;
    tentativasErradas = 0;
    digitalWrite(ledVermelho, LOW);
    Serial.println("Bloqueio finalizado. Sistema liberado para novas tentativas.");
  }
}


//   MÁQUINA DE ESTADOS DA PORTA - lida com o sensor Hall
// Chamada a cada iteração do loop(). NÃO usa delay() em nenhum ponto,
// para não travar o servidor web nem a serial com o Uno.
void monitorarPorta() {
  bool fechadaAgora = portaFechada(); // true = ímã detectado
  if (fechadaAgora) {
    // Porta está FECHADA neste instante 
    if (portaAberta) {
      // Ela tinha acabado de ser fechada agora (estava aberta antes):
      // desliga o alarme imediatamente e reinicia o temporizador.
      portaAberta = false;
      alarmePortaAberta = false;
      bipTocando = false;
      digitalWrite(buzzer, LOW);

      // Se a fechadura estava destrancada por causa dessa abertura,
      // tranca de novo agora que a porta fechou.
      if (portaDestrancada) {
        servoFechadura.write(ANGULO_TRANCADO);
        digitalWrite(ledVerde, LOW);
        portaDestrancada = false;
      }

      Serial.println("Porta fechada. Buzzer desligado, sistema liberado para novas tentativas.");
    }
    else if (portaDestrancada &&
             (millis() - momentoDestravou >= TEMPO_MAX_DESTRANCADA_SEM_ABRIR)) {
      // Segurança: destrancou mas ninguém abriu a porta -> tranca sozinho
      servoFechadura.write(ANGULO_TRANCADO);
      digitalWrite(ledVerde, LOW);
      portaDestrancada = false;
      Serial.println("Ninguém abriu a porta a tempo. Trancando automaticamente.");
    }
  }
  else {
    // Porta está ABERTA neste instante 
    if (!portaAberta) {
      // Acabou de abrir agora: começa a contar o tempo
      portaAberta = true;
      momentoAbriuPorta = millis();
      alarmePortaAberta = false;
      Serial.println("Porta aberta.");
    }
    else if (!alarmePortaAberta &&
             (millis() - momentoAbriuPorta >= TEMPO_ALERTA_PORTA)) {
      // Já passou do tempo de tolerância sem fechar: liga o modo alarme
      alarmePortaAberta = true;
      ultimoBip = millis() - INTERVALO_BIP; // permite bipar já na próxima checagem
      Serial.println("ALERTA: porta aberta há muito tempo! Feche a porta.");
    }
  }

  // Geração do bipe periódico 
  if (alarmePortaAberta) {
    unsigned long agora = millis();

    if (!bipTocando && (agora - ultimoBip >= INTERVALO_BIP)) {
      digitalWrite(buzzer, HIGH);
      bipTocando = true;
      inicioBip = agora;
      ultimoBip = agora;
    }

    if (bipTocando && (agora - inicioBip >= DURACAO_BIP)) {
      digitalWrite(buzzer, LOW);
      bipTocando = false;
    }
  }
}

// Imã detectado = porta fechada
bool portaFechada() {
  return digitalRead(hall) == LOW;
}

// Comunicação com Arduino
void processarSerialUno() {
  while (Serial2.available()) {
    char c = Serial2.read();
    if (c == '\n') {
      bufferSerialUno.trim();

      if (bufferSerialUno.startsWith("SENHA:")) {
        String tentativa = bufferSerialUno.substring(6);
        uint8_t resultado = validarSenha(tentativa, "LCD/Uno");

        if (resultado == 0) {
          Serial2.println("OK");
        } else if (resultado == 1) {
          Serial2.println("ERRO:" + String(MAX_TENTATIVAS - tentativasErradas));
        } else if (resultado == 3) {
          Serial2.println("PORTA_ABERTA");
        } else {
          Serial2.println("BLOQUEADO");
        }
      }
      bufferSerialUno = "";
    } else {
      bufferSerialUno += c;
    }
  }
}

//IP
void conectarWiFi() {
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Conectando ao Wi-Fi");
  uint8_t tentativas = 0;
  while (WiFi.status() != WL_CONNECTED && tentativas < 40) {
    delay(500);
    Serial.print(".");
    tentativas++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConectado! Acesse a fechadura em:");
    Serial.print("http://");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nFalha ao conectar. Verifique SSID/senha.");
  }
}


//Web, foi feita em HTML
void handleRoot() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html lang="pt-br">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Fechadura Eletrônica</title>
  <style>
    body { font-family: Arial, sans-serif; background:#1b1f27; color:#eee;
           display:flex; justify-content:center; align-items:center; height:100vh; margin:0;}
    .card { background:#262b36; padding:30px; border-radius:16px; width:300px; text-align:center;
            box-shadow: 0 8px 24px rgba(0,0,0,0.4);}
    h1 { font-size:20px; margin-bottom:20px;}
    input { width:90%; padding:12px; font-size:18px; border-radius:8px; border:none;
            text-align:center; letter-spacing:4px; margin-bottom:15px;}
    button { width:95%; padding:12px; font-size:16px; border:none; border-radius:8px;
             background:#3a86ff; color:white; cursor:pointer;}
    button:hover { background:#2667cc; }
    #resultado { margin-top:15px; font-weight:bold; min-height:20px;}
  </style>
</head>
<body>
  <div class="card">
    <h1>🔒 Fechadura Eletrônica</h1>
    <input type="password" id="senha" maxlength="4" placeholder="Digite a senha" inputmode="numeric">
    <button onclick="enviar()">Destravar</button>
    <div id="resultado"></div>
  </div>

<script>
async function enviar() {
  const senha = document.getElementById('senha').value;
  const resDiv = document.getElementById('resultado');
  resDiv.style.color = '#eee';
  resDiv.innerText = 'Verificando...';

  try {
    const resp = await fetch('/destrancar?senha=' + encodeURIComponent(senha));
    const texto = await resp.text();

    if (texto === 'OK') {
      resDiv.style.color = '#2ecc71';
      resDiv.innerText = 'Acesso liberado!';
    } else if (texto.startsWith('ERRO')) {
      resDiv.style.color = '#e74c3c';
      resDiv.innerText = 'Senha incorreta. ' + texto.split(':')[1] + ' tentativa(s) restante(s).';
    } else if (texto === 'PORTA_ABERTA') {
      resDiv.style.color = '#f39c12';
      resDiv.innerText = 'A porta já está aberta. Feche-a antes de tentar novamente.';
    } else {
      resDiv.style.color = '#e74c3c';
      resDiv.innerText = 'Sistema bloqueado. Aguarde e tente novamente.';
    }
  } catch (e) {
    resDiv.style.color = '#e74c3c';
    resDiv.innerText = 'Erro de comunicação com a fechadura.';
  }
}
</script>
</body>
</html>
)rawliteral";

  server.send(200, "text/html", html);
}

// Recebe tentativa web
void handleDestrancar() {
  if (!server.hasArg("senha")) {
    server.send(400, "text/plain", "FALTA_SENHA");
    return;
  }

  String tentativa = server.arg("senha");
  uint8_t resultado = validarSenha(tentativa, "Web");

  if (resultado == 0) {
    server.send(200, "text/plain", "OK");
  } else if (resultado == 1) {
    server.send(200, "text/plain", "ERRO:" + String(MAX_TENTATIVAS - tentativasErradas));
  } else if (resultado == 2) {
    server.send(200, "text/plain", "BLOQUEADO");
  } else if (resultado == 3) {
    server.send(200, "text/plain", "PORTA_ABERTA");
  }
}

// Trocar senha
// Uso: /trocarsenha?atual=????&nova=????
void handleTrocarSenha() {
  if (!server.hasArg("atual") || !server.hasArg("nova")) {
    server.send(400, "text/plain", "PARAMETROS_FALTANDO");
    return;
  }

  String atual = server.arg("atual");
  String nova = server.arg("nova");

  if (atual != senhaCorreta) {
    server.send(200, "text/plain", "SENHA_ATUAL_INCORRETA");
    return;
  }
  if (nova.length() != TAM_SENHA) {
    server.send(200, "text/plain", "TAMANHO_INVALIDO");
    return;
  }
  senhaCorreta = nova;
  // muda a nova senha
  preferencias.begin("fechadura", false);
  preferencias.putString("senha", senhaCorreta);
  preferencias.end();

  server.send(200, "text/plain", "SENHA_ALTERADA");
  Serial.println("Senha alterada com sucesso via web.");
}

//para bugs
void handleStatus() {
  String json = "{";
  json += "\"bloqueado\":" + String(bloqueado ? "true" : "false") + ",";
  json += "\"tentativas_erradas\":" + String(tentativasErradas) + ",";
  json += "\"porta_fechada\":" + String(portaFechada() ? "true" : "false") + ",";
  json += "\"porta_aberta\":" + String(portaAberta ? "true" : "false") + ",";
  json += "\"alarme_porta_aberta\":" + String(alarmePortaAberta ? "true" : "false") + ",";
  json += "\"porta_destrancada\":" + String(portaDestrancada ? "true" : "false");
  json += "}";
  server.send(200, "application/json", json);
}