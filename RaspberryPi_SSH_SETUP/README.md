# Raspberry Pi SSH Setup

راهنمای راه‌اندازی **Raspberry Pi 4 Model B (4GB)** به‌صورت **Headless** و اتصال امن از ترمینال **macOS**. این مثال فقط آماده‌سازی سیستم‌عامل، شبکه و SSH را پوشش می‌دهد؛ نصب MQTT Broker در مثال مستقل دیگری انجام خواهد شد.

## پیش‌نیازها

- Raspberry Pi 4 Model B و منبع تغذیه مناسب آن
- کارت microSD و کارت‌خوان برای مک
- مک متصل به همان شبکه محلی Raspberry Pi
- اتصال Wi-Fi یا کابل Ethernet
- نرم‌افزار [Raspberry Pi Imager](https://www.raspberrypi.com/software/)

> **نکته:** مراحل نوشتن سیستم‌عامل، اطلاعات کارت microSD انتخاب‌شده را پاک می‌کنند. قبل از شروع، کارت صحیح را انتخاب کنید و از داده‌های مهم نسخه پشتیبان داشته باشید.

## ۱. نصب Raspberry Pi OS با Imager

1. **Raspberry Pi Imager** را روی مک اجرا کنید.
2. در بخش **Device**، مدل **Raspberry Pi 4** را انتخاب کنید.
3. در بخش **OS**، گزینه **Raspberry Pi OS Lite (64-bit)** را انتخاب کنید؛ برای راه‌اندازی بدون مانیتور، محیط گرافیکی ضروری نیست.
4. در بخش **Storage**، کارت microSD موردنظر را انتخاب کنید.
5. به بخش **OS Customisation** بروید؛ محل و شکل این گزینه ممکن است بین نسخه‌های Imager فرق کند.

مقادیر زیر فقط برای **مثال آموزشی** هستند و می‌توانید آن‌ها را تغییر بدهید:

| گزینه | مقدار نمونه | توضیح |
|---|---|---|
| Hostname | `iot-pi` | نام دستگاه در شبکه |
| Username | `iotuser` | نام کاربری دلخواه؛ کاربر `pi` را پیش‌فرض فرض نکنید |
| Password | رمز عبور قوی و اختصاصی | در GitHub ذخیره نکنید |
| Wi-Fi | SSID و رمز شبکه خودتان | اگر Ethernet دارید، اتصال کابلی هم ممکن است |
| Locale | کشور، منطقه زمانی، صفحه‌کلید | مطابق محل استفاده |
| Remote Access | Enable SSH | ضروری برای اتصال Headless |

برای احراز هویت SSH، یکی از دو مسیر را انتخاب کنید:

- **مسیر ساده آموزشی:** هنگام نصب، Password Authentication را فعال کنید و پس از اولین اتصال، SSH Key را طبق مرحله ۵ تنظیم کنید.
- **مسیر مبتنی بر کلید از ابتدا:** ابتدا روی مک کلید SSH بسازید و **فقط کلید عمومی** (`.pub`) را در تنظیمات Imager وارد کنید.

در پایان روی **Write** بزنید و منتظر اتمام نوشتن و تأیید داده‌ها بمانید.

## ۲. اولین راه‌اندازی Raspberry Pi

1. کارت microSD را از مک جدا کنید و داخل Raspberry Pi قرار دهید.
2. Raspberry Pi را با منبع تغذیه مناسب روشن کنید.
3. چند دقیقه فرصت دهید تا اولین راه‌اندازی کامل شود و دستگاه به شبکه متصل شود.
4. اگر از Wi-Fi استفاده کرده‌اید، مک و Raspberry Pi باید در شبکه‌ای باشند که دسترسی متقابل را مجاز می‌کند؛ برخی شبکه‌های Guest ارتباط دستگاه‌ها با یکدیگر را مسدود می‌کنند.

## ۳. پیدا کردن آدرس شبکه

روی **ترمینال مک**، نام میزبان نمونه را آزمایش کنید:

```bash
ping -c 3 iot-pi.local
```

اگر پاسخ دریافت شد، معمولاً می‌توانید از همین نام برای SSH استفاده کنید. نام `iot-pi.local` فقط وقتی درست است که hostname را `iot-pi` تنظیم کرده باشید و شناسایی mDNS در شبکه کار کند.

**اگر نام میزبان پیدا نشد:** وارد پنل مدیریت روتر شوید و در فهرست DHCP / Connected Devices، IP مربوط به Raspberry Pi را پیدا کنید. برای نمونه، ممکن است آدرسی شبیه `192.168.1.50` داشته باشد؛ این IP فرضی است.

> برای پیدا کردن IP، نیازی نیست شبکه را به‌صورت گسترده اسکن کنید. فهرست دستگاه‌های روتر معمولاً روشن‌تر و دقیق‌تر است.

## ۴. اتصال SSH از مک

روی **ترمینال مک**:

```bash
ssh iotuser@iot-pi.local
```

یا با IP واقعی که پیدا کرده‌اید:

```bash
ssh iotuser@192.168.1.50
```

در اولین اتصال، SSH اثرانگشت کلید میزبان را نشان می‌دهد. قبل از پذیرش، مطمئن شوید دستگاه همان Raspberry Pi موردنظر است. اگر Password Authentication فعال باشد، رمز حساب Raspberry Pi درخواست می‌شود.

پس از اتصال، دستورات زیر را **روی Raspberry Pi** اجرا کنید:

```bash
whoami
hostname
hostname -I
ip route
```

- `whoami`: نام کاربر فعلی
- `hostname`: نام میزبان
- `hostname -I`: آدرس‌های IP فعلی دستگاه
- `ip route`: مسیرها و دروازه پیش‌فرض شبکه

برای پایان دادن به نشست SSH:

```bash
exit
```

## ۵. تنظیم SSH Key (پیشنهادی)

**اگر هنگام Imager احراز هویت با کلید عمومی را تنظیم کرده‌اید و اتصال با کلید کار می‌کند، این مرحله را رد کنید.** در غیر این صورت، مراحل زیر را انجام دهید.

### ۵.۱. ساخت کلید روی مک

روی **مک** ابتدا بررسی کنید که کلید عمومی از قبل وجود دارد یا نه:

```bash
ls -l ~/.ssh/id_ed25519.pub
```

اگر وجود ندارد، کلید جدید بسازید:

```bash
ssh-keygen -t ed25519 -C "mac-to-iot-pi"
```

می‌توانید مسیر پیش‌فرض `~/.ssh/id_ed25519` را انتخاب کنید و برای کلید خصوصی یک Passphrase بگذارید. **اگر از قبل کلید دارید، بدون بررسی آن را بازنویسی نکنید.**

### ۵.۲. انتقال کلید عمومی به Raspberry Pi

این دستور را روی **مک** اجرا کنید. فرض شده ورود با رمز عبور هنوز فعال است:

```bash
ssh iotuser@iot-pi.local 'umask 077; mkdir -p ~/.ssh; cat >> ~/.ssh/authorized_keys; chmod 700 ~/.ssh; chmod 600 ~/.ssh/authorized_keys' < ~/.ssh/id_ed25519.pub
```

فقط محتوای فایل `.pub` منتقل می‌شود. **کلید خصوصی را روی GitHub، در README یا روی دستگاه مقصد کپی نکنید.**

### ۵.۳. آزمون اتصال فقط با کلید

روی **مک**:

```bash
ssh -o PreferredAuthentications=publickey -o PasswordAuthentication=no iotuser@iot-pi.local
```

اگر اتصال موفق بود، تأیید می‌شود که روش ورود با کلید کار می‌کند. ممکن است Passphrase مربوط به کلید خصوصی درخواست شود.

### ۵.۴. غیرفعال‌کردن ورود با رمز عبور (اختیاری؛ پس از آزمون موفق)

**پیش از انجام این مرحله، نشست SSH فعلی را باز نگه دارید** و از پنجره دوم مک ورود با کلید را با موفقیت آزمایش کنید. روی Raspberry Pi:

```bash
printf 'PasswordAuthentication no\nPermitRootLogin no\n' | sudo tee /etc/ssh/sshd_config.d/99-iot-course.conf
sudo sshd -t
sudo systemctl reload ssh
sudo sshd -T | grep -E '^(passwordauthentication|permitrootlogin) '
```

انتظار داریم مقادیر مؤثر `passwordauthentication no` و `permitrootlogin no` باشند. سپس **از یک پنجره تازه** دوباره با SSH Key متصل شوید. اگر اتصال ناموفق بود، پیش از بستن نشست قبلی تنظیمات را اصلاح کنید.

## ۶. تنظیم IP ثابت (Static IP)

برای اینکه اتصال SSH و سرویس‌های آینده مانند MQTT به آدرس قابل پیش‌بینی متکی باشند، Raspberry Pi باید آدرسی پایدار داشته باشد. دو راه وجود دارد:

### ۶.۱. روش پیشنهادی: DHCP Reservation روی روتر

این روش به شما **آدرس IP ثابت از طریق DHCP** می‌دهد و نیازی به تغییر تنظیمات شبکه روی Raspberry Pi ندارد.

۱. روی Raspberry Pi، رابط شبکه و آدرس MAC آن را پیدا کنید:

```bash
nmcli device status
ip link show wlan0
# برای اتصال کابلی، به‌جای wlan0 از eth0 یا نام واقعی رابط استفاده کنید.
```

۲. وارد پنل روتر شوید و در بخش **DHCP Reservation / Static Lease**، MAC مربوط به رابط متصل را به یک IP آزاد در همان شبکه اختصاص دهید.

۳. پس از تمدید اتصال یا راه‌اندازی مجدد، آدرس را بررسی کنید:

```bash
hostname -I
ip -4 route
```

> آدرس رزروشده باید با تنظیمات شبکه خودتان مطابقت داشته باشد. مثلاً `192.168.1.50` فقط یک نمونه است. این روش برای کلاس و دسترسی Headless، احتمال قطع ناخواسته SSH را کمتر می‌کند.

### ۶.۲. روش جایگزین: IP استاتیک روی خود Raspberry Pi با NetworkManager

در Raspberry Pi OS جدید (Bookworm و نسخه‌های بعدی)، ابزار پیش‌فرض مدیریت شبکه **NetworkManager** است؛ در این روش از `nmcli` استفاده می‌کنیم. تنظیم دستی IP در نشست SSH ممکن است ارتباط را فوراً قطع کند؛ **این کار را تنها وقتی انجام دهید که به روتر یا نمایشگر/صفحه‌کلید محلی برای بازیابی دسترسی دارید.**

ابتدا تنظیمات فعلی را یادداشت کنید:

```bash
nmcli -f NAME,DEVICE connection show --active
ip -4 addr
ip -4 route
```

نام اتصال فعال، آدرس آزاد، Prefix، Gateway و DNS را با توجه به شبکه **واقعی خودتان** تعیین کنید. مثال زیر فقط برای شبکه فرضی `192.168.1.0/24` است؛ بدون جایگزینی مقادیر اجرا نکنید:

```bash
sudo nmcli connection modify "YOUR_CONNECTION_NAME" \
  ipv4.method manual \
  ipv4.addresses "192.168.1.50/24" \
  ipv4.gateway "192.168.1.1" \
  ipv4.dns "192.168.1.1 1.1.1.1"
```

پس از اطمینان از درست‌بودن آدرس‌ها و نبود تداخل با DHCP و سایر دستگاه‌ها، در صورت داشتن راه بازیابی، اتصال را دوباره فعال کنید:

```bash
sudo nmcli connection up "YOUR_CONNECTION_NAME"
```

> این دستور ممکن است نشست SSH فعلی را قطع کند. با **IP جدید** دوباره وصل شوید. برای بازگشت از حالت دستی به DHCP (از طریق کنسول محلی در صورت قطع اتصال):

```bash
sudo nmcli connection modify "YOUR_CONNECTION_NAME" \
  ipv4.method auto ipv4.addresses "" ipv4.gateway "" ipv4.dns ""
sudo nmcli connection up "YOUR_CONNECTION_NAME"
```

## ۷. سخت‌سازی دسترسی SSH

### ۷.۱. ورود با SSH Key و جلوگیری از ورود root

در **مرحله ۵** کلید `ed25519` ساخته و اتصال از مک با آن آزمایش شد. بعد از تأیید ورود موفق با کلید، تنظیمات زیر باید در `/etc/ssh/sshd_config.d/99-iot-course.conf` ثبت شده باشند:

```text
PasswordAuthentication no
PermitRootLogin no
```

- `PasswordAuthentication no`: ورود SSH با رمز عبور را غیرفعال می‌کند (برای حساب‌های معمولی نیز).
- `PermitRootLogin no`: ورود مستقیم حساب root از طریق SSH را، با رمز یا کلید، غیرفعال می‌کند. برای کارهای مدیریتی از حساب معمولی و `sudo` استفاده کنید.

روی Raspberry Pi، صحت پیکربندی و تنظیمات مؤثر را بررسی کنید:

```bash
sudo sshd -t
sudo sshd -T | grep -E '^(passwordauthentication|permitrootlogin) '
```

**مهم:** پیش از غیرفعال‌کردن احراز هویت با رمز عبور، از پنجره دوم ترمینال مک اتصال با SSH Key را آزمایش کنید و نشست قبلی را باز نگه دارید.

### ۷.۲. تغییر پورت SSH (اختیاری)

پورت پیش‌فرض SSH برابر `22/tcp` است. تغییر پورت می‌تواند از حجم تلاش‌های خودکار روی پورت پیش‌فرض بکاهد، **اما جایگزین SSH Key، کنترل دسترسی یا Firewall نیست.** برای شبکه محلی دوره، تغییر پورت الزامی نیست.

اگر تصمیم دارید پورت را مثلاً به `2222/tcp` تغییر دهید:

۱. **پیش از تغییر پورت**، اگر UFW فعال است، اجازه دسترسی به پورت جدید را از شبکه مورداعتماد صادر کنید. شبکه زیر صرفاً مثال است:

```bash
sudo ufw allow from 192.168.1.0/24 to any port 2222 proto tcp
```

۲. در Raspberry Pi فایل تنظیمات اختصاصی دوره را باز کنید:

```bash
sudo nano /etc/ssh/sshd_config.d/99-iot-course.conf
```

خط `Port 2222` را به فایل اضافه کنید؛ سپس بررسی کنید در سایر فایل‌های `sshd_config` تنظیم متعارض دیگری برای `Port` وجود نداشته باشد:

```bash
sudo grep -RnE '^[[:space:]]*Port[[:space:]]+' /etc/ssh/sshd_config /etc/ssh/sshd_config.d/
sudo sshd -t
sudo sshd -T | grep '^port '
```

تنظیم مؤثر باید شامل پورت موردنظر باشد و در صورت قصد بستن ۲۲، نباید همچنان روی ۲۲ گوش کند. بعد سرویس را Reload کنید:

```bash
sudo systemctl reload ssh
sudo ss -ltnp | grep sshd
```

۳. **نشست SSH قبلی را نبندید.** از پنجره جدید مک، با پورت جدید وارد شوید:

```bash
ssh -p 2222 iotuser@iot-pi.local
```

۴. فقط بعد از موفقیت اتصال روی پورت جدید و اطمینان از بسته‌شدن پورت ۲۲ در تنظیمات `sshd`، قانون قبلی UFW برای پورت ۲۲ را حذف کنید (اگر ایجاد کرده‌اید). قوانین را قبل از حذف با `sudo ufw status numbered` بررسی کنید.

> تغییر SSH Port بیشتر یک تنظیم مدیریتی اختیاری است؛ برای امنیت واقعی، کلید SSH، به‌روزرسانی و محدودسازی دسترسی مهم‌ترند.

## ۸. تنظیم فایروال UFW و بستن پورت‌های غیرضروری

**اصل کار:** ابتدا دسترسی SSH را مجاز کنید، *بعد* فایروال را فعال کنید. در غیر این صورت ممکن است دسترسی از راه دور قطع شود. شبکه `192.168.1.0/24` و پورت‌های زیر مثال‌اند؛ آن‌ها را مطابق شبکه و پورت واقعی SSH جایگزین کنید.

۱. نصب UFW روی Raspberry Pi:

```bash
sudo apt update
sudo apt install ufw -y
```

۲. قبل از فعال‌سازی، وضعیت را بررسی و قوانین مناسب برای SSH تنظیم کنید. اگر از پورت پیش‌فرض ۲۲ استفاده می‌کنید:

```bash
sudo ufw default deny incoming
sudo ufw default allow outgoing
sudo ufw allow from 192.168.1.0/24 to any port 22 proto tcp
sudo ufw status verbose
```

اگر پورت را به ۲۲۲۲ منتقل کرده‌اید، برای همان شبکه مورداعتماد به‌جای ۲۲، پورت ۲۲۲۲ را باز کنید؛ تا تأیید موفق اتصال، قانون پورت قبلی را حذف نکنید.

۳. **در حالی که نشست SSH فعلی باز است**، UFW را فعال کنید:

```bash
sudo ufw enable
sudo ufw status numbered
```

۴. با یک ترمینال جدید از مک مجدداً SSH بزنید. اگر اتصال درست برقرار شد، بررسی کنید هیچ قانون `allow` غیرضروری باقی نمانده باشد. برای حذف یک قانون مشخص، شماره یا متن آن را از خروجی `status numbered` بررسی کنید و طبق مستندات UFW حذف کنید؛ شماره قوانین بعد از هر حذف ممکن است تغییر کند.

**آزمون محلی وضعیت پورت‌ها** روی Raspberry Pi:

```bash
sudo ss -ltnp
sudo ufw status verbose
```

در مراحل بعدی، ممکن است Broker به پورت MQTT (معمولاً `1883/tcp` در حالت بدون TLS) نیاز داشته باشد. **فعلاً این پورت را باز نکنید.** هنگام نصب Mosquitto، فقط اتصال‌های موردنیاز از شبکه قابل اعتماد را مجاز می‌کنیم؛ سرویس MQTT نباید بدون طراحی امنیتی در اینترنت عمومی در دسترس قرار بگیرد.

## ۹. به‌روزرسانی اولیه سیستم‌عامل (اختیاری، پس از اتصال)

روی Raspberry Pi:

```bash
sudo apt update
sudo apt full-upgrade -y
```

اگر به‌روزرسانی‌ها نیاز به راه‌اندازی مجدد داشتند، با `sudo reboot` دستگاه را راه‌اندازی مجدد کنید و دوباره SSH بزنید.

## ۱۰. عیب‌یابی سریع

| مشکل | بررسی پیشنهادی |
|---|---|
| `Could not resolve hostname` | نام میزبان و کارکرد mDNS را بررسی کنید؛ از IP روتر استفاده کنید. |
| `Connection timed out` | وضعیت تغذیه، Wi-Fi/Ethernet، IP، پورت SSH و قوانین UFW را بررسی کنید. |
| `Connection refused` | مطمئن شوید SSH فعال است و روی پورت موردنظر گوش می‌دهد. |
| `Permission denied (publickey)` | نام کاربری، کلید استفاده‌شده و مجوزهای `~/.ssh/authorized_keys` را بررسی کنید. |
| هشدار `REMOTE HOST IDENTIFICATION HAS CHANGED` | قبل از تغییر `known_hosts`، تغییر واقعی دستگاه یا کلید میزبان را از یک مسیر مورداعتماد تأیید کنید. |
| پس از تنظیم IP دستی، SSH قطع شد | از کنسول محلی، Gateway، Prefix، تداخل IP و اتصال فعال NetworkManager را بررسی یا به DHCP برگردانید. |

در صورت دسترسی محلی به Raspberry Pi، وضعیت سرویس را می‌توان با دستور زیر بررسی کرد:

```bash
sudo systemctl status ssh
```

## ۱۱. آزمون نهایی

- [ ] Raspberry Pi OS از کارت microSD بوت می‌شود.
- [ ] Raspberry Pi در شبکه محلی قابل شناسایی است.
- [ ] اتصال SSH از مک برقرار می‌شود.
- [ ] دستورات `whoami`، `hostname` و `hostname -I` خروجی قابل انتظار دارند.
- [ ] ورود مبتنی بر SSH Key آزمایش شده است.
- [ ] ورود مستقیم root غیرفعال شده و نتیجه با `sshd -T` بررسی شده است.
- [ ] آدرس پایدار با DHCP Reservation یا IP دستی مناسب تنظیم شده است (در صورت نیاز).
- [ ] اگر پورت SSH تغییر کرده، اتصال از مک با پورت جدید آزمایش شده است.
- [ ] UFW فعال است و فقط پورت‌های موردنیاز برای شبکه مورداعتماد مجازند (در صورت نصب).

**نتیجه:** Raspberry Pi آماده مدیریت از راه دور است. نصب و پیکربندی Mosquitto MQTT Broker، مرحله‌ای مستقل پس از این راه‌اندازی خواهد بود.

## منابع رسمی

- [Raspberry Pi Imager](https://www.raspberrypi.com/software/)
- [Raspberry Pi — Getting Started](https://www.raspberrypi.com/documentation/computers/getting-started.html)
- [Raspberry Pi — Networking, DHCP & Static IP](https://www.raspberrypi.com/documentation/computers/configuration.html#manage-dhcp-and-static-ip)
- [Raspberry Pi — Remote Access / SSH](https://www.raspberrypi.com/documentation/computers/remote-access.html)
- [UFW Manual](https://manpages.ubuntu.com/manpages/resolute/man8/ufw.8.html)
- [OpenSSH — sshd_config](https://man.openbsd.org/sshd_config)
