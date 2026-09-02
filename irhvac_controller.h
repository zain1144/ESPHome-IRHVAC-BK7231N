#pragma once

#include <IRac.h>
#include <IRrecv.h>
#include <IRutils.h>
#include <irremote_esphome_bridge.h>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include "esphome/components/json/json_util.h"
#include "esphome/components/remote_base/remote_base.h"
#include "esphome/components/remote_transmitter/remote_transmitter.h"
#include "esphome/components/web_server_base/web_server_base.h"
#include "esphome/core/log.h"

// A shared Tasmota-compatible IRHVAC controller for MQTT and HTTP.
// The IRremoteESP8266 fork encodes the selected A/C protocol. ESPHome then
// emits the captured mark/space timings through its LibreTiny-safe backend.
namespace esphome_irhvac {

static const char *const TAG = "irhvac";
static esphome::remote_transmitter::RemoteTransmitterComponent *transmitter = nullptr;
static IRac ac(0);
static IRrecv decoder(0, 8, 50, false);
static bool busy = false;
static bool http_handler_registered = false;
static bool transmitted_once = false;
static uint32_t last_transmit_ms = 0;
static bool received_once = false;
static uint32_t last_receive_ms = 0;
static bool received_state_valid = false;
static stdAc::state_t received_state;

static const uint32_t RECEIVE_DUPLICATE_WINDOW_MS = 50;
static const uint32_t RECEIVE_ECHO_WINDOW_MS = 300;
static const uint8_t HVAC_RETRY_TOLERANCE_ADD = 10;

struct SendResult {
  bool success{false};
  size_t timing_count{0};
  std::string error;
  std::string response_json;
};

struct ReceiveResult {
  bool publish{false};
  bool decoded{false};
  bool hvac{false};
  std::string response_json;
};

static bool read_bool(JsonObjectConst command, const char *key, bool old_value) {
  JsonVariantConst value = command[key];
  if (value.isNull()) return old_value;
  if (value.is<bool>()) return value.as<bool>();
  if (value.is<const char *>())
    return IRac::strToBool(value.as<const char *>(), old_value);
  return value.as<int>() != 0;
}

static void write_state(JsonObject command, const stdAc::state_t &state) {
    command["Vendor"] = typeToString(state.protocol).c_str();
    command["Model"] = state.model;
    command["Command"] = IRac::commandTypeToString(state.command).c_str();
    command["Mode"] = IRac::opmodeToString(state.mode).c_str();
    command["Power"] = state.power ? "On" : "Off";
    command["Celsius"] = state.celsius ? "On" : "Off";
    command["Temp"] = state.degrees;
    command["FanSpeed"] = IRac::fanspeedToString(state.fanspeed).c_str();
    command["SwingV"] = IRac::swingvToString(state.swingv).c_str();
    command["SwingH"] = IRac::swinghToString(state.swingh).c_str();
    command["Quiet"] = state.quiet ? "On" : "Off";
    command["Turbo"] = state.turbo ? "On" : "Off";
    command["Econo"] = state.econo ? "On" : "Off";
    command["Light"] = state.light ? "On" : "Off";
    command["Filter"] = state.filter ? "On" : "Off";
    command["Clean"] = state.clean ? "On" : "Off";
    command["Beep"] = state.beep ? "On" : "Off";
    command["Sleep"] = state.sleep;
    command["iFeel"] = state.iFeel ? "On" : "Off";
    if (state.sensorTemperature == kNoTempValue)
      command["SensorTemp"] = nullptr;
    else
      command["SensorTemp"] = state.sensorTemperature;
}

static std::string state_json(const SendResult &result) {
  return esphome::json::build_json([&](JsonObject root) {
    root["Status"] = result.success ? "SUCCESS" : "FAILED";
    root["Timings"] = static_cast<uint32_t>(result.timing_count);
    if (!result.error.empty()) root["Error"] = result.error;
    write_state(root["IRHVAC"].to<JsonObject>(), ac.next);
  });
}

static ReceiveResult receive_raw(
    const esphome::remote_base::RawTimings &timings,
    const uint8_t tolerance = 55) {
  ReceiveResult result;
  const uint32_t now = esphome::millis();

  if (busy ||
      (transmitted_once &&
       static_cast<uint32_t>(now - last_transmit_ms) < RECEIVE_ECHO_WINDOW_MS)) {
    ESP_LOGD(TAG, "Ignoring receiver echo from the local transmitter");
    return result;
  }
  if (received_once &&
      static_cast<uint32_t>(now - last_receive_ms) <
          RECEIVE_DUPLICATE_WINDOW_MS) {
    ESP_LOGD(TAG, "Ignoring duplicate IR receive event");
    return result;
  }

  // IRremoteESP8266 expects unsigned durations in kRawTick units, with a
  // leading gap at index 0. ESPHome supplies signed microsecond timings where
  // marks are positive and spaces are negative.
  std::vector<uint16_t> rawbuf;
  rawbuf.reserve(timings.size() + 2);
  rawbuf.push_back(1);

  bool started = false;
  bool expect_mark = true;
  for (const int32_t timing : timings) {
    if (!started && timing <= 0) continue;
    if (timing == 0) continue;

    const bool is_mark = timing > 0;
    if (is_mark != expect_mark) {
      ESP_LOGW(TAG, "Invalid IR mark/space sequence; check receiver inversion");
      return result;
    }
    started = true;

    const uint32_t duration = timing > 0
                                  ? static_cast<uint32_t>(timing)
                                  : static_cast<uint32_t>(-static_cast<int64_t>(timing));
    const uint32_t ticks = std::max<uint32_t>(
        1, (duration + (kRawTick / 2)) / kRawTick);
    rawbuf.push_back(static_cast<uint16_t>(std::min<uint32_t>(
        ticks, std::numeric_limits<uint16_t>::max())));
    expect_mark = !expect_mark;
  }

  if (!started || rawbuf.size() <= kStartOffset + 2 ||
      rawbuf.size() >= std::numeric_limits<uint16_t>::max()) {
    return result;
  }

  const uint16_t rawlen = static_cast<uint16_t>(rawbuf.size());
  rawbuf.push_back(0);  // Room for IRrecv::decode()'s trailing sentinel.

  decode_results decoded{};
  decoder.setTolerance(tolerance);
  if (!decoder.decodeRaw(&decoded, rawbuf.data(), rawlen)) return result;

  bool recovered_hvac_state = false;
  stdAc::state_t recovered_state;
  if (decoded.decode_type == decode_type_t::UNKNOWN) {
    // Retry all enabled decoders with a slightly wider tolerance, but accept
    // the retry only when it produces a supported HVAC state. This remains
    // vendor-neutral and avoids promoting a weak non-HVAC match.
    decode_results retry{};
    const uint8_t retry_tolerance = static_cast<uint8_t>(std::min<uint16_t>(
        100, static_cast<uint16_t>(tolerance) +
                 HVAC_RETRY_TOLERANCE_ADD));
    decoder.setTolerance(retry_tolerance);
    if (decoder.decodeRaw(&retry, rawbuf.data(), rawlen) &&
        retry.decode_type != decode_type_t::UNKNOWN) {
      // Hash decoding reports roughly one bit per mark/space pair. Require a
      // recovered protocol to explain at least 75% of that capture so a short
      // protocol matching only the prefix of a long HVAC frame is rejected.
      const bool covers_capture =
          retry.bits != 0 &&
          static_cast<uint32_t>(retry.bits) * 4 >=
              static_cast<uint32_t>(decoded.bits) * 3;
      if (covers_capture) {
        const stdAc::state_t *previous =
            received_state_valid &&
                    received_state.protocol == retry.decode_type
                ? &received_state
                : nullptr;
        if (IRAcUtils::decodeToState(&retry, &recovered_state, previous)) {
          decoded = retry;
          recovered_hvac_state = true;
          ESP_LOGD(TAG, "Recovered %s HVAC frame with %u%% tolerance",
                   typeToString(decoded.decode_type).c_str(),
                   static_cast<unsigned>(retry_tolerance));
        }
      } else {
        ESP_LOGD(TAG,
                 "Rejected short %s retry (%u bits for %u-bit raw capture)",
                 typeToString(retry.decode_type).c_str(),
                 static_cast<unsigned>(retry.bits),
                 static_cast<unsigned>(decoded.bits));
      }
    }
    decoder.setTolerance(tolerance);
  }

  received_once = true;
  result.decoded = true;

  stdAc::state_t state;
  if (recovered_hvac_state) {
    state = recovered_state;
    result.hvac = true;
  } else {
    const stdAc::state_t *previous =
        received_state_valid && received_state.protocol == decoded.decode_type
            ? &received_state
            : nullptr;
    result.hvac = IRAcUtils::decodeToState(&decoded, &state, previous);
  }
  if (result.hvac) {
    received_state = state;
    received_state_valid = true;
    // Keep partial future IRHVAC commands aligned with the physical remote.
    ac.next = state;
  }

  const std::string protocol(typeToString(decoded.decode_type).c_str());
  const std::string value(resultToHexidecimal(&decoded).c_str());
  result.response_json = esphome::json::build_json([&](JsonObject root) {
    JsonObject received = root["IrReceived"].to<JsonObject>();
    received["Protocol"] = protocol;
    received["Bits"] = decoded.bits;
    if (decoded.decode_type == decode_type_t::UNKNOWN)
      received["Hash"] = value;
    else
      received["Data"] = value;
    received["Repeat"] = decoded.repeat ? 1 : 0;
    if (result.hvac)
      write_state(received["IRHVAC"].to<JsonObject>(), state);
  });
  result.publish = true;

  if (result.hvac) {
    const std::string hvac_json =
        esphome::json::build_json([&](JsonObject root) {
          write_state(root, state);
        });
    ESP_LOGI(TAG, "Received IRHVAC: %s", hvac_json.c_str());
  } else {
    ESP_LOGI(TAG, "Received IR: protocol=%s bits=%u hvac=no",
             protocol.c_str(), static_cast<unsigned>(decoded.bits));
  }
  // Start the duplicate guard after the potentially expensive decode. This
  // also suppresses a queued 64-bit Gree half-frame following Kelvinator.
  last_receive_ms = esphome::millis();
  return result;
}

static SendResult send_object(JsonObjectConst input, const char *source) {
  SendResult result;
  JsonObjectConst command = input;
  if (!input["IRHVAC"].isNull())
    command = input["IRHVAC"].as<JsonObjectConst>();

  if (command.isNull()) {
    result.error = "Missing IRHVAC object";
    result.response_json = state_json(result);
    return result;
  }
  if (transmitter == nullptr) {
    result.error = "IR transmitter is not configured";
    result.response_json = state_json(result);
    return result;
  }
  if (busy) {
    result.error = "IR transmitter is busy";
    result.response_json = state_json(result);
    return result;
  }

  const char *vendor = command["Vendor"] | "UNKNOWN";
  ac.next.protocol = strToDecodeType(vendor);
  if (!IRac::isProtocolSupported(ac.next.protocol)) {
    result.error = std::string("Unsupported vendor: ") + vendor;
    result.response_json = state_json(result);
    ESP_LOGE(TAG, "%s", result.error.c_str());
    return result;
  }

  JsonVariantConst model = command["Model"];
  if (!model.isNull()) {
    ac.next.model = model.is<const char *>()
                        ? IRac::strToModel(model.as<const char *>(), -1)
                        : model.as<int16_t>();
  }
  if (!command["Command"].isNull())
    ac.next.command = IRac::strToCommandType(command["Command"].as<const char *>());
  if (!command["Mode"].isNull())
    ac.next.mode = IRac::strToOpmode(command["Mode"].as<const char *>());
  if (!command["Temp"].isNull()) ac.next.degrees = command["Temp"].as<float>();
  if (!command["FanSpeed"].isNull())
    ac.next.fanspeed = IRac::strToFanspeed(command["FanSpeed"].as<const char *>());
  if (!command["SwingV"].isNull())
    ac.next.swingv = IRac::strToSwingV(command["SwingV"].as<const char *>());
  if (!command["SwingH"].isNull())
    ac.next.swingh = IRac::strToSwingH(command["SwingH"].as<const char *>());

  ac.next.power = read_bool(command, "Power", ac.next.power);
  ac.next.celsius = read_bool(command, "Celsius", ac.next.celsius);
  ac.next.quiet = read_bool(command, "Quiet", ac.next.quiet);
  ac.next.turbo = read_bool(command, "Turbo", ac.next.turbo);
  ac.next.econo = read_bool(command, "Econo", ac.next.econo);
  ac.next.light = read_bool(command, "Light", ac.next.light);
  ac.next.filter = read_bool(command, "Filter", ac.next.filter);
  ac.next.clean = read_bool(command, "Clean", ac.next.clean);
  ac.next.beep = read_bool(command, "Beep", ac.next.beep);
  ac.next.iFeel = read_bool(command, "iFeel", ac.next.iFeel);
  if (!command["Sleep"].isNull()) ac.next.sleep = command["Sleep"].as<int16_t>();
  JsonVariantConst sensor_temp = command["SensorTemp"];
  if (!sensor_temp.isUnbound()) {
    if (sensor_temp.isNull())
      ac.next.sensorTemperature = kNoTempValue;
    else
      ac.next.sensorTemperature = sensor_temp.as<float>();
  }

  busy = true;
  auto transmit = transmitter->transmit();
  auto *transmit_data = transmit.get_data();
  irremote_esphome_bridge::begin(transmitter, transmit_data);
  result.success = ac.sendAc();
  irremote_esphome_bridge::end();

  result.timing_count = transmit_data->get_data().size();
  if (result.success && result.timing_count != 0) {
    transmit.perform();
    transmitted_once = true;
    last_transmit_ms = esphome::millis();
  } else {
    result.success = false;
    result.error = "IRac rejected the requested state";
  }
  busy = false;

  result.response_json = state_json(result);
  ESP_LOGI(TAG, "%s IRHVAC: vendor=%s timings=%u result=%s", source, vendor,
           static_cast<unsigned>(result.timing_count),
           result.success ? "SUCCESS" : "FAILED");
  return result;
}

static SendResult send_json(const std::string &payload, const char *source) {
  SendResult result;
  const bool parsed = esphome::json::parse_json(payload, [&](JsonObject root) {
    result = send_object(root, source);
    return true;
  });
  if (!parsed) {
    result.error = "Invalid JSON payload";
    result.response_json = state_json(result);
  }
  return result;
}

class TasmotaIRHVACHandler : public AsyncWebHandler {
 public:
  bool canHandle(AsyncWebServerRequest *request) const override {
    return request->method() == HTTP_GET && request->url() == "/cm";
  }

  void handleRequest(AsyncWebServerRequest *request) override {
    if (!request->hasArg("cmnd")) {
      request->send(400, "application/json",
                    "{\"Status\":\"FAILED\",\"Error\":\"Missing cmnd parameter\"}");
      return;
    }

    String full_command = request->arg("cmnd");
    full_command.trim();
    const int separator = full_command.indexOf(' ');
    String command_name = separator < 0 ? full_command : full_command.substring(0, separator);
    command_name.toUpperCase();
    if (command_name != "IRHVAC") {
      request->send(400, "application/json",
                    "{\"Status\":\"FAILED\",\"Error\":\"Only IRHVAC is supported\"}");
      return;
    }
    if (separator < 0) {
      request->send(400, "application/json",
                    "{\"Status\":\"FAILED\",\"Error\":\"Missing IRHVAC JSON payload\"}");
      return;
    }

    String payload = full_command.substring(separator + 1);
    payload.trim();
    SendResult result = send_json(std::string(payload.c_str()), "HTTP");
    request->send(result.success ? 200 : 400, "application/json",
                  result.response_json.c_str());
  }
};

static void setup(
    esphome::remote_transmitter::RemoteTransmitterComponent *remote_transmitter) {
  transmitter = remote_transmitter;
  if (http_handler_registered) return;
  if (esphome::web_server_base::global_web_server_base == nullptr) {
    ESP_LOGE(TAG, "web_server is required for the Tasmota-compatible /cm endpoint");
    return;
  }
  esphome::web_server_base::global_web_server_base->add_handler(
      new TasmotaIRHVACHandler());
  http_handler_registered = true;
  ESP_LOGI(TAG, "Tasmota-compatible HTTP endpoint ready: GET /cm?cmnd=IRHVAC%%20{JSON}");
}

}  // namespace esphome_irhvac
