#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>

// OLED configuration
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

// DHT22 configuration
#define DHTPIN 4
#define DHTTYPE DHT22

Adafruit_SSD1306 display(
  SCREEN_WIDTH,
  SCREEN_HEIGHT,
  &Wire,
  OLED_RESET
);

DHT dht(DHTPIN, DHTTYPE);

void setup() {
  Serial.begin(115200);

  // Initialize I2C: SDA = GPIO21, SCL = GPIO22
  Wire.begin(21, 22);

  // Initialize DHT22
  dht.begin();

  // Initialize OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println("OLED not found!");
    while (true) {
      delay(1000);
    }
  }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  display.setTextSize(2);
  display.setCursor(15, 10);
  display.println("ESP32");

  display.setTextSize(1);
  display.setCursor(15, 40);
  display.println("DHT22 Sensor");

  display.display();
  delay(2500);
}

void loop() {
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();

  // Check sensor readings
  if (isnan(temperature) || isnan(humidity)) {
    Serial.println("DHT22 reading failed!");

    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 20);
    display.println("Sensor Error!");
    display.display();

    delay(2500);
    return;
  }

  // Serial monitor
  Serial.print("Temperature: ");
  Serial.print(temperature);
  Serial.println(" C");

  Serial.print("Humidity: ");
  Serial.print(humidity);
  Serial.println(" %");

  // Update OLED
  display.clearDisplay();

  // Header
  display.setTextSize(1);
  display.setCursor(20, 0);
  display.println("DHT22 Monitor");
  display.drawLine(0, 12, 127, 12, SSD1306_WHITE);

  // Temperature
  display.setTextSize(1);
  display.setCursor(0, 20);
  display.println("Temperature:");

  display.setTextSize(2);
  display.setCursor(15, 31);
  display.print(temperature, 1);
  display.print(" C");

  // Humidity
  display.setTextSize(1);
  display.setCursor(0, 53);
  display.print("Humidity: ");
  display.print(humidity, 1);
  display.print(" %");

  display.display();
  delay(2500);
}
