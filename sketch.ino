// Deteksi Kebakaran Dini v2 - ESP32 + DHT22 + Gas sensor (MQ-2 Wokwi) + LDR flame + MQTT
// Status: normal -> waspada (1 sensor lewat batas) -> bahaya (>=2 sensor, terkonfirmasi, latching)

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <DHT.h>

// ---------- Konfigurasi ----------
const char* WIFI_SSID   = "Wokwi-GUEST";
const char* WIFI_PASS   = "";
const char* MQTT_HOST   = "broker.hivemq.com";
const uint16_t MQTT_PORT = 1883;
const char* DEVICE_ID   = "esp32-01";

// Backend Vercel (Telegram + Supabase). Isi setelah deploy.
// API_KEY harus sama dengan env DEVICE_API_KEY di Vercel. Jangan bagikan project Wokwi ini secara publik.
const char* API_BASE = "https://NAMA-PROJECT.vercel.app";
const char* API_KEY  = "ISI_API_KEY";

// Ganti suffix agar topik unik di broker publik (samakan dengan flow Node-RED)
#define TOPIC_BASE "kampus/iot/kebakaran/"
const char* T_TELEMETRY = TOPIC_BASE "telemetry";
const char* T_STATUS    = TOPIC_BASE "status";
const char* T_COMMAND   = TOPIC_BASE "command";
const char* T_ALERT     = TOPIC_BASE "alert";

// Pin
const int PIN_DHT = 15;
const int PIN_GAS = 35;     // AOUT sensor gas (ADC1)
const int PIN_LDR = 34;     // AO LDR (ADC1)
const int PIN_BTN_RESET = 4;
const int PIN_LED_RED = 26, PIN_LED_GREEN = 27, PIN_BUZZER = 25;

// Kalibrasi LDR: isi dari nilai RAW di Serial Monitor.
// RAW_DARK  = nilai saat gelap (tidak ada api), RAW_BRIGHT = nilai saat terang kuat (api).
// Boleh dibalik (RAW_DARK > RAW_BRIGHT atau sebaliknya), map() menangani keduanya.
const int LDR_RAW_DARK = 4000;
const int LDR_RAW_BRIGHT = 200;
// Kalibrasi gas: RAW saat udara bersih dan saat gas/asap maksimum
const int GAS_RAW_CLEAN = 0;
const int GAS_RAW_MAX = 4095;

// Ambang (contoh simulasi, justifikasi di laporan) + hysteresis
const float TEMP_ON = 50.0, TEMP_OFF = 45.0;      // deg C
const float SMOKE_ON = 60.0, SMOKE_OFF = 40.0;    // % level gas
const float FLAME_ON = 70.0, FLAME_OFF = 50.0;    // % intensitas cahaya (LDR)
const int CONFIRM_N = 3;                          // sampel berturut-turut (1 s/sampel)

// Interval (ms)
const unsigned long SAMPLE_MS = 1000;
const unsigned long TELEMETRY_MS = 5000;
const unsigned long MQTT_RETRY_MS = 5000;
const unsigned long ALERT_COOLDOWN_MS = 60000;
const unsigned long HISTORY_MS = 30000;           // simpan riwayat ke Supabase tiap 30 detik

// ---------- State ----------
DHT dht(PIN_DHT, DHT22);
WiFiClient wifi;
PubSubClient mqtt(wifi);

float temperature = NAN, humidity = NAN, smoke = 0, flame = 0;
int gasRaw = 0, ldrRaw = 0;
bool tempHigh = false, smokeHigh = false, flameHigh = false;
int activeCount = 0;
int confirmCount = 0;
bool latched = false;
const char* statusStr = "normal";
const char* lastStatus = "";

unsigned long tSample = 0, tTelemetry = 0, tMqttTry = 0, tLastAlert = 0, tHistory = 0;
bool alertSentOnce = false;
bool resetRequested = false;

// ---------- MQTT ----------
void onMessage(char* topic, byte* payload, unsigned int len) {
  String msg;
  for (unsigned int i = 0; i < len; i++) msg += (char)payload[i];
  msg.trim();
  msg.toLowerCase();
  if (msg == "reset") resetRequested = true;
}

void publishStatus() {
  JsonDocument d;
  d["device"] = DEVICE_ID;
  d["status"] = statusStr;
  d["alarm"] = latched;
  d["buzzer"] = latched;
  d["ts_ms"] = millis();
  char buf[192];
  size_t n = serializeJson(d, buf);
  mqtt.publish(T_STATUS, (const uint8_t*)buf, n, true);
}

void publishTelemetry() {
  JsonDocument d;
  d["device"] = DEVICE_ID;
  if (!isnan(temperature)) d["temperature"] = temperature;
  if (!isnan(humidity)) d["humidity"] = humidity;
  d["smoke"] = round(smoke);
  d["flame"] = round(flame);
  d["status"] = statusStr;
  d["alarm"] = latched;
  d["ts_ms"] = millis();
  char buf[256];
  size_t n = serializeJson(d, buf);
  mqtt.publish(T_TELEMETRY, (const uint8_t*)buf, n);
}

void publishAlert() {
  JsonDocument d;
  d["device"] = DEVICE_ID;
  d["event"] = "potensi_kebakaran";
  d["temperature"] = temperature;
  d["smoke"] = round(smoke);
  d["flame"] = round(flame);
  d["status"] = statusStr;
  d["ts_ms"] = millis();
  char buf[256];
  size_t n = serializeJson(d, buf);
  mqtt.publish(T_ALERT, (const uint8_t*)buf, n);
}

// Kirim pembacaan terkini ke backend Vercel (path: /api/alert atau /api/telemetry)
bool postApi(const char* path) {
  if (WiFi.status() != WL_CONNECTED) return false;
  JsonDocument d;
  d["device"] = DEVICE_ID;
  if (!isnan(temperature)) d["temperature"] = temperature;
  if (!isnan(humidity)) d["humidity"] = humidity;
  d["smoke"] = round(smoke);
  d["flame"] = round(flame);
  d["status"] = statusStr;
  d["ts_ms"] = millis();
  String body;
  serializeJson(d, body);

  WiFiClientSecure secure;
  secure.setInsecure();               // simulasi: sertifikat tidak diverifikasi
  HTTPClient http;
  http.setConnectTimeout(3000);
  http.setTimeout(4000);
  if (!http.begin(secure, String(API_BASE) + path)) return false;
  http.addHeader("Content-Type", "application/json");
  http.addHeader("x-api-key", API_KEY);
  int code = http.POST(body);
  http.end();
  Serial.printf("[HTTP] POST %s -> %d\n", path, code);
  return code >= 200 && code < 300;
}

void mqttMaintain() {
  if (WiFi.status() != WL_CONNECTED) {
    if (millis() - tMqttTry >= MQTT_RETRY_MS) {
      tMqttTry = millis();
      WiFi.begin(WIFI_SSID, WIFI_PASS);
    }
    return;
  }
  if (mqtt.connected()) { mqtt.loop(); return; }
  if (millis() - tMqttTry < MQTT_RETRY_MS) return;
  tMqttTry = millis();
  String cid = String(DEVICE_ID) + "-" + String((uint32_t)esp_random(), HEX);
  Serial.println("[MQTT] connecting...");
  if (mqtt.connect(cid.c_str(), nullptr, nullptr, T_STATUS, 0, true,
                   "{\"device\":\"esp32-01\",\"status\":\"offline\"}")) {
    mqtt.subscribe(T_COMMAND);
    publishStatus();
    Serial.println("[MQTT] connected");
  } else {
    Serial.printf("[MQTT] failed rc=%d\n", mqtt.state());
  }
}

// ---------- Logika ----------
float rawToPercent(int raw, int rawLow, int rawHigh) {
  float p = (float)(raw - rawLow) * 100.0f / (float)(rawHigh - rawLow);
  return constrain(p, 0.0f, 100.0f);
}

int readAvg(int pin) {
  long sum = 0;
  for (int i = 0; i < 8; i++) sum += analogRead(pin);
  return sum / 8;
}

void sampleSensors() {
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  if (!isnan(t)) temperature = t;   // abaikan NaN, pakai nilai terakhir
  if (!isnan(h)) humidity = h;

  gasRaw = readAvg(PIN_GAS);
  ldrRaw = readAvg(PIN_LDR);
  smoke = rawToPercent(gasRaw, GAS_RAW_CLEAN, GAS_RAW_MAX);
  flame = rawToPercent(ldrRaw, LDR_RAW_DARK, LDR_RAW_BRIGHT);

  if (temperature > TEMP_ON) tempHigh = true;
  else if (temperature <= TEMP_OFF) tempHigh = false;
  if (smoke > SMOKE_ON) smokeHigh = true;
  else if (smoke <= SMOKE_OFF) smokeHigh = false;
  if (flame > FLAME_ON) flameHigh = true;
  else if (flame <= FLAME_OFF) flameHigh = false;

  activeCount = (int)tempHigh + (int)smokeHigh + (int)flameHigh;

  // bahaya bila minimal 2 dari 3 sensor aktif dan bertahan CONFIRM_N sampel
  if (activeCount >= 2) confirmCount++;
  else confirmCount = 0;

  if (confirmCount >= CONFIRM_N) latched = true;

  if (resetRequested || digitalRead(PIN_BTN_RESET) == LOW) {
    resetRequested = false;
    latched = false;
    confirmCount = 0;
    alertSentOnce = false;
    Serial.println("[RESET] alarm direset");
  }

  if (latched) statusStr = "bahaya";
  else if (activeCount >= 1) statusStr = "waspada";
  else statusStr = "normal";
}

void driveOutputs() {
  bool blink = (millis() / 500) % 2;
  if (latched) {
    digitalWrite(PIN_LED_RED, HIGH);
    digitalWrite(PIN_LED_GREEN, LOW);
    if (blink) tone(PIN_BUZZER, 2000); else noTone(PIN_BUZZER);
  } else if (strcmp(statusStr, "waspada") == 0) {
    digitalWrite(PIN_LED_RED, blink);
    digitalWrite(PIN_LED_GREEN, LOW);
    noTone(PIN_BUZZER);
  } else {
    digitalWrite(PIN_LED_RED, LOW);
    digitalWrite(PIN_LED_GREEN, HIGH);
    noTone(PIN_BUZZER);
  }
}

void handleAlert() {
  if (!latched) return;
  bool cooled = !alertSentOnce || (millis() - tLastAlert >= ALERT_COOLDOWN_MS);
  if (cooled) {
    if (mqtt.connected()) publishAlert();
    bool ok = postApi("/api/alert");   // Telegram + log database, tidak bergantung MQTT
    alertSentOnce = true;
    tLastAlert = millis();
    Serial.printf("[ALERT] mqtt=%d http=%d\n", mqtt.connected(), ok);
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_BTN_RESET, INPUT_PULLUP);
  pinMode(PIN_LED_RED, OUTPUT);
  pinMode(PIN_LED_GREEN, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);
  dht.begin();
  analogReadResolution(12);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setBufferSize(512);
  mqtt.setCallback(onMessage);
}

void loop() {
  mqttMaintain();
  unsigned long now = millis();

  if (now - tSample >= SAMPLE_MS) {
    tSample = now;
    sampleSensors();
    // gasRaw dan ldrRaw dicetak untuk kalibrasi
    Serial.printf("T=%.1f H=%.1f | gasRaw=%d smoke=%.0f%% | ldrRaw=%d flame=%.0f%% | aktif=%d cnt=%d status=%s\n",
                  temperature, humidity, gasRaw, smoke, ldrRaw, flame, activeCount, confirmCount, statusStr);
    if (mqtt.connected() && strcmp(statusStr, lastStatus) != 0) publishStatus();
    lastStatus = statusStr;
    handleAlert();
  }

  if (now - tTelemetry >= TELEMETRY_MS) {
    tTelemetry = now;
    if (mqtt.connected()) publishTelemetry();
  }

  if (now - tHistory >= HISTORY_MS) {
    tHistory = now;
    postApi("/api/telemetry");
  }

  driveOutputs();   // lokal, tetap jalan walau MQTT putus
}
