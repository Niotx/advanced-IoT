/*
 * ESP32 MQTT Reliability — DHT22 + SSD1306 + Mosquitto on Raspberry Pi
 * Arduino IDE / ESP32 DevKit V1
 *
 * Learning goals:
 * - Local sensing and OLED work without MQTT.
 * - Timed Wi-Fi and MQTT retries without infinite while loops.
 * - A bounded RAM queue; periodic LittleFS snapshots during outages.
 * - Replay buffered readings after reconnection (best effort, MQTT QoS 0).
 * - NTP measurement time when available; null otherwise.
 * - INFO / WARN / ERROR logs over Serial.
 *
 * This is a teaching example, NOT a lossless or fully non-blocking data logger.
 * PubSubClient connect/TCP operations can briefly block.
 */

#include <WiFi.h>
#include <Wire.h>
#include <LittleFS.h>
#include <PubSubClient.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>
#include <time.h>
#include <math.h>
#include <esp_system.h>
#include "secrets.h"  // Locally created from secrets.h.example; do NOT upload to GitHub.

// ---------------- Hardware ----------------
#define DHT_PIN 4
#define DHT_TYPE DHT22
#define OLED_SDA 21
#define OLED_SCL 22
#define OLED_ADDRESS 0x3C
#define OLED_WIDTH 128
#define OLED_HEIGHT 64
#define OLED_RESET -1

// ---------------- Device and topics ----------------
const char* DEVICE_ID = "esp32-room-01";
const char* TOPIC_TELEMETRY = "iot/room-01/telemetry";
const char* TOPIC_STATUS = "iot/room-01/status";

// ---------------- Reliability policy ----------------
const uint32_t SAMPLE_INTERVAL_MS = 2500UL;
const uint32_t WIFI_RETRY_MS = 10000UL;
const uint32_t MQTT_RETRY_MS = 5000UL;
const uint32_t REPLAY_INTERVAL_MS = 150UL;
const uint32_t FLASH_SNAPSHOT_INTERVAL_MS = 30000UL;
const uint32_t DIAGNOSTIC_INTERVAL_MS = 30000UL;
const uint16_t BUFFER_CAPACITY = 120;  // Up to ~5 minutes at 2.5 s/read.

// Choose a partition scheme with a filesystem partition in Arduino IDE.
// DO NOT enable formatting on mount failure unless you accept loss of old data.
const bool FORMAT_LITTLEFS_ON_MOUNT_FAILURE = false;
const char* SNAPSHOT_FILE = "/pending.csv";
const char* TEMP_FILE = "/pending.tmp";

// ---------------- Components ----------------
DHT dht(DHT_PIN, DHT_TYPE);
Adafruit_SSD1306 display(OLED_WIDTH, OLED_HEIGHT, &Wire, OLED_RESET);
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

// Store scaled integers so snapshot parsing does not depend on scanf float support.
struct Reading {
  char bootId[9];          // Random 8-hex-digit ID of the boot that took the reading.
  uint32_t sequence;      // Unique together with bootId (within the example).
  uint32_t uptimeMs;      // milliseconds since the source boot, NOT wall-clock time.
  uint32_t epochSeconds;  // UTC Unix timestamp at measurement, or 0 if NTP unavailable.
  int16_t temperature10;  // degrees C x 10
  uint16_t humidity10;    // relative humidity % x 10
  bool buffered;          // recorded while offline / waiting behind earlier records
};

Reading pending[BUFFER_CAPACITY];
uint16_t pendingHead = 0;
uint16_t pendingCount = 0;
uint32_t droppedReadings = 0;
uint32_t nextSequence = 0;
char currentBootId[9] = "00000000";

bool oledAvailable = false;
bool flashAvailable = false;
bool flashSnapshotExists = false;
bool wifiWasConnected = false;
bool mqttWasConnected = false;
bool ntpStarted = false;
bool ntpLogged = false;

uint32_t lastSampleMs = 0;
uint32_t lastWiFiAttemptMs = 0;
uint32_t lastMqttAttemptMs = 0;
uint32_t lastReplayMs = 0;
uint32_t lastFlashAttemptMs = 0;
uint32_t lastDiagnosticsMs = 0;

Reading& oldestReading() { return pending[pendingHead]; }

void removeOldest() {
  if (pendingCount == 0) return;
  pendingHead = (pendingHead + 1U) % BUFFER_CAPACITY;
  --pendingCount;
}

void addPending(const Reading& item) {
  if (pendingCount == BUFFER_CAPACITY) {
    removeOldest();  // Explicit overflow policy: drop the oldest measurement.
    ++droppedReadings;
    Serial.println("[WARN] Buffer full: oldest reading dropped");
  }
  uint16_t tail = (pendingHead + pendingCount) % BUFFER_CAPACITY;
  pending[tail] = item;
  ++pendingCount;
}

// Snapshot format: V1 header followed by simple CSV records.
// Write a temporary file, then rename it over the previous snapshot.
// This is best-effort persistence, not a transactional database.
bool saveSnapshot() {
  if (!flashAvailable) return false;

  File f = LittleFS.open(TEMP_FILE, "w");
  if (!f) {
    Serial.println("[ERROR] LittleFS: cannot create temporary snapshot");
    return false;
  }

  bool ok = (f.println("V1") > 0);
  for (uint16_t i = 0; ok && i < pendingCount; ++i) {
    const Reading& r = pending[(pendingHead + i) % BUFFER_CAPACITY];
    char line[112];
    int n = snprintf(line, sizeof(line), "%s,%lu,%lu,%lu,%d,%u,%u",
      r.bootId,
      (unsigned long)r.sequence,
      (unsigned long)r.uptimeMs,
      (unsigned long)r.epochSeconds,
      (int)r.temperature10,
      (unsigned int)r.humidity10,
      (unsigned int)r.buffered);
    if (n < 0 || n >= (int)sizeof(line) || f.println(line) == 0) ok = false;
  }
  f.flush();
  f.close();

  if (!ok) {
    LittleFS.remove(TEMP_FILE);
    Serial.println("[ERROR] LittleFS: snapshot write failed");
    return false;
  }

  if (!LittleFS.rename(TEMP_FILE, SNAPSHOT_FILE)) {
    Serial.println("[ERROR] LittleFS: snapshot rename failed");
    LittleFS.remove(TEMP_FILE);
    return false;
  }

  flashSnapshotExists = true;
  Serial.printf("[INFO] Saved %u buffered readings to flash\n", pendingCount);
  return true;
}

void restoreSnapshot() {
  if (!flashAvailable || !LittleFS.exists(SNAPSHOT_FILE)) return;

  File f = LittleFS.open(SNAPSHOT_FILE, "r");
  if (!f) {
    Serial.println("[ERROR] LittleFS: cannot open saved buffer");
    return;
  }

  flashSnapshotExists = true;
  String header = f.readStringUntil('\n');
  header.trim();
  if (header != "V1") {
    Serial.println("[ERROR] LittleFS: unsupported buffer snapshot version");
    f.close();
    return;
  }

  uint16_t restored = 0;
  while (f.available()) {
    String line = f.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) continue;

    Reading r = {};
    unsigned long seq, uptime, epoch;
    int temp10;
    unsigned int hum10, buffered;
    int count = sscanf(line.c_str(), "%8[0-9a-fA-F],%lu,%lu,%lu,%d,%u,%u",
      r.bootId, &seq, &uptime, &epoch, &temp10, &hum10, &buffered);
    if (count != 7 || temp10 < -400 || temp10 > 800 || hum10 > 1000 || buffered > 1) {
      Serial.println("[WARN] LittleFS: skipped invalid saved record");
      continue;
    }

    r.sequence = (uint32_t)seq;
    r.uptimeMs = (uint32_t)uptime;
    r.epochSeconds = (uint32_t)epoch;
    r.temperature10 = (int16_t)temp10;
    r.humidity10 = (uint16_t)hum10;
    r.buffered = true; // Any restored record is replayed after a restart.
    addPending(r);
    ++restored;
  }
  f.close();
  Serial.printf("[INFO] Restored %u unsent readings from LittleFS\n", restored);
}

void maintainFlash(uint32_t now) {
  if (!flashAvailable) return;
  if (pendingCount == 0) {
    if (flashSnapshotExists) {
      if (LittleFS.remove(SNAPSHOT_FILE)) {
        flashSnapshotExists = false;
        Serial.println("[INFO] Saved backlog cleared from flash");
      } else {
        Serial.println("[ERROR] LittleFS: could not remove drained snapshot");
      }
    }
    return;
  }

  // Avoid needless flash writes while current live data is being sent normally.
  if (mqttClient.connected()) return;
  if ((uint32_t)(now - lastFlashAttemptMs) < FLASH_SNAPSHOT_INTERVAL_MS) return;
  lastFlashAttemptMs = now;
  saveSnapshot();
}

void oledHeader() {
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("DHT22 / MQTT");
  display.drawLine(0, 12, OLED_WIDTH - 1, 12, SSD1306_WHITE);
}

void oledStatus() {
  display.setTextSize(1);
  display.setCursor(0, 55);
  display.printf("W:%s M:%s Q:%u",
    WiFi.status() == WL_CONNECTED ? "OK" : "--",
    mqttClient.connected() ? "OK" : "--",
    pendingCount);
}

void showReadings(float tempC, float humidity) {
  if (!oledAvailable) return;
  display.clearDisplay();
  oledHeader();
  display.setTextSize(2);
  display.setCursor(0, 17);
  display.printf("%.1f C", tempC);
  display.setCursor(0, 36);
  display.printf("%.1f %%", humidity);
  oledStatus();
  display.display();
}

void showSensorError() {
  if (!oledAvailable) return;
  display.clearDisplay();
  oledHeader();
  display.setTextSize(1);
  display.setCursor(0, 24);
  display.println("Sensor Error!");
  oledStatus();
  display.display();
}

void maintainWiFi(uint32_t now) {
  bool connected = (WiFi.status() == WL_CONNECTED);
  if (connected) {
    if (!wifiWasConnected) {
      Serial.print("[INFO] Wi-Fi connected. IP: ");
      Serial.println(WiFi.localIP());
      wifiWasConnected = true;
      if (!ntpStarted) {
        configTime(0, 0, "pool.ntp.org", "time.google.com"); // UTC
        ntpStarted = true;
        Serial.println("[INFO] NTP configured (waiting for synchronization)");
      }
    }
    return;
  }

  if (wifiWasConnected) {
    wifiWasConnected = false;
    Serial.println("[WARN] Wi-Fi connection lost");
  }
  if ((uint32_t)(now - lastWiFiAttemptMs) < WIFI_RETRY_MS) return;

  lastWiFiAttemptMs = now;
  Serial.println("[INFO] Wi-Fi connection attempt");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

void maintainMQTT(uint32_t now) {
  if (mqttClient.connected()) {
    mqttClient.loop(); // Required for keepalive; also detects remote disconnects.
    if (mqttClient.connected()) return;
  }

  if (mqttWasConnected) {
    mqttWasConnected = false;
    Serial.println("[WARN] MQTT disconnected");
  }
  if (WiFi.status() != WL_CONNECTED) return;
  if ((uint32_t)(now - lastMqttAttemptMs) < MQTT_RETRY_MS) return;

  lastMqttAttemptMs = now;
  Serial.println("[INFO] MQTT connection attempt");
  bool ok = mqttClient.connect(
    DEVICE_ID, MQTT_USERNAME, MQTT_PASSWORD,
    TOPIC_STATUS, 1, true, "offline"  // Last Will: QoS 1, retained
  );
  if (!ok) {
    Serial.printf("[WARN] MQTT connect failed (state=%d)\n", mqttClient.state());
    return;
  }

  mqttWasConnected = true;
  Serial.println("[INFO] MQTT connected");
  if (!mqttClient.publish(TOPIC_STATUS, "online", true)) {
    Serial.println("[WARN] Could not publish retained online status");
  }
}

bool buildPayload(const Reading& r, char* out, size_t capacity) {
  char timestamp[20];
  if (r.epochSeconds == 0) {
    snprintf(timestamp, sizeof(timestamp), "null");
  } else {
    snprintf(timestamp, sizeof(timestamp), "%lu", (unsigned long)r.epochSeconds);
  }

  int len = snprintf(out, capacity,
    "{\"device_id\":\"%s\",\"boot_id\":\"%s\",\"sequence\":%lu,"
    "\"temperature\":%.1f,\"humidity\":%.1f,\"uptime_ms\":%lu,"
    "\"measured_at_unix\":%s,\"buffered\":%s}",
    DEVICE_ID, r.bootId, (unsigned long)r.sequence,
    r.temperature10 / 10.0f, r.humidity10 / 10.0f,
    (unsigned long)r.uptimeMs, timestamp,
    r.buffered ? "true" : "false");
  return len >= 0 && (size_t)len < capacity;
}

void publishOldest(uint32_t now) {
  if (!mqttClient.connected() || pendingCount == 0) return;
  if ((uint32_t)(now - lastReplayMs) < REPLAY_INTERVAL_MS) return;
  lastReplayMs = now;

  char payload[320];
  if (!buildPayload(oldestReading(), payload, sizeof(payload))) {
    Serial.println("[ERROR] Payload buffer insufficient; dropping unencodable record");
    removeOldest();
    ++droppedReadings;
    return;
  }

  // PubSubClient publishes at QoS 0. true means it wrote to the TCP client,
  // not that the Broker or Subscriber durably acknowledged this reading.
  if (!mqttClient.publish(TOPIC_TELEMETRY, payload, false)) {
    oldestReading().buffered = true;
    Serial.println("[WARN] MQTT publish failed; reading kept in buffer");
    return;
  }

  Serial.printf("[INFO] MQTT publish (queued left %u): %s\n", pendingCount - 1, payload);
  removeOldest();
}

void sampleSensor() {
  float t = dht.readTemperature();
  float h = dht.readHumidity();

  if (!isfinite(t) || !isfinite(h) || t < -40 || t > 80 || h < 0 || h > 100) {
    Serial.println("[ERROR] DHT22: invalid sensor reading");
    showSensorError();
    return; // Never queue or publish invalid data.
  }

  Reading r = {};
  snprintf(r.bootId, sizeof(r.bootId), "%s", currentBootId);
  r.sequence = ++nextSequence;
  r.uptimeMs = millis();
  time_t current = time(nullptr);
  // Reject unset clock values. If no NTP time exists, leave the timestamp unknown.
  r.epochSeconds = (current >= (time_t)1700000000) ? (uint32_t)current : 0;
  r.temperature10 = (int16_t)lroundf(t * 10.0f);
  r.humidity10 = (uint16_t)lroundf(h * 10.0f);
  r.buffered = (!mqttClient.connected() || pendingCount > 0);

  addPending(r);
  showReadings(t, h);
  Serial.printf("[INFO] DHT22: %.1f C, %.1f %%, queue=%u\n", t, h, pendingCount);
}

void setup() {
  Serial.begin(115200);
  snprintf(currentBootId, sizeof(currentBootId), "%08lx", (unsigned long)esp_random());
  Serial.printf("[INFO] Starting reliability example, boot_id=%s\n", currentBootId);

  Wire.begin(OLED_SDA, OLED_SCL);
  dht.begin();
  oledAvailable = display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS);
  if (!oledAvailable) {
    Serial.println("[ERROR] OLED unavailable; sensing/network processing will continue");
  } else {
    display.setTextColor(SSD1306_WHITE);
    display.clearDisplay();
    oledHeader();
    display.setCursor(0, 26);
    display.println("Starting...");
    display.display();
  }

  flashAvailable = LittleFS.begin(FORMAT_LITTLEFS_ON_MOUNT_FAILURE);
  if (flashAvailable) {
    Serial.println("[INFO] LittleFS mounted");
    restoreSnapshot();
  } else {
    Serial.println("[WARN] LittleFS not mounted. RAM buffer still works; no reboot persistence");
  }

  WiFi.mode(WIFI_STA);
  mqttClient.setServer(MQTT_HOST, MQTT_PORT);
  if (!mqttClient.setBufferSize(512)) {
    Serial.println("[ERROR] MQTT packet buffer allocation failed");
  }
  mqttClient.setSocketTimeout(2);  // Seconds for MQTT socket reads, not strict non-blocking TCP.

  uint32_t now = millis();
  lastSampleMs = now - SAMPLE_INTERVAL_MS;
  lastWiFiAttemptMs = now - WIFI_RETRY_MS;
  lastMqttAttemptMs = now - MQTT_RETRY_MS;
  lastReplayMs = now - REPLAY_INTERVAL_MS;
  lastFlashAttemptMs = now;
  lastDiagnosticsMs = now;
}

void loop() {
  uint32_t now = millis();
  maintainWiFi(now);
  maintainMQTT(now);

  if (ntpStarted && !ntpLogged && time(nullptr) >= (time_t)1700000000) {
    ntpLogged = true;
    Serial.println("[INFO] NTP synchronized; future samples get Unix timestamps");
  }

  if ((uint32_t)(now - lastSampleMs) >= SAMPLE_INTERVAL_MS) {
    lastSampleMs = now;
    sampleSensor();
  }

  publishOldest(now); // One pending reading per iteration; oldest first.
  maintainFlash(now);

  if ((uint32_t)(now - lastDiagnosticsMs) >= DIAGNOSTIC_INTERVAL_MS) {
    lastDiagnosticsMs = now;
    Serial.printf("[INFO] Diagnostics: queue=%u/%u, dropped=%lu, flash=%s\n",
      pendingCount, BUFFER_CAPACITY, (unsigned long)droppedReadings,
      flashAvailable ? "mounted" : "unavailable");
  }
}
