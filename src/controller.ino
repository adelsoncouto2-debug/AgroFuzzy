/*
  Ligações (pinos podem ser alterados abaixo):
    - DS18B20  : DATA no GPIO 4 (resistor de 4,7k entre DATA e 3V3)
    - Umidade  : saída analógica do sensor no GPIO 34 (ADC1)
    - Relé/bomba: IN do módulo relé no GPIO 26
*/

#include <OneWire.h>
#include <DallasTemperature.h>

#define PIN_DS18B20 4
#define PIN_UMIDADE 34
#define PIN_BOMBA   26
const bool RELE_ATIVO_BAIXO = false;

// Calibração do sensor de umidade (valores brutos do ADC de 12 bits, 0-4095).
// Meça o valor com o sensor no ar/solo seco e com ele na água/solo encharcado.
const int ADC_SECO    = 4095;   // potenciômetro todo para a direita = solo seco (0 %)
const int ADC_MOLHADO = 0;      // potenciômetro todo para a esquerda = solo molhado (100 %)
const unsigned long INTERVALO_DECISAO_MS = 5000UL;  // decide a cada 5 s (em vez de 1 min)
const float T_MAX_S  = 60.0;
const float PASSO_S  = 0.5;   

const float T_MINIMO_ACIONAR_S = 1.0;

OneWire oneWire(PIN_DS18B20);
DallasTemperature sensorTemp(&oneWire);

bool bombaLigada = false;
unsigned long bombaFimMs = 0;
unsigned long ultimaDecisaoMs = 0;

float trapezio(float x, float a, float b, float c, float d) {
  if (x < a || x > d) return 0.0;
  if (x >= b && x <= c) return 1.0;
  if (x < b) return (x - a) / (b - a);
  return (d - x) / (d - c);
}

float triangulo(float x, float a, float b, float c) {
  return trapezio(x, a, b, b, c);
}
float uBaixa(float u)    { return trapezio(u, 0, 0, 35, 45); }
float uMedia(float u)    { return trapezio(u, 35, 45, 55, 65); }
float uAdequada(float u) { return trapezio(u, 55, 65, 75, 85); }
float uAlta(float u)     { return trapezio(u, 75, 85, 100, 100); }
float tBaixa(float t)    { return trapezio(t, -10, -10, 15, 20); }
float tAdequada(float t) { return trapezio(t, 15, 20, 30, 35); }
float tAlta(float t)     { return trapezio(t, 30, 35, 60, 60); }
float tempoZero(float t)  { return trapezio(t, 0, 0, 2, 5); }
float tempoCurto(float t) { return triangulo(t, 5, 15, 25); }
float tempoMedio(float t) { return triangulo(t, 20, 30, 40); }
float tempoLongo(float t) { return trapezio(t, 35, 45, 60, 60); }
float calculaTempoFuzzy(float umidade, float temperatura) {
  umidade = constrain(umidade, 0.0, 100.0);

  // 1) Fuzzificação
  float uB = uBaixa(umidade);
  float uM = uMedia(umidade);
  float uA = uAdequada(umidade);
  float uH = uAlta(umidade);

  float tB = tBaixa(temperatura);
  float tA = tAdequada(temperatura);
  float tH = tAlta(temperatura);
  // Regras complementares (completam a base para T adequada e U média):
  //   U adequada e T adequada -> curto
  //   U média    e T baixa    -> curto
  //   U média    e T adequada -> médio
  //   U média    e T alta     -> médio
  //   U baixa    e T adequada -> longo
  float forcaZero  = uH;
  float forcaCurto = max(max(min(uA, tB), min(uA, tA)), min(uM, tB));
  float forcaMedio = max(max(min(uA, tH), min(uM, tA)),
                         max(min(uM, tH), min(uB, tB)));
  float forcaLongo = max(min(uB, tA), min(uB, tH));

  float numerador = 0.0;
  float denominador = 0.0;

  for (float t = 0.0; t <= T_MAX_S; t += PASSO_S) {
    float mu = 0.0;
    mu = max(mu, min(forcaZero,  tempoZero(t)));
    mu = max(mu, min(forcaCurto, tempoCurto(t)));
    mu = max(mu, min(forcaMedio, tempoMedio(t)));
    mu = max(mu, min(forcaLongo, tempoLongo(t)));

    numerador   += t * mu;
    denominador += mu;
  }

  if (denominador <= 0.0) return 0.0;
  return numerador / denominador;
}

float lerUmidadeSolo() {
  long soma = 0;
  const int N = 10;
  for (int i = 0; i < N; i++) {
    soma += analogRead(PIN_UMIDADE);
    delay(5);
  }
  float bruto = soma / (float)N;
  float pct = (bruto - ADC_SECO) * 100.0 / (float)(ADC_MOLHADO - ADC_SECO);
  return constrain(pct, 0.0, 100.0);
}

float lerTemperatura() {
  sensorTemp.requestTemperatures();
  return sensorTemp.getTempCByIndex(0);  
}
void ligarBomba() {
  digitalWrite(PIN_BOMBA, RELE_ATIVO_BAIXO ? LOW : HIGH);
  bombaLigada = true;
}

void desligarBomba() {
  digitalWrite(PIN_BOMBA, RELE_ATIVO_BAIXO ? HIGH : LOW);
  bombaLigada = false;
}
void setup() {
  Serial.begin(115200);
  pinMode(PIN_BOMBA, OUTPUT);
  desligarBomba();

  analogReadResolution(12);
  sensorTemp.begin();

  Serial.println("Irrigacao Fuzzy - ESP32 iniciada");
  ultimaDecisaoMs = millis() - INTERVALO_DECISAO_MS;  
}

void loop() {
  unsigned long agora = millis();
  if (bombaLigada && (long)(agora - bombaFimMs) >= 0) {
    desligarBomba();
    Serial.println("Bomba desligada.");
  }
  if (!bombaLigada && (agora - ultimaDecisaoMs >= INTERVALO_DECISAO_MS)) {
    ultimaDecisaoMs = agora;

    float umidade = lerUmidadeSolo();
    float temperatura = lerTemperatura();

    if (temperatura < -100.0) {
      Serial.println("Falha no DS18B20: verifique a ligacao. Irrigacao cancelada.");
      return;
    }

    float tIrrig = calculaTempoFuzzy(umidade, temperatura);

    Serial.print("U = ");  Serial.print(umidade, 1);
    Serial.print(" %  |  T = "); Serial.print(temperatura, 1);
    Serial.print(" C  |  t* = "); Serial.print(tIrrig, 1);
    Serial.println(" s");

    if (tIrrig >= T_MINIMO_ACIONAR_S) {
      bombaFimMs = agora + (unsigned long)(tIrrig * 1000.0);
      ligarBomba();
      Serial.println("Bomba ligada.");
    } else {
      Serial.println("Sem necessidade de irrigar.");
    }
  }
}
