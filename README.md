# ORION — Documentação 
### Rover de resgate em desastres · Global Solution 2026.1 · Space Connect

**FIAP — Engenharia de Software · 1ESR**
## Integrantes

- Leonardo Henrique Basseti — RM 574039

---

## Descrição do projeto

O **ORION** é um *digital twin* (centro de comando) para rovers de resgate que atuam em desastres como Brumadinho, deslizamentos e enchentes — ambientes onde humanos não podem entrar e a infraestrutura de telecomunicações foi destruída.

Este repositório contém o **módulo de Edge Computing & Computer System** do projeto: a simulação do **"corpo" do rover** em Arduino UNO. O Arduino representa o rover operando dentro dos escombros, onde o sinal de rádio é bloqueado pela massa de concreto e metal e, por isso, **cai de forma intermitente**.

A solução demonstra, na prática, o conceito central do ORION: **quando o rover perde o sinal, ele não para nem age às cegas — processa e decide localmente (edge computing), continua a missão com autonomia e guarda tudo que descobre, sincronizando com o centro de comando assim que o sinal retorna.**

## Objetivo da solução

Demonstrar que um rover de resgate pode operar de forma **resiliente à perda de comunicação**, garantindo que nenhuma informação crítica (como a localização de uma vítima) seja perdida quando o sinal cai. Isso é feito com três mecanismos de Edge Computing:

1. **Telemetria em tempo real** quando há sinal (modo ONLINE);
2. **Autonomia local + armazenamento (buffer)** quando o sinal cai (modo OFFLINE) — o rover desvia de obstáculos sozinho e guarda os dados;
3. **Sincronização (store-and-forward)** quando o sinal retorna — o buffer é enviado ao centro de comando.

### Conexão com a Indústria Espacial
A arquitetura do ORION se apoia em três ativos espaciais: **imagem de satélite** (mapa do terreno), **posicionamento GNSS** (georreferenciamento de vítimas, simulado no código) e **conectividade via satélite** (liga o posto de comando ao mundo externo). A premissa de operar sob comunicação intermitente é a mesma da operação de rovers em Marte, onde a autonomia embarcada é obrigatória.

## Componentes utilizados

| Componente | Função no rover | Pino(s) |
|---|---|---|
| Arduino Uno | Unidade de processamento (cérebro do rover) | — |
| Sensor ultrassônico HC-SR04 | Detecta obstáculos (distância ajustável ao vivo no Wokwi) | TRIG=9, ECHO=10 |
| Servo motor | Direção do rover (desvio autônomo) | PWM=11 |
| Sensor PIR de movimento | Detecta sinal de vida (possível vítima) | OUT=2 |
| Sensor DHT22 | Telemetria de temperatura e umidade | DATA=4 |
| Potenciômetro | Simula o nível de bateria do rover | SIG=A0 |
| Botão (pushbutton) | Simula a perda/retorno de sinal: cada clique faz o rover entrar (OFFLINE) ou sair (ONLINE) de uma zona sem comunicação nos escombros | 7 |
| LED verde | Link OK / transmitindo | 5 |
| LED amarelo | Modo autonomia (offline) | 6 |
| LED azul | Vítima detectada | 8 |
| Buzzer | Alerta sonoro de vítima | 3 |
| Resistores 220Ω (x3) | Proteção dos LEDs | — |

## Explicação do funcionamento

**Sobre o botão (simulação do sinal):** dentro dos escombros, o sinal de rádio é bloqueado pela massa de concreto e metal, caindo de forma intermitente. Como não é possível recriar essa perda física de sinal no simulador, o **botão representa esse fenômeno**: cada clique simula o rover **entrando em uma zona sem sinal (OFFLINE)** ou **voltando a ter comunicação (ONLINE)**. É o gatilho que nos permite demonstrar, na prática, como o rover se comporta nos dois cenários.

A cada ciclo (1,5 s), o rover lê todos os sensores e verifica o estado do link (alternado pelo botão):

- **Modo ONLINE (LED verde):** o rover transmite a telemetria em tempo real ao centro de comando pelo Monitor Serial. Se houver vítimas guardadas no buffer, ele as sincroniza primeiro.
- **Modo OFFLINE (LED amarelo):** o rover entra em **autonomia local**. Se o ultrassônico detectar um obstáculo a menos de 25 cm, o servo gira para **desviar sozinho**. Todas as leituras são **guardadas em um buffer** local (store-and-forward), em vez de transmitidas.
- **Detecção de vítima (qualquer modo):** quando o PIR detecta movimento, o LED azul acende, o buzzer dispara e a **coordenada GNSS simulada** é registrada (online: enviada na hora; offline: guardada no buffer).
- **Alertas de risco ambiental (qualquer modo):** o rover monitora a telemetria e dispara alertas no Serial quando a **temperatura passa de 50°C** (risco de **incêndio** nos escombros) ou a **umidade passa de 85%** (risco de **alagamento** na área). Online o alerta é transmitido na hora; offline ele acompanha o registro e reaparece no relatório de sincronização.
- **Retorno do sinal:** ao clicar o botão de volta para ONLINE, o rover detecta a transição OFFLINE→ONLINE e **despeja o buffer inteiro** no Monitor Serial, mostrando tudo que foi coletado no escuro — inclusive vítimas georreferenciadas.

> **Edge Computing na prática:** a decisão de desviar e a preservação dos dados acontecem **no próprio dispositivo**, sem depender da conexão com o centro. É isso que torna o rover resiliente à perda de sinal.

## Estrutura do circuito

O circuito é organizado em torno de uma **protoboard**, que distribui a energia de forma limpa:

- O Arduino envia **5V e GND** uma única vez para os **trilhos de energia** da protoboard (linha vermelha + e linha azul −).
- Cada componente puxa a alimentação do trilho mais próximo, evitando fios cruzando o tabuleiro.
- Os três sensores (HC-SR04, PIR, DHT22) são alimentados pelos trilhos e ligam seus pinos de **sinal** diretamente às portas do Arduino (TRIG=9, ECHO=10, PIR=2, DHT=4).
- O **potenciômetro** está ligado à porta analógica A0 e simula a bateria.
- O **botão (pushbutton)** está ligado ao pino 7 (com resistor de pull-up interno) e ao trilho −; cada clique alterna o estado do link entre ONLINE e OFFLINE.
- Os três **LEDs de status** usam resistores de 220Ω em série; seus cátodos vão para o trilho −.
- O **buzzer** (pino 3) e o **servo** (PWM=11) completam os atuadores de alerta e direção.

O arquivo `diagram.json` contém o circuito completo com a protoboard, pronto para abrir no Wokwi.

## Instruções de execução

1. Acesse [https://wokwi.com](https://wokwi.com) e crie um novo projeto **Arduino Uno**.
2. Substitua o conteúdo de `sketch.ino` (Codificação) e de `diagram.json` pelos arquivos deste repositório.
3. Adicone o conteudo do `libraries.txt` com o conteúdo do arquivo com o mesmo nome deste repositório (instala a biblioteca do DHT22) ou adicione manualmente pela aba "Library Manager" no Wokwi.
4. Clique em **▶ Start the simulation**.
5. **Demonstração:**
   - O rover começa **ONLINE** (LED verde): veja a telemetria em tempo real.
   - **Clique no botão para ir OFFLINE:** o LED amarelo acende e o rover entra em autonomia local (passa a guardar tudo no buffer).
   - **Crie um obstáculo:** o "obstáculo" no Wokwi é a distância medida pelo HC-SR04. Com a simulação rodando, **clique no sensor HC-SR04** — aparece um controle de distância. **Arraste-o para menos de 25 cm**: o rover detecta o obstáculo e o servo gira para desviar sozinho. Arraste de volta para cima para "liberar" o caminho.
   - **Simule uma vítima:** clique no **sensor PIR** para disparar o movimento — o LED azul acende, o buzzer toca e a coordenada é guardada no buffer.
   - **Simule riscos ambientais:** clique no **sensor DHT22** e ajuste os valores — suba a **temperatura acima de 50°C** para disparar o alerta de **incêndio**, ou a **umidade acima de 85%** para o alerta de **alagamento**. As mensagens `[RISCO]` aparecem no Serial.
   - **Clique no botão de novo para voltar ONLINE:** veja o buffer ser sincronizado, com as vítimas georreferenciadas.

## Ou use o Link da simulação:

> Simulação no Wokwi: https://wokwi.com/projects/466281537279903745
