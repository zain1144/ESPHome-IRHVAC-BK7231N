# ESPHome IRHVAC for BK7231N

فيرموير ESPHome لمرسل IR يعمل على شرائح Beken/LibreTiny ويستقبل أمر
`IRHVAC` بصيغة Tasmota. يدعم MQTT، كما يدعم مسار HTTP المتوافق مع Tasmota:

```text
GET /cm?cmnd=IRHVAC%20{JSON}
```

المشروع لا يقتصر على Kelvinator. اسم الشركة في الحقل `Vendor` يُمرر إلى
`IRac` في مكتبة IRremoteESP8266، لذلك يمكن استخدام أي بروتوكول HVAC تدعمه
المكتبة، مع مراعاة أن الخصائص المتاحة تختلف من شركة وموديل إلى آخر.

## كيف يعمل

هناك طبقتان منفصلتان:

1. IRremoteESP8266 v2.9.0 يحول حالة `IRHVAC` العامة إلى نبضات البروتوكول
   الخاص بالشركة المختارة.
2. طبقة الربط تلتقط `mark/space` الناتجة وترسلها بواسطة
   `remote_transmitter` في ESPHome، وهو المسار المتوافق مع LibreTiny وBK7231N.

يستعمل MQTT وHTTP الدالة نفسها تمامًا، لذلك ينتجان التسلسل نفسه من نبضات IR.

## الملفات

- `ir-blaster-bk7231n.yaml`: إعداد ESPHome الكامل.
- `irhvac_controller.h`: تحليل JSON، حفظ حالة المكيف، إرسال IR، ومسار HTTP.
- `secrets.example.yaml`: قالب بيانات Wi-Fi وMQTT دون أسرار حقيقية.
- `examples/home-assistant.yaml`: مثال Home Assistant عبر HTTP وMQTT.

المكتبة المعدلة موجودة في:

<https://github.com/zain1144/IRremoteESP8266-ESPHome-LibreTiny>

## المتطلبات

- لوحة BK7231N متوافقة مع LibreTiny.
- ESPHome 2026.8.2 أو أحدث متوافق.
- LED أشعة تحت الحمراء مع مقاومة ودائرة قيادة مناسبة.
- في الجهاز الذي اختُبر عليه المشروع خرج IR الفعلي هو `P7`.

لا توصل LED عالي التيار مباشرة بطرف المعالج. استخدم ترانزستورًا ومقاومات
مناسبة إذا كانت لوحة الجهاز لا تحتوي أصلًا على دائرة قيادة IR.

## التثبيت

انسخ `secrets.example.yaml` إلى `secrets.yaml` ثم ضع بياناتك:

```yaml
wifi_ssid: "YOUR_WIFI_NAME"
wifi_password: "YOUR_WIFI_PASSWORD"
mqtt_broker: "192.168.1.10"
mqtt_username: "YOUR_MQTT_USERNAME"
mqtt_password: "YOUR_MQTT_PASSWORD"
```

تحقق من الإعداد وابنِه:

```bash
esphome config ir-blaster-bk7231n.yaml
esphome run ir-blaster-bk7231n.yaml
```

الإعداد يثبت LibreTiny على `1.12.1`. أثناء الاختبار الفعلي نجح البناء على
`1.13.0`، لكن جهاز BK7231N المختبر لم يعد إلى الشبكة بعد OTA، ولذلك بقي
الإصدار الذي سبق التحقق من إقلاعه. يمكن إزالة قسم `framework` لاحقًا بعد
التأكد من حل المشكلة على لوحتك.

## الإرسال عبر MQTT مثل Tasmota

الموضوع الافتراضي:

```text
cmnd/ir-blaster/IRHVAC
```

للتوافق مع إضافة `hristo-atanasov/Tasmota-IRHVAC` يشترك الفيرموير أيضًا في
الموضوع ذي الأحرف الصغيرة:

```text
cmnd/ir-blaster/irhvac
```

مواضيع MQTT حساسة لحالة الأحرف؛ الاشتراك المزدوج مقصود لمحاكاة معالجة أوامر
Tasmota التي لا تتأثر عادة بطريقة كتابة اسم الأمر.

والـ payload هو كائن IRHVAC مباشرة:

```json
{
  "Vendor": "KELVINATOR",
  "Model": -1,
  "Command": "Control",
  "Mode": "Cool",
  "Power": "On",
  "Celsius": "On",
  "Temp": 24,
  "FanSpeed": "Min",
  "SwingV": "Auto",
  "SwingH": "Off",
  "Quiet": "Off",
  "Turbo": "Off",
  "Econo": "Off",
  "Light": "On",
  "Filter": "On",
  "Clean": "Off",
  "Beep": "Off",
  "Sleep": -1,
  "iFeel": "Off",
  "SensorTemp": null
}
```

ويُقبل أيضًا الغلاف التالي:

```json
{"IRHVAC":{"Vendor":"KELVINATOR","Power":"On","Mode":"Cool","Temp":24}}
```

مثال `mosquitto_pub`:

```bash
mosquitto_pub -h 192.168.1.10 -t cmnd/ir-blaster/IRHVAC \
  -m '{"Vendor":"KELVINATOR","Power":"On","Mode":"Cool","Temp":24,"FanSpeed":"Min","SwingV":"Auto"}'
```

ومن Console في جهاز Tasmota آخر:

```text
Publish cmnd/ir-blaster/IRHVAC {"Vendor":"KELVINATOR","Power":"On","Mode":"Cool","Temp":24,"FanSpeed":"Min","SwingV":"Auto"}
```

النتيجة تُنشر افتراضيًا إلى:

```text
stat/ir-blaster/RESULT
```

وتُنشر نسخة أيضًا إلى `tele/ir-blaster/RESULT`. أما LWT المتوافق مع Tasmota
فهو `tele/ir-blaster/LWT` بقيمتي `Online` و`Offline` وحالة retain.

## إعداد إضافة Tasmota-IRHVAC في Home Assistant

الإعداد التالي متوافق مباشرة مع الفيرموير:

```yaml
climate:
  - platform: tasmota_irhvac
    name: "Kelvinator AC"
    command_topic: "cmnd/ir-blaster/irhvac"
    state_topic: "tele/ir-blaster/RESULT"
    state_topic_2: "stat/ir-blaster/RESULT"
    availability_topic: "tele/ir-blaster/LWT"
    vendor: "KELVINATOR"
    hvac_model: "-1"
    min_temp: 16
    max_temp: 30
    target_temp: 24
    supported_modes:
      - "cool"
      - "heat"
      - "dry"
      - "fan_only"
      - "auto"
      - "off"
    supported_fan_speeds:
      - "auto"
      - "min"
      - "medium"
      - "max"
    supported_swing_list:
      - "off"
      - "vertical"
```

كانت أسباب عدم استجابة الإضافة للنسخة الأولى من الفيرموير هي اختلاف حالة
الأحرف في موضوع `IRHVAC/irhvac` واختلاف موضوع وقيم LWT الافتراضية في ESPHome.
الحقول الإضافية التي ترسلها الإضافة مثل `StateMode` و`Clock` و`Weekday` لا
تمنع الإرسال؛ يتجاهلها المتحكم عندما لا يحتاجها IRac.

## الإرسال عبر HTTP بنفس شكل Tasmota

المسار المتوافق هو `/cm` واسم معامل الاستعلام هو `cmnd`. القيمة يجب أن تبدأ
بكلمة `IRHVAC` ثم مسافة ثم JSON:

```text
http://192.168.0.102/cm?cmnd=IRHVAC%20%7B%22Vendor%22%3A%22KELVINATOR%22%2C%22Power%22%3A%22On%22%2C%22Mode%22%3A%22Cool%22%2C%22Temp%22%3A24%7D
```

الأفضل ترك أداة HTTP تنفذ URL encoding بدل كتابة الرابط يدويًا.

باستخدام curl:

```bash
curl -G 'http://192.168.0.102/cm' \
  --data-urlencode 'cmnd=IRHVAC {"Vendor":"KELVINATOR","Power":"On","Mode":"Cool","Temp":24,"FanSpeed":"Min","SwingV":"Auto"}'
```

باستخدام PowerShell:

```powershell
$payload = '{"Vendor":"KELVINATOR","Power":"On","Mode":"Cool","Temp":24,"FanSpeed":"Min","SwingV":"Auto"}'
Invoke-RestMethod -Method Get -Uri 'http://192.168.0.102/cm' -Body @{ cmnd = "IRHVAC $payload" }
```

الاستجابة الناجحة تكون JSON وتحتوي `Status: SUCCESS` وعدد التوقيتات والحالة
المطبقة. الأخطاء مثل JSON غير صالح أو شركة غير مدعومة ترجع HTTP 400 مع وصف.

## مثال لشركة أخرى

لا يحتاج تغيير الفيرموير عند تغيير الشركة. غيّر `Vendor` والحقول فقط:

```json
{
  "Vendor": "GREE",
  "Model": 1,
  "Power": "On",
  "Mode": "Heat",
  "Temp": 27,
  "FanSpeed": "Auto",
  "SwingV": "Auto"
}
```

إن كان البروتوكول مدعومًا في IRremoteESP8266 ولكن خاصية معينة غير موجودة في
الموديل، تتولى المكتبة تجاهلها أو تقريبها وفق تطبيق البروتوكول.

## معنى الحقول

| الحقل | أمثلة | الوصف |
|---|---|---|
| `Vendor` | `KELVINATOR`, `GREE`, `DAIKIN` | بروتوكول المكيف في IRremoteESP8266 |
| `Model` | `-1`, `1` أو اسم مدعوم | موديل البروتوكول عند الحاجة |
| `Command` | `Control` | نوع رسالة المكيف |
| `Mode` | `Auto`, `Cool`, `Heat`, `Dry`, `Fan`, `Off` | وضع التشغيل |
| `Power` | `On`, `Off`, `true`, `false` | الطاقة |
| `Celsius` | `On`, `Off` | مئوية أو فهرنهايت |
| `Temp` | `24` | درجة الحرارة المطلوبة |
| `FanSpeed` | `Auto`, `Min`, `Low`, `Medium`, `High`, `Max` | سرعة المروحة |
| `SwingV` | `Off`, `Auto`, `Highest`, `High`, `Middle`, `Low`, `Lowest` | اتجاه الريش الرأسي |
| `SwingH` | `Off`, `Auto`, `Left`, `Middle`, `Right`, `Wide` | اتجاه الريش الأفقي |
| `Quiet`, `Turbo`, `Econo` | `On`, `Off` | أوضاع إضافية |
| `Light`, `Filter`, `Clean`, `Beep`, `iFeel` | `On`, `Off` | خصائص اختيارية حسب المكيف |
| `Sleep` | `-1` أو مدة | وضع النوم |
| `SensorTemp` | رقم أو `null` | حرارة المستشعر الخارجي |

الحقول غير الموجودة في أمر لاحق تحتفظ بقيمتها السابقة، مثل Tasmota stateful
IRHVAC. إرسال `SensorTemp: null` يمسح حرارة المستشعر السابقة.

## Home Assistant

يوجد مثال جاهز في `examples/home-assistant.yaml`. يمكن استعمال MQTT مع إضافتك
الحالية دون تغيير موضوع الأمر، أو استعمال `rest_command` لمسار HTTP.

## الحماية

مسار `/cm` يتبع مصادقة `web_server` في ESPHome لأنه مسجل عبر handler المحمي.
يمكن إضافة Basic Authentication:

```yaml
web_server:
  port: 80
  auth:
    username: !secret web_username
    password: !secret web_password
```

عند تفعيلها استخدم Basic Auth في عميل HTTP. لا تضع كلمات المرور داخل رابط
عام، ولا ترفع `secrets.yaml` إلى GitHub.

## استكشاف الأعطال

- ظهور `Unsupported vendor` يعني أن اسم `Vendor` غير معروف أو أن بروتوكوله
  غير مفعّل في المكتبة.
- ظهور `timings=0` يعني أن IRac رفض الحالة أو لم يولد رسالة.
- ظهور `SUCCESS` دون التقاط IR غالبًا سببه طرف GPIO غير الصحيح أو دائرة LED.
- في الجهاز المختبر خرج IR هو `P7`، وليس `P8` الموجود في إعداد قديم.
- اختبر بمستقبل IR قريب أولًا قبل توجيه الجهاز إلى المكيف.
- استخدم `esphome logs ir-blaster-bk7231n.yaml` لرؤية رسائل `irhvac`.

## الاعتماد والترخيص

المشروع يعتمد على IRremoteESP8266 ويحافظ المستودع المعدل على ترخيص ومصدر
المشروع الأصلي. طبقة ESPHome الموجودة في هذا المستودع مرخصة بترخيص MIT.

