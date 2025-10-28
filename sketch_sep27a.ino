#include <WiFi.h>
#include <PubSubClient.h>
#include <ESP32Servo.h>
#include "DHT.h"
#include <HTTPClient.h>
#include <UrlEncode.h>

// ==========================================================
// CONFIGURAÇÃO DE REDE
// ==========================================================
const char* ssid = "Guga";
const char* password = "seila1234";

// ==========================================================
// CONFIGURAÇÃO DO BROKER MQTT
// ==========================================================
const char* mqttServer = "test.mosquitto.org";
const int mqttPort = 1883;
const char* mqttTopic = "esp32/robo/comando";

WiFiClient espClient;
PubSubClient client(espClient);

// ==========================================================
// CALLMEBOT (para alerta via WhatsApp)
// ==========================================================
String phoneNumber = "557188939694"; // Seu número (sem o 9)
String apiKey = "4171698";           // Sua APIKEY do Callmebot

void sendMessage(String message) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[CALLMEBOT] WiFi desconectado, não é possível enviar mensagem.");
    return;
  }

  String url = "https://api.callmebot.com/whatsapp.php?phone=" + phoneNumber +
               "&text=" + urlEncode(message) +
               "&apikey=" + apiKey;

  HTTPClient http;
  http.begin(url);
  int httpResponseCode = http.GET();  // ✅ Callmebot usa GET

  if (httpResponseCode == 200) {
    Serial.println("[CALLMEBOT] ✅ Mensagem enviada com sucesso!");
  } else {
    Serial.print("[CALLMEBOT] ❌ Erro ao enviar mensagem. Código: ");
    Serial.println(httpResponseCode);
  }

  http.end();
}

// ==========================================================
// PINOS E COMPONENTES
// ==========================================================
#define SERVO_ESQ 14
#define SERVO_DIR 27
#define LED_VERDE 12
#define LED_VERMELHO 13
#define PIR_PIN 26
#define DHT_PIN 15
#define LDR_PIN 34
#define BTN_PIN 25  // Botão LIGA/DESLIGA

Servo servoEsq;
Servo servoDir;
#define DHTTYPE DHT11
DHT dht(DHT_PIN, DHTTYPE);

// ==========================================================
// VARIÁVEIS DE ESTADO
// ==========================================================
bool roboLigado = true;
bool botaoAnterior = HIGH;
unsigned long ultimoDebounce = 0;
const unsigned long tempoDebounce = 300;

// ==========================================================
// FUNÇÕES DE MOVIMENTO
// ==========================================================
void pararRobo() {
  servoEsq.write(90);
  servoDir.write(90);
  Serial.println("[AÇÃO] Motores parados.");
}

void moverFrente() {
  servoEsq.write(0);
  servoDir.write(180);
  Serial.println("[AÇÃO] Movendo para frente.");
}

void moverTras() {
  servoEsq.write(180);
  servoDir.write(0);
  Serial.println("[AÇÃO] Movendo para trás.");
}

void virarEsquerda() {
  servoEsq.write(90);
  servoDir.write(180);
  Serial.println("[AÇÃO] Virando à esquerda.");
}

void virarDireita() {
  servoEsq.write(0);
  servoDir.write(90);
  Serial.println("[AÇÃO] Virando à direita.");
}

// ==========================================================
// CALLBACK MQTT
// ==========================================================
void callback(char* topic, byte* payload, unsigned int length) {
  String comando = "";
  for (int i = 0; i < length; i++) comando += (char)payload[i];
  comando.trim();

  Serial.printf("\n[MQTT] Mensagem recebida: %s\n", comando.c_str());

  if (comando == "DESLIGAR") {
    roboLigado = false;
    pararRobo();
    digitalWrite(LED_VERDE, LOW);
    digitalWrite(LED_VERMELHO, HIGH);
    Serial.println("[STATUS] Robô desligado remotamente!");
  } else if (comando == "LIGAR") {
    roboLigado = true;
    digitalWrite(LED_VERDE, HIGH);
    digitalWrite(LED_VERMELHO, LOW);
    Serial.println("[STATUS] Robô ligado novamente!");
  } else if (roboLigado) {
    if (comando == "FRENTE") moverFrente();
    else if (comando == "TRAS") moverTras();
    else if (comando == "ESQUERDA") virarEsquerda();
    else if (comando == "DIREITA") virarDireita();
    else if (comando == "PARADO") pararRobo();
  }
}

// ==========================================================
// CONEXÕES
// ==========================================================
void conectarWiFi() {
  Serial.printf("[WIFI] Conectando a %s", ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n[WIFI] ✅ Conectado!");
}

void conectarMQTT() {
  while (!client.connected()) {
    Serial.println("[MQTT] Tentando conexão...");
    if (client.connect("ESP32_Robo_Fisico")) {
      Serial.println("[MQTT] ✅ Conectado!");
      client.subscribe(mqttTopic);
    } else {
      Serial.println("[MQTT] ❌ Falha, tentando novamente...");
      delay(2000);
    }
  }
}

// ==========================================================
// CÁLCULO DE PROBABILIDADE
// ==========================================================
float calcularProbabilidade(float temp, float umid, int luz, bool presenca) {
  float prob = 0;
  if (temp >= 15 && temp <= 30) prob += 25;
  if (umid >= 40 && umid <= 70) prob += 25;
  if (luz > 2000) prob += 20;
  if (presenca) prob += 30;
  return prob;
}

// ==========================================================
// ENVIO PARA BACKEND PYTHON (HTTP POST)
// ==========================================================
void enviarDadosServidor(float temperatura, float umidade, int luz, bool presenca, float prob) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[HTTP] WiFi desconectado, não foi possível enviar dados.");
    return;
  }

  HTTPClient http;
  http.begin("http://10.219.216.239:5000/leituras"); // 🔹 Troque pelo IP do servidor Python
  http.addHeader("Content-Type", "application/json");

  String jsonData = "{";
  jsonData += "\"timestamp\":\"" + String(millis()) + "\",";
  jsonData += "\"temperatura_c\":" + String(temperatura, 1) + ",";
  jsonData += "\"umidade_pct\":" + String(umidade, 0) + ",";
  jsonData += "\"luminosidade\":" + String(luz) + ",";
  jsonData += "\"presenca\":" + String(presenca ? 1 : 0) + ",";
  jsonData += "\"probabilidade_vida\":" + String(prob, 1);
  jsonData += "}";

  int httpResponseCode = http.POST(jsonData);

  if (httpResponseCode > 0) {
    Serial.printf("[HTTP] Dados enviados com sucesso (%d)\n", httpResponseCode);
  } else {
    Serial.printf("[HTTP] Falha ao enviar (%d)\n", httpResponseCode);
  }

  http.end();
}

// ==========================================================
// SETUP
// ==========================================================
void setup() {
  Serial.begin(115200);
  pinMode(LED_VERDE, OUTPUT);
  pinMode(LED_VERMELHO, OUTPUT);
  pinMode(PIR_PIN, INPUT);
  pinMode(LDR_PIN, INPUT);
  pinMode(BTN_PIN, INPUT_PULLUP);

  servoEsq.attach(SERVO_ESQ);
  servoDir.attach(SERVO_DIR);
  dht.begin();

  conectarWiFi();
  client.setServer(mqttServer, mqttPort);
  client.setCallback(callback);
  conectarMQTT();

  digitalWrite(LED_VERDE, HIGH);
  digitalWrite(LED_VERMELHO, LOW);

  Serial.println("[SISTEMA] Robô pronto para explorar!");
}

// ==========================================================
// LOOP PRINCIPAL
// ==========================================================
bool hasSend = false;

void loop() {
  if (!client.connected()) conectarMQTT();
  client.loop();

  // --- Lógica do botão com clique único ---
  int leituraAtual = digitalRead(BTN_PIN);
  if (leituraAtual != botaoAnterior) {
    if (leituraAtual == LOW && (millis() - ultimoDebounce > tempoDebounce)) {
      roboLigado = !roboLigado;
      if (!roboLigado) {
        pararRobo();
        digitalWrite(LED_VERDE, LOW);
        digitalWrite(LED_VERMELHO, HIGH);
        Serial.println("[BOTÃO] Robô desligado manualmente.");
      } else {
        digitalWrite(LED_VERDE, HIGH);
        digitalWrite(LED_VERMELHO, LOW);
        Serial.println("[BOTÃO] Robô religado manualmente.");
      }
      ultimoDebounce = millis();
    }
  }
  botaoAnterior = leituraAtual;

  if (!roboLigado) return;  // Robô desligado — não faz nada

  // --- Leitura dos sensores a cada 2 segundos ---
  static unsigned long anterior = 0;
  if (millis() - anterior >= 10000) {
    anterior = millis();

    float temperature = dht.readTemperature();
    float humidity = dht.readHumidity();
    int luz = analogRead(LDR_PIN);
    bool presenca = digitalRead(PIR_PIN);

    if (isnan(temperature) || isnan(humidity)) {
      Serial.println("[ERRO] Falha ao ler DHT11!");
      return;
    }

    float prob = calcularProbabilidade(temperature, humidity, luz, presenca);

    Serial.printf("Temp: %.1f°C | Umid: %.0f%% | Luz: %d | Presença: %s | Probabilidade: %.0f%%\n",
                  temperature, humidity, luz, presenca ? "Sim" : "Não", prob);

    if (prob > 75) {
      digitalWrite(LED_VERDE, LOW);
      digitalWrite(LED_VERMELHO, HIGH);
      Serial.println("⚠️ ALERTA! Alta probabilidade de vida detectada!");
      if (!hasSend) {
        hasSend = true;
        sendMessage("⚠️ Alerta! Alta probabilidade de vida detectada no planeta!");
      }
    } else {
      digitalWrite(LED_VERDE, HIGH);
      digitalWrite(LED_VERMELHO, LOW);
      Serial.println("Exploração normal. Nenhum indício relevante detectado.");
      hasSend = false;
    }

    // Envio para o servidor Python
    enviarDadosServidor(temperature, humidity, luz, presenca, prob);
  }
}
