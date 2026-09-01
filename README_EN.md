# ESPHome IRHVAC for BK7231N

[النسخة العربية](README.md)

ESPHome firmware for an IR transmitter running on Beken/LibreTiny chips. It
accepts Tasmota-style `IRHVAC` commands over MQTT and HTTP.

The project supports the HVAC protocols provided by IRremoteESP8266. The
protocol, model, and requested features are selected in the JSON payload, so
the firmware does not need to be changed when switching between supported
protocols.

## How it works

1. IRremoteESP8266 converts the common `IRHVAC` state into protocol timings.
2. The ESPHome bridge captures the generated `mark/space` timings.
3. ESPHome's `remote_transmitter` sends the physical IR signal on LibreTiny.

MQTT and HTTP use the same sending path and therefore generate the same signal
for the same state.

## Main files

- `ir-blaster-bk7231n.yaml`: complete ESPHome configuration.
- `irhvac_controller.h`: JSON parsing, state management, IR transmission, and
  the HTTP endpoint.

Modified library:

<https://github.com/zain1144/IRremoteESP8266-ESPHome-LibreTiny>

## Requirements

- A LibreTiny-compatible BK7231N board.
- A compatible ESPHome release.
- An IR LED with a suitable driver circuit.
- The default IR output pin in the example is `P7`.

Do not connect a high-current IR LED directly to the MCU pin. Use the board's
existing driver circuit or a suitable transistor and resistors.

## Build and installation

Configure the network and MQTT values for your environment, then run:

```bash
esphome config ir-blaster-bk7231n.yaml
esphome run ir-blaster-bk7231n.yaml
```

The example pins LibreTiny to `1.12.1`, which is the version verified to boot
and run reliably on the tested device.

## Updating from the web interface

The configuration enables the `web_server` OTA platform alongside ESPHome's
native OTA platform. After the initial firmware installation, open the device
page, find the `OTA Update` section, select `firmware.bin`, and click
`Update`.

Do not disconnect power while the firmware is uploading or the device is
restarting.

## MQTT commands

Command topic:

```text
cmnd/ir-blaster/IRHVAC
```

A lowercase subscription is also available for clients that use it:

```text
cmnd/ir-blaster/irhvac
```

MQTT topics are case-sensitive, so both subscriptions are intentional.

Generic payload:

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

Replace `VENDOR_NAME` with a protocol name exposed by IRremoteESP8266, then
set `Model` and the optional features for the target device.

Example using `mosquitto_pub`:

```bash
mosquitto_pub -h MQTT_BROKER -t cmnd/ir-blaster/IRHVAC \
  -m '{"Vendor":"VENDOR_NAME","Model":-1,"Power":"On","Mode":"Cool","Temp":24,"FanSpeed":"Auto"}'
```

Example from a Tasmota console:

```text
Publish cmnd/ir-blaster/IRHVAC {"Vendor":"VENDOR_NAME","Model":-1,"Power":"On","Mode":"Cool","Temp":24,"FanSpeed":"Auto"}
```

The wrapped format is also accepted:

```json
{"IRHVAC":{"Vendor":"VENDOR_NAME","Model":-1,"Power":"On","Mode":"Cool","Temp":24}}
```

Results are published to:

```text
stat/ir-blaster/RESULT
tele/ir-blaster/RESULT
```

Connection state is published to:

```text
tele/ir-blaster/LWT
```

with an `Online` or `Offline` payload.

## Tasmota-compatible HTTP commands

Endpoint:

```text
GET /cm?cmnd=IRHVAC%20{JSON}
```

The query parameter is named `cmnd`. Its value starts with `IRHVAC`, followed
by a space and the JSON object.

Example using `curl`:

```bash
curl -G 'http://DEVICE_IP/cm' \
  --data-urlencode 'cmnd=IRHVAC {"Vendor":"VENDOR_NAME","Model":-1,"Power":"On","Mode":"Cool","Temp":24,"FanSpeed":"Auto"}'
```

Example using PowerShell:

```powershell
$payload = '{"Vendor":"VENDOR_NAME","Model":-1,"Power":"On","Mode":"Cool","Temp":24,"FanSpeed":"Auto"}'
Invoke-RestMethod -Method Get -Uri 'http://DEVICE_IP/cm' -Body @{ cmnd = "IRHVAC $payload" }
```

Successful response:

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

Invalid JSON and unsupported protocols return HTTP 400 with an error
description.

## Fields

| Field | Common values | Description |
|---|---|---|
| `Vendor` | Supported protocol name | Protocol to use |
| `Model` | `-1`, number, or supported name | Protocol model when required |
| `Command` | `Control` | Command type |
| `Mode` | `Auto`, `Cool`, `Heat`, `Dry`, `Fan`, `Off` | Operating mode |
| `Power` | `On`, `Off`, `true`, `false` | Power state |
| `Celsius` | `On`, `Off` | Temperature unit |
| `Temp` | Number | Target temperature |
| `FanSpeed` | `Auto`, `Min`, `Low`, `Medium`, `High`, `Max` | Fan speed |
| `SwingV` | `Off`, `Auto`, or supported position | Vertical swing |
| `SwingH` | `Off`, `Auto`, or supported position | Horizontal swing |
| `Quiet`, `Turbo`, `Econo` | `On`, `Off` | Additional modes |
| `Light`, `Filter`, `Clean`, `Beep`, `iFeel` | `On`, `Off` | Optional features |
| `Sleep` | `-1` or duration | Sleep mode |
| `SensorTemp` | Number or `null` | External sensor temperature |

Not every feature is available for every protocol or model. IRremoteESP8266
maps the common state to the closest representation supported by the selected
protocol.

Fields omitted from a later command retain their previous values.
`SensorTemp: null` clears the previous sensor value.

## Security

The `/cm` endpoint follows the ESPHome `web_server` authentication setting.
When authentication is enabled, use HTTP Basic Authentication and do not place
credentials in a public URL.

## Troubleshooting

- `Unsupported vendor`: the protocol name is unknown or disabled.
- `timings=0`: the library did not generate a message for the requested state.
- `SUCCESS` without a detected signal: check the GPIO, driver circuit, and LED
  direction.
- Verify the signal with a nearby IR receiver before aiming the transmitter at
  the target device.
- Run `esphome logs ir-blaster-bk7231n.yaml` to view `irhvac` messages.

## Attribution and license

The project uses IRremoteESP8266 and preserves the original project's source
and license. The ESPHome integration layer in this repository uses the MIT
license.

