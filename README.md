# Fechadura Eletrônica Inteligente com ESP32 e Arduino Uno
Sistema de fechadura eletrônica com dois pontos de acesso — teclado físico (LCD Keypad Shield) e página web — controlados por uma única senha sincronizada.
O projeto foi desenvolvido utilizando ESP32 como controlador principal e Arduino Uno como terminal de entrada local, simulando o funcionamento de uma fechadura eletrônica residencial.

# Visão Geral
O sistema permite que a porta seja destrancada por dois meios diferentes:
  Teclado físico utilizando um LCD Keypad Shield conectado ao Arduino Uno.
  Página web hospedada pela ESP32 acessível pela rede Wi-Fi.
A senha é armazenada exclusivamente na ESP32, garantindo que ambos os métodos de acesso utilizem sempre a mesma autenticação.
Além disso, o sistema monitora o estado da porta através de um sensor de efeito Hall e controla automaticamente o travamento utilizando um servomotor.

# Principais Funcionalidades
Login por senha através do teclado físico.
Login por senha através da página web.
Senha única sincronizada entre todos os acessos.
Comunicação serial entre ESP32 e Arduino Uno.
Controle da fechadura utilizando servomotor.
Detecção da abertura e fechamento da porta através de sensor Hall.
Travamento automático após fechamento da porta.
Alarme sonoro quando a porta permanece aberta por muito tempo.
Sistema de segurança contra tentativas de invasão.
Bloqueio temporário após três senhas incorretas consecutivas.
Armazenamento permanente da senha utilizando a memória não-volátil (Preferences) da ESP32.

# Arquitetura do Sistema

                  Página Web
                       │
                  Wi-Fi (HTTP)
                       │
                ┌──────────────┐
                │    ESP32     │
                │--------------│
                │ Senha Mestre │
                │ Web Server   │
                │ Servo Motor  │
                │ Sensor Hall  │
                │ LEDs         │
                │ Buzzer       │
                └──────┬───────┘
                       │
                 Comunicação Serial
                       │
                ┌──────────────┐
                │ Arduino Uno  │
                │--------------│
                │ LCD Keypad   │
                │ Shield       │
                └──────────────┘

# Componentes Utilizados
Hardware
ESP32
Arduino Uno
LCD Keypad Shield
Servo Motor SG90
Sensor de Efeito Hall
Buzzer
LEDs indicador verde e vermelho
Protoboard
Jumpers
Fonte de alimentação

# Funcionamento
1. Inicialização
Ao ligar o sistema:
A ESP32 inicializa todos os periféricos.
Carrega a senha salva na memória.
Inicia o servidor web.
Aguarda conexões.

2. Acesso Local
O usuário digita os quatro dígitos utilizando o LCD Keypad Shield.
O Arduino Uno envia a tentativa de senha para a ESP32 através da comunicação serial.
A ESP32 realiza a validação e devolve o resultado.

3. Acesso Web
O usuário acessa a página hospedada pela ESP32.
Após digitar a senha, a requisição é enviada ao servidor embarcado.
A autenticação utiliza exatamente a mesma rotina usada pelo teclado físico.

4. Abertura da Fechadura
Quando a senha está correta:
  - o servomotor destranca a porta;
  - o LED verde é acionado;
  - a fechadura permanece destrancada.

5. Fechamento Automático
O sensor Hall monitora constantemente o estado da porta.
Após detectar:
  - abertura da porta;
  - fechamento novamente,
  - o sistema aciona automaticamente o servomotor para travar a fechadura.

7. Segurança
Caso ocorram três tentativas consecutivas com senha incorreta:
  - o buzzer dispara um alarme;
  - novas tentativas são bloqueadas durante alguns segundos.

8. Porta Aberta
Se a porta permanecer aberta além do tempo permitido:
  - o buzzer emite avisos periódicos até que ela seja fechada.

# Possíveis Melhorias
Cadastro de múltiplos usuários.
Histórico de acessos.
Integração com aplicativo móvel.
Autenticação por RFID.
Leitor biométrico.
Integração com MQTT e IoT.
Notificações pelo celular.
Registro de eventos em banco de dados.

## Integrantes
Projeto desenvolvido pelos alunos da Faculdade de Engenharia Elétrica (FEELT) da Universidade Federal de Uberlândia (UFU):
- Adryelle Alves Vieira
- Artur Fulgoni da Silva
- Heitor Ferreira Machado
- Maria Eduarda Soares Rabelo
- Sandro Campos Martins Filho
Projeto desenvolvido para a disciplina de Programação Procedimental.
