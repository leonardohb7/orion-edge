/*
 * ====================================================================
 *  ORION - Rover de resgate em desastres  |  Modulo Edge Computing
 *  Global Solution 2026 - Industria Espacial - FIAP ESW 1o Ano
 *  Aluno: Leonardo Henrique Basseti - RM 574039
 * --------------------------------------------------------------------
 *  O Arduino representa o "corpo" do rover ORION operando nos escombros.
 *  Demonstra na pratica o conceito central do projeto:
 *    - Telemetria em tempo real quando ha sinal (ONLINE)
 *    - Autonomia local + armazenamento (buffer) quando o sinal cai (OFFLINE)
 *    - Sincronizacao (store-and-forward) quando o sinal retorna
 *    - Deteccao e georreferenciamento de vitimas
 *  Edge Computing = processar e decidir LOCALMENTE, sem depender do centro.
 * ====================================================================
 */

#include <DHT.h>
#include <Servo.h>

// ----------------------- Mapeamento de pinos -----------------------
#define PIN_TRIG     9    // HC-SR04 - dispara o pulso
#define PIN_ECHO     10   // HC-SR04 - recebe o eco
#define PIN_PIR      2    // Sensor de movimento (possivel vitima)
#define PIN_DHT      4    // Sensor de temperatura/umidade
#define PIN_BATERIA  A0   // Potenciometro simulando a bateria
#define PIN_LINK     7    // Botao: cada clique alterna ONLINE/OFFLINE
#define PIN_LED_ON   5    // LED verde  - link OK / transmitindo
#define PIN_LED_OFF  6    // LED amarelo - modo autonomia (offline)
#define PIN_LED_VIT  8    // LED azul   - vitima detectada
#define PIN_BUZZER   3    // Buzzer     - alerta sonoro de vitima
#define PIN_SERVO    11   // Servo      - direcao do rover (desvio)

#define DIST_OBSTACULO_CM 25   // distancia que dispara o desvio
#define TEMP_INCENDIO_C   50   // acima disso: risco de incendio
#define UMID_ALAGAMENTO   85   // acima disso: risco de alagamento
#define BUF_MAX           20   // capacidade do buffer local

DHT dht(PIN_DHT, DHT22);
Servo direcao;

// ----------------------- Buffer (store-and-forward) ----------------
struct Registro {
  unsigned long t;     // instante (ms desde o boot)
  float temp;          // temperatura (C)
  float umid;          // umidade (%)
  int   bateria;       // bateria (%)
  int   distancia;     // distancia ao obstaculo (cm)
  bool  vitima;        // vitima detectada neste registro?
  float lat;           // latitude simulada (GNSS)
  float lng;           // longitude simulada (GNSS)
};

Registro buffer[BUF_MAX];
int bufCount = 0;

// Coordenada-base aproximada de Brumadinho/MG (simulacao de GNSS)
const float BASE_LAT = -20.1192;
const float BASE_LNG = -44.1219;
int passo = 0;   // avanca a "posicao" do rover a cada ciclo

// ----------------------- Estado do link e do botao -----------------
bool online = true;               // estado atual do link (comeca ONLINE)
bool linkAnterior = true;         // para detectar a transicao OFFLINE->ONLINE
int  btnEstavel = HIGH;           // ultimo estado estavel do botao
int  btnLeituraAnt = HIGH;        // ultima leitura crua do botao
unsigned long tDebounce = 0;      // marca de tempo do debounce
const unsigned long DEBOUNCE_MS = 50;
unsigned long tProximoCiclo = 0;  // controla o ritmo da telemetria
const unsigned long INTERVALO_MS = 1500;

// =================================================================
void setup() {
  Serial.begin(9600);
  pinMode(PIN_TRIG, OUTPUT);
  pinMode(PIN_ECHO, INPUT);
  pinMode(PIN_PIR, INPUT);
  pinMode(PIN_LINK, INPUT_PULLUP);   // botao: solto = HIGH, pressionado = LOW
  pinMode(PIN_LED_ON, OUTPUT);
  pinMode(PIN_LED_OFF, OUTPUT);
  pinMode(PIN_LED_VIT, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);

  dht.begin();
  direcao.attach(PIN_SERVO);
  direcao.write(90);   // rover apontando "em frente"

  Serial.println(F("=================================================="));
  Serial.println(F("  ORION ROVER - inicializando sistemas..."));
  Serial.println(F("  Edge Computing ativo. Operando nos escombros."));
  Serial.println(F("==================================================\n"));
  delay(1000);
}

// ----------------------- Leitura dos sensores ----------------------
int lerDistanciaCm() {
  digitalWrite(PIN_TRIG, LOW);  delayMicroseconds(2);
  digitalWrite(PIN_TRIG, HIGH); delayMicroseconds(10);
  digitalWrite(PIN_TRIG, LOW);
  long dur = pulseIn(PIN_ECHO, HIGH, 30000); // timeout 30ms
  if (dur == 0) return 400;                  // nada detectado = livre
  return (int)(dur * 0.034 / 2.0);
}

int lerBateria() {
  int leitura = analogRead(PIN_BATERIA);     // 0..1023
  return map(leitura, 0, 1023, 0, 100);      // 0..100 %
}

// ----------------------- Autonomia local ---------------------------
// Sem sinal, o rover decide sozinho: se houver obstaculo, desvia.
void desviarObstaculo(int distancia) {
  if (distancia < DIST_OBSTACULO_CM) {
    Serial.print(F("   [NAV] Obstaculo a "));
    Serial.print(distancia);
    Serial.println(F(" cm -> rover desviando sozinho (servo)."));
    direcao.write(40);   delay(300);   // vira para desviar
    direcao.write(140);  delay(300);
    direcao.write(90);                 // retoma a frente
  }
}

// ----------------------- Buffer ------------------------------------
void guardarNoBuffer(Registro r) {
  if (bufCount < BUF_MAX) {
    buffer[bufCount++] = r;
    Serial.print(F("   [OFFLINE] Sem sinal. Registro guardado no buffer ("));
    Serial.print(bufCount); Serial.print(F("/")); Serial.print(BUF_MAX);
    Serial.println(F(")."));
  } else {
    Serial.println(F("   [OFFLINE] Buffer cheio! Descartando o registro mais antigo."));
    for (int i = 1; i < BUF_MAX; i++) buffer[i - 1] = buffer[i];
    buffer[BUF_MAX - 1] = r;
  }
}

// Sincronizacao: ao reconectar, despeja tudo que foi guardado.
void sincronizarBuffer() {
  if (bufCount == 0) return;
  Serial.println(F("\n>>> [SYNC] Sinal restabelecido! Enviando buffer ao centro de comando..."));
  for (int i = 0; i < bufCount; i++) {
    Registro r = buffer[i];
    Serial.print(F("    > t=")); Serial.print(r.t);
    Serial.print(F("ms | T=")); Serial.print(r.temp, 1);
    Serial.print(F("C | Umid=")); Serial.print(r.umid, 0);
    Serial.print(F("% | Bat=")); Serial.print(r.bateria);
    Serial.print(F("% | Dist=")); Serial.print(r.distancia);
    Serial.print(F("cm"));
    if (r.temp > TEMP_INCENDIO_C) Serial.print(F(" | RISCO: INCENDIO"));
    if (r.umid > UMID_ALAGAMENTO)  Serial.print(F(" | RISCO: ALAGAMENTO"));
    if (r.vitima) {
      Serial.print(F(" | *** VITIMA @ "));
      Serial.print(r.lat, 4); Serial.print(F(", "));
      Serial.print(r.lng, 4); Serial.print(F(" ***"));
    }
    Serial.println();
  }
  Serial.print(F(">>> [SYNC] ")); Serial.print(bufCount);
  Serial.println(F(" registro(s) sincronizado(s). Buffer limpo.\n"));
  bufCount = 0;
}

// ----------------------- Alertas de risco ambiental ----------------
// Avalia temperatura e umidade e imprime alertas. Usada nos dois modos:
// ONLINE o alerta vai ao operador; OFFLINE ele acompanha o registro no buffer.
void avaliarRiscos(float temp, float umid) {
  if (temp > TEMP_INCENDIO_C) {
    Serial.print(F("   [RISCO] Temperatura alta ("));
    Serial.print(temp, 1);
    Serial.println(F("C) -> possivel INCENDIO nos escombros!"));
  }
  if (umid > UMID_ALAGAMENTO) {
    Serial.print(F("   [RISCO] Umidade alta ("));
    Serial.print(umid, 0);
    Serial.println(F("%) -> possivel ALAGAMENTO na area!"));
  }
}

// ----------------------- Alerta de vitima --------------------------
void alertaVitima(float lat, float lng) {
  digitalWrite(PIN_LED_VIT, HIGH);
  tone(PIN_BUZZER, 1200, 250);
  Serial.print(F("   [VITIMA] Sinal de vida detectado! Coordenada GNSS: "));
  Serial.print(lat, 4); Serial.print(F(", ")); Serial.println(lng, 4);
}

// =================================================================
void loop() {
  // ---------- Botao: lido a cada passagem do loop (com debounce) ----------
  // O loop roda continuamente e nunca fica "travado" esperando, entao
  // nenhum clique e perdido. Cada clique alterna ONLINE <-> OFFLINE.
  int leitura = digitalRead(PIN_LINK);
  if (leitura != btnLeituraAnt) {
    tDebounce = millis();          // a leitura mudou: reinicia o timer
  }
  if ((millis() - tDebounce) > DEBOUNCE_MS) {
    if (leitura != btnEstavel) {   // estado realmente estavel e diferente
      btnEstavel = leitura;
      if (btnEstavel == LOW) {     // botao acabou de ser pressionado
        online = !online;
        Serial.print(F("\n*** Botao pressionado -> link agora: "));
        Serial.println(online ? F("ONLINE ***\n") : F("OFFLINE ***\n"));
      }
    }
  }
  btnLeituraAnt = leitura;

  // Detecta a transicao OFFLINE -> ONLINE para sincronizar o buffer
  if (online && !linkAnterior) sincronizarBuffer();
  linkAnterior = online;

  // LEDs de link respondem no ato (verde = ONLINE, amarelo = OFFLINE)
  digitalWrite(PIN_LED_ON,  online ? HIGH : LOW);
  digitalWrite(PIN_LED_OFF, online ? LOW  : HIGH);

  // ---------- Ritmo da telemetria sem travar o loop ----------
  // So executa o ciclo de leitura/transmissao a cada INTERVALO_MS,
  // mas continua lendo o botao o tempo todo entre um ciclo e outro.
  if (millis() - tProximoCiclo < INTERVALO_MS) return;
  tProximoCiclo = millis();

  // ---------- Leitura dos sensores ----------
  float temp = dht.readTemperature();
  float umid = dht.readHumidity();
  if (isnan(temp)) temp = 0;
  if (isnan(umid)) umid = 0;
  int bateria   = lerBateria();
  int distancia = lerDistanciaCm();
  bool vitima   = (digitalRead(PIN_PIR) == HIGH);

  // ---------- "Posicao" simulada do rover (GNSS) ----------
  passo++;
  float lat = BASE_LAT + passo * 0.00012;
  float lng = BASE_LNG + passo * 0.00009;

  // ---------- LED de vitima ----------
  if (!vitima) digitalWrite(PIN_LED_VIT, LOW);

  // ---------- Monta o registro atual ----------
  Registro r = { millis(), temp, umid, bateria, distancia, vitima, lat, lng };

  if (online) {
    // -------- MODO ONLINE: telemetria em tempo real --------
    // O operador no centro de comando ve tudo e decide os comandos
    // (incluindo desviar de obstaculos). O rover apenas reporta.
    Serial.print(F("[ONLINE]  T=")); Serial.print(temp, 1);
    Serial.print(F("C | Umid=")); Serial.print(umid, 0);
    Serial.print(F("% | Bat=")); Serial.print(bateria);
    Serial.print(F("% | Dist=")); Serial.print(distancia);
    Serial.print(F("cm"));
    if (distancia < DIST_OBSTACULO_CM)
      Serial.print(F(" | OBSTACULO detectado (operador decide o desvio)"));
    Serial.println(F(" | transmitindo ao centro de comando."));
    avaliarRiscos(temp, umid);
    if (vitima) alertaVitima(lat, lng);
  } else {
    // -------- MODO OFFLINE: autonomia local + buffer --------
    // Sem sinal, o rover decide sozinho (edge): desvia do obstaculo
    // e guarda tudo no buffer ate o sinal voltar.
    desviarObstaculo(distancia);
    avaliarRiscos(temp, umid);
    if (vitima) alertaVitima(lat, lng);
    guardarNoBuffer(r);
  }
}
