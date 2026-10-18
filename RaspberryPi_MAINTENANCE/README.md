# Raspberry Pi Remote Maintenance — Slide 42

**هدف:** نگهداری و بررسی Raspberry Pi که در پروژه دوره نقش MQTT Broker و محل اجرای Python Subscriber را دارد؛ با تمرکز بر دسترسی SSH، بررسی سرویس‌ها، لاگ‌ها، به‌روزرسانی برنامه‌ریزی‌شده، انتقال فایل و معرفی Ansible.

> این یک **راهنمای عملی نگهداری** است، نه یک سرویس خودکار یا نرم‌افزار نصب‌شدنی جدید. بخش Remote Logging، صرفاً **Logهای سرویس Raspberry Pi** را بررسی می‌کند؛ ارسال Log از خود ESP32 به Raspberry Pi در مثال‌های فعلی پیاده نشده است.

## ۱. پیش‌نیاز و اصول ایمنی

- دسترسی SSH پایدار و ترجیحاً با SSH Key؛ راهنما: [RaspberryPi_SSH_SETUP](../RaspberryPi_SSH_SETUP/README.md).
- حسابی با مجوز `sudo` برای مدیریت سرویس‌ها؛ دسترسی SSH را هنگام کار با UFW قطع نکنید.
- از وضعیت کنونی سرویس، گواهی‌ها، ACL و تنظیمات مهم **نسخه پشتیبان امن** تهیه کنید؛ فایل‌های دارای رمز و Private Key را در Repository یا پوشه عمومی نگه ندارید.
- به‌روزرسانی و Restart را ترجیحاً در **پنجره نگهداری** انجام دهید و پس از هر مرحله عملکرد MQTT را بررسی کنید.

## ۲. SSH و بررسی اولیه

روی Mac (نام کاربری و Hostname مثال هستند):

```bash
ssh iotuser@iot-pi.local
```

روی Raspberry Pi:

```bash
hostname -I
uptime
free -h
df -h
sudo systemctl status mosquitto --no-pager
```

موارد مهم: فضای خالی دیسک، میزان حافظه، زمان Uptime و وضعیت Mosquitto.

## ۳. مشاهده Log و تشخیص خطا

```bash
sudo journalctl -u mosquitto -n 30 --no-pager
sudo journalctl -u mosquitto --since "1 hour ago" --no-pager
sudo systemctl is-active mosquitto
```

- بسته به تنظیمات Logging خود Mosquitto، **تمام پیام‌ها الزاماً در Journal ثبت نمی‌شوند**. در صورت نیاز مسیر Log تنظیم‌شده در پیکربندی سرویس را بررسی کنید.
- برنامه Python Subscriber ممکن است مستقل از systemd و داخل یک Terminal اجرا شود؛ در این حالت لاگ آن الزاماً در Journalِ Mosquitto دیده نمی‌شود.
- برای Logging پایدار برنامه Python در محیط واقعی، می‌توان بعداً یک systemd service مستقل طراحی کرد؛ **این README چنین سرویسی را نصب نمی‌کند**.

## ۴. APT — بررسی و نصب کنترل‌شده به‌روزرسانی‌ها

ابتدا فقط فهرست بسته‌ها و موارد قابل ارتقا را بررسی کنید:

```bash
sudo apt update
apt list --upgradable
```

پس از بررسی Backup، زمان مناسب و تأثیر بر سرویس‌های موجود:

```bash
sudo apt full-upgrade
```

این دستور ممکن است بسته‌های حساس یا وابستگی‌ها را تغییر دهد. بدون نیاز و برنامه بازیابی، آن را روی سیستم عملیاتی بحرانی اجرا نکنید. در صورت نیاز به Reboot بعد از آپدیت، زمانی را انتخاب کنید که توقف کوتاه Broker قابل قبول است:

```bash
sudo reboot
```

پس از بالا آمدن دستگاه و اتصال مجدد SSH:

```bash
sudo systemctl status mosquitto --no-pager
```

این فرایند **ارتقای Major Version سیستم‌عامل** را به‌صورت خودکار انجام نمی‌دهد؛ برای آن برنامه و مستندات جدا لازم است.

## ۵. rsync — انتقال امن فایل مثال Subscriber

در این مثال مسیر محلی روی Mac، همان پوشه Repository است. ابتدا پوشه مقصد را روی Raspberry Pi بسازید:

```bash
ssh iotuser@iot-pi.local 'mkdir -p ~/iot/subscriber'
```

در **ریشه Repository** روی Mac، ابتدا Dry Run انجام دهید:

```bash
rsync -avn --exclude '.env' --exclude 'secrets.h' \
  RaspberryPi_MQTT_SUBSCRIBER/ \
  iotuser@iot-pi.local:~/iot/subscriber/
```

اگر فهرست تغییرات درست بود، انتقال واقعی:

```bash
rsync -av --exclude '.env' --exclude 'secrets.h' \
  RaspberryPi_MQTT_SUBSCRIBER/ \
  iotuser@iot-pi.local:~/iot/subscriber/
```

- نمونه بالا از SSH برای انتقال استفاده می‌کند. اگر SSH روی پورت دیگری است، آن را به‌صورت مناسب با `-e 'ssh -p PORT'` تنظیم کنید.
- از `--delete` بدون Backup و بررسی مسیر دقیق مقصد استفاده نکنید.
- فایل‌های رمز، تنظیمات خاص محیط و کلیدهای خصوصی را به‌صورت اتفاقی منتقل یا Commit نکنید.
- کپی فایل به‌تنهایی به معنی راه‌اندازی سرویس، نصب Dependency یا Restart برنامه نیست. بعد از انتقال، اجرای Python Subscriber را طبق README خودش بررسی کنید.

## ۶. Ansible — آشنایی مفهومی و نمونه محدود

اگر چند Raspberry Pi داشته باشیم، Ansible می‌تواند کارهای تکرارشونده را با SSH از یک کامپیوتر مدیریتی انجام دهد. در این دوره **استقرار چند دستگاهی واقعی اجرا نمی‌شود**؛ نمونه زیر صرفاً شکل Inventory و یک بررسی Read-only را نشان می‌دهد.

نمونه `inventory.ini` در کامپیوتر مدیریتی (اطلاعات Placeholder هستند):

```ini
[raspberry_pi]
lab-pi ansible_host=iot-pi.local ansible_user=iotuser
```

نمونه Playbook مفهومی و بدون تغییر سیستم:

```yaml
- name: Check broker state
  hosts: raspberry_pi
  gather_facts: false
  tasks:
    - name: Check Mosquitto service
      ansible.builtin.command: systemctl is-active mosquitto
      register: broker_status
      changed_when: false
    - name: Show result
      ansible.builtin.debug:
        var: broker_status.stdout
```

برای اجرای واقعی باید Ansible روی کامپیوتر مدیریتی نصب باشد و SSH Key/Inventory درست تنظیم شده باشند. در محیط حساس، Playbookهای تغییر وضعیت را ابتدا روی دستگاه آزمایشگاهی امتحان کنید.

## ۷. Checklist نگهداری و Recovery

- [ ] وضعیت SSH و IP پایدار بررسی شده است.
- [ ] فضای دیسک، منابع و Uptime بررسی شده‌اند.
- [ ] وضعیت و Logهای Mosquitto مشاهده شده‌اند.
- [ ] از فایل‌های حساس و تنظیمات Broker Backup خصوصی تهیه شده است.
- [ ] بسته‌های قابل آپدیت بررسی شده‌اند، بدون ارتقای نسنجیده.
- [ ] `rsync --dry-run` قبل از کپی واقعی انجام شده است.
- [ ] پس از هر تغییر، MQTT pub/sub با حساب مجاز مجدداً تست شده است.
- [ ] دسترسی TLS/8883 و ACL در صورت استفاده همچنان برقرار است.
- [ ] روش برگشت به تنظیمات قبلی و پشتیبان داده‌ها مشخص است.

**یادآوری:** این بخش نگهداری Raspberry Pi است؛ OTA Firmware مربوط به ESP32 در [ESP32_OTA_UPDATE](../ESP32_OTA_UPDATE/README.md) آموزش داده می‌شود. Logهای ESP32 از راه دور و Ansible fleet deployment کامل، **هنوز پیاده نشده‌اند**.

## ۸. منابع رسمی

- [Raspberry Pi OS documentation](https://www.raspberrypi.com/documentation/computers/os.html)
- [Ansible documentation](https://docs.ansible.com/ansible/latest/index.html)
- [rsync manual](https://download.samba.org/pub/rsync/rsync.1)
