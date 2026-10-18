/*
  ESP32 OTA Update — Advanced IoT, Slides 39–41
  Target: ESP32 DevKit V1 + DHT22 on GPIO4 + SSD1306 I2C OLED (0x3C).

  LAB ONLY: ArduinoOTA password authentication is not a replacement for
  signed firmware or authenticated, encrypted firmware transport.
  Do not expose this OTA service to the public internet.

  First install this sketch by USB using an OTA-capable partition scheme.
  Change FIRMWARE_VERSION and upload next version over trusted LAN.
  Replacing previous MQTT/TLS sketches with this independent demo removes
  their networking and buffering features; it is NOT an incremental patch.
*/

#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoOTA.h>
#include <Wire.h>
#include <DHT.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "secrets.h"

// Change to "1.1.0" for the second (network) upload.
constexpr char FIRMWARE_VERSION[] = "1.0.0";

constexpr uint8_t DHT_PIN = 4;
constexpr uint8_t OLED_SDA_PIN = 21;
constexpr uint8_t OLED_SCL_PIN = 22;
constexpr uint8_t OLED_ADDRESS = 0x3C;
constexpr uint8_t OLED_WIDTH = 128;
constexpr uint8_t OLED_HEIGHT = 64;
constexpr uint32_t SENSOR_INTERVAL_MS = 2500;  // DHT22: >=2 seconds
constexpr uint32_t WIFI_RETRY_INTERVAL_MS = 10000;
constexpr uint32_t SCREEN_INTERVAL_MS = 500;

DHT dht(DHT_PIN, DHT22);
Adafruit_SSD1306 display(OLED_WIDTH, OLED_HEIGHT, &Wire, -1);

bool displayReady = false;
bool otaStarted = false;
bool updateInProgress = false;
bool sensorValid = false;
float lastTemperature = NAN;
float lastHumidity = NAN;
uint32_t lastSampleMs = 0;
uint32_t lastWifiRetryMs = 0;
uint32_t lastScreenMs = 0;
uint8_t lastLoggedProgress = 255;

void renderNormalScreen() {
  if (!displayReady || updateInProgress) return;
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("OTA demo v");
  display.println(FIRMWARE_VERSION);

  display.setCursor(0, 16);
  if (sensorValid) {
    display.print("T:");
    display.print(lastTemperature, 1);
    display.print("C  H:");
    display.print(lastHumidity, 1);
    display.println("%");
  } else {
    display.println("DHT22: invalid / wait");
  }

  display.setCursor(0, 32);
  if (WiFi.status() == WL_CONNECTED) {
    display.print("IP:");
    display.println(WiFi.localIP());
  } else {
    display.println("Wi-Fi: disconnected");
  }
  display.setCursor(0, 48);
  display.println(otaStarted ? "OTA: READY (LAN)" : "OTA: waiting Wi-Fi");
  display.display();
}

void renderUpdateScreen(uint8_t progress) {
  if (!displayReady) return;
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("Firmware updating");
  display.setCursor(0, 22);
  display.print("Progress: ");
  display.print(progress);
  display.println("%");
  display.setCursor(0, 44);
  display.println("Keep device powered");
  display.display();
}

void startOTAService() {
  if (otaStarted || WiFi.status() != WL_CONNECTED) return;

  ArduinoOTA.setHostname(OTA_HOSTNAME);
  ArduinoOTA.setPassword(OTA_PASSWORD);

  ArduinoOTA.onStart([]() {
    updateInProgress = true;
    lastLoggedProgress = 255;
    Serial.println("[OTA] Firmware transfer started");
    renderUpdateScreen(0);
  });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    if (total == 0) return;
    const uint8_t percent = static_cast<uint8_t>((100ULL * progress) / total);
    const uint8_t decile = percent / 10;
    if (decile != lastLoggedProgress) {
      lastLoggedProgress = decile;
      Serial.printf("[OTA] Transfer %u%%\n", percent);
      renderUpdateScreen(percent);
    }
  });

  ArduinoOTA.onEnd([]() {
    Serial.println("[OTA] Transfer finished; rebooting to new firmware");
    renderUpdateScreen(100);
  });

  ArduinoOTA.onError([](ota_error_t error) {
    updateInProgress = false;
    Serial.printf("[OTA] Error code: %u\n", static_cast<unsigned int>(error));
    Serial.println("[OTA] Check Wi-Fi, password, partition size and upload logs");
  });

  ArduinoOTA.begin();
  otaStarted = true;
  Serial.print("[INFO] OTA service ready at ");
  Serial.print(WiFi.localIP());
  Serial.print(" (hostname: ");
  Serial.print(OTA_HOSTNAME);
  Serial.println(")");
}

void maintainWiFi() {
  if (WiFi.status() == WL_CONNECTED) {
    startOTAService();
    return;
  }

  const uint32_t now = millis();
  if (now - lastWifiRetryMs >= WIFI_RETRY_INTERVAL_MS) {
    lastWifiRetryMs = now;
    Serial.println("[WARN] Wi-Fi unavailable; requesting reconnection");
    WiFi.reconnect();
  }
}

void readSensor() {
  const float humidity = dht.readHumidity();
  const float temperature = dht.readTemperature();

  if (isnan(temperature) || isnan(humidity)) {
    sensorValid = false;
    Serial.println("[ERROR] DHT22 reading invalid");
    return;
  }
  sensorValid = true;
  lastTemperature = temperature;
  lastHumidity = humidity;
  Serial.printf("[INFO] Firmware v%s | T=%.1f C | RH=%.1f%%\n",
                FIRMWARE_VERSION, temperature, humidity);
}

void setup() {
  Serial.begin(115200);
  delay(100);  // Serial startup only; no long blocking connection loops.
  Wire.begin(OLED_SDA_PIN, OLED_SCL_PIN);
  dht.begin();

  displayReady = display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS);
  if (!displayReady) {
    Serial.println("[WARN] OLED not detected; OTA and Serial remain available");
  }

  Serial.printf("[INFO] Starting ESP32 OTA demo v%s\n", FIRMWARE_VERSION);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  lastWifiRetryMs = millis();
  lastSampleMs = millis() - SENSOR_INTERVAL_MS;
  renderNormalScreen();
}

void loop() {
  maintainWiFi();
  if (otaStarted && WiFi.status() == WL_CONNECTED) {
    // Must be called frequently; avoid long blocking operations in loop().
    ArduinoOTA.handle();
  }
  if (updateInProgress) return;

  const uint32_t now = millis();
  if (now - lastSampleMs >= SENSOR_INTERVAL_MS) {
    lastSampleMs = now;
    readSensor();
  }
  if (now - lastScreenMs >= SCREEN_INTERVAL_MS) {
    lastScreenMs = now;
    renderNormalScreen();
  }
  delay(1);  // Yield briefly to the ESP32 background scheduler.
}
