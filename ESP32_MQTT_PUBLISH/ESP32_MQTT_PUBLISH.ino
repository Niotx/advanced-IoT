#include <WiFi.h>
#include <Wire.h>
#include <PubSubClient.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>
#include "secrets.h"  // Copy secrets.h.example to secrets.h and edit values.

// -------------------- Hardware --------------------
#define DHT_PIN 4
#define DHT_TYPE DHT22
#define OLED_SDA 21
#define OLED_SCL 22
#define OLED_ADDRESS 0x3C
#define OLED_WIDTH 128
#define OLED_HEIGHT 64
#define OLED_RESET -1

// -------------------- MQTT topics --------------------
const char* DEVICE_ID = "esp32-room-01";
const char* TOPIC_TELEMETRY = "iot/room-01/telemetry";
const char* TOPIC_STATUS = "iot/room-01/status";

// DHT22 requires at least ~2 seconds between measurements.
const unsigned long SAMPLE_INTERVAL_MS = 2500UL;
const unsigned long WIFI_RETRY_MS = 10000UL;
const unsigned long MQTT_RETRY_MS = 5000UL;

DHT dht(DHT_PIN, DHT_TYPE);
Adafruit_SSD1306 display(OLED_WIDTH, OLED_HEIGHT, &Wire, OLED_RESET);
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

unsigned long lastSampleMs = 0;
unsigned long lastWifiAttemptMs = 0;
unsigned long lastMqttAttemptMs = 0;

void drawHeader() {
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("DHT22 Monitor");
  display.drawLine(0, 12, OLED_WIDTH - 1, 12, SSD1306_WHITE);
}

void showSensorError() {
  display.clearDisplay();
  drawHeader();
  display.setCursor(0, 22);
  display.println("Sensor Error!");
  display.setCursor(0, 43);
  display.println(mqttClient.connected() ? "MQTT: online" : "MQTT: offline");
  display.display();
}

void showReadings(float temperature, float humidity) {
  display.clearDisplay();
  drawHeader();
  display.setCursor(0, 18);
  display.print("Temperature:");
  display.setTextSize(2);
  display.setCursor(15, 29);
  display.print(temperature, 1);
  display.println(" C");
  display.setTextSize(1);
  display.setCursor(0, 52);
  display.print("Humidity: ");
  display.print(humidity, 1);
  display.print(" %");
  display.display();
}

void maintainWiFi(unsigned long now) {
  if (WiFi.status() == WL_CONNECTED) {
    return;
  }

  if (now - lastWifiAttemptMs < WIFI_RETRY_MS) {
    return;
  }

  lastWifiAttemptMs = now;
  Serial.println("Wi-Fi: attempting connection...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

void maintainMQTT(unsigned long now) {
  if (WiFi.status() != WL_CONNECTED) {
    return;
  }

  if (mqttClient.connected()) {
    mqttClient.loop();  // Handle keepalive traffic.
    return;
  }

  if (now - lastMqttAttemptMs < MQTT_RETRY_MS) {
    return;
  }

  lastMqttAttemptMs = now;
  Serial.println("MQTT: attempting connection...");

  // Last Will: if the connection fails unexpectedly, the broker eventually
  // publishes a retained "offline" status on TOPIC_STATUS.
  bool connected = mqttClient.connect(
    DEVICE_ID,
    MQTT_USERNAME,
    MQTT_PASSWORD,
    TOPIC_STATUS,
    1,     // Last Will QoS
    true,  // Retain Last Will
    "offline"
  );

  if (connected) {
    Serial.println("MQTT: connected");
    // Overwrite the previously retained "offline" state.
    mqttClient.publish(TOPIC_STATUS, "online", true);
  } else {
    Serial.print("MQTT: failed, state=");
    Serial.println(mqttClient.state());
  }
}

void readAndPublish() {
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();

  if (isnan(temperature) || isnan(humidity)) {
    Serial.println("DHT22: sensor reading failed");
    showSensorError();
    return;
  }

  Serial.print("Temperature: ");
  Serial.print(temperature, 1);
  Serial.print(" C, Humidity: ");
  Serial.print(humidity, 1);
  Serial.println(" %");
  showReadings(temperature, humidity);

  if (!mqttClient.connected()) {
    Serial.println("MQTT: offline, measurement not sent");
    return;
  }

  // uptime_ms is NOT a wall-clock timestamp. NTP is covered in a later lesson.
  char payload[192];
  int length = snprintf(
    payload, sizeof(payload),
    "{\"device_id\":\"%s\",\"temperature\":%.1f,\"humidity\":%.1f,\"uptime_ms\":%lu}",
    DEVICE_ID,
    temperature,
    humidity,
    (unsigned long)millis()
  );

  if (length < 0 || length >= (int)sizeof(payload)) {
    Serial.println("MQTT: JSON payload too large");
    return;
  }

  // PubSubClient publishes telemetry at QoS 0; no retained telemetry.
  if (mqttClient.publish(TOPIC_TELEMETRY, payload, false)) {
    Serial.print("MQTT published: ");
    Serial.println(payload);
  } else {
    Serial.println("MQTT: publish failed");
  }
}

void setup() {
  Serial.begin(115200);
  Wire.begin(OLED_SDA, OLED_SCL);
  dht.begin();

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    Serial.println("OLED not found! Verify wiring and I2C address.");
    while (true) {
      delay(1000);
    }
  }

  display.setTextColor(SSD1306_WHITE);
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 20);
  display.println("ESP32 MQTT Monitor");
  display.display();

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  lastWifiAttemptMs = millis();

  mqttClient.setServer(MQTT_HOST, MQTT_PORT);
  if (!mqttClient.setBufferSize(512)) {
    Serial.println("MQTT: could not allocate buffer");
  }
  mqttClient.setSocketTimeout(2);  // seconds; each connect attempt can still block.

  // The first measurement is taken immediately on entering loop().
  lastSampleMs = millis() - SAMPLE_INTERVAL_MS;
}

void loop() {
  unsigned long now = millis();
  maintainWiFi(now);
  maintainMQTT(now);

  if (now - lastSampleMs >= SAMPLE_INTERVAL_MS) {
    lastSampleMs = now;
    readAndPublish();
  }
}
