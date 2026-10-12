/*
  Advanced IoT - Part 08: MQTT over TLS (server-authenticated TLS)
  ESP32 DevKit V1 + DHT22 (GPIO4) + SSD1306 OLED (SDA21 / SCL22).

  This is a standalone lesson example, NOT a replacement for the offline-buffering
  sketch. MQTT telemetry is QoS 0; network/TLS connection attempts may briefly block.

  Setup:
    1) Copy secrets.h.example to secrets.h (same sketch directory).
    2) Fill in Wi-Fi, MQTT credentials, broker host, and entire root CA PEM.
    3) Ensure broker cert SAN matches MQTT_HOST. Do NOT use setInsecure().
*/
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <Wire.h>
#include <PubSubClient.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>
#include <time.h>
#include <math.h>
#include <string.h>
#include "secrets.h"

// ----- Hardware -----
constexpr uint8_t DHT_PIN = 4;
constexpr uint8_t OLED_SDA = 21;
constexpr uint8_t OLED_SCL = 22;
constexpr uint8_t OLED_ADDR = 0x3C;
constexpr uint8_t OLED_WIDTH = 128;
constexpr uint8_t OLED_HEIGHT = 64;
constexpr int8_t OLED_RESET = -1;

// ----- MQTT -----
constexpr char DEVICE_ID[] = "esp32-room-01";
constexpr char TOPIC_TELEMETRY[] = "iot/room-01/telemetry";
constexpr char TOPIC_STATUS[] = "iot/room-01/status";

// DHT22 readings must be at least about 2 seconds apart.
constexpr unsigned long SAMPLE_INTERVAL_MS = 2500UL;
constexpr unsigned long WIFI_RETRY_MS = 10000UL;
constexpr unsigned long MQTT_RETRY_MS = 5000UL;
constexpr time_t MIN_REASONABLE_UNIX_TIME = 1704067200;  // 2024-01-01 UTC.

DHT dht(DHT_PIN, DHT22);
Adafruit_SSD1306 display(OLED_WIDTH, OLED_HEIGHT, &Wire, OLED_RESET);
WiFiClientSecure tlsClient;
PubSubClient mqttClient(tlsClient);

bool displayReady = false;
bool ntpRequested = false;
unsigned long lastSampleMs = 0;
unsigned long lastWifiAttemptMs = 0;
unsigned long lastMqttAttemptMs = 0;

bool clockIsPlausible() {
  // Plausible time is necessary for X.509 validation, but is NOT a proof of
  // trustworthy time. Production systems need an appropriate time trust model.
  return time(nullptr) >= MIN_REASONABLE_UNIX_TIME;
}

void drawHeader() {
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("DHT22 | MQTT TLS");
  display.drawLine(0, 11, OLED_WIDTH - 1, 11, SSD1306_WHITE);
}

void showReading(float temperature, float humidity, bool valid) {
  if (!displayReady) return;
  display.clearDisplay();
  drawHeader();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 17);
  if (valid) {
    display.print("Temp: ");
    display.print(temperature, 1);
    display.println(" C");
    display.print("Humidity: ");
    display.print(humidity, 1);
    display.println(" %");
  } else {
    display.println("Sensor reading failed!");
  }
  display.setCursor(0, 43);
  display.print("WiFi: ");
  display.println(WiFi.status() == WL_CONNECTED ? "OK" : "OFFLINE");
  display.print("TLS: ");
  if (!clockIsPlausible()) {
    display.println("WAIT NTP");
  } else {
    display.println(mqttClient.connected() ? "CONNECTED" : "OFFLINE");
  }
  display.display();
}

void maintainWiFi(unsigned long now) {
  if (WiFi.status() == WL_CONNECTED) {
    if (!ntpRequested) {
      // Asynchronous SNTP initialization: don't wait in a long loop.
      configTime(0, 0, "pool.ntp.org", "time.google.com");
      ntpRequested = true;
      Serial.println("[INFO] NTP sync requested");
    }
    return;
  }
  if (now - lastWifiAttemptMs < WIFI_RETRY_MS) return;
  lastWifiAttemptMs = now;
  Serial.println("[WARN] Wi-Fi disconnected; retrying...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

void maintainMQTT(unsigned long now) {
  if (WiFi.status() != WL_CONNECTED) return;
  if (mqttClient.connected()) {
    mqttClient.loop();
    return;
  }
  if (!clockIsPlausible()) {
    // X.509 certificate notBefore/notAfter checks need a plausible clock.
    // Sensor + OLED continue while waiting for NTP.
    return;
  }
  if (now - lastMqttAttemptMs < MQTT_RETRY_MS) return;
  lastMqttAttemptMs = now;
  Serial.print("[INFO] Connecting to MQTT over TLS: ");
  Serial.print(MQTT_HOST);
  Serial.print(":");
  Serial.println(MQTT_PORT);

  const bool ok = mqttClient.connect(
    DEVICE_ID,
    MQTT_USERNAME,
    MQTT_PASSWORD,
    TOPIC_STATUS,
    1,     // Last Will QoS 1 (requested at connect time)
    true,  // Retained offline state
    "offline"
  );
  if (ok) {
    Serial.println("[INFO] MQTT over TLS connected");
    if (!mqttClient.publish(TOPIC_STATUS, "online", true)) {
      Serial.println("[WARN] Failed to publish online status");
    }
  } else {
    Serial.print("[WARN] MQTT/TLS connection failed; mqtt_state=");
    Serial.println(mqttClient.state());
    // mqtt_state alone may not tell you whether TLS verification or MQTT auth failed.
    // Verify NTP, SAN, CA, ACL, broker logs, and firewall separately.
  }
}

void sampleAndPublish() {
  const float temperature = dht.readTemperature();
  const float humidity = dht.readHumidity();
  const bool valid = !isnan(temperature) && !isnan(humidity);
  showReading(temperature, humidity, valid);

  if (!valid) {
    Serial.println("[ERROR] DHT22 returned invalid measurement");
    return;
  }

  Serial.print("[INFO] Sensor T=");
  Serial.print(temperature, 1);
  Serial.print("C RH=");
  Serial.println(humidity, 1);

  if (!mqttClient.connected()) {
    Serial.println("[WARN] No secure MQTT connection; sample not sent");
    return;
  }

  // Compatible with prior Python subscriber (device_id, temperature,
  // humidity, uptime_ms); NTP-enabled time is an optional extra field.
  const time_t measuredAt = time(nullptr);
  char timestamp[32];
  if (measuredAt >= MIN_REASONABLE_UNIX_TIME) {
    snprintf(timestamp, sizeof(timestamp), "%lld", (long long)measuredAt);
  } else {
    strcpy(timestamp, "null");
  }
  char payload[256];
  const int length = snprintf(
    payload, sizeof(payload),
    "{\"device_id\":\"%s\",\"temperature\":%.1f,\"humidity\":%.1f,"
    "\"uptime_ms\":%lu,\"measured_at_unix\":%s}",
    DEVICE_ID, temperature, humidity, (unsigned long)millis(), timestamp
  );
  if (length < 0 || length >= (int)sizeof(payload)) {
    Serial.println("[ERROR] JSON payload too large");
    return;
  }

  // PubSubClient.publish() sends QoS 0 telemetry, without a delivery guarantee.
  if (mqttClient.publish(TOPIC_TELEMETRY, payload, false)) {
    Serial.print("[INFO] Published TLS telemetry: ");
    Serial.println(payload);
  } else {
    Serial.println("[WARN] MQTT publish failed");
  }
}

void setup() {
  Serial.begin(115200);
  Wire.begin(OLED_SDA, OLED_SCL);
  dht.begin();
  displayReady = display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
  if (!displayReady) {
    Serial.println("[ERROR] SSD1306 not found; continuing headless");
  } else {
    display.setTextColor(SSD1306_WHITE);
    display.clearDisplay();
    display.setCursor(0, 20);
    display.println("Starting MQTT TLS...");
    display.display();
  }

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  lastWifiAttemptMs = millis();

  // Never replace certificate validation with setInsecure().
  tlsClient.setCACert(MQTT_ROOT_CA);
  mqttClient.setServer(MQTT_HOST, MQTT_PORT);
  if (!mqttClient.setBufferSize(512)) {
    Serial.println("[ERROR] MQTT buffer allocation failed");
  }
  mqttClient.setSocketTimeout(2);
  lastSampleMs = millis() - SAMPLE_INTERVAL_MS;
}

void loop() {
  const unsigned long now = millis();
  maintainWiFi(now);
  maintainMQTT(now);
  if (millis() - lastSampleMs >= SAMPLE_INTERVAL_MS) {
    lastSampleMs = millis();
    sampleAndPublish();
  }
}
