# Raspberry Pi MQTT Subscriber — Python

در این مثال، پیام‌هایی را که **ESP32** با سنسور **DHT22** منتشر می‌کند، روی **Raspberry Pi** با Python دریافت و نمایش می‌دهیم. کد با دو Topic و قالب JSON موجود در [`ESP32_MQTT_PUBLISH`](../ESP32_MQTT_PUBLISH/README.md) سازگار است.

> هدف این پوشه **دریافت و بررسی پیام‌ها** است؛ هنوز پایگاه داده، داشبورد، TLS یا بافرکردن داده‌ها پیاده‌سازی نمی‌کنیم.

## فایل‌ها

```text
RaspberryPi_MQTT_SUBSCRIBER/
├── subscriber.py
└── README.md
```

## پیش‌نیازها

- راه‌اندازی Raspberry Pi و SSH طبق [`RaspberryPi_SSH_SETUP`](../RaspberryPi_SSH_SETUP/README.md).
- نصب Mosquitto، فعال‌بودن Listener و ساخت حساب MQTT طبق [`RaspberryPi_MQTT_BROKER`](../RaspberryPi_MQTT_BROKER/README.md).
- بارگذاری کد [`ESP32_MQTT_PUBLISH`](../ESP32_MQTT_PUBLISH/README.md) روی ESP32؛ اتصال Wi-Fi و Broker برقرار باشد.
- Python 3 و اینترنت موقت برای نصب کتابخانه Python، یا بسته نصب‌شده از قبل.

**نکته امنیتی:** این پروژه برای شبکه محلی مورداعتماد و پورت `1883` (بدون TLS) است. Broker را روی اینترنت عمومی قرار ندهید. رمز واقعی MQTT را در کد یا GitHub ننویسید.

## ۱. بررسی Broker و Topicها

**همه دستورهای این بخش روی Raspberry Pi اجرا می‌شوند** (می‌توانید با SSH از مک وصل شوید).

ابتدا از فعال‌بودن Mosquitto اطمینان پیدا کنید:

```bash
sudo systemctl status mosquitto --no-pager
```

ESP32 باید این Topicها را منتشر کند:

```text
iot/room-01/telemetry
iot/room-01/status
```

برای آزمایش دستی با `mosquitto_sub` نیز می‌توانید به [راهنمای Broker](../RaspberryPi_MQTT_BROKER/README.md) مراجعه کنید؛ در ادامه همین راهنما، Subscriber پایتونی را اجرا می‌کنیم که رمز را به‌شکل مخفی دریافت می‌کند.

## ۲. آماده‌کردن Python

یک محیط مجازی **خارج از پوشه GitHub** بسازید تا فایل‌های آن ناخواسته Commit نشوند:

```bash
python3 -m venv ~/iot-course-venv
source ~/iot-course-venv/bin/activate
python -m pip install 'paho-mqtt>=2.1,<3'
```

اگر خطای نبود `venv` دریافت کردید، روی Raspberry Pi OS مبتنی بر Debian می‌توانید بسته موردنیاز را نصب کنید و دوباره محیط مجازی بسازید:

```bash
sudo apt update
sudo apt install -y python3-venv
```

این نمونه با **Callback API Version 2** کتابخانه Paho MQTT نوشته شده است.

## ۳. اجرای Subscriber

ابتدا وارد پوشه مثال شوید (مسیر را مطابق محل دریافت ریپازیتوری تغییر دهید):

```bash
cd ~/advanced-IoT/RaspberryPi_MQTT_SUBSCRIBER
```

نام کاربری MQTT ساخته‌شده در راهنمای Broker را به محیط Shell بدهید:

```bash
export MQTT_USERNAME='iot_device'
```

چون برنامه روی **خود Raspberry Pi** اجرا می‌شود، آدرس پیش‌فرض Broker برابر `127.0.0.1` و پورت پیش‌فرض `1883` است. اگر در دستگاه دیگری برنامه را اجرا می‌کنید، آدرس واقعی Raspberry Pi را مشخص کنید:

```bash
export MQTT_HOST='192.168.1.50'
export MQTT_PORT='1883'
```

برای اجرای نمونه:

```bash
python subscriber.py
```

برنامه رمز MQTT را **بدون نمایش کاراکترهای تایپ‌شده** درخواست می‌کند. آن را وارد کنید و Enter بزنید. برای توقف از `Ctrl+C` استفاده کنید.

> اگر در همان ترمینال SSH برنامه را اجرا می‌کنید، از واردکردن رمز در اسلاید، ویدئو یا فایل کد خودداری کنید. برای ضبط از حساب آزمایشی استفاده کنید.

## ۴. خروجی مورد انتظار

وقتی ESP32 آنلاین باشد، خروجی چیزی شبیه نمونه زیر است (اعداد نمونه‌اند):

```text
Connected to 127.0.0.1:1883
Subscribed: iot/room-01/telemetry, iot/room-01/status
[STATUS] (retained) online
[TELEMETRY] esp32-room-01: 24.6 C, 42.8 %, uptime=152000 ms
[TELEMETRY] esp32-room-01: 24.7 C, 42.9 %, uptime=154500 ms
```

توضیح فیلدها:

| فیلد | مفهوم |
|---|---|
| `device_id` | شناسه ESP32 ارسال‌کننده |
| `temperature` | دمای اندازه‌گیری‌شده، درجه سلسیوس |
| `humidity` | رطوبت نسبی، درصد |
| `uptime_ms` | زمان سپری‌شده از راه‌اندازی ESP32 برحسب میلی‌ثانیه؛ **تاریخ/ساعت واقعی نیست** |

کد Python هر دو Topic را Subscribe می‌کند. پیام وضعیت **متنی** (`online` یا `offline`) است؛ پیام Telemetry **JSON** است و برنامه آن را Parse می‌کند. اگر پیام JSON نامعتبر یا فیلدهای ضروری ناقص باشند، خطا چاپ می‌شود و برنامه ادامه می‌دهد.

**Retain و Last Will:** اگر Broker آخرین وضعیت را نگه داشته باشد، هنگام اتصال می‌توانید فوراً پیام `(retained)` ببینید. در صورت قطع غیرمنتظره ESP32، پیام `offline` فقط **پس از تشخیص قطع از طرف Broker** منتشر می‌شود و ممکن است با تأخیر برسد. پیام Telemetry در کد فعلی retained نیست.

## ۵. تست عملی برای ضبط دوره

1. برنامه `subscriber.py` را روی Raspberry Pi اجرا کنید؛ اتصال به Broker و Subscribe را بررسی کنید.
2. ESP32 را روشن کنید و مقادیر دما/رطوبت روی OLED را با خروجی Python مقایسه کنید.
3. یک JSON معتبر در Topic `telemetry` دریافت و فیلدهای آن را توضیح دهید.
4. ESP32 را خاموش کنید؛ پس از شناسایی قطع از سمت Broker، تغییر `status` به `offline` را بررسی کنید.
5. دوباره ESP32 را روشن کنید تا پیام `online` و اندازه‌گیری‌های تازه مشاهده شوند.

**حد پروژه فعلی:** Python فقط پیام‌ها را چاپ می‌کند؛ آن‌ها را ذخیره نمی‌کند. همچنین قطع اولیه اتصال Broker ممکن است باعث پایان برنامه شود؛ مدیریت کامل قطع ارتباط و ذخیره‌سازی در بخش Reliability دوره آموزش داده می‌شود.

## ۶. رفع خطاهای رایج

| علامت | بررسی |
|---|---|
| `ModuleNotFoundError: No module named 'paho'` | محیط مجازی را فعال و `paho-mqtt` را نصب کنید. |
| `Set MQTT_USERNAME first` | دستور `export MQTT_USERNAME='iot_device'` را اجرا کنید. |
| `Connection refused` | وضعیت Mosquitto، Host/Port و Listener را بررسی کنید. |
| `Not authorized` | نام کاربری، رمز و فایل حساب‌های Mosquitto را بررسی کنید. |
| اتصال برقرار است ولی Telemetry دیده نمی‌شود | Serial Monitor روی ESP32، تطابق Topic، اتصال MQTT و معتبر بودن داده DHT22 را بررسی کنید. |
| فقط `online` می‌بینید | وضعیت retained است؛ منتظر Publish جدید تله‌متری بمانید و برنامه ESP32 را بررسی کنید. |
| خطای `Invalid telemetry` | نوع و نام فیلدهای JSON را با کد Publisher تطبیق دهید. |

دستورات عیب‌یابی سمت Raspberry Pi:

```bash
sudo systemctl status mosquitto --no-pager
sudo journalctl -u mosquitto -n 30 --no-pager
sudo ss -lntp | grep ':1883'
```

## معیار پایان

- [ ] Subscriber پایتونی به Broker متصل می‌شود.
- [ ] JSON واقعی ESP32 دریافت و به فیلدهای دما، رطوبت، شناسه و uptime تبدیل می‌شود.
- [ ] وضعیت `online` و در صورت قطع غیرمنتظره، `offline` قابل مشاهده است.
- [ ] رمز MQTT در ریپازیتوری ذخیره نشده است.

## منابع

- [Eclipse Paho MQTT Python — Documentation](https://eclipse.dev/paho/clients/python/)
- [Eclipse Paho MQTT Python — GitHub](https://github.com/eclipse-paho/paho.mqtt.python)
- [Eclipse Mosquitto](https://mosquitto.org/documentation/)
