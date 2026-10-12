# Mosquitto TLS + ACL — Raspberry Pi (Part 08, Slides 35–38)

این راهنما **ادامه مثال قبلی Broker** است، نه یک نصب جدید و مستقل. در پایان، ESP32 از طریق TLS به Mosquitto روی Raspberry Pi متصل می‌شود، هویت **Broker** را با یک CA آزمایشگاهی بررسی می‌کند و با حساب اختصاصی و ACL فقط می‌تواند روی Topicهای خودش پیام بفرستد.

> **دامنه آزمایش:** TLS با احراز هویت **سرور** + Username/Password درون کانال TLS + ACL. این مثال **Mutual TLS (گواهی کلاینت)** نیست و جایگزین مدیریت امن Credential در محیط تولیدی نمی‌شود. گواهی‌ها **مختص آزمایشگاه خودتان** هستند. فایل‌های `*.key`، فایل رمز کاربران و اطلاعات واقعی را هرگز در GitHub نگذارید.

## ۰) پیش‌نیاز و راه جلوگیری از قطع سرویس

- Raspberry Pi OS، `mosquitto`، `mosquitto-clients` و SSH از قبل راه‌اندازی شده باشند؛ به [README قبلی Broker](README.md) مراجعه کنید.
- IP ثابت یا DHCP Reservation برای Pi، مثال: `192.168.1.50`؛ نام اختیاری: `iot-pi.local`.
- ESP32 و Raspberry Pi در یک شبکه قابل دسترس باشند؛ ساعت ESP32 از NTP تنظیم شود (برای اعتبارسنجی X.509).
- **قبل از دست‌کاری تنظیمات** یک جلسه SSH فعال نگه دارید و تنظیمات فعلی را Backup کنید:

```bash
sudo cp /etc/mosquitto/conf.d/iot-course.conf \
  /etc/mosquitto/iot-course.conf.before-tls.bak
sudo systemctl status mosquitto --no-pager
sudo ufw status verbose
```

اگر فایل پیکربندی قبلی نام متفاوتی دارد، **مسیر واقعی خودتان** را جایگزین کنید. از چند `listener 1883` یا `listener 8883` هم‌زمان در فایل‌های مختلف پرهیز کنید. `include_dir` را در `/etc/mosquitto/mosquitto.conf` بررسی کنید.

**استراتژی مهاجرت:** اول Listener امن ۸۸۸۳ را در کنار ۱۸۸۳ موجود راه‌اندازی و تست می‌کنیم؛ پس از تایید، دسترسی شبکه‌ای ۱۸۸۳ را به localhost محدود می‌کنیم. مثال‌های قبلی که هنوز روی ۱۸۸۳ هستند بعد از محدودسازی باید برای ۸۸۸۳ به‌روزرسانی شوند.

## ۱) ساخت گواهی اختصاصی آزمایشگاه

این دستورات `openssl` را برای **محیط آزمایشگاهی** روی Raspberry Pi اجرا می‌کنیم. در استقرار واقعی، **کلید خصوصی CA باید از Broker جدا نگهداری شود** و صدور/چرخش گواهی طبق سیاست سازمان باشد.

```bash
mkdir -p ~/iot-tls-lab
cd ~/iot-tls-lab
umask 077

# Change these to YOUR broker values before running the commands.
BROKER_IP="192.168.1.50"
BROKER_DNS="iot-pi.local"

# Local teaching CA -- keep the private key ca.key SECRET.
openssl genrsa -out ca.key 3072
openssl req -new -x509 -sha256 -days 365 \
  -key ca.key -out ca.crt \
  -subj "/CN=Advanced IoT Lab Root CA" \
  -addext "basicConstraints=critical,CA:TRUE" \
  -addext "keyUsage=critical,keyCertSign,cRLSign"

# Server-specific private key and certificate signing request.
openssl genrsa -out server.key 2048
openssl req -new -sha256 -key server.key -out server.csr \
  -subj "/CN=${BROKER_DNS}"

# SAN must match exactly the DNS name or IP used by the ESP32.
printf 'subjectAltName=DNS:%s,IP:%s\nextendedKeyUsage=serverAuth\nkeyUsage=digitalSignature,keyEncipherment\n' \
  "$BROKER_DNS" "$BROKER_IP" > server.ext
openssl x509 -req -sha256 -days 365 \
  -in server.csr -CA ca.crt -CAkey ca.key -CAcreateserial \
  -out server.crt -extfile server.ext

# Verify chain AND the exact IP identity (or use -verify_hostname for DNS).
openssl verify -CAfile ca.crt -verify_ip "$BROKER_IP" server.crt
openssl x509 -in server.crt -noout -subject -dates -ext subjectAltName
```

اگر قرار است ESP32 به `192.168.1.50` وصل شود، باید همان IP در SAN سرور باشد؛ وجود آن فقط در CN کافی نیست. برای نام شبکه `iot-pi.local` نیز باید `DNS:iot-pi.local` در SAN باشد **و ESP32 بتواند آن نام را Resolve کند**. تغییر IP/نام Broker پس از صدور گواهی ممکن است به صدور گواهی جدید نیاز داشته باشد.

> **امنیت کلیدها:** `ca.crt` یک گواهی **عمومی** است و برای اعتبارسنجی به ESP32 منتقل می‌شود. اما `ca.key` و `server.key` **محرمانه‌اند**. کلید `ca.key` را بعد از صدور، به نگهداری امن خارج از Pi منتقل کنید و نسخه اضافه را مطابق سیاست امنیتی حذف کنید؛ حذف فایل روی کارت حافظه لزوماً پاک‌سازی فیزیکی قطعی نیست. **هیچ‌کدام از کلیدهای خصوصی را در مخزن یا ویدئو منتشر نکنید.**

## ۲) نصب گواهی سرور با مجوز مناسب

```bash
sudo install -d -o root -g mosquitto -m 750 /etc/mosquitto/certs
sudo install -o root -g mosquitto -m 644 ca.crt /etc/mosquitto/certs/ca.crt
sudo install -o root -g mosquitto -m 644 server.crt /etc/mosquitto/certs/server.crt
sudo install -o root -g mosquitto -m 640 server.key /etc/mosquitto/certs/server.key
```

**کلید CA را در مسیر Mosquitto کپی نکنید.** برای مصرف‌کننده‌های مجاز (ESP32 و کامپیوتر آزمایشگاه) فقط محتوای عمومی `ca.crt` موردنیاز است؛ آن را از یک کانال مطمئن منتقل کنید و Fingerprint آن را با نسخه اصلی مقایسه کنید.

## ۳) ساخت کاربران و ACL محدود به Topic

فایل Password از بخش قبلی از قبل وجود دارد. **دیگر از `mosquitto_passwd -c` استفاده نکنید**؛ این گزینه فایل را از نو می‌سازد و حساب‌های قبلی را از بین می‌برد.

```bash
sudo mosquitto_passwd /etc/mosquitto/passwd esp32_room01
sudo mosquitto_passwd /etc/mosquitto/passwd pi_monitor
sudo chown root:mosquitto /etc/mosquitto/passwd
sudo chmod 640 /etc/mosquitto/passwd
```

رمزهای قوی و **متفاوت** وارد کنید. هیچ رمزی در دستورها، فایل‌های Git یا ضبط نمایش ندهید.

فایل ACL ایجاد کنید:

```bash
sudo nano /etc/mosquitto/acl
```

```conf
# Only ESP32 can publish its own telemetry and retained online/offline status.
user esp32_room01
topic write iot/room-01/telemetry
topic write iot/room-01/status

# Python / CLI monitoring account is read-only for this room.
user pi_monitor
topic read iot/room-01/#
```

```bash
sudo chown root:mosquitto /etc/mosquitto/acl
sudo chmod 640 /etc/mosquitto/acl
```

این ACL برای مثال امنیتی جدید نوشته شده. حساب قدیمی `iot_device` از فصل‌های قبلی به‌صورت پیش‌فرض مجوز دریافت نمی‌کند؛ برای ادامه آن مثال‌ها یا از `pi_monitor` استفاده کنید یا سیاست حداقلی جدا بنویسید. فایل ACL نمونه را بدون بازبینی، به سامانه در حال بهره‌برداری اعمال نکنید.

## ۴) افزودن TLS Listener بدون قطع‌کردن ۱۸۸۳

روی همان فایلی که در مثال قبلی ساخته بودیم کار می‌کنیم تا تنظیمات تکراری ایجاد نشوند:

```bash
sudo nano /etc/mosquitto/conf.d/iot-course.conf
```

در **مرحله اول آزمایش**، محتوای آن را با پیکربندی زیر تطبیق دهید (مقدار Listener قبلی ۱۸۸۳ موقتاً باقی است):

```conf
# Lab migration: temporary old LAN listener. Remove or loopback-bind later.
listener 1883

# TLS listener (server-authenticated TLS; no mandatory client certificate).
listener 8883
certfile /etc/mosquitto/certs/server.crt
keyfile /etc/mosquitto/certs/server.key
require_certificate false
tls_version tlsv1.2

# These apply globally to listeners when per_listener_settings is not enabled.
allow_anonymous false
password_file /etc/mosquitto/passwd
acl_file /etc/mosquitto/acl
```

> توجه: `acl_file` و `password_file` در این شیوه **روی هر دو Listener** اعمال می‌شوند؛ `per_listener_settings true` را در فایل دیگر فعال نکنید. اگر در تنظیمات قبلی از plugin امنیتی یا Listenerهای متعدد استفاده می‌کنید، تنظیمات را جداگانه تطبیق دهید و این نمونه را کورکورانه جایگزین نکنید. در بعضی نسخه‌های جدید Mosquitto افزونه‌های جدیدتری برای مدیریت Authorization وجود دارند، اما این مثال از فایل ACL ساده استفاده می‌کند.

**ابتدا Firewall را برای آدرس‌های شبکه محلی مورداعتماد تنظیم کنید و مطمئن شوید SSH مجاز است:**

```bash
sudo ufw status verbose
# 192.168.1.0/24 is an EXAMPLE; replace with your real LAN subnet.
sudo ufw allow from 192.168.1.0/24 to any port 8883 proto tcp
sudo systemctl restart mosquitto
sudo systemctl status mosquitto --no-pager
sudo ss -lnt | grep ':8883'
```

پورت ۸۸۸۳ را روی روتر به اینترنت عمومی Forward نکنید. TLS جایگزین Firewall یا ACL نیست.

## ۵) آزمایش TLS، Authentication و Authorization

### ۵.۱ اعتبارسنجی زنجیره و نام سرور

**روی دستگاهی که فایل عمومی `ca.crt` را دارد:**

```bash
openssl s_client -connect 192.168.1.50:8883 \
  -CAfile ca.crt -verify_ip 192.168.1.50 \
  -verify_return_error </dev/null
```

در خروجی به نتیجه Verification دقت کنید. این دستور فقط TLS handshake را می‌آزماید، نه MQTT login را.

### ۵.۲ MQTT Subscribe امن

در دو ترمینال با `mosquitto-clients`، IP و مسیر CA را جایگزین کنید. **در ویدئو فقط از Credential آزمایشگاهی یک‌بارمصرف استفاده کنید**؛ قرار دادن رمز در `-P` می‌تواند آن را در history یا فهرست فرایندها آشکار کند.

ترمینال ۱ (با حساب فقط خواندنی):

```bash
mosquitto_sub -h 192.168.1.50 -p 8883 \
  --cafile ./ca.crt -u pi_monitor -P 'LAB_PASSWORD_ONLY' \
  -t 'iot/room-01/#' -v -d
```

ترمینال ۲ (با حساب فقط نوشتنی):

```bash
mosquitto_pub -h 192.168.1.50 -p 8883 \
  --cafile ./ca.crt -u esp32_room01 -P 'LAB_PASSWORD_ONLY' \
  -t 'iot/room-01/telemetry' \
  -m '{"device_id":"esp32-room-01","temperature":24.6,"humidity":42.8,"uptime_ms":10000}' -d
```

باید پیام در ترمینال اول دیده شود. برای آزمون منفی، تلاش کنید با حساب `esp32_room01` روی `iot/other-room/telemetry` پیام Publish کنید و با حساب `pi_monitor` به یک Topic غیرمجاز Publish کنید؛ در MQTT v3.1.1 و ابزارهای مختلف، نتیجه ردشدن مجوز ممکن است در لاگ Broker روشن‌تر از خروجی کلاینت باشد. برای بررسی از `sudo journalctl -u mosquitto -n 80 --no-pager` استفاده کنید.

> استفاده از CA آزمایشگاهی به معنی اعتماد پیش‌فرض کل سیستم نیست؛ فقط کلاینت‌هایی که **همان CA عمومی** را درست نصب و بررسی کرده‌اند به گواهی Broker اعتماد می‌کنند. اگر خطای Hostname دارید، از گزینه `--insecure` برای ردشدن از بررسی هویت استفاده نکنید.

## ۶) محدودکردن دسترسی شبکه‌ای MQTT بدون TLS (بعد از موفقیت تست)

پس از اطمینان از اینکه ESP32 و ابزارهای جدید روی TLS/8883 کار می‌کنند، خط زیر را در `iot-course.conf`:

```conf
listener 1883
```

به شکل زیر تغییر دهید تا پورت قدیمی فقط در خود Raspberry Pi قابل دسترس باشد:

```conf
listener 1883 127.0.0.1
```

سپس:

```bash
sudo systemctl restart mosquitto
sudo ss -lnt | grep -E ':1883|:8883'
sudo ufw status numbered
```

قانون قدیمی دسترسی شبکه‌ای ۱۸۸۳ را فقط پس از بررسی شماره و اطمینان از مجوز SSH حذف کنید (`sudo ufw delete NUMBER`). با `mosquitto_sub` و ESP32 دوباره روی ۸۸۸۳ تست بگیرید. انتظار داریم کلاینت LAN دیگر نتواند از ۱۸۸۳ وصل شود، ولی ۸۸۸۳ همچنان فعال باشد. بازبودن پورت ۱۸۸۳ روی `127.0.0.1` صرفاً برای بررسی محلی است و رمزنگاری ندارد.

## ۷) پیکربندی ESP32

به [ESP32_MQTT_TLS](../ESP32_MQTT_TLS/README.md) بروید. فایل `secrets.h` را از قالب بسازید و در `MQTT_ROOT_CA` **کل محتویات** `ca.crt` را قرار دهید. مقدار `MQTT_HOST` باید دقیقاً با SAN سرور منطبق باشد. هیچ `setInsecure()` در این مثال وجود ندارد. NTP باید قبل از TLS به زمان معتبر برسد.

## ۸) سناریوهای خطا و محدودیت‌ها

| وضعیت | نشانه و بررسی پیشنهادی |
|---|---|
| `Connection refused` | سرویس Mosquitto، پورت ۸۸۸۳، آدرس IP و Firewall را بررسی کنید. |
| Certificate verification failure | محتوای CA، SAN DNS/IP، زمان دستگاه، تاریخ انقضا و chain را بررسی کنید. |
| Username/Password رد می‌شود | دو کاربر تازه، `password_file` و لاگ Broker را بررسی کنید. |
| اتصال برقرار است ولی پیام دیده نمی‌شود | ACL، Topic، تفاوت publish/subscribe و QoS 0 را بررسی کنید. |
| Mosquitto پس از restart اجرا نمی‌شود | گواهی/کلید، مجوز `server.key`، گزینه‌های متعارض و Listener تکراری را بررسی کنید. |
| `iot_device` قبلی دیگر به Topicها دسترسی ندارد | ACL جدید را اعمال کرده‌اید؛ مجوز حداقلی جدا تعریف کنید یا از حساب‌های جدید استفاده کنید. |

**مشاهدات قابل‌ثبت در ضبط:** قبل/بعد از TLS با Wireshark، پورت ۱۸۸۳ در برابر ۸۸۸۳، اعتبارسنجی موفق CA، یک تلاش دسترسی غیرمجاز، و پیام امن ESP32. در TLS به‌طور معمول محتوای MQTT در Capture قابل خواندن نیست، ولی متادیتای اتصال مثل IPها و پورت هنوز قابل مشاهده است.

**محدودیت‌ها:** این مثال سرور را احراز هویت می‌کند؛ خود ESP32 با MQTT Username/Password احراز هویت می‌شود (نه گواهی کلاینت). داده‌های Telemetry با `PubSubClient.publish()` و QoS 0 منتشر می‌شوند. رمزنگاری TLS به معنی بافرینگ یا تحویل تضمین‌شده نیست. هیچ گواهی خصوصی یا Secret آماده‌ای همراه مخزن منتشر نشده است.

## منابع رسمی

- [Mosquitto TLS documentation](https://mosquitto.org/man/mosquitto-tls-7.html)
- [Mosquitto configuration reference](https://mosquitto.org/man/mosquitto-conf-5.html)
- [ESP32 Arduino secure clients](https://github.com/espressif/arduino-esp32/tree/master/libraries/NetworkClientSecure)
