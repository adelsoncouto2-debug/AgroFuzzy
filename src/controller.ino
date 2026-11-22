/*
  Irrigação com Lógica Fuzzy - ESP8266 NodeMCU + miniservidor web
  Culturas: soja, café, cana-de-açúcar e milho

  A página web (botões de cultura) envia para a NodeMCU:
      POST http://192.168.4.1/api/cultura
      Corpo (JSON): {"cultura":"cafe"}
      Campos opcionais: "fase_critica": true/false   (estádio de maior prioridade hídrica)
                        "bloqueio": true/false       (suspende a irrigação por manejo,
                                                      ex.: estresse hídrico controlado do café)
  Outras rotas:
      GET /api/status   -> estado atual (cultura, leituras, dose, bomba)

  MODELOS FUZZY
  - Soja : umidade U (%) + temperatura  (modelo original do artigo, 10 regras).
  - Café, cana e milho: esgotamento normalizado Q (%) + temperatura + estádio
    (fase crítica ou não), conforme o documento de parâmetros.
      D (%) = 100 * (leitura - leitura_CC) / (leitura_PMP - leitura_CC)
      Q (%) = 100 * D / D_lim      (D_lim = esgotamento de referência da cultura)

  SAÍDA: dose (fração da dose máxima, 0 a 1) pelo centro de gravidade;
         volume = dose * VOLUME_MAX_ML ; t* = volume / vazão.

  Bibliotecas: OneWire e DallasTemperature (instalar). ESP8266WiFi e
  ESP8266WebServer já vêm com o pacote esp8266.
  Placa: "NodeMCU 1.0 (ESP-12E Module)"

  Ligações (nomes impressos na NodeMCU):
    - Sensor de umidade (AOUT): A0
    - DS18B20 (DATA): D2 (GPIO 4), resistor de 4,7k entre DATA e 3V
    - Relé/bomba (IN): D1 (GPIO 5) - ainda não usado
*/

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#define PIN_DS18B20 4    // D2
#define PIN_UMIDADE A0
#define PIN_BOMBA   5    // D1

const char* AP_SSID  = "IrrigacaoFuzzy";
const char* AP_SENHA = "irrigacao123";

const bool RELE_ATIVO_BAIXO = false;

// Soja (umidade em %): leitura com sensor seco no ar e com a haste na água.
const int ADC_SECO    = 850;
const int ADC_MOLHADO = 400;
// Café, cana e milho (esgotamento): leitura no SEU solo na capacidade de campo (CC)
// e no ponto de murcha permanente (PMP)
const int ADC_CC  = 500;
const int ADC_PMP = 750;
//nao irriga
const int ADC_MIN_VALIDO = 20;
const int ADC_MAX_VALIDO = 1010;

// Esgotamento de referência D_lim (%) de cada cultura (ordem: soja, café, cana, milho).
// Café: 50 % (critério experimental da Embrapa Cerrados). Cana e milho: 45 % é só
// ponto de partida dentro da faixa 40-50 % do documento. SUBSTITUA por recomendação local.
const float D_LIM[] = { 0.0, 50.0, 45.0, 45.0 };

// Temperatura fixa SÓ PARA TESTE enquanto o DS18B20 não estiver ligado.
const bool  USAR_TEMPERATURA_FIXA = false;
const float TEMPERATURA_FIXA_C    = 25.0;

const unsigned long INTERVALO_DECISAO_MS = 5000UL;  //5s

const float VAZAO_L_H     = 200.0; 
const float VOLUME_MAX_ML = 500.0; 
const float DOSE_MINIMA   = 0.05;   
const int N_PASSOS = 200;           // resolução da defuzzificação

enum Cultura { SOJA = 0, CAFE = 1, CANA = 2, MILHO = 3 };
const char* NOME_CULTURA[] = { "soja", "cafe", "cana", "milho" };

ESP8266WebServer server(80);
OneWire oneWire(PIN_DS18B20);
DallasTemperature sensorTemp(&oneWire);

Cultura culturaAtual = SOJA;          // ao ligar, começa na soja
bool faseCritica = false;             // estádio de maior prioridade hídrica?
bool irrigacaoBloqueada = false;      // bloqueio de manejo

bool bombaLigada = false;
unsigned long bombaFimMs = 0;
unsigned long ultimaDecisaoMs = 0;

int   ultimoBruto = 0;
float ultimaEntrada = 0.0;            // U (%) na soja, Q (%) nas demais
bool  ultimaEntradaEhQ = false;
float ultimaTemperatura = 0.0;
float ultimaDose = 0.0;
float ultimoVolumeMl = 0.0;
float ultimoTempoS = 0.0;
String ultimoMotivo = "aguardando primeira leitura";

float trapezio(float x, float a, float b, float c, float d) {
  if (x < a || x > d) return 0.0;
  if (x >= b && x <= c) return 1.0;
  if (x < b) return (x - a) / (b - a);
  return (d - x) / (d - c);
}

float triangulo(float x, float a, float b, float c) {
  return trapezio(x, a, b, b, c);
}

float uBaixa(float u)    { return trapezio(u, 0, 0, 45, 55); }
float uMedia(float u)    { return trapezio(u, 45, 55, 55, 65); }
float uAdequada(float u) { return trapezio(u, 55, 65, 75, 85); }
float uAlta(float u)     { return trapezio(u, 75, 85, 100, 100); }
float tBaixa(float t)    { return trapezio(t, 10, 10, 15, 20); }
float tAdequada(float t) { return trapezio(t, 15, 20, 30, 35); }
float tAlta(float t)     { return trapezio(t, 30, 35, 45, 45); }
// --- Café, cana e milho: esgotamento normalizado Q (%) ---
float qBaixo(float q)    { return trapezio(q, 0, 0, 20, 40); }
float qAdequado(float q) { return trapezio(q, 20, 40, 70, 100); }
float qAtencao(float q)  { return trapezio(q, 70, 100, 120, 140); }
float qAlto(float q)     { return trapezio(q, 120, 140, 200, 200); }
// --- Café, cana e milho: temperatura do ar (°C) ---
float ctBaixa(float t)    { return trapezio(t, 5, 5, 15, 20); }
float ctModerada(float t) { return trapezio(t, 15, 20, 28, 33); }
float ctElevada(float t)  { return trapezio(t, 28, 33, 45, 45); }
float doseZero(float d)  { return trapezio(d, 0.0, 0.0, 0.033, 0.083); }
float doseCurta(float d) { return triangulo(d, 0.083, 0.25, 0.417); }
float doseMedia(float d) { return triangulo(d, 0.333, 0.5, 0.667); }
float doseLonga(float d) { return trapezio(d, 0.583, 0.75, 1.0, 1.0); }
float defuzzifica(float forcaZero, float forcaCurto, float forcaMedio, float forcaLongo) {
  float numerador = 0.0;
  float denominador = 0.0;
  for (int i = 0; i <= N_PASSOS; i++) {
    float d = (float)i / N_PASSOS;
    float mu = 0.0;
    mu = max(mu, min(forcaZero,  doseZero(d)));
    mu = max(mu, min(forcaCurto, doseCurta(d)));
    mu = max(mu, min(forcaMedio, doseMedia(d)));
    mu = max(mu, min(forcaLongo, doseLonga(d)));
    numerador   += d * mu;
    denominador += mu;
  }
  if (denominador <= 0.0) return 0.0;
  return numerador / denominador;
}
float calculaDoseSoja(float umidade, float temperatura) {
  umidade     = constrain(umidade, 0.0, 100.0);
  temperatura = constrain(temperatura, 10.0, 45.0);

  float uB = uBaixa(umidade);
  float uM = uMedia(umidade);
  float uA = uAdequada(umidade);
  float uH = uAlta(umidade);
  float tB = tBaixa(temperatura);
  float tA = tAdequada(temperatura);
  float tH = tAlta(temperatura);

  // soja aqui
  float forcaZero  = max(uH, min(uA, tA));
  float forcaCurto = max(max(min(uA, tB), min(uA, tH)), min(uM, tA));
  float forcaMedio = max(max(min(uM, tB), min(uM, tH)), min(uB, tB));
  float forcaLongo = max(min(uB, tA), min(uB, tH));

  return defuzzifica(forcaZero, forcaCurto, forcaMedio, forcaLongo);
}

// cafe, cana e milho aqui
// critica: 1.0 = fase crítica, 0.0 = fase de menor prioridade (aceita valores intermediários)
float calculaDoseQ(float q, float temperatura, float critica) {
  q           = constrain(q, 0.0, 200.0);
  temperatura = constrain(temperatura, 5.0, 45.0);
  critica     = constrain(critica, 0.0, 1.0);
  float menor = 1.0 - critica;

  float qB = qBaixo(q);
  float qA = qAdequado(q);
  float qT = qAtencao(q);
  float qH = qAlto(q);
  float tB = ctBaixa(temperatura);
  float tM = ctModerada(temperatura);
  float tE = ctElevada(temperatura);

  float forcaZero  = max(qB, max(min(qA, menor), min(qA, min(critica, max(tB, tM)))));
  float forcaCurto = max(max(min(qA, min(critica, tE)), min(qT, menor)),
                         min(qT, min(critica, tB)));
  float forcaMedio = min(qT, min(critica, max(tM, tE)));
  float forcaLongo = qH;

  return defuzzifica(forcaZero, forcaCurto, forcaMedio, forcaLongo);
}

int lerAdcMedio() {
  long soma = 0;
  const int N = 20;  //pega média
  for (int i = 0; i < N; i++) {
    soma += analogRead(PIN_UMIDADE);
    delay(5);
  }
  return (int)(soma / N);
}

float brutoParaUmidadePct(int bruto) {
  float pct = (bruto - ADC_SECO) * 100.0 / (float)(ADC_MOLHADO - ADC_SECO);
  return constrain(pct, 0.0, 100.0);
}

// Valores negativos viram 0; acima de 100 indicam que passou da faixa de água disponível.
float brutoParaEsgotamento(int bruto) {
  float d = 100.0 * (bruto - ADC_CC) / (float)(ADC_PMP - ADC_CC);
  return constrain(d, 0.0, 1000.0);
}

float lerTemperatura() {
  if (USAR_TEMPERATURA_FIXA) return TEMPERATURA_FIXA_C;
  sensorTemp.requestTemperatures();
  return sensorTemp.getTempCByIndex(0);  // -127 indica falha de leitura
}

void ligarBomba() {
  digitalWrite(PIN_BOMBA, RELE_ATIVO_BAIXO ? LOW : HIGH);
  bombaLigada = true;
}

void desligarBomba() {
  digitalWrite(PIN_BOMBA, RELE_ATIVO_BAIXO ? HIGH : LOW);
  bombaLigada = false;
}

void executaCiclo() {
  int bruto = lerAdcMedio();
  float temperatura = lerTemperatura();
  ultimoBruto = bruto;

  if (temperatura < -100.0) {
    ultimoMotivo = "falha no DS18B20";
    ultimaDose = 0.0; ultimoVolumeMl = 0.0; ultimoTempoS = 0.0;
    Serial.println("Falha no DS18B20: verifique a ligacao. Irrigacao cancelada.");
    return;
  }
  ultimaTemperatura = temperatura;

  float dose = 0.0;
  if (bruto <= ADC_MIN_VALIDO || bruto >= ADC_MAX_VALIDO) {
    ultimoMotivo = "sensor de umidade invalido";
  } else if (irrigacaoBloqueada) {
    ultimoMotivo = "irrigacao bloqueada por manejo";
  } else if (culturaAtual == SOJA) {
    ultimaEntrada = brutoParaUmidadePct(bruto);
    ultimaEntradaEhQ = false;
    dose = calculaDoseSoja(ultimaEntrada, temperatura);
    ultimoMotivo = "ok";
  } else {
    float d = brutoParaEsgotamento(bruto);
    ultimaEntrada = 100.0 * d / D_LIM[culturaAtual];  
    ultimaEntradaEhQ = true;
    dose = calculaDoseQ(ultimaEntrada, temperatura, faseCritica ? 1.0 : 0.0);
    ultimoMotivo = "ok";
  }

  float volumeMl = dose * VOLUME_MAX_ML;
  float vazaoMlS = VAZAO_L_H * 1000.0 / 3600.0;
  float tIrrig = volumeMl / vazaoMlS;
  ultimaDose = dose; ultimoVolumeMl = volumeMl; ultimoTempoS = tIrrig;

  if (USAR_TEMPERATURA_FIXA) Serial.println("[TESTE] temperatura FIXA (sem DS18B20)");
  Serial.print("cultura = "); Serial.print(NOME_CULTURA[culturaAtual]);
  Serial.print(faseCritica ? " (fase critica)" : " (fase normal)");
  Serial.print("  |  A0 = "); Serial.print(bruto);
  Serial.print("  |  "); Serial.print(ultimaEntradaEhQ ? "Q = " : "U = ");
  Serial.print(ultimaEntrada, 1);
  Serial.print("  |  T = "); Serial.print(temperatura, 1);
  Serial.print(" C  |  dose = "); Serial.print(dose * 100.0, 1);
  Serial.print(" %  |  volume = "); Serial.print(volumeMl, 0);
  Serial.print(" mL  |  t* = "); Serial.print(tIrrig, 1);
  Serial.println(" s");
  if (ultimoMotivo != "ok") Serial.println(ultimoMotivo);

  if (dose >= DOSE_MINIMA) {
    bombaFimMs = millis() + (unsigned long)(tIrrig * 1000.0);
    ligarBomba();
    ultimoMotivo = "irrigando";
    Serial.println("Bomba ligada.");
  } else {
    if (ultimoMotivo == "ok") ultimoMotivo = "sem necessidade de irrigar";
    Serial.println("Sem necessidade de irrigar.");
  }
}


//web
// Extrai o texto de "chave":"valor" de um JSON simples.
String extraiTexto(const String& corpo, const char* chave) {
  String busca = String("\"") + chave + "\"";
  int i = corpo.indexOf(busca);
  if (i < 0) return "";
  i = corpo.indexOf(':', i + busca.length());
  if (i < 0) return "";
  int ini = corpo.indexOf('"', i + 1);
  if (ini < 0) return "";
  int fim = corpo.indexOf('"', ini + 1);
  if (fim < 0) return "";
  return corpo.substring(ini + 1, fim);
}

int extraiBool(const String& corpo, const char* chave) {
  String busca = String("\"") + chave + "\"";
  int i = corpo.indexOf(busca);
  if (i < 0) return -1;
  i = corpo.indexOf(':', i + busca.length());
  if (i < 0) return -1;
  String resto = corpo.substring(i + 1);
  resto.trim();
  if (resto.startsWith("true"))  return 1;
  if (resto.startsWith("false")) return 0;
  return -1;
}


//aceita qualquer coisa
int identificaCultura(String nome) {
  nome.trim();
  nome.toLowerCase();
  if (nome.startsWith("soja"))  return SOJA;
  if (nome.startsWith("caf"))   return CAFE;
  if (nome.startsWith("cana"))  return CANA;
  if (nome.startsWith("milho")) return MILHO;
  return -1;
}

void enviaCors() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
  server.sendHeader("Access-Control-Allow-Private-Network", "true");
  server.sendHeader("Access-Control-Max-Age", "600");
}

void respondeJson(int codigo, const String& corpo) {
  enviaCors();
  server.send(codigo, "application/json", corpo);
}

String statusJson() {
  String s = "{\"ok\":true,\"cultura\":\"";
  s += NOME_CULTURA[culturaAtual];
  s += "\",\"fase_critica\":";
  s += faseCritica ? "true" : "false";
  s += ",\"bloqueio\":";
  s += irrigacaoBloqueada ? "true" : "false";
  s += ",\"leitura_bruta\":";
  s += String(ultimoBruto, 0);
  s += ",\"entrada_tipo\":\"";
  s += ultimaEntradaEhQ ? "Q" : "U";
  s += "\",\"entrada\":";
  s += String(ultimaEntrada, 1);
  s += ",\"temperatura_c\":";
  s += String(ultimaTemperatura, 1);
  s += ",\"dose_pct\":";
  s += String(ultimaDose * 100.0, 1);
  s += ",\"volume_ml\":";
  s += String(ultimoVolumeMl, 0);
  s += ",\"tempo_s\":";
  s += String(ultimoTempoS, 1);
  s += ",\"bomba_ligada\":";
  s += bombaLigada ? "true" : "false";
  s += ",\"motivo\":\"";
  s += ultimoMotivo;
  s += "\"}";
  return s;
}

void tratarOpcoes() {
  enviaCors();
  server.send(204);
}

void tratarStatus() {
  respondeJson(200, statusJson());
}

void tratarCulturaPost() {
  String corpo = server.arg("plain");
  int c = identificaCultura(extraiTexto(corpo, "cultura"));
  if (c < 0) {
    respondeJson(400, "{\"ok\":false,\"erro\":\"cultura desconhecida\"}");
    return;
  }
  culturaAtual = (Cultura)c;
  int fase = extraiBool(corpo, "fase_critica");
  if (fase >= 0) faseCritica = (fase == 1);
  int bloqueio = extraiBool(corpo, "bloqueio");
  if (bloqueio >= 0) irrigacaoBloqueada = (bloqueio == 1);

  Serial.print("Cultura selecionada pelo site: ");
  Serial.println(NOME_CULTURA[culturaAtual]);

  ultimaDecisaoMs = millis() - INTERVALO_DECISAO_MS;  // decide logo (se a bomba estiver parada)
  respondeJson(200, statusJson());
}

void tratarNaoEncontrado() {
  if (server.method() == HTTP_OPTIONS) {
    tratarOpcoes();
  } else {
    respondeJson(404, "{\"ok\":false,\"erro\":\"rota nao encontrada\"}");
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_BOMBA, OUTPUT);
  desligarBomba();
  sensorTemp.begin();

  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_SENHA);
  Serial.println();
  Serial.print("Rede Wi-Fi: "); Serial.println(AP_SSID);
  Serial.print("Endereco do controlador: http://");
  Serial.println(WiFi.softAPIP().toString());

  server.on("/api/cultura", HTTP_POST, tratarCulturaPost);
  server.on("/api/cultura", HTTP_OPTIONS, tratarOpcoes);
  server.on("/api/cultura", HTTP_GET, tratarStatus);
  server.on("/api/status", HTTP_GET, tratarStatus);
  server.onNotFound(tratarNaoEncontrado);
  server.begin();

  Serial.println("Irrigacao Fuzzy (miniservidor) iniciada. Cultura inicial: soja");
  ultimaDecisaoMs = millis() - INTERVALO_DECISAO_MS;  // decide logo no início
}

void loop() {
  server.handleClient();
  unsigned long agora = millis();

  if (bombaLigada && (long)(agora - bombaFimMs) >= 0) {
    desligarBomba();
    Serial.println("Bomba desligada.");
  }

  if (!bombaLigada && (agora - ultimaDecisaoMs >= INTERVALO_DECISAO_MS)) {
    ultimaDecisaoMs = agora;
    executaCiclo();
  }
}