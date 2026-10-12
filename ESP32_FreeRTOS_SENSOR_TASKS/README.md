# ESP32 FreeRTOS — Sensor Tasks, Queue & Synchronization

**بخش هفتم دوره (Slides 31–34): Firmware Architecture & FreeRTOS**

در این مثال مستقل، خواندن سنسور **DHT22** و نمایش داده روی **SSD1306 OLED** را به دو **FreeRTOS Task** جدا تقسیم می‌کنیم. یک **Queue** اطلاعات را بین Taskها منتقل می‌کند؛ برای جلوگیری از تداخل متن‌های Serial نیز از **Mutex** استفاده می‌کنیم. موضوعات Task Priority، زمان‌بندی، خطای سنسور و Queue Full را آزمایش می‌کنیم.

> این مثال عمداً **Wi-Fi، MQTT، NTP و LittleFS** ندارد. هدف یادگیری تفکیک مسئولیت‌ها و هم‌زمانی است، نه تبدیل تمام کد قبلی به FreeRTOS. برای تله‌متری و بافر به [ESP32_MQTT_RELIABILITY](../ESP32_MQTT_RELIABILITY/README.md) مراجعه کنید.

## ۱. سخت‌افزار و اتصالات

| قطعه | اتصال ESP32 DevKit V1 |
|---|---|
| DHT22 VCC | 3.3V |
| DHT22 GND | GND |
| DHT22 DATA | GPIO4 |
| OLED SSD1306 VCC | 3.3V |
| OLED GND | GND |
| OLED SDA | GPIO21 |
| OLED SCL | GPIO22 |

- OLED استفاده‌شده: **128×64، I2C، آدرس `0x3C`** (اگر مدل شما آدرس دیگری دارد، `OLED_ADDRESS` را اصلاح کنید).
- برای **DHT22 خام / بدون ماژول** معمولاً مقاومت Pull-up حدود **10kΩ** بین DATA و 3.3V لازم است. بعضی ماژول‌های آماده آن را روی برد دارند.
- اتصالات را **در حالت قطع تغذیه** انجام دهید؛ از 5V به پایه DATA سنسور/ESP32 وصل نکنید.
- این مثال برای **ESP32 DevKit V1** نوشته شده؛ انتخاب Board و پورت را متناسب با برد خودتان انجام دهید.

## ۲. پیش‌نیاز نرم‌افزاری

1. در **Arduino IDE**، بسته **esp32 by Espressif Systems** را در Boards Manager نصب کنید.
2. از Library Manager این کتابخانه‌ها را نصب کنید:
   - `DHT sensor library` — Adafruit
   - `Adafruit Unified Sensor` — وابستگی DHT
   - `Adafruit GFX Library`
   - `Adafruit SSD1306`
3. FreeRTOS Task، Queue و Semaphore در **Arduino-ESP32** از قبل وجود دارند؛ برای این مثال نیاز به نصب یک کتابخانه FreeRTOS جداگانه نیست.
4. فایل [`ESP32_FreeRTOS_SENSOR_TASKS.ino`](./ESP32_FreeRTOS_SENSOR_TASKS.ino) را باز کنید و **Board = ESP32 Dev Module** (یا مدل صحیح بردتان) و Port مناسب را انتخاب کنید.
5. Upload کنید و Serial Monitor را روی **115200 baud** باز کنید.

نام پوشه و فایل `.ino` باید یکسان باقی بماند تا Arduino IDE آن را به‌عنوان یک Sketch مستقل بشناسد.

## ۳. معماری این مثال

```text
                ┌────────────────┐
DHT22 GPIO4 ──► │   SensorTask   │ Priority 2 / every ~2500 ms
                └───────┬────────┘
                        │ xQueueSend() — copy of SensorReading
                        ▼
                ┌────────────────┐
                │ FreeRTOS Queue │ capacity = 5 measurements
                └───────┬────────┘
                        │ xQueueReceive() — waits for data
                        ▼
                ┌────────────────┐
                │  DisplayTask   │ Priority 1 / OLED owner
                └───────┬────────┘
                        ▼
                 SSD1306 OLED

SensorTask & DisplayTask ──► Serial Mutex ──► Serial Monitor
```

**مالکیت منابع:**

- `SensorTask` **تنها وظیفه‌ای است که DHT22 را می‌خواند**؛ نیازی به محافظت جداگانه از DHT در این معماری نداریم.
- بعد از `setup()`، فقط `DisplayTask` به OLED فرمان می‌دهد. بنابراین برای OLED در این مثال **Mutex لازم نیست**.
- هر دو Task برای چاپ Log از **Mutex مخصوص Serial** استفاده می‌کنند تا متن‌هایشان در هم ادغام نشود.
- هر Queue در این مثال یک **مصرف‌کننده** دارد؛ اگر چند Task از همان Queue دریافت کنند، هر داده الزاماً به همه آن‌ها نمی‌رسد. برای چند مصرف‌کننده، Queueهای مستقل یا الگوی Broadcast لازم است.

### ساختار داده

```cpp
struct SensorReading {
  uint32_t sequence;
  uint32_t capturedAtMs;
  float temperatureC;
  float humidityPercent;
  bool valid;
};
```

یک **کپی** از این ساختار با `xQueueSend()` به Queue منتقل و با `xQueueReceive()` از آن دریافت می‌شود. کد، اشاره‌گر به یک متغیر محلیِ موقت را بین Taskها ردوبدل نمی‌کند.

### نکات زمان‌بندی و اولویت

- `vTaskDelayUntil()` دوره تقریبی خواندن سنسور را **۲۵۰۰ میلی‌ثانیه** تنظیم می‌کند؛ محدودیت زمان‌بندی DHT22 (حداقل حدود ۲ ثانیه) رعایت شده است.
- `xQueueReceive(..., portMAX_DELAY)` فقط **DisplayTask** را تا رسیدن داده منتظر نگه می‌دارد؛ Task دیگر می‌تواند اجرا شود.
- `SensorTask` با اولویت **۲** و `DisplayTask` با اولویت **۱** ایجاد می‌شود. این اعداد صرفاً انتخاب آموزشی‌اند و به‌تنهایی تضمین زمان واقعی سخت (Hard Real-Time) نمی‌دهند.
- در Arduino-ESP32، اندازه Stack داده‌شده به `xTaskCreate` در این محیط **برحسب بایت** است. اگر Platform یا کتابخانه را تغییر دادید، قرارداد API همان محیط را بررسی کنید.
- نمایشگر SSD1306 روی I2C و عملیات سنسور ممکن است مقداری زمان بگیرند. اجرای هم‌زمان به معنی اجرای بی‌تأخیر همه توابع نیست.

## ۴. خروجی مورد انتظار

در حالت عادی، OLED عنوان `FreeRTOS | DHT22`، مقادیر دما و رطوبت، شماره نمونه (`Seq`) و تعداد داده‌های منتظر در Queue (`Q`) را نشان می‌دهد.

**نمونه نمایشی از فرمت Log** (این اعداد اندازه‌گیری واقعی یا خروجی تأییدشده سخت‌افزاری نیستند):

```text
[INFO] SensorTask, DisplayTask, Queue, Mutex initialized
[INFO] Queued seq=1, T=24.6 C, RH=42.8%
[INFO] Display consumed seq=1, age=35 ms, remaining=0
[INFO] Queued seq=2, T=24.5 C, RH=42.7%
```

اگر OLED پیدا نشود، برنامه هشدار می‌دهد و دریافت Queue و Logهای Serial را ادامه می‌دهد؛ داده‌ها روی نمایشگر نشان داده نمی‌شوند.

## ۵. آزمایش‌های Slides 33–34

### آزمایش A — اجرای عادی Task و Queue

1. کد را بدون تغییر Upload کنید؛ `DEMO_SLOW_DISPLAY = false` باشد.
2. روی OLED تغییر شماره `Seq` را مشاهده کنید.
3. در Serial Monitor هر حدود **۲.۵ ثانیه**، پیام `Queued` و سپس `Display consumed` را بررسی کنید.
4. متوجه شوید که Queue جدا از مسیر اصلی `loop()` داده را جابه‌جا می‌کند.

### آزمایش B — Queue Full و سیاست Drop

1. در ابتدای Sketch، `DEMO_SLOW_DISPLAY` را روی `true` تنظیم کنید و دوباره Upload کنید.
2. پس از مصرف هر پیام، DisplayTask عمداً **۸ ثانیه** تأخیر خواهد داشت؛ SensorTask همچنان در حدود **۲.۵ ثانیه** نمونه‌برداری می‌کند.
3. بعد از پرشدن Queue پنج‌عضوی، در Serial Monitor پیام `Queue full: dropped seq=...` ظاهر می‌شود.
4. توضیح دهید که `xQueueSend(sensorQueue, &reading, 0)` **منتظر فضای خالی نمی‌ماند** و در این مثال، **نمونه جدید** را Drop می‌کند. این رفتار آگاهانه برای آموزش Backpressure است، نه سیاست ایده‌آل همه محصولات.
5. پس از پایان آزمایش، `DEMO_SLOW_DISPLAY` را دوباره `false` کنید.

> توجه: خروجی کند نمایشگر ممکن است مربوط به نمونه‌های قدیمی باشد؛ `age` در Serial Monitor به فهم تأخیر کمک می‌کند. این صف **بافر پایدار یا دیتالاگر** نیست؛ با خاموش‌کردن ESP32 از بین می‌رود.

### آزمایش C — خطای سنسور

1. **تغذیه را قطع کنید** و سپس برای آزمایش، اتصال DATA سنسور را جدا کنید.
2. دستگاه را روشن کنید و ببینید آیا `[ERROR] DHT22 reading invalid` و `Sensor Error!` ظاهر می‌شود.
3. پس از خاموش‌کردن دوباره، اتصال را به حالت درست برگردانید و عملکرد طبیعی را بررسی کنید.

خطای سنسور نیز با `valid=false` به DisplayTask منتقل می‌شود، اما مقادیر نامعتبر **به‌عنوان دمای صحیح نمایش داده نمی‌شوند**.

### آزمایش D — نقش Mutex و تفاوت Semaphore

در کد، تابع `logEvent()` قبل از نوشتن روی Serial، `xSemaphoreTake(serialMutex, ...)` انجام می‌دهد و پس از چاپ، آن را با `xSemaphoreGive()` آزاد می‌کند.

- **Mutex:** مالکیت انحصاری منبع مشترک (اینجا Serial)، با ویژگی‌هایی مثل Priority Inheritance در FreeRTOS.
- **Binary Semaphore:** مناسب برای **اعلام رویداد** بین Taskها؛ در این Sketch ایجاد نشده است.
- **Queue:** علاوه بر هماهنگی، خود **داده** را منتقل می‌کند؛ در همین Sketch استفاده شده است.

برای مثال، اگر بعداً یک Task منتظر رخداد آماده‌شدن شبکه باشد، می‌توان برای اطلاع‌رسانی از Binary Semaphore استفاده کرد؛ اما برای نمایش OLED در این پروژه استفاده از **یک مالک مشخص و Queue** ساده‌تر است.

## ۶. خطاهای رایج و محدودیت‌های طراحی

| مشاهده | بررسی / توضیح |
|---|---|
| `OLED not found` | SDA=GPIO21، SCL=GPIO22، تغذیه 3.3V و آدرس `0x3C` را بررسی کنید. |
| `DHT22 reading invalid` | حداقل فاصله خواندن، Pull-up، سیم DATA، تغذیه و GND مشترک را بررسی کنید. |
| `Queue full` | در آزمون کندکردن نمایشگر طبیعی است؛ در حالت عادی مدت اجرای Taskها و اولویت‌ها را بررسی کنید. |
| `Cannot create Queue/Mutex` یا `Task creation failed` | حافظه آزاد، Stackها و تنظیمات محیط ESP32 را بررسی کنید. |
| Reset ناخواسته / Stack Overflow | اندازه Stack، چرخه‌های اجرا و مصرف حافظه را با ابزارهای عیب‌یابی بررسی کنید. |
| داده برای چند مصرف‌کننده لازم است | Queue فعلی نقطه‌به‌نقطه است؛ هر مصرف‌کننده معمولاً Queue یا روش انتشار مستقل می‌خواهد. |

**چه چیزهایی عمداً پیاده‌سازی نشده‌اند؟** MQTT، NTP، ذخیره داده در Flash، Multi-consumer Broadcast، مدیریت Deadlock پیچیده و Remote Logging. در این مثال عمداً **هیچ اطلاعات محرمانه‌ای وجود ندارد** و فایل `secrets.h` هم لازم نیست.

**وضعیت آزمون:** این مثال برای Arduino-ESP32 و سخت‌افزار اشاره‌شده آماده شده اما تا زمان بررسی روی ESP32 واقعی، **تأیید کامپایل نهایی با ابزار Arduino IDE و آزمون سخت‌افزاری نشده است**. مقادیر نمایشی بالا نمونه هستند، نه نتیجه آزمایش واقعی.

## منابع

- [Espressif — FreeRTOS in ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/system/freertos_idf.html)
- [FreeRTOS — Queues](https://www.freertos.org/Documentation/02-Kernel/02-Kernel-features/02-Queues-mutexes-and-semaphores/01-Queues)
- [FreeRTOS — Semaphores and Mutexes](https://www.freertos.org/Documentation/02-Kernel/02-Kernel-features/02-Queues-mutexes-and-semaphores/00-QueueManagement)
- [Adafruit DHT Sensor Library](https://github.com/adafruit/DHT-sensor-library)
