# Raspberry Pi MQTT Broker — Mosquitto

در این مثال، **Raspberry Pi 4 Model B** را به یک **MQTT Broker محلی** با استفاده از **Eclipse Mosquitto** تبدیل می‌کنیم. در پایان، می‌توانیم یک پیام آزمایشی را Publish و Subscribe کنیم و اتصال دستگاه‌هایی مثل ESP32 را از شبکه محلی بپذیریم.

> این پوشه فقط **راه‌اندازی و آزمون Broker** را پوشش می‌دهد؛ کد ESP32 و مفاهیم پیشرفته MQTT در مثال‌های مستقل آموزش داده می‌شوند.

## پیش‌نیازها

- Raspberry Pi OS راه‌اندازی‌شده و دسترسی SSH فعال.
- اتصال Raspberry Pi و مک/ESP32 به یک شبکه محلی که ارتباط بین دستگاه‌ها را مجاز می‌کند.
- دسترسی به ترمینال Raspberry Pi با یک حساب دارای `sudo`.
- آدرس IP پایدار برای Raspberry Pi (ترجیحاً **DHCP Reservation**).

برای راه‌اندازی اولیه، به [RaspberryPi_SSH_SETUP](../RaspberryPi_SSH_SETUP/README.md) مراجعه کنید.

## ۱. نصب Mosquitto

**همه دستورهای این بخش را روی Raspberry Pi اجرا کنید** (مستقیم یا از طریق SSH).

```bash
sudo apt update
sudo apt install -y mosquitto mosquitto-clients
```

سرویس را فعال و وضعیت آن را بررسی کنید:

```bash
sudo systemctl enable --now mosquitto
sudo systemctl status mosquitto --no-pager
```

در خروجی باید `active (running)` دیده شود. با دستور زیر نسخه نصب‌شده را ببینید:

```bash
mosquitto -h 2>&1 | head -n 2
```

## ۲. اولین آزمایش: Publish / Subscribe محلی

در نسخه‌های جدید Mosquitto، **بدون تعریف Listener شبکه‌ای، Broker معمولاً فقط اتصال از خود Raspberry Pi را می‌پذیرد**. پس ابتدا ارتباط محلی را تست می‌کنیم.

**ترمینال اول روی Raspberry Pi (Subscriber):**

```bash
mosquitto_sub -h localhost -t 'test/topic' -v
```

**ترمینال دوم روی Raspberry Pi (Publisher):**

```bash
mosquitto_pub -h localhost -t 'test/topic' -m 'Hello MQTT'
```

خروجی مورد انتظار در ترمینال اول:

```text
test/topic Hello MQTT
```

> این تست اولیه، قبل از تغییر تنظیمات پیش‌فرض اجرا می‌شود. اگر قبلاً Broker را تنظیم کرده‌اید، ممکن است برای اتصال محلی هم نام کاربری و رمز عبور لازم باشد.

## ۳. ساخت کاربر MQTT

برای اینکه ESP32 و کلاینت‌های دیگر بتوانند از شبکه متصل شوند، ابتدا احراز هویت را تنظیم می‌کنیم.

```bash
sudo mosquitto_passwd -c /etc/mosquitto/passwd iot_device
```

برای کاربر `iot_device` یک رمز عبور **قوی و اختصاصی** وارد کنید. این دستور رمز را تعاملی می‌گیرد و آن را به شکل هش‌شده ذخیره می‌کند.

> **مهم:** گزینه `-c` فایل رمز موجود را **بازنویسی** می‌کند. برای افزودن کاربر بعدی یا تغییر رمز کاربر موجود، `-c` را حذف کنید: `sudo mosquitto_passwd /etc/mosquitto/passwd USERNAME`.

اجازه خواندن فایل را برای سرویس Mosquitto تنظیم کنید:

```bash
sudo chown root:mosquitto /etc/mosquitto/passwd
sudo chmod 640 /etc/mosquitto/passwd
```

فایل `/etc/mosquitto/passwd` را **هرگز** داخل GitHub قرار ندهید.

## ۴. اجازه اتصال از شبکه محلی

ابتدا مطمئن شوید فایل اصلی پیکربندی Mosquitto پوشه `conf.d` را بارگذاری می‌کند:

```bash
grep -n 'include_dir' /etc/mosquitto/mosquitto.conf
```

در نصب معمول Raspberry Pi OS با بسته Debian، مسیر `include_dir /etc/mosquitto/conf.d` وجود دارد. اگر وجود نداشت، قبل از ادامه تنظیمات نصب خود را بررسی کنید.

فایل تنظیمات اختصاصی دوره را بسازید:

```bash
sudo nano /etc/mosquitto/conf.d/iot-course.conf
```

محتوای زیر را داخل آن قرار دهید:

```conf
# MQTT over TCP, for use inside a trusted local network
listener 1883

# Require username/password
allow_anonymous false
password_file /etc/mosquitto/passwd
```

- `listener 1883`: دریافت اتصال MQTT روی پورت TCP شماره ۱۸۸۳؛ بدون تعیین IP، ممکن است روی تمام رابط‌های شبکه گوش دهد.
- `allow_anonymous false`: جلوگیری از اتصال بدون احراز هویت.
- `password_file`: معرفی فایل حساب‌های مجاز به Broker.

**پیش از بازکردن Listener، اگر UFW فعال است، فقط شبکه محلی مورداعتماد را مجاز کنید.** شبکه `192.168.1.0/24` صرفاً مثال است؛ آن را متناسب با شبکه واقعی خود جایگزین کنید:

```bash
sudo ufw status
sudo ufw allow from 192.168.1.0/24 to any port 1883 proto tcp
```

اگر هنوز UFW فعال نیست، ابتدا دستورالعمل امن‌سازی و بازبودن پورت SSH را در [راهنمای SSH](../RaspberryPi_SSH_SETUP/README.md) مرور کنید. سپس فایروال را مطابق همان راهنما فعال کنید. **پورت 1883 را روی روتر به اینترنت عمومی Forward نکنید.**

سپس سرویس را راه‌اندازی مجدد و وضعیت را بررسی کنید:

```bash
sudo systemctl restart mosquitto
sudo systemctl status mosquitto --no-pager
sudo ss -lntp | grep ':1883'
```

اگر سرویس اجرا نشد، ابتدا [بخش رفع خطا](#۷-رفع-خطاهای-رایج) را ببینید. اگر از قبل فایل‌های پیکربندی دیگر Mosquitto دارید، از نداشتن Listener یا تنظیمات متعارض مطمئن شوید.

## ۵. آزمایش ارتباط احراز هویت‌شده

ابتدا **IP واقعی Raspberry Pi** را پیدا کنید:

```bash
hostname -I
```

در مثال‌های زیر، `192.168.1.50` فقط یک IP فرضی است و باید با آدرس دستگاه شما جایگزین شود.

### ۵.۱. آزمایش روی Raspberry Pi

**ترمینال اول (Subscriber):**

```bash
mosquitto_sub -h localhost -p 1883 -u iot_device -P 'YOUR_PASSWORD' -t 'test/topic' -v
```

**ترمینال دوم (Publisher):**

```bash
mosquitto_pub -h localhost -p 1883 -u iot_device -P 'YOUR_PASSWORD' -t 'test/topic' -m 'Hello MQTT'
```

خروجی مورد انتظار:

```text
test/topic Hello MQTT
```

### ۵.۲. آزمایش از دستگاه دیگری در LAN

روی مک، در صورت نصب بودن `mosquitto-clients` یا کلاینت سازگار، **آدرس Broker** را به IP واقعی Raspberry Pi تغییر دهید:

```bash
mosquitto_sub -h 192.168.1.50 -p 1883 -u iot_device -P 'YOUR_PASSWORD' -t 'test/topic' -v
```

سپس از ترمینال Raspberry Pi دستور Publish بخش قبل را اجرا کنید و دریافت پیام روی مک را بررسی کنید. می‌توانید همین آزمایش را در **MQTT Explorer** با مشخصات Host، Port، Username و Password انجام دهید.

> **نکته امنیتی مربوط به ضبط:** دستورهای دارای `-P` فقط برای نمایش شکل فرمان هستند. قرار دادن **رمز واقعی** در آرگومان خط فرمان ممکن است آن را در Shell History یا فهرست فرایندها آشکار کند. در ویدئو از رمز آزمایشی استفاده کنید و آن را بعداً تغییر دهید؛ هیچ رمزی را در اسلاید، GitHub یا تصویر ضبط‌شده منتشر نکنید. پورت ۱۸۸۳ رمزنگاری ندارد؛ **MQTT over TLS** را در بخش امنیت دوره اضافه می‌کنیم.

## ۶. معیار پایان این مثال

- [ ] سرویس Mosquitto فعال است (`active (running)`).
- [ ] Publish/Subscribe محلی به درستی کار می‌کند.
- [ ] اتصال بدون احراز هویت به Listener شبکه‌ای رد می‌شود.
- [ ] کلاینت مجاز از شبکه محلی می‌تواند متصل شود.
- [ ] فایروال فقط دسترسی‌های لازم را می‌پذیرد و SSH همچنان قابل استفاده است.

## ۷. رفع خطاهای رایج

| مشکل | مورد بررسی |
|---|---|
| `Connection refused` از مک یا ESP32 | IP صحیح است؟ Broker روی 1883 گوش می‌دهد؟ UFW یا شبکه Guest مانع نشده است؟ |
| `Connection refused` در آزمون اولیه محلی | وضعیت سرویس، لاگ و Listenerهای موجود را بررسی کنید. |
| `Not authorized` | نام کاربری، رمز عبور، `allow_anonymous` و مسیر فایل رمز را بررسی کنید. |
| سرویس پس از تغییر تنظیمات اجرا نمی‌شود | وجود فایل رمز، مجوز خواندن آن و تکراری نبودن Listenerها را بررسی کنید. |
| پیام در Subscriber دیده نمی‌شود | Topic دقیقاً یکسان باشد؛ ابتدا Subscriber را اجرا کنید و سپس Publish کنید. |

دستورهای عیب‌یابی:

```bash
sudo systemctl status mosquitto --no-pager
sudo journalctl -u mosquitto -n 50 --no-pager
sudo ss -lntp | grep ':1883'
sudo ufw status verbose
```

## گام بعدی

با آماده‌شدن Broker، می‌توانیم یک **ESP32 MQTT Publisher** بسازیم و داده‌های DHT22 را به Raspberry Pi ارسال کنیم. این کد در پوشه مستقل خودش همراه با README اختصاصی قرار خواهد گرفت.

## منابع رسمی

- [Eclipse Mosquitto — Documentation](https://mosquitto.org/documentation/)
- [Mosquitto — Authentication Methods](https://mosquitto.org/documentation/authentication-methods/)
- [Mosquitto — Version 2.0 Changes](https://mosquitto.org/documentation/migrating-to-2-0/)
- [Mosquitto — Configuration Reference](https://mosquitto.org/man/mosquitto-conf-5.html)
