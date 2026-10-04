# ESP32 MQTT Publisher — DHT22 + SSD1306

در این مثال، **ESP32 DevKit V1** هر ۲.۵ ثانیه دما و رطوبت **DHT22** را می‌خواند، روی **OLED SSD1306** نشان می‌دهد و در صورت برقراری ارتباط، داده را به **Mosquitto روی Raspberry Pi** ارسال می‌کند. این پوشه یک مثال مستقل برای **Arduino IDE** است.

> این نمونه برای **شبکه محلی قابل اعتماد و آزمایشگاهی** طراحی شده است. ارتباط MQTT روی TCP/1883 رمزنگاری‌شده نیست. در بخش امنیت دوره، TLS را اضافه می‌کنیم؛ پورت ۱۸۸۳ را به اینترنت عمومی باز نکنید.

## فایل‌ها

```text
ESP32_MQTT_PUBLISH/
├── ESP32_MQTT_PUBLISH.ino  # کد قابل بازکردن در Arduino IDE
├── secrets.h.example      # الگوی تنظیمات؛ بدون رمز واقعی
├── .gitignore             # جلوگیری از Commit تصادفی secrets.h
└── README.md
```

بعد از انجام تنظیمات، یک فایل محلی دیگر به نام `secrets.h` خواهید داشت که **نباید** وارد GitHub شود.

## پیش‌نیازها

- ESP32 DevKit V1، DHT22 و نمایشگر OLED I2C مبتنی بر SSD1306 (128×64).
- Raspberry Pi و Mosquitto با **Listener شبکه‌ای، حساب MQTT و تنظیم UFW** طبق [راهنمای Broker](../RaspberryPi_MQTT_BROKER/README.md).
- اتصال ESP32 و Raspberry Pi به یک شبکه محلی که ارتباط TCP/1883 بین آن‌ها مجاز است.
- بسته **ESP32 by Espressif Systems** در Arduino IDE.
- کتابخانه‌های Arduino IDE: **PubSubClient**، **DHT sensor library by Adafruit**، **Adafruit Unified Sensor**، **Adafruit GFX Library** و **Adafruit SSD1306**. کتابخانه‌های `WiFi` و `Wire` همراه بسته برد ESP32 هستند.

## ۱. سیم‌کشی

اتصالات همان مثال [ESP32_DHT22_OLED](../ESP32_DHT22_OLED/README.md) هستند:

| قطعه | پایه قطعه | پایه ESP32 |
|---|---|---|
| DHT22 | DATA | GPIO4 |
| DHT22 | VCC / GND | 3.3V / GND |
| OLED SSD1306 | SDA / SCL | GPIO21 / GPIO22 |
| OLED SSD1306 | VCC / GND | 3.3V / GND |

برای **سنسور خام DHT22** معمولاً مقاومت Pull-up حدود `10kΩ` بین DATA و 3.3V لازم است؛ برخی ماژول‌های آماده این مقاومت را دارند. آدرس I2C در این کد `0x3C` در نظر گرفته شده است.

## ۲. آماده‌کردن Raspberry Pi

اگر Broker را راه‌اندازی نکرده‌اید، ابتدا [RaspberryPi_MQTT_BROKER](../RaspberryPi_MQTT_BROKER/README.md) را انجام دهید.

روی Raspberry Pi، آدرس IP شبکه را مشاهده کنید:

```bash
hostname -I
```

از فعال‌بودن سرویس Mosquitto مطمئن شوید:

```bash
sudo systemctl status mosquitto --no-pager
```

برای این مثال، Broker باید روی پورت `1883` به کلاینت‌های مجاز شبکه محلی پاسخ دهد. از نام کاربری و رمز عبوری که در مرحله تنظیم Broker ساخته‌اید استفاده کنید.

## ۳. تنظیم ESP32 در Arduino IDE

1. پوشه `ESP32_MQTT_PUBLISH` را روی رایانه دریافت کنید.
2. فایل `secrets.h.example` را **کپی** کنید و نام کپی را `secrets.h` بگذارید. فایل نمونه را دست‌نخورده نگه دارید.
3. مقادیر `WIFI_SSID`، `WIFI_PASSWORD`، `MQTT_HOST`، `MQTT_USERNAME` و `MQTT_PASSWORD` را با اطلاعات واقعی خودتان جایگزین کنید. `MQTT_HOST` باید IP **Raspberry Pi** باشد، نه IP خود ESP32.
4. فایل `ESP32_MQTT_PUBLISH.ino` را در Arduino IDE باز کنید؛ برد و پورت ESP32 را انتخاب و Upload کنید.
5. Serial Monitor را روی `115200 baud` باز کنید. اگر خروجی نمی‌بینید، دکمه Reset را یک بار فشار دهید.

> **امنیت:** `.gitignore` برای Git محلی از Commit فایل `secrets.h` جلوگیری می‌کند؛ اما در بارگذاری دستی از طریق وب GitHub همچنان مراقب باشید آن را انتخاب نکنید. رمزهای واقعی را در فیلم ضبط‌شده هم نشان ندهید.

## ۴. مشاهده پیام‌ها

برای دیدن داده‌ها، **MQTT Explorer** را با IP Raspberry Pi، پورت ۱۸۸۳ و حساب MQTT تنظیم کنید و به Topicهای زیر نگاه کنید:

```text
iot/room-01/telemetry
iot/room-01/status
```

همچنین روی Raspberry Pi می‌توانید در یک ترمینال جدید، با رمز **آزمایشی** از Subscriber استفاده کنید (مقادیر نمونه را تغییر دهید):

```bash
mosquitto_sub -h localhost -p 1883 -u iot_device -P 'YOUR_PASSWORD' -t 'iot/room-01/#' -v
```

**توجه:** قرار دادن رمز واقعی در دستور خط فرمان ممکن است آن را در Shell History یا فهرست فرایندها ثبت کند. برای ضبط دوره از یک حساب آزمایشی استفاده کنید و رمز آن را بعداً تغییر دهید.

**نمونه خروجی مورد انتظار:**

```text
iot/room-01/status online
iot/room-01/telemetry {"device_id":"esp32-room-01","temperature":24.6,"humidity":42.8,"uptime_ms":19372}
```

مقادیر بالا صرفاً نمونه‌اند. پیام `telemetry` تقریباً هر ۲.۵ ثانیه **در صورت معتبر بودن خواندن سنسور و اتصال MQTT** ارسال می‌شود. این پیام retained نیست؛ در مقابل، وضعیت `online` به‌صورت retained ارسال می‌شود و پیام **Last Will** می‌تواند پس از شناسایی قطع غیرمنتظره دستگاه، مقدار retained `offline` را منتشر کند. نمایش وضعیت `offline` ممکن است با تأخیر انجام شود.

### ساختار JSON

| فیلد | مفهوم |
|---|---|
| `device_id` | شناسه دستگاه؛ برای چند ESP32 باید یکتا باشد |
| `temperature` | دما برحسب درجه سلسیوس |
| `humidity` | رطوبت نسبی برحسب درصد |
| `uptime_ms` | زمان سپری‌شده از روشن‌شدن ESP32، برحسب میلی‌ثانیه |

**چرا `timestamp` نداریم؟** اسلاید Telemetry ساختار نهایی پیام شامل زمان واقعی را معرفی می‌کند، اما این نسخه هنوز ساعت را با NTP همگام نکرده است. `uptime_ms` یک **Timestamp واقعی** نیست؛ در مبحث NTP، زمان معتبر اندازه‌گیری را اضافه خواهیم کرد.

## ۵. رفتار برنامه و محدودیت‌های فعلی

- خواندن سنسور و OLED حتی هنگام قطع شبکه ادامه دارد.
- برنامه برای اتصال مجدد Wi-Fi و MQTT تلاش‌های دوره‌ای انجام می‌دهد؛ هر تلاش اتصال MQTT **ممکن است برای مدت کوتاهی مسدودکننده باشد**.
- هنگام قطع ارتباط، داده‌های اندازه‌گیری‌شده **ذخیره یا بعداً ارسال نمی‌شوند**. Buffering و بازیابی را در مبحث Reliability پیاده‌سازی می‌کنیم.
- این مثال از **PubSubClient** استفاده می‌کند که در تابع Publish فقط **QoS 0** ارائه می‌دهد؛ QoS 1 و QoS 2 در اسلاید مفهومی بررسی شدند، اما این نمونه با آن سطوح Publish نمی‌کند. مقدار QoS در تنظیم **Last Will** مستقل از QoS ارسال Telemetry است.
- کتابخانه PubSubClient طبق اعلان فعلی نگهدارنده‌اش دیگر به‌صورت فعال توسعه داده نمی‌شود؛ در این مرحله صرفاً برای یک مثال ساده Arduino IDE انتخاب شده است. برای پروژه تولیدی، وضعیت نگهداری و گزینه‌های جایگزین را دوباره ارزیابی کنید.
- `MQTT_PORT = 1883` و `WiFiClient` یعنی **بدون TLS**. نمونه TLS در بخش مستقل امنیت دوره خواهد آمد.

## ۶. رفع خطاهای رایج

| علامت | مورد بررسی |
|---|---|
| `secrets.h: No such file or directory` | یک کپی از `secrets.h.example` به نام `secrets.h` در **همان پوشه Sketch** ایجاد کنید. |
| `MQTT: failed, state=-2` | اتصال TCP برقرار نمی‌شود؛ IP، شبکه Wi-Fi، Listener و UFW را بررسی کنید. |
| `MQTT: failed, state=4/5` | نام کاربری یا رمز و تنظیمات احراز هویت Broker را بررسی کنید. |
| پیام MQTT نمی‌رسد | ابتدا `mosquitto_sub` را اجرا کنید؛ Topic، آدرس Broker و وضعیت Serial را بررسی کنید. |
| `DHT22: sensor reading failed` | پایه GPIO4، تغذیه، GND و Pull-up سنسور را بررسی کنید. |
| `OLED not found!` | SDA/SCL، تغذیه و آدرس I2C را بررسی کنید. |

در صورت اختلال سرویس، روی Raspberry Pi این دستورها کمک می‌کنند:

```bash
sudo journalctl -u mosquitto -n 30 --no-pager
sudo ss -lntp | grep ':1883'
sudo ufw status
```

## معیار پایان

- [ ] مقادیر معتبر دما و رطوبت روی OLED نمایش داده می‌شوند.
- [ ] پیام JSON در `iot/room-01/telemetry` روی Raspberry Pi دریافت می‌شود.
- [ ] پس از قطع موقت Wi-Fi، نمایش محلی ادامه دارد.
- [ ] پس از برقراری مجدد ارتباط، داده‌های **جدید** Publish می‌شوند؛ داده‌های زمان قطعی بازیابی نمی‌شوند.
- [ ] هنگام قطع غیرمنتظره MQTT، پس از تشخیص Broker، وضعیت `offline` در Topic وضعیت مشاهده می‌شود.

## منابع

- [PubSubClient — GitHub](https://github.com/knolleary/pubsubclient)
- [PubSubClient — API](https://pubsubclient.knolleary.net/api)
- [Eclipse Mosquitto](https://mosquitto.org/documentation/)
- [Adafruit DHT library](https://github.com/adafruit/DHT-sensor-library)
