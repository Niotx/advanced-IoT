# ESP32 MQTT over TLS — Part 08 (Slides 35–38)

**هدف:** ارسال دما و رطوبت DHT22 از ESP32 به Mosquitto روی Raspberry Pi **از طریق TLS اعتبارسنجی‌شده**. همان OLED قبلی به صورت محلی کار می‌کند و پیام JSON برای Subscriber Python قبلی قابل فهم است.

> این یک مثال **مستقل** برای امنیت MQTT است، نه جایگزین `ESP32_MQTT_RELIABILITY`: در این کد **LittleFS/Offline Buffering وجود ندارد**. با قطع اتصال، اندازه‌گیری در OLED ادامه پیدا می‌کند، اما پیام‌های MQTT آن بازه ذخیره نمی‌شوند. کد نمونه تا زمان آزمون روی ESP32 واقعی، تست سخت‌افزاری‌شده محسوب نمی‌شود.

## سخت‌افزار و سیم‌کشی

| قطعه | پین ESP32 DevKit V1 |
|---|---|
| DHT22 DATA | GPIO 4 |
| DHT22 VCC / GND | 3.3V / GND |
| SSD1306 SDA / SCL | GPIO 21 / GPIO 22 |
| SSD1306 VCC / GND | 3.3V / GND |

آدرس OLED: `0x3C`، رزولوشن: `128×64`. سنسور DHT22 بدون برد واسط ممکن است به مقاومت Pull-up حدود ۱۰ کیلو اهم بین DATA و 3.3V نیاز داشته باشد؛ ماژول‌های آماده ممکن است مقاومت را از قبل داشته باشند. قبل از اتصال، Pinout واقعی برد را بررسی کنید.

## کتابخانه‌ها

- Arduino IDE + برد `ESP32 Dev Module` از Espressif، مطابق سخت‌افزار شما.
- `PubSubClient`، `DHT sensor library` و وابستگی `Adafruit Unified Sensor`، `Adafruit GFX Library`، `Adafruit SSD1306`.
- `WiFi.h`، `WiFiClientSecure.h`، `Wire.h` و `time.h` همراه ESP32 Arduino Core در دسترس هستند.

## ۱) اول Broker امن را آماده کنید

**قبل از Upload Sketch،** آموزش [TLS_SETUP.md](../RaspberryPi_MQTT_BROKER/TLS_SETUP.md) را اجرا کنید. در نهایت باید:

- یک Mosquitto Listener روی TCP/8883 با Server Certificate معتبر داشته باشید.
- حساب اختصاصی `esp32_room01` با حق `write` روی `iot/room-01/telemetry` و `iot/room-01/status` داشته باشید.
- گواهی عمومی ریشه **`ca.crt`** را در اختیار داشته باشید.
- `openssl s_client` و `mosquitto_pub/sub --cafile ca.crt` اتصال امن را با موفقیت بررسی کرده باشند.

در شبکه آزمایشگاهی، آدرس نمونه `192.168.1.50` صرفاً **مثال** است؛ از IP واقعی Pi استفاده کنید. IP یا DNS مقصد باید در SAN گواهی سرور ذکر شده باشد.

## ۲) تنظیم فایل شخصی secrets.h

داخل همین پوشه:

```bash
cp secrets.h.example secrets.h
```

در `secrets.h` مقادیر زیر را پر کنید:

| مقدار | توضیح |
|---|---|
| `WIFI_SSID` و `WIFI_PASSWORD` | شبکه Wi-Fi و رمز آن |
| `MQTT_HOST` | IP یا DNS واقعی Broker؛ **منطبق با SAN گواهی** |
| `MQTT_PORT` | `8883` |
| `MQTT_USERNAME` | `esp32_room01` |
| `MQTT_PASSWORD` | رمز مربوط به همین کاربر، فقط در فایل محلی |
| `MQTT_ROOT_CA` | **تمام محتویات عمومی** فایل `ca.crt` در قالب PEM |

برای بخش CA، بلوک نمونه را **کامل** با محتوای گواهی واقعی جایگزین کنید، نه اینکه فقط یک سطر تصادفی در بلوک قرار دهید. شکل درست:

```cpp
static const char MQTT_ROOT_CA[] = R"EOF(
-----BEGIN CERTIFICATE-----
...base64 lines copied from YOUR OWN ca.crt...
-----END CERTIFICATE-----
)EOF";
```

این بلوک تنها یک **نمایش آموزشی** است و بدون CA واقعی قابل استفاده نیست. **هرگز** `ca.key` یا `server.key` را در Firmware جایگذاری نکنید. `ca.crt` عمومی است؛ کلید خصوصی نیست.

`.gitignore` فایل `secrets.h` را از ثبت تصادفی منع می‌کند، ولی این کافی نیست: فایل‌هایی که قبلاً Commit شده‌اند باید از Git Tracking کنار گذاشته شوند و رمزهای افشاشده باید تعویض شوند. Secrets در حافظه Firmware هم بدون مکانیزم‌های حفاظتی فیزیکی، الزاماً غیرقابل استخراج نیستند.

## ۳) Upload و اولین اجرا

1. فایل `ESP32_MQTT_TLS.ino` را در Arduino IDE باز کنید (پوشه و فایل هم‌نام هستند).
2. Board/Port درست را انتخاب کنید و برنامه را Upload کنید.
3. Serial Monitor را روی `115200` تنظیم کنید.
4. ابتدا اتصال Wi-Fi، سپس درخواست NTP و پس از معتبر شدن ساعت، تلاش اتصال TLS/MQTT را مشاهده کنید.
5. مقادیر دما و رطوبت باید روی OLED حتی در نبود Broker نشان داده شوند.

نمونه رویدادهای **نمایشی (نه خروجی واقعی برد)**:

```text
[INFO] NTP sync requested
[INFO] Connecting to MQTT over TLS: 192.168.1.50:8883
[INFO] MQTT over TLS connected
[INFO] Published TLS telemetry: {"device_id":"esp32-room-01",...}
```

اگر ساعت معتبری از NTP دریافت نشده باشد، برنامه اتصال TLS را **شروع نمی‌کند**، ولی خواندن محلی ادامه دارد. زمان NTP لازم است تا تاریخ اعتبار Certificate بررسی شود. صرف رسیدن ساعت به یک تاریخ معقول اثبات امنیت زمان منبع نیست؛ این نسخه برای آزمایشگاه طراحی شده است.

## ۴) مشاهده MQTT امن

با کاربر `pi_monitor` و `ca.crt` واقعی:

```bash
mosquitto_sub -h 192.168.1.50 -p 8883 \
  --cafile ./ca.crt -u pi_monitor -P 'LAB_PASSWORD_ONLY' \
  -t 'iot/room-01/#' -v
```

**به‌جای رمز واقعی در ویدئو از حساب/رمز یک‌بارمصرف استفاده کنید**؛ `-P` ممکن است رمز را در Shell history و فهرست فرایندها فاش کند. در [TLS_SETUP.md](../RaspberryPi_MQTT_BROKER/TLS_SETUP.md) آزمایش مرحله‌ای CLI، تأیید Certificate و ACL شرح داده شده است.

نمونه Payload (فقط برای نمایش فرمت):

```json
{
  "device_id": "esp32-room-01",
  "temperature": 24.6,
  "humidity": 42.8,
  "uptime_ms": 152000,
  "measured_at_unix": 1790000000
}
```

فیلدهای `device_id`، `temperature`، `humidity` و `uptime_ms` با Subscriber قبلی سازگارند. فیلد `measured_at_unix`، زمان Unix اندازه‌گیری است؛ در این مثال وقتی MQTT متصل است، ساعت قبلاً معتبر ارزیابی شده است. عددهای JSON بالا صرفاً **نمونه ساختار هستند، نه اندازه‌گیری یا Timestamp واقعی**.

- `iot/room-01/telemetry`: دما و رطوبت، **QoS 0** و بدون Retain.
- `iot/room-01/status`: پیام retained `online` و Last Will برابر retained `offline` در صورت قطع غیرمنتظره و تشخیص آن توسط Broker.

**مهم:** Last Will هنگام قطع شبکه فوری نیست؛ Broker پس از تشخیص گم‌شدن اتصال (از جمله timeout مربوط به keepalive) پیام را منتشر می‌کند. قطع graceful می‌تواند LWT را فعال نکند. تنظیم صحیح گواهی TLS، تضمین تحویل پیام‌ها یا قابلیت Replay نمی‌دهد.

## ۵) آزمون‌های عملی بخش امنیت

| آزمون | رفتار مورد انتظار |
|---|---|
| Wi-Fi / Broker فعال و NTP معتبر | ESP32 از ۸۸۸۳ وصل می‌شود؛ OLED و Subscriber پیام می‌بینند. |
| حذف/جایگزینی CA با CA اشتباه | بررسی گواهی TLS شکست می‌خورد؛ MQTT وصل نمی‌شود. |
| IP/DNS Broker نامنطبق با SAN | اتصال باید به دلیل بررسی هویت سرور رد شود. |
| رمز اشتباه کاربر | احراز هویت MQTT رد می‌شود؛ سرور Log می‌نویسد. |
| توقف Mosquitto | داده روی OLED ادامه دارد؛ ارسال MQTT انجام نمی‌شود. |
| Wireshark روی ۸۸۸۳ | Packetهای شبکه و TLS دیده می‌شوند، نه متن آشکار MQTT. |

بدون `setInsecure()` خطاهای گواهی را رفع کنید. برای تست دسترسی غیرمجاز به Topic، از کاربر تست و دستورات در `TLS_SETUP.md` استفاده کنید؛ با Firmware خودتان Topic سایر دستگاه‌ها را دستکاری نکنید.

## ۶) محدودیت‌های این مثال

- تنها **server-authenticated TLS** ارائه می‌شود؛ client certificate یا mTLS ندارد.
- TLS برای حفاظت از داده در مسیر انتقال است؛ برای حفظ داده هنگام قطعی، به مثال مستقل [Reliability](../ESP32_MQTT_RELIABILITY/README.md) مراجعه کنید.
- `PubSubClient.publish()` تله‌متری را با **QoS 0** ارسال می‌کند؛ تحویل تضمین‌شده یا ذخیره دائمی داده ارائه نمی‌شود.
- تلاش اتصال Wi-Fi/TCP/TLS ممکن است مدت کوتاهی Blocking باشد؛ این مثال یک Scheduler بی‌درنگ سخت‌گیرانه نیست.
- کلید/رمز داخل Firmware با `.gitignore` در برابر دسترسی فیزیکی یا استخراج Flash محافظت نمی‌شود. Provisioning واقعی و Credential Rotation به طراحی محصول نیاز دارد.
- CA آزمایشگاهی و گواهی با اعتبار محدود برای آموزش ساخته می‌شوند؛ در محیط Production نیازمند سیاست مدیریت و چرخش گواهی، حفاظت کلیدها، Secret Provisioning و مانیتورینگ هستیم.

## ۷) رفع اشکال

| خطا | بررسی |
|---|---|
| `WAIT NTP` روی OLED | دسترسی اینترنت/DNS برای NTP، کیفیت Wi-Fi و تنظیم زمان را بررسی کنید. |
| TLS certificate validation failed | کل CA، SAN IP/DNS، تاریخ گواهی و ساعت دستگاه را بررسی کنید. |
| MQTT connection failed با ساعت معتبر | Username/Password، ACL، Listener 8883 و Broker logs را بررسی کنید. |
| OLED روشن نمی‌شود | 3.3V/GND، SDA21/SCL22، آدرس 0x3C و کتابخانه‌ها را بررسی کنید. |
| DHT22 error | سیم DATA روی GPIO4، مقاومت Pull-up، برق صحیح و فاصله نمونه‌برداری را بررسی کنید. |
| TLS وصل می‌شود ولی Subscriber پیام نمی‌بیند | Topic، ACL حساب `pi_monitor`، سرور TLS و اجرای Subscriber روی ۸۸۸۳ با CA را بررسی کنید. |

## منابع

- [راهنمای TLS و ACL روی Mosquitto](../RaspberryPi_MQTT_BROKER/TLS_SETUP.md)
- [ESP32 Arduino — NetworkClientSecure](https://github.com/espressif/arduino-esp32/tree/master/libraries/NetworkClientSecure)
- [Mosquitto TLS](https://mosquitto.org/man/mosquitto-tls-7.html)
