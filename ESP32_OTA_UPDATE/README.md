# ESP32 OTA Update — Slides 39–41

**هدف:** یک ESP32 DevKit V1 را ابتدا از طریق USB با Firmware نسخه `1.0.0` راه‌اندازی کنیم و سپس نسخه `1.1.0` را **بدون اتصال USB برای Upload** و از طریق Wi-Fi در شبکه آزمایشگاهی نصب کنیم. نسخه جدید باید روی OLED نمایش داده شود و خواندن DHT22 همچنان کار کند.

> **دامنه امنیتی این مثال:** روش `ArduinoOTA` اینجا برای **آموزش روی LAN مورداعتماد** است. گذرواژه OTA به‌تنهایی به معنی انتقال رمزنگاری‌شده، امضای Firmware، تأیید هویت Firmware یا Rollback خودکار نیست. سرویس OTA را به اینترنت عمومی باز نکنید. این پروژه مستقل است و MQTT/TLS، LittleFS یا قابلیت‌های مثال‌های قبلی را در خودش ندارد.

## ۱. سخت‌افزار و اتصالات

| قطعه | ESP32 DevKit V1 |
|---|---|
| DHT22 VCC | 3.3V |
| DHT22 DATA | GPIO4 |
| DHT22 GND | GND |
| OLED SSD1306 VCC | 3.3V |
| OLED GND | GND |
| OLED SDA | GPIO21 |
| OLED SCL | GPIO22 |

- نمایشگر: **SSD1306، 128×64، I2C، آدرس `0x3C`**. در صورت تفاوت، `OLED_ADDRESS` را در Sketch اصلاح کنید.
- برای DHT22 **بدون برد ماژول** معمولاً Pull-up حدود **10kΩ** بین DATA و 3.3V لازم است؛ ماژول‌های آماده ممکن است Pull-up داخلی داشته باشند.
- همه سیم‌ها را در حالت قطع برق وصل کنید. تغذیه و زمین مشترک باشند.

## ۲. آماده‌سازی Arduino IDE و فایل محرمانه

1. بسته `esp32 by Espressif Systems` را در Boards Manager نصب کنید و Board صحیح ESP32 را انتخاب کنید.
2. کتابخانه‌ها: `DHT sensor library` (Adafruit)، `Adafruit Unified Sensor`، `Adafruit GFX Library` و `Adafruit SSD1306` را نصب کنید. `WiFi` و `ArduinoOTA` با Arduino-ESP32 ارائه می‌شوند.
3. نام پوشه و فایل Sketch باید `ESP32_OTA_UPDATE` باشد.
4. `secrets.h.example` را **در همین پوشه** با نام `secrets.h` کپی کنید، سپس SSID، گذرواژه Wi-Fi، گذرواژه OTA قوی و Hostname دلخواه را در نسخه محلی تنظیم کنید. فایل نمونه تنها Placeholder دارد؛ آن را با مقادیر واقعی بازنویسی نکنید.
5. `.gitignore` فایل محلی `secrets.h` را از Git مخفی می‌کند؛ اگر قبلاً Commit شده باشد، به‌تنهایی آن را از تاریخچه حذف نمی‌کند.
6. در **Tools → Partition Scheme** یک طرح *دارای دو OTA application slot* انتخاب کنید. **No OTA** مناسب نیست. نام گزینه و اندازه پارتیشن‌ها با نسخه Arduino Core و ظرفیت Flash متفاوت است. تغییر Partition Scheme ممکن است داده‌های Flash قبلی از جمله LittleFS/NVS را از بین ببرد؛ قبل از فلش‌کردن Backup بگیرید.
7. دسترسی Wi-Fi روی همان LAN مورد اعتماد را بررسی کنید. Guest Wi-Fi و AP isolation ممکن است مانع دسترسی کامپیوتر به ESP32 شوند.

## ۳. مرحله A — نصب نسخه 1.0.0 با USB

1. مطمئن شوید در فایل `ESP32_OTA_UPDATE.ino` مقدار زیر تنظیم شده است:

   ```cpp
   constexpr char FIRMWARE_VERSION[] = "1.0.0";
   ```
2. اولین بار Sketch را با **USB** آپلود کنید؛ OTA بدون نصب اولیه Firmware دارای سرویس OTA ممکن نیست.
3. `Serial Monitor` را روی **115200 baud** باز کنید.
4. بعد از اتصال Wi-Fi، IP دستگاه را در Serial Monitor و OLED ببینید. OLED باید `OTA demo v1.0.0`، داده سنسور و وضعیت `OTA: READY (LAN)` را نشان دهد.
5. اگر OLED وصل نیست، Sketch باید ادامه پیدا کند و اطلاعات در Serial دیده شوند.

## ۴. مرحله B — آپلود نسخه 1.1.0 روی Wi-Fi

1. مقدار نسخه را به `"1.1.0"` تغییر دهید و **Save** کنید؛ تغییر رشته نسخه برای این آزمایش نشانه قابل مشاهده نسخه جدید است.
2. ESP32 را **روشن و روی شبکه نگه دارید**. اکنون لازم نیست کابل USB به Mac متصل بماند (برای تأمین برق از منبع USB مناسب استفاده کنید).
3. در Arduino IDE، پورت شبکه مربوط به Hostname دستگاه را در منوی **Tools → Port** انتخاب کنید. کشف پورت ممکن است به mDNS، شبکه و نسخه Arduino IDE بستگی داشته باشد؛ در بعضی محیط‌ها باید روش Upload شبکه از طریق ابزارهای Arduino-ESP32 پیکربندی شود.
4. در صورت درخواست، گذرواژه OTA را از فایل محلی وارد کنید. برای جلوگیری از انتشار اتفاقی، گذرواژه را در ویدئو یا دستور Shell قابل مشاهده نمایش ندهید.
5. Upload را انجام دهید. OTA ابتدا Firmware جدید را دریافت می‌کند و سپس ESP32 راه‌اندازی مجدد می‌شود؛ **قطع برق عمدی یا تزریق Firmware خراب جزو این مثال نیست**.
6. پس از راه‌اندازی مجدد، روی OLED و Serial باید `1.1.0` ظاهر شود. سپس معتبر بودن دما و رطوبت را بررسی کنید.

### خروجی مورد انتظار (نمونه قالب، نه نتیجه تست سخت‌افزاری)

```text
[INFO] Starting ESP32 OTA demo v1.0.0
[INFO] OTA service ready at 192.168.x.x (hostname: esp32-room01-ota)
[OTA] Firmware transfer started
[OTA] Transfer 10%
...
[OTA] Transfer finished; rebooting to new firmware
[INFO] Starting ESP32 OTA demo v1.1.0
```

## ۵. معماری، محدودیت‌ها و تفاوت Rollback

```text
Mac / Arduino IDE (trusted Wi-Fi)
             |
    ArduinoOTA network upload
             |
    ESP32 OTA-capable partition table
      ├── ota_0: current application
      └── ota_1: next application
             |
       reboot / run new version
```

- این مثال **تشخیص کامل سلامت نسخه تازه، امضای Firmware یا بازگشت خودکار به نسخه قبلی** را پیاده‌سازی نمی‌کند. انتخاب طرح دو OTA Slot به‌تنهایی تضمین Rollback نیست.
- برای Rollback کنترل‌شده در ESP-IDF باید **Bootloader، پارتیشن‌ها، قابلیت Rollback و سیاست تأیید سلامت Application** با هم طراحی شوند. اسم گزینه‌های `sdkconfig` و پیش‌فرض آن‌ها بین نسخه‌های ESP-IDF تغییر می‌کند؛ نسخه SDK را بررسی کنید. در حالت Application-confirmation می‌توان از `esp_ota_mark_app_valid_cancel_rollback()` یا مسیر رد نسخه نامعتبر استفاده کرد.
- برای محیط Production، دریافت امن نسخه از منبع مورد اعتماد، Firmware Signing، Secure Boot، Flash Encryption، مدیریت Credential، کنترل دسترسی و رویه Recovery را متناسب با Threat Model ارزیابی کنید. این مثال آموزشی **مکانیزم انتشار امن Production نیست**.
- ArduinoOTA به اجرای منظم `ArduinoOTA.handle()` نیاز دارد؛ توابع Blocking طولانی می‌توانند باعث اختلال در دریافت Firmware شوند.
- این Sketch هر ~2.5 ثانیه DHT22 را می‌خواند. در زمان آپدیت نمایش پیشرفت فعال می‌شود و خواندن سنسور تا پایان عملیات متوقف می‌ماند.
- این مثال **جایگزین کامل** Sketchهای `ESP32_MQTT_PUBLISH`، `ESP32_MQTT_RELIABILITY` و `ESP32_MQTT_TLS` است؛ آپلود آن روی برد، Firmware قبلی را عوض می‌کند و آن قابلیت‌ها را دیگر اجرا نمی‌کند.

## ۶. سناریوهای آزمون و عیب‌یابی

| مشاهده | بررسی |
|---|---|
| OTA network port دیده نمی‌شود | اتصال Wi-Fi ESP32 و Mac به یک LAN، mDNS/Bonjour، وضعیت Router/AP Isolation و پورت/فایروال Mac را بررسی کنید. IP در Serial یا OLED را ببینید. |
| Upload با رمز اشتباه رد می‌شود | مقدار `OTA_PASSWORD` محلی و رمز واردشده در Arduino IDE را مقایسه کنید. رمز واقعی را منتشر نکنید. |
| `Not enough space` یا خطای Partition | از داشتن OTA Slot و کافی‌بودن فضای پارتیشن برای باینری جدید اطمینان بگیرید. |
| Wi-Fi قطع می‌شود | سیگنال، SSID، رمز، تأمین برق و مسیر شبکه را بررسی کنید. کد با فاصله زمانی مشخص تلاش مجدد دارد، ولی تمام خطاها را تضمینی بازیابی نمی‌کند. |
| OLED یافت نمی‌شود | آدرس `0x3C`، اتصال SDA/SCL و تغذیه را بررسی کنید؛ Serial فعال می‌ماند. |
| DHT22 مقدار نامعتبر می‌دهد | فاصله نمونه‌برداری، Pull-up، DATA=GPIO4، تغذیه و زمین مشترک را بررسی کنید. |
| نسخه پس از Upload عوض نشده | لاگ Upload، مسیر پورت انتخاب‌شده، مقدار `FIRMWARE_VERSION` در باینری جدید و بوت مجدد را بررسی کنید. |

**چک‌لیست ضبط:** ابتدا v1.0.0 را با USB نشان دهید، سپس کابل را فقط از کامپیوتر جدا کنید و تغذیه برد را حفظ کنید، Upload نسخه v1.1.0 را از شبکه انجام دهید، پیشرفت روی نمایشگر و نسخه جدید به همراه قرائت صحیح DHT22 را نشان دهید. اگر Demo واقعی موفق نبود، خروجی نمونه را به‌عنوان نتیجه واقعی نمایش ندهید.

## ۷. منابع رسمی

- [Arduino-ESP32 — OTA documentation](https://docs.espressif.com/projects/arduino-esp32/en/latest/ota_web_update.html) (روش‌های OTA متفاوت‌اند؛ `ArduinoOTA` این مثال با HTTP/Web Update یکی نیست).
- [ESP-IDF — OTA and rollback](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/system/ota.html)
- [ESP-IDF — App rollback configuration](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/kconfig-reference.html)
