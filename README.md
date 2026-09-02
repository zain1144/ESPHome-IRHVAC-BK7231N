# ESPHome IRHVAC for BK7231N

[English version](README_EN.md)

فيرموير ESPHome لمرسل ومستقبل IR يعمل على شرائح Beken/LibreTiny. يستقبل
أوامر `IRHVAC` بصيغة Tasmota عبر MQTT أو HTTP، ويفك إشارات الريموت الواردة
وينشرها ببنية متوافقة مع Tasmota.

يدعم المشروع بروتوكولات HVAC التي توفرها مكتبة IRremoteESP8266. تُحدد
البروتوكول والموديل والخصائص المطلوبة داخل JSON، دون الحاجة إلى تغيير
الفيرموير عند الانتقال بين البروتوكولات المدعومة.

## كيف يعمل

1. تحوّل IRremoteESP8266 حالة `IRHVAC` العامة إلى نبضات البروتوكول المطلوب.
2. يلتقط جسر ESPHome توقيتات `mark/space`.
3. يرسل `remote_transmitter` الإشارة عبر خرج IR على LibreTiny.
4. يلتقط `remote_receiver` الإشارات الواردة ويمررها إلى مفكك
   IRremoteESP8266 الكامل.
5. تتحول إشارات المكيف المعروفة إلى حالة HVAC عامة وتنشر داخل
   `IrReceived.IRHVAC` عبر MQTT.

يستخدم MQTT وHTTP مسار الإرسال نفسه، لذلك ينتجان الإشارة نفسها عند استخدام
الحالة ذاتها.

## الملفات الرئيسية

- `ir-blaster-bk7231n.yaml`: إعداد ESPHome الكامل.
- `irhvac_controller.h`: تحليل JSON، إدارة الحالة، الإرسال، ومسار HTTP.
- `library.json`: يتيح لـ ESPHome جلب ملف المتحكم تلقائيًا من GitHub.

المكتبة المعدلة:

<https://github.com/zain1144/IRremoteESP8266-ESPHome-LibreTiny>

## ما الذي عُدّل في المكتبة؟

الفرع المستخدم مبني على الإصدار الرسمي `IRremoteESP8266 2.9.0`. النسخة
المختبرة هنا هي commit ‏`04b20e7`. لم تُغير مرمّزات أو مفككات بروتوكولات
المكيفات مثل Gree وKelvinator؛ بقي `IRac` وجميع ملفات `ir_*.cpp` الخاصة
بالبروتوكولات من upstream. الفرق عن `v2.9.0` محصور في خمسة ملفات مصدر
(`167` سطرًا مضافًا وسطرين محذوفين):

- أضيفت callbacks اختيارية إلى `IRsend.h` و`IRsend.cpp` لتسليم توقيتات
  `mark/space` وتردد الحامل وduty cycle إلى ESPHome. إذا لم تُثبت callbacks
  يبقى مسار الإرسال الأصلي للمكتبة كما هو.
- أضيف `irremote_esphome_bridge.h` لتحويل التوقيتات التي أنشأها البروتوكول
  إلى `RemoteTransmitData`، ثم ينفذ `remote_transmitter` الإرسال الفعلي على
  LibreTiny.
- أضيف `IRrecv::decodeRaw()` لكي يمرر المتحكم إطارًا التقطه
  `remote_receiver` إلى مفككات IRremoteESP8266 الكاملة.
- تحجب حواجز `#if defined(LIBRETINY)` backend الالتقاط الأصلي المكتوب
  لـESP8266/ESP32 فقط. على BK7231N يملك ESPHome رجل الاستقبال، بينما تظل
  المكتبة مسؤولة عن معرفة البروتوكول وتحويله إلى حالة HVAC.

نفس الفرع يخدم أيضًا مشروع
[`ESPHome-IRHVAC-ESP`](https://github.com/zain1144/ESPHome-IRHVAC-ESP): تعديلات
الإرسال عامة، أما `decodeRaw()` وحواجز backend فهي اللازمة لمسار استقبال
LibreTiny تحديدًا.

### التثبيت والتحديث

يتابع YAML افتراضيًا الفرع `#esphome-libretiny`. لا يجلب PlatformIO آخر
commit لمكتبة Git تلقائيًا في كل compile لأنه يحتفظ بنسخة cache. بعد تحديث
الفرع استخدم **Clean Build Files** من واجهة ESPHome ثم أعد البناء.

لنسخة قابلة للتكرار يمكن تثبيت المكتبة التي اختُبرت فعليًا:

```yaml
ir_library_source: https://github.com/zain1144/IRremoteESP8266-ESPHome-LibreTiny.git#04b20e7
```

لا يمكن استبدالها حاليًا برابط `IRremoteESP8266` الرسمي من دون تعديل؛ النسخة
الرسمية لا تحتوي جسر ESPHome ولا `decodeRaw()` ولا حواجز LibreTiny. عند صدور
إصدار upstream أحدث، يُدمج داخل هذا الفرع أولًا، ثم يعاد اختبار الإرسال وفك
الاستقبال على الجهاز قبل اعتماده.

## المتطلبات

- لوحة BK7231N متوافقة مع LibreTiny.
- ESPHome متوافق.
- LED أشعة تحت الحمراء مع دائرة قيادة مناسبة ووحدة استقبال IR.
- في لوحة Tuya Generic IRC03: الإرسال على `P7` والاستقبال على `P8`.

لا توصل LED عالي التيار مباشرة بطرف المعالج. استخدم دائرة القيادة الموجودة
في اللوحة أو ترانزستورًا ومقاومات مناسبة.

## البناء والتثبيت

ضع إعدادات الشبكة وMQTT الخاصة ببيئتك، ثم نفّذ:

```bash
esphome config ir-blaster-bk7231n.yaml
esphome run ir-blaster-bk7231n.yaml
```

لا تحتاج إلى نسخ `irhvac_controller.h` يدويًا؛ يجلبه قسم `libraries` من
هذا المستودع، وتضيفه الصيغة `<irhvac_controller.h>` إلى البناء.

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

## استقبال أوامر الريموت

يفعّل ملف YAML الكامل المستقبل الموجود على `P8`. يلتقط ESPHome كل إطار،
ثم تفكّه IRremoteESP8266 وتنشر النتيجة على:

```text
tele/ir-blaster/RESULT
```

عند استقبال إطار HVAC مدعوم تكون البنية مثل Tasmota:

```json
{
  "IrReceived": {
    "Protocol": "KELVINATOR",
    "Bits": 128,
    "Data": "0x10900450000000001090045000000000",
    "Repeat": 0,
    "IRHVAC": {
      "Vendor": "KELVINATOR",
      "Model": -1,
      "Command": "Control",
      "Mode": "Cool",
      "Power": "On",
      "Celsius": "On",
      "Temp": 25,
      "FanSpeed": "Min",
      "SwingV": "Off",
      "SwingH": "Off",
      "Quiet": "Off",
      "Turbo": "Off",
      "Econo": "Off",
      "Light": "Off",
      "Filter": "Off",
      "Clean": "On",
      "Beep": "Off",
      "Sleep": -1,
      "iFeel": "Off",
      "SensorTemp": null
    }
  }
}
```

قيمة `Data` أعلاه مثال فقط. إذا كان البروتوكول معروفًا لكنه ليس HVAC تُنشر
معلومات البروتوكول والبيانات من دون `IRHVAC`. وإذا لم يُعرف البروتوكول
يظهر `Hash` بدل `Data`.

تظهر إطارات HVAC التي فُكّت بنجاح أيضًا في سجل الجهاز ككائن أمر كامل:

```text
[I][irhvac]: Received IRHVAC: {"Vendor":"KELVINATOR","Model":-1,"Command":"Control",...}
```

وتبقى إطارات `UNKNOWN` وغير HVAC على سطر السجل المختصر الذي يعرض البروتوكول
وعدد البتات.

تُستخدم آخر حالة HVAC فُكّت بنجاح كأساس لأوامر الإرسال الجزئية اللاحقة.
كما توجد مهلة 300 مللي ثانية تمنع المستقبل الداخلي من نشر صدى الإشارة التي
أرسلها الجهاز نفسه.

يستخدم إعداد IRC03 سماحية استقبال `55%` في ESPHome وIRremoteESP8266 معًا.
يمكن تغييرها من substitution باسم `ir_receive_tolerance` إذا استُخدمت دائرة
استقبال مختلفة تحتاج مطابقة أدق.
تكون قيمة substitution المسماة `ir_receive_idle` افتراضيًا `60ms`، لكي تبقى
الفجوات الداخلية الطويلة لإطارات HVAC ضمن التقاط واحد من دون اعتبار إطار
64-bit الحقيقي غير صالح؛ الاختلاف الوحيد أنه ينتظر سكون الخط 60 مللي ثانية.
إذا أعطت المحاولة العادية نتيجة `UNKNOWN`، تُعاد محاولة جميع المفككات المفعلة
مرة واحدة بسماحية `65%`. ولا تُقبل المحاولة الإضافية إلا إذا أنتجت حالة HVAC
مدعومة وكان عدد بتاتها يفسر 75% على الأقل من الالتقاط الخام. يمنع ذلك قبول
بروتوكول قصير صادف أنه طابق بداية إطار طويل، مع بقاء الاستعادة مستقلة عن شركة
المكيف. كما تبدأ مهلة منع التكرار بعد انتهاء فك الإطار حتى لا يُنشر جزء موجود
في الطابور بعد إطار كامل صحيح مباشرة.

صُممت بنية JSON وموضوع MQTT لتعمل مع المستهلك الذي يعالج رسائل
`IrReceived` القادمة من Tasmota. لا ينشر هذا المشروع خيارات Tasmota الخاصة
مثل ضغط البيانات الخام (`SetOption58`) أو `DataLSB`.

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
- إذا ظهر تحذير عن ترتيب `mark/space` فتحقق من أن الاستقبال على `P8` وأن
  `inverted: true` ما زالت مفعلة.
- ظهور بروتوكول مفكوك من دون `IRHVAC` يعني أن المكتبة عرفت الإطار لكنها لا
  تستطيع تحويله إلى حالة مكيف عامة.
- استخدم `esphome logs ir-blaster-bk7231n.yaml` لعرض رسائل `irhvac`.

## الاعتماد والترخيص

يعتمد المشروع على IRremoteESP8266 ويحافظ على ترخيص ومصدر المشروع الأصلي.
طبقة ESPHome في هذا المستودع مرخصة بترخيص MIT.

