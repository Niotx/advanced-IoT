/*
  Advanced IoT | Slides 31–34: Firmware Architecture & FreeRTOS

  Hardware: ESP32 DevKit V1 + DHT22 on GPIO4 + SSD1306 OLED (I2C)
  SensorTask is the only owner of DHT22.
  DisplayTask is the only owner of the OLED after setup().
  A FreeRTOS Queue copies SensorReading values between the two tasks.
  A Mutex prevents interleaved Serial logs from the two tasks.

  No Wi-Fi, MQTT, NTP, or offline persistence in this independent example.
*/

#include <Arduino.h>
#include <Wire.h>
#include <DHT.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <math.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

constexpr uint8_t DHT_PIN = 4;
constexpr uint8_t DHT_TYPE = DHT22;
constexpr uint8_t I2C_SDA_PIN = 21;
constexpr uint8_t I2C_SCL_PIN = 22;
constexpr uint8_t OLED_ADDRESS = 0x3C;
constexpr int OLED_WIDTH = 128;
constexpr int OLED_HEIGHT = 64;

constexpr uint32_t SENSOR_INTERVAL_MS = 2500;  // DHT22: at least ~2 seconds
constexpr UBaseType_t SENSOR_QUEUE_LENGTH = 5;

// Set to true ONLY for the Queue Full validation exercise (Slide 34).
// The display task then deliberately runs slower than the sensor task.
constexpr bool DEMO_SLOW_DISPLAY = false;
constexpr uint32_t SLOW_DISPLAY_DELAY_MS = 8000;

struct SensorReading {
  uint32_t sequence;
  uint32_t capturedAtMs;
  float temperatureC;
  float humidityPercent;
  bool valid;
};

DHT dht(DHT_PIN, DHT_TYPE);
Adafruit_SSD1306 display(OLED_WIDTH, OLED_HEIGHT, &Wire, -1);

QueueHandle_t sensorQueue = nullptr;
SemaphoreHandle_t serialMutex = nullptr;
bool oledReady = false;  // Written in setup(), before tasks are created.

void logEvent(const char* level, const char* message) {
  if (serialMutex != nullptr &&
      xSemaphoreTake(serialMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    Serial.printf("[%s] %s\n", level, message);
    xSemaphoreGive(serialMutex);
  }
}

void renderReading(const SensorReading& reading) {
  if (!oledReady) {
    return;  // No display hardware; still consume Queue and print diagnostics.
  }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("FreeRTOS | DHT22");
  display.drawLine(0, 11, 127, 11, SSD1306_WHITE);

  if (!reading.valid) {
    display.setCursor(0, 23);
    display.print("Sensor Error!");
    display.setCursor(0, 36);
    display.print("Check DHT22 wiring");
  } else {
    display.setCursor(0, 21);
    display.print("Temp: ");
    display.print(reading.temperatureC, 1);
    display.print(" C");

    display.setCursor(0, 36);
    display.print("Humidity: ");
    display.print(reading.humidityPercent, 1);
    display.print(" %");
  }

  display.setCursor(0, 53);
  display.print("Seq:");
  display.print(reading.sequence);
  display.print(" Q:");
  display.print(static_cast<unsigned>(uxQueueMessagesWaiting(sensorQueue)));
  display.display();
}

void sensorTask(void* /* parameter */) {
  const TickType_t intervalTicks = pdMS_TO_TICKS(SENSOR_INTERVAL_MS);
  TickType_t previousWake = xTaskGetTickCount();
  uint32_t nextSequence = 0;
  uint32_t droppedSamples = 0;
  char message[120];

  for (;;) {
    SensorReading reading = {};
    reading.sequence = ++nextSequence;
    reading.capturedAtMs = millis();
    reading.humidityPercent = dht.readHumidity();
    reading.temperatureC = dht.readTemperature();
    reading.valid = isfinite(reading.temperatureC) &&
                    isfinite(reading.humidityPercent) &&
                    reading.humidityPercent >= 0.0f &&
                    reading.humidityPercent <= 100.0f;

    if (!reading.valid) {
      snprintf(message, sizeof(message),
               "DHT22 reading invalid (seq=%lu)",
               static_cast<unsigned long>(reading.sequence));
      logEvent("ERROR", message);
    }

    // Non-blocking send: if the queue is full, the newest reading is dropped.
    // This is intentional for illustrating bounded buffers and backpressure.
    if (xQueueSend(sensorQueue, &reading, 0) != pdPASS) {
      ++droppedSamples;
      snprintf(message, sizeof(message),
               "Queue full: dropped seq=%lu (total dropped=%lu)",
               static_cast<unsigned long>(reading.sequence),
               static_cast<unsigned long>(droppedSamples));
      logEvent("WARN", message);
    } else if (reading.valid) {
      snprintf(message, sizeof(message),
               "Queued seq=%lu, T=%.1f C, RH=%.1f%%",
               static_cast<unsigned long>(reading.sequence),
               reading.temperatureC, reading.humidityPercent);
      logEvent("INFO", message);
    }

    // Wait only this task; FreeRTOS may run other ready tasks meanwhile.
    vTaskDelayUntil(&previousWake, intervalTicks);
  }
}

void displayTask(void* /* parameter */) {
  SensorReading reading = {};
  char message[112];

  for (;;) {
    // Block THIS task until a reading is received; no busy-waiting.
    if (xQueueReceive(sensorQueue, &reading, portMAX_DELAY) == pdTRUE) {
      renderReading(reading);

      snprintf(message, sizeof(message),
               "Display consumed seq=%lu, age=%lu ms, remaining=%u",
               static_cast<unsigned long>(reading.sequence),
               static_cast<unsigned long>(millis() - reading.capturedAtMs),
               static_cast<unsigned>(uxQueueMessagesWaiting(sensorQueue)));
      logEvent("INFO", message);

      if (DEMO_SLOW_DISPLAY) {
        vTaskDelay(pdMS_TO_TICKS(SLOW_DISPLAY_DELAY_MS));
      }
    }
  }
}

void setup() {
  Serial.begin(115200);
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  dht.begin();

  oledReady = display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS);
  if (oledReady) {
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(0, 10);
    display.println("FreeRTOS starting...");
    display.display();
  } else {
    Serial.println("[WARN] OLED not found; Serial diagnostics still work");
  }

  sensorQueue = xQueueCreate(SENSOR_QUEUE_LENGTH, sizeof(SensorReading));
  serialMutex = xSemaphoreCreateMutex();
  if (sensorQueue == nullptr || serialMutex == nullptr) {
    Serial.println("[ERROR] Cannot create Queue/Mutex; check free heap");
    while (true) {
      delay(1000);
    }
  }

  // Stack arguments below are bytes in Arduino-ESP32's ESP-IDF FreeRTOS.
  const BaseType_t displayCreated = xTaskCreate(
    displayTask, "DisplayTask", 6144, nullptr, 1, nullptr);
  const BaseType_t sensorCreated = xTaskCreate(
    sensorTask, "SensorTask", 4096, nullptr, 2, nullptr);

  if (displayCreated != pdPASS || sensorCreated != pdPASS) {
    logEvent("ERROR", "Task creation failed; check available memory");
    while (true) {
      delay(1000);
    }
  }

  logEvent("INFO", "SensorTask, DisplayTask, Queue, Mutex initialized");
}

void loop() {
  // Arduino's loopTask is not used for application work in this example.
  vTaskDelay(pdMS_TO_TICKS(1000));
}
