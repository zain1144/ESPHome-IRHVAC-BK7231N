# ESPHome IRHVAC for BK7231N

[English version](README_EN.md)

فيرموير ESPHome لمرسل IR يعمل على شرائح Beken/LibreTiny ويستقبل أوامر
`IRHVAC` بصيغة Tasmota عبر MQTT أو HTTP.

يدعم المشروع بروتوكولات HVAC التي توفرها مكتبة IRremoteESP8266. تُحدد
البروتوكول والموديل والخصائص المطلوبة داخل JSON، دون الحاجة إلى تغيير
الفيرموير عند الانتقال بين البروتوكولات المدعومة.

## كيف يعمل

1. تحوّل IRremoteESP8266 حالة `IRHVAC` العامة إلى نبضات البروتوكول المطلوب.
2. يلتقط جسر ESPHome توقيتات `mark/space`.
3. يرسل `remote_transmitter` الإشارة عبر خرج IR على LibreTiny.

يستخدم MQTT وHTTP مسار الإرسال نفسه، لذلك ينتجان الإشارة نفسها عند استخدام
الحالة ذاتها.

## الملفات الرئيسية

- `ir-blaster-bk7231n.yaml`: إعداد ESPHome الكامل.
- `irhvac_controller.h`: تحليل JSON، إدارة الحالة، الإرسال، ومسار HTTP.

المكتبة المعدلة:

<https://github.com/zain1144/IRremoteESP8266-ESPHome-LibreTiny>

## المتطلبات

- لوحة BK7231N متوافقة مع LibreTiny.
- ESPHome متوافق.
- LED أشعة تحت الحمراء مع دائرة قيادة مناسبة.
- خرج IR الافتراضي في الإعداد هو `P7`.

لا توصل LED عالي التيار مباشرة بطرف المعالج. استخدم دائرة القيادة الموجودة
في اللوحة أو ترانزستورًا ومقاومات مناسبة.

## البناء والتثبيت

ضع إعدادات الشبكة وMQTT الخاصة ببيئتك، ثم نفّذ:

```bash
esphome config ir-blaster-bk7231n.yaml
esphome run ir-blaster-bk7231n.yaml
```

يثبت المثال LibreTiny على `1.12.1` لأنه الإصدار الذي تم التحقق من إقلاعه
واستقراره على الجهاز المختبر.

## التحديث من صفحة الويب

يفعّل الإعداد منصة `web_server` OTA إلى جانب منصة ESPHome الأصلية. بعد
تثبيت الفيرموير لأول مرة، افتح صفحة الجهاز وانتقل إلى قسم `OTA Update`،
ثم اختر ملف `firmware.bin` واضغط `Update`.

لا تفصل الطاقة أثناء رفع الفيرموير أو أثناء إعادة تشغيل الجهاز.

## الإرسال عبر MQTT

موضوع الأمر:

```text
cmnd/ir-blaster/IRHVAC
```

يوجد أيضًا اشتراك بالحروف الصغيرة للتوافق مع العملاء الذين يستخدمونه:

```text
cmnd/ir-blaster/irhvac
```

مواضيع MQTT حساسة لحالة الأحرف، لذلك الاشتراك المزدوج مقصود.

مثال payload عام:

```json
{
  "Vendor": "VENDOR_NAME",
  "Model": -1,
  "Command": "Control",
  "Mode": "Cool",
  "Power": "On",
  "Celsius": "On",
  "Temp": 24,
  "FanSpeed": "Auto",
  "SwingV": "Auto",
  "SwingH": "Off",
  "Quiet": "Off",
  "Turbo": "Off",
  "Econo": "Off",
  "Light": "Off",
  "Filter": "Off",
  "Clean": "Off",
  "Beep": "Off",
  "Sleep": -1,
  "iFeel": "Off",
  "SensorTemp": null
}
```

استبدل `VENDOR_NAME` باسم البروتوكول الذي تعرضه IRremoteESP8266، واضبط
`Model` والخصائص وفق الجهاز.

مثال باستخدام `mosquitto_pub`:

```bash
mosquitto_pub -h MQTT_BROKER -t cmnd/ir-blaster/IRHVAC \
  -m '{"Vendor":"VENDOR_NAME","Model":-1,"Power":"On","Mode":"Cool","Temp":24,"FanSpeed":"Auto"}'
```

مثال من Console في Tasmota:

```text
Publish cmnd/ir-blaster/IRHVAC {"Vendor":"VENDOR_NAME","Model":-1,"Power":"On","Mode":"Cool","Temp":24,"FanSpeed":"Auto"}
```

يمكن أيضًا إرسال الغلاف التالي:

```json
{"IRHVAC":{"Vendor":"VENDOR_NAME","Model":-1,"Power":"On","Mode":"Cool","Temp":24}}
```

تُنشر النتيجة على:

```text
stat/ir-blaster/RESULT
tele/ir-blaster/RESULT
```

وحالة الاتصال على:

```text
tele/ir-blaster/LWT
```

بقيمة `Online` أو `Offline`.

## الإرسال عبر HTTP مثل Tasmota

المسار:

```text
GET /cm?cmnd=IRHVAC%20{JSON}
```

اسم معامل الاستعلام هو `cmnd`، وتبدأ قيمته بكلمة `IRHVAC` ثم مسافة ثم
كائن JSON.

مثال باستخدام `curl`:

```bash
curl -G 'http://DEVICE_IP/cm' \
  --data-urlencode 'cmnd=IRHVAC {"Vendor":"VENDOR_NAME","Model":-1,"Power":"On","Mode":"Cool","Temp":24,"FanSpeed":"Auto"}'
```

مثال باستخدام PowerShell:

```powershell
$payload = '{"Vendor":"VENDOR_NAME","Model":-1,"Power":"On","Mode":"Cool","Temp":24,"FanSpeed":"Auto"}'
Invoke-RestMethod -Method Get -Uri 'http://DEVICE_IP/cm' -Body @{ cmnd = "IRHVAC $payload" }
```

الاستجابة الناجحة:

```json
{
  "Status": "SUCCESS",
  "Timings": 280,
  "IRHVAC": {
    "Vendor": "VENDOR_NAME",
    "Model": -1,
    "Power": "On",
    "Mode": "Cool",
    "Temp": 24
  }
}
```

تعيد الأخطاء مثل JSON غير صالح أو بروتوكول غير مدعوم استجابة HTTP 400 مع
وصف للمشكلة.

## الحقول

| الحقل | القيم المعتادة | الوصف |
|---|---|---|
| `Vendor` | اسم بروتوكول مدعوم | البروتوكول المستخدم |
| `Model` | `-1`، رقم، أو اسم مدعوم | الموديل عند الحاجة |
| `Command` | `Control` | نوع الأمر |
| `Mode` | `Auto`, `Cool`, `Heat`, `Dry`, `Fan`, `Off` | وضع التشغيل |
| `Power` | `On`, `Off`, `true`, `false` | حالة الطاقة |
| `Celsius` | `On`, `Off` | وحدة الحرارة |
| `Temp` | رقم | الحرارة المطلوبة |
| `FanSpeed` | `Auto`, `Min`, `Low`, `Medium`, `High`, `Max` | سرعة المروحة |
| `SwingV` | `Off`, `Auto`, أو موضع مدعوم | اتجاه الريش الرأسي |
| `SwingH` | `Off`, `Auto`, أو موضع مدعوم | اتجاه الريش الأفقي |
| `Quiet`, `Turbo`, `Econo` | `On`, `Off` | أوضاع إضافية |
| `Light`, `Filter`, `Clean`, `Beep`, `iFeel` | `On`, `Off` | خصائص اختيارية |
| `Sleep` | `-1` أو مدة | وضع النوم |
| `SensorTemp` | رقم أو `null` | حرارة المستشعر الخارجي |

ليست كل الخصائص متاحة في كل بروتوكول أو موديل. تتولى IRremoteESP8266 تحويل
الحالة إلى أقرب تمثيل يدعمه البروتوكول.

الحقول غير الموجودة في أمر لاحق تحتفظ بقيمتها السابقة. إرسال
`SensorTemp: null` يمسح القيمة السابقة.

## الحماية

مسار `/cm` يتبع إعداد مصادقة `web_server` في ESPHome. عند تفعيل المصادقة
استخدم Basic Authentication في عميل HTTP، ولا تضع بيانات الدخول داخل رابط
عام.

## استكشاف الأعطال

- `Unsupported vendor`: اسم البروتوكول غير معروف أو غير مفعّل.
- `timings=0`: لم تولد المكتبة رسالة للحالة المطلوبة.
- `SUCCESS` دون التقاط IR: تحقق من GPIO ودائرة LED واتجاهها.
- تحقق من الإشارة بمستقبل IR قريب قبل توجيه المرسل إلى الجهاز.
- استخدم `esphome logs ir-blaster-bk7231n.yaml` لعرض رسائل `irhvac`.

## الاعتماد والترخيص

يعتمد المشروع على IRremoteESP8266 ويحافظ على ترخيص ومصدر المشروع الأصلي.
طبقة ESPHome في هذا المستودع مرخصة بترخيص MIT.

