/*
  FECHADURA ELETRÔNICA - TERMINAL LOCAL (ARDUINO UNO + LCD KEYPAD SHIELD)
  Responsabilidades desta placa:
    - Ler os botões do LCD Keypad Shield para o usuário digitar a senha.
    - Enviar a tentativa de senha para a ESP32 via Serial (SoftwareSerial).
    - Exibir no LCD a resposta que a ESP32 devolver.
*/

#include <LiquidCrystal.h>
#include <SoftwareSerial.h>

LiquidCrystal lcd(8, 9, 4, 5, 6, 7);

//Comunicação serial com a ESP32 
#define PINO_RX 2   // TX2 da ESP32 (GPIO17)
#define PINO_TX 3   // RX2 da ESP32 (GPIO16) 
SoftwareSerial serialESP(PINO_RX, PINO_TX);

// botões
enum Botao { BTN_DIREITA, BTN_CIMA, BTN_BAIXO, BTN_ESQUERDA, BTN_SELECT, BTN_NENHUM };

//senha digitada
const uint8_t TAM_SENHA = 4;
uint8_t digitos[TAM_SENHA] = {0, 0, 0, 0};
uint8_t posicaoAtual = 0;

Botao botaoConfirmado = BTN_NENHUM; 
Botao candidato = BTN_NENHUM;      
unsigned long momentoCandidato = 0;
const unsigned long TEMPO_CONFIRMACAO = 30; 

void setup() {
  lcd.begin(16, 2);
  serialESP.begin(9600);
  Serial.begin(9600); 
  mostrarTelaInicial();
}

void loop() {
  Botao leituraAtual = lerBotaoBruto();

  if (leituraAtual != candidato) {
    // reinicia a contagem de estabilidade
    candidato = leituraAtual;
    momentoCandidato = millis();
    return;
  }

  // valor ainda não ficou estável tempo suficiente -> aguarda
  if (millis() - momentoCandidato < TEMPO_CONFIRMACAO) return;

  // valor estável e confirmado: só age se for diferente do último processado
  if (candidato != botaoConfirmado) {
    botaoConfirmado = candidato;

    if (botaoConfirmado != BTN_NENHUM) {
      processarBotao(botaoConfirmado);
      atualizarDisplaySenha();
    }
  }
}

// Executa a ação correspondente ao botão confirmado
void processarBotao(Botao botao) {
  switch (botao) {
    case BTN_CIMA:
      digitos[posicaoAtual] = (digitos[posicaoAtual] + 1) % 10;
      break;

    case BTN_BAIXO:
      digitos[posicaoAtual] = (digitos[posicaoAtual] + 9) % 10; // -1 sem ficar negativo
      break;

    case BTN_DIREITA:
      posicaoAtual = (posicaoAtual + 1) % TAM_SENHA;
      break;

    case BTN_ESQUERDA:
      posicaoAtual = (posicaoAtual + TAM_SENHA - 1) % TAM_SENHA;
      break;

    case BTN_SELECT:
      enviarSenhaParaESP32();
      break;

    default:
      break;
  }
}

// Leitura sem nenhum tipo de filtro
Botao lerBotaoBruto() {
  int valor = analogRead(A0);

  if (valor > 1000) return BTN_NENHUM;
  if (valor < 50)   return BTN_DIREITA;
  if (valor < 195)  return BTN_CIMA;
  if (valor < 380)  return BTN_BAIXO;
  if (valor < 555)  return BTN_ESQUERDA;
  if (valor < 790)  return BTN_SELECT;
  return BTN_NENHUM;
}


// atualiza display
void mostrarTelaInicial() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Digite a senha:");
  atualizarDisplaySenha();
}

// Mostra os dígitos digitados, com um cursor '^' indicando a posição atual
void atualizarDisplaySenha() {
  lcd.setCursor(0, 1);
  for (uint8_t i = 0; i < TAM_SENHA; i++) {
    lcd.print(digitos[i]);
    lcd.print(" ");
  }
  lcd.print("  SEL=OK");

}

// Envia e recebe a resposta

void enviarSenhaParaESP32() {
  // Monta a string 
  String senha = "";
  for (uint8_t i = 0; i < TAM_SENHA; i++) {
    senha += String(digitos[i]);
  }

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Verificando...");

  serialESP.println("SENHA:" + senha);

  String resposta = aguardarResposta(3000); 

  lcd.clear();
  lcd.setCursor(0, 0);

  if (resposta == "OK") {
    lcd.print("Acesso liberado!");
    delay(2500);
  } else if (resposta.startsWith("ERRO")) {
    lcd.print("Senha incorreta!");
    lcd.setCursor(0, 1);
    lcd.print("Restam: " + resposta.substring(5));
    delay(2500);
  } else if (resposta == "BLOQUEADO") {
    lcd.print("Sistema bloqueado");
    lcd.setCursor(0, 1);
    lcd.print("Aguarde...");
    delay(2500);
  } else if (resposta == "PORTA_ABERTA") {
    lcd.print("Porta ja aberta!");
    lcd.setCursor(0, 1);
    lcd.print("Feche-a primeiro");
    delay(2500);
  } else {
    lcd.print("Sem resposta da");
    lcd.setCursor(0, 1);
    lcd.print("ESP32 (verifique)");
    delay(2500);
  }

  // reseta para nova tentativa
  for (uint8_t i = 0; i < TAM_SENHA; i++) digitos[i] = 0;
  posicaoAtual = 0;
  mostrarTelaInicial();
}

// Aguarda a resposta
String aguardarResposta(unsigned long timeoutMs) {
  unsigned long inicio = millis();
  String linha = "";

  while (millis() - inicio < timeoutMs) {
    while (serialESP.available()) {
      char c = serialESP.read();
      if (c == '\n') {
        linha.trim();
        if (linha.length() > 0) return linha;
      } else {
        linha += c;
      }
    }
  }
  return ""; // timeout: nenhuma resposta recebida
}
