#include "MqttService.h"
#include "Logger.h"
#include "ConfigStore.h"
#include "SensorManager.h"
#include "SignalOutputManager.h"
#include "BoardPins.h"
#include <WiFi.h>

MqttService* MqttService::instance_ = nullptr;

void MqttService::begin(NodeConfig& config, ConfigStore* store,
                         SensorManager* sensors, SignalOutputManager* signalOut) {
  config_ = &config;
  store_ = store;
  sensors_ = sensors;
  signalOut_ = signalOut;
  const bool bufOk = client_.setBufferSize(8192);
  Serial.printf("[MQTT] buffer size set to 8192: %s (actual=%u)\n", bufOk ? "OK" : "FAILED", client_.getBufferSize());
  client_.setServer(config.mqtt.host.c_str(), config.mqtt.port);
  client_.setCallback(MqttService::callback);
  instance_ = this;
}

void MqttService::connect() {
  if (config_ == nullptr || !config_->mqtt.enabled || config_->mqtt.host.isEmpty() || client_.connected()) return;

  client_.setServer(config_->mqtt.host.c_str(), config_->mqtt.port);

  const String clientId = config_->mqtt.clientId.isEmpty()
      ? (config_->nodeId + "-sensor-node")
      : config_->mqtt.clientId;

  // Last Will Testament — Mosquitto publishes this if the node disconnects
  // unexpectedly (power loss, crash, WiFi drop). Retained so the agent sees
  // the offline state immediately on reconnect.
  const String willTopic = config_->mqtt.topicPrefix + "/node/" + config_->nodeId + "/status";
  const String willPayload = "{\"nodeId\":\"" + config_->nodeId + "\",\"mqttConnected\":false,\"wifiConnected\":false,\"apMode\":false,\"ipAddress\":\"\",\"rssi\":0,\"uptimeMs\":0,\"version\":\"" + String(MAVSPHERE_SENSOR_NODE_VERSION) + "\"}";

  bool ok = false;
  if (config_->mqtt.username.isEmpty()) {
    ok = client_.connect(clientId.c_str(), willTopic.c_str(), 0, true, willPayload.c_str());
  } else {
    ok = client_.connect(
      clientId.c_str(),
      config_->mqtt.username.c_str(),
      config_->mqtt.password.c_str(),
      willTopic.c_str(), 0, true, willPayload.c_str()
    );
  }

  if (ok) {
    Logger::info("MQTT connected");
    subscribeCommands();
    subscribeSignals();

    // Publish a full retained snapshot of every configured digital sensor on
    // every MQTT connect/reconnect. This lets the layout-agent/backend recover
    // correct CLEAR/OCCUPIED state after broker, backend, agent, or node restarts
    // instead of waiting for the next edge/change event.
    if (sensors_ != nullptr) {
      sensors_->publishAllSensorStates(config_->nodeId, "mqtt-connect");
    }
  } else {
    Logger::warn("MQTT connect failed rc=" + String(client_.state()));
  }
}

void MqttService::loop(bool wifiConnected) {
  if (config_ == nullptr || !config_->mqtt.enabled || !wifiConnected) return;

  if (!client_.connected()) {
    if (millis() - lastConnectAttemptMs_ > 5000) {
      lastConnectAttemptMs_ = millis();
      connect();
    }
    return;
  }

  client_.loop();
}

bool MqttService::isConnected() {
  return client_.connected();
}

bool MqttService::publishJson(const String& topic, JsonDocument& doc, bool retained) {
  if (!client_.connected()) return false;

  String payload;
  serializeJson(doc, payload);
  return client_.publish(topic.c_str(), payload.c_str(), retained);
}

void MqttService::publishStatus(const NodeStatus& status, const String& nodeId) {
  if (config_ == nullptr || !client_.connected()) return;

  JsonDocument doc;
  doc["nodeId"] = nodeId;
  doc["wifiConnected"] = status.wifiConnected;
  doc["apMode"] = status.apMode;
  doc["mqttConnected"] = status.mqttConnected;
  doc["ipAddress"] = status.ipAddress;
  doc["rssi"] = status.rssi;
  doc["uptimeMs"] = status.uptimeMs;
  doc["version"] = status.version;

  if (sensors_ != nullptr) {
    doc["rfidConfigured"] = sensors_->isRfidConfigured();
    doc["rfidEnabled"] = sensors_->isRfidConfigured();  // legacy/compat
    doc["rfidDetected"] = sensors_->isRfidDetected();
    doc["rfidReady"] = sensors_->isRfidReady();
    doc["rfidReaderId"] = sensors_->rfidReaderId();
    doc["rfidVersionReg"] = sensors_->rfidVersionHex();
    const String err = sensors_->rfidLastError();
    if (err.length() > 0) {
      doc["rfidLastError"] = err;
    }
  }

  const String topic = config_->mqtt.topicPrefix + "/node/" + nodeId + "/status";
  publishJson(topic, doc, false);
}

void MqttService::publishHeartbeat(const NodeStatus& status, const String& nodeId) {
  if (config_ == nullptr || !client_.connected()) return;

  JsonDocument doc;
  doc["nodeId"] = nodeId;
  doc["wifiConnected"] = status.wifiConnected;
  doc["apMode"] = status.apMode;
  doc["mqttConnected"] = status.mqttConnected;
  doc["ipAddress"] = status.ipAddress;
  doc["rssi"] = status.rssi;
  doc["uptimeMs"] = status.uptimeMs;
  doc["version"] = status.version;
  doc["tsMs"] = millis();

  if (sensors_ != nullptr) {
    doc["rfidConfigured"] = sensors_->isRfidConfigured();
    doc["rfidEnabled"] = sensors_->isRfidConfigured();  // legacy/compat
    doc["rfidDetected"] = sensors_->isRfidDetected();
    doc["rfidReady"] = sensors_->isRfidReady();
    doc["rfidReaderId"] = sensors_->rfidReaderId();
    doc["rfidVersionReg"] = sensors_->rfidVersionHex();
    const String err = sensors_->rfidLastError();
    if (err.length() > 0) {
      doc["rfidLastError"] = err;
    }
  }

  const String topic = config_->mqtt.topicPrefix + "/node/" + nodeId + "/heartbeat";
  publishJson(topic, doc, false);
}

void MqttService::publishSensorEvent(const String& nodeId, const SensorConfig& cfg, bool active) {
  if (config_ == nullptr || !client_.connected()) return;

  JsonDocument doc;
  doc["nodeId"] = nodeId;
  doc["sensorId"] = cfg.id;
  doc["name"] = cfg.name;
  doc["type"] = ConfigStore::sensorTypeToString(cfg.type);
  doc["active"] = active;
  doc["state"] = (cfg.type == SensorType::IrBreakBeam)
      ? (active ? "BROKEN" : "CLEAR")
      : (active ? "OCCUPIED" : "CLEAR");
  doc["gpio"] = cfg.gpio;
  doc["tsMs"] = millis();

  const String topic = config_->mqtt.topicPrefix + "/node/" + nodeId + "/sensor/" + cfg.id + "/state";
  publishJson(topic, doc, true);
}

void MqttService::publishRfidEvent(const String& nodeId, const String& readerId, const String& tagId) {
  if (config_ == nullptr || !client_.connected()) return;

  JsonDocument doc;
  doc["nodeId"] = nodeId;
  doc["readerId"] = readerId;
  doc["tagId"] = tagId;
  doc["tsMs"] = millis();

  const String topic = config_->mqtt.topicPrefix + "/node/" + nodeId + "/rfid/" + readerId + "/tag";
  publishJson(topic, doc, false);
}

void MqttService::subscribeCommands() {
  if (config_ == nullptr || !client_.connected()) return;

  const String topic = config_->mqtt.topicPrefix + "/node/" + config_->nodeId + "/cmd/#";
  client_.subscribe(topic.c_str());
  Logger::info("Subscribed to " + topic);
}

void MqttService::subscribeSignals() {
  if (config_ == nullptr || !client_.connected() || signalOut_ == nullptr) return;
  if (config_->signalOutputs.empty()) return;

  // Subscribe to wildcard signal topic: {prefix}/signal/+/aspect
  const String topic = config_->mqtt.topicPrefix + "/signal/+/aspect";
  client_.subscribe(topic.c_str());
  Logger::info("Subscribed to signal aspects: " + topic);
}

void MqttService::callback(char* topic, byte* payload, unsigned int length) {
  Serial.printf("[MQTT-CB] topic=%s len=%u\n", topic, length);
  if (instance_ != nullptr) {
    instance_->handleMessage(topic, payload, length);
  }
}

void MqttService::publishCommandReply(const String& suffix, JsonDocument& doc, bool retained) {
  if (config_ == nullptr || !client_.connected()) return;

  const String topic = config_->mqtt.topicPrefix + "/node/" + config_->nodeId + "/reply/" + suffix;
  publishJson(topic, doc, retained);
}

bool MqttService::applyConfigPayload(const String& payload) {
  if (config_ == nullptr || store_ == nullptr) return false;

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, payload);
  if (err) {
    Logger::warn("[Config] JSON parse failed: " + String(err.c_str()) + " payload_len=" + String(payload.length()));
    return false;
  }

  config_->nodeId = doc["nodeId"] | config_->nodeId;
  // Only update WiFi credentials if non-empty — config push from MavSphere
  // sends blank WiFi fields and must not overwrite locally-configured credentials.
  const String newSsid = doc["wifi"]["ssid"] | "";
  if (newSsid.length() > 0) config_->wifi.ssid = newSsid;

  const String newWifiPass = doc["wifi"]["password"] | "";
  if (newWifiPass.length() > 0 && newWifiPass != "***") {
    config_->wifi.password = newWifiPass;
  }

  const String newHostname = doc["wifi"]["hostname"] | "";
  if (newHostname.length() > 0) config_->wifi.hostname = newHostname;

  config_->mqtt.enabled = doc["mqtt"]["enabled"] | config_->mqtt.enabled;
  // Only update host if non-empty — push from MavSphere sends empty host intentionally
  const String newMqttHost = doc["mqtt"]["host"] | "";
  if (newMqttHost.length() > 0) config_->mqtt.host = newMqttHost;
  config_->mqtt.port = doc["mqtt"]["port"] | config_->mqtt.port;
  config_->mqtt.username = doc["mqtt"]["username"] | config_->mqtt.username;
  if (String(doc["mqtt"]["password"] | "") != "***") {
    config_->mqtt.password = doc["mqtt"]["password"] | config_->mqtt.password;
  }
  config_->mqtt.clientId = doc["mqtt"]["clientId"] | config_->mqtt.clientId;
  config_->mqtt.topicPrefix = doc["mqtt"]["topicPrefix"] | config_->mqtt.topicPrefix;

  if (doc["sensors"].is<JsonArray>()) {
    config_->sensors.clear();
    for (JsonObject s : doc["sensors"].as<JsonArray>()) {
      SensorConfig sc;
      // Backend sends "sensorId", firmware config uses "id"
      sc.id = s["sensorId"] | s["id"] | "";
      sc.name = s["name"] | sc.id;
      sc.type = ConfigStore::sensorTypeFromString(s["type"] | "DISABLED");
      sc.gpio = s["gpio"] | 255;
      sc.enabled = s["enabled"] | false;
      sc.invert = s["invert"] | false;
      sc.debounceMs = s["debounceMs"] | 30;
      config_->sensors.push_back(sc);
    }
  }

  config_->rfid.enabled = doc["rfid"]["enabled"] | config_->rfid.enabled;

  // Backend sends rfid.readers[] array. Firmware supports up to
  // BOARD_RFID_MAX_READERS readers on the same dedicated HSPI bus. Keep the
  // legacy flat config as reader 1 for older/manual configs.
  config_->rfidReaders.clear();
  if (doc["rfid"]["readers"].is<JsonArray>() && doc["rfid"]["readers"].as<JsonArray>().size() > 0) {
    size_t idx = 0;
    for (JsonObject r : doc["rfid"]["readers"].as<JsonArray>()) {
      if (idx >= BOARD_RFID_MAX_READERS) {
        Logger::warn("[Config] ignoring RFID reader beyond firmware max " + String(BOARD_RFID_MAX_READERS));
        break;
      }

      RfidReaderConfig rc;
      rc.enabled = r["enabled"] | config_->rfid.enabled;
      if (r["sensorId"].is<const char*>()) {
        rc.id = String(r["sensorId"].as<const char*>());
      } else if (r["id"].is<const char*>()) {
        rc.id = String(r["id"].as<const char*>());
      } else {
        rc.id = String("reader") + String(idx + 1);
      }
      rc.sckPin = r["sckPin"] | config_->rfid.sckPin;
      rc.misoPin = r["misoPin"] | config_->rfid.misoPin;
      rc.mosiPin = r["mosiPin"] | config_->rfid.mosiPin;
      rc.duplicateSuppressMs = r["duplicateSuppressMs"] | (uint32_t)200;

      if (idx == 0) { rc.ssPin = r["ssPin"] | BOARD_RFID1_SS_PIN; rc.rstPin = r["rstPin"] | BOARD_RFID1_RST_PIN; }
#if BOARD_RFID_MAX_READERS >= 2
      if (idx == 1) { rc.ssPin = r["ssPin"] | BOARD_RFID2_SS_PIN; rc.rstPin = r["rstPin"] | BOARD_RFID2_RST_PIN; }
#endif
#if BOARD_RFID_MAX_READERS >= 3
      if (idx == 2) { rc.ssPin = r["ssPin"] | BOARD_RFID3_SS_PIN; rc.rstPin = r["rstPin"] | BOARD_RFID3_RST_PIN; }
#endif

      config_->rfidReaders.push_back(rc);
      if (idx == 0) {
        config_->rfid = rc;
      }
      idx++;
    }
  } else {
    // Fallback: flat rfid fields (legacy / manual config)
    config_->rfid.id  = doc["rfid"]["id"]  | config_->rfid.id;
    config_->rfid.ssPin   = doc["rfid"]["ssPin"]   | config_->rfid.ssPin;
    config_->rfid.rstPin  = doc["rfid"]["rstPin"]  | config_->rfid.rstPin;
    config_->rfid.sckPin  = doc["rfid"]["sckPin"]  | config_->rfid.sckPin;
    config_->rfid.misoPin = doc["rfid"]["misoPin"] | config_->rfid.misoPin;
    config_->rfid.mosiPin = doc["rfid"]["mosiPin"] | config_->rfid.mosiPin;
    config_->rfid.duplicateSuppressMs = doc["rfid"]["duplicateSuppressMs"] | (uint32_t)200;
    if (config_->rfid.enabled) {
      config_->rfidReaders.push_back(config_->rfid);
    }
  }

  // Parse signalOutputs array
  if (doc["signalOutputs"].is<JsonArray>()) {
    config_->signalOutputs.clear();
    for (JsonObject sig : doc["signalOutputs"].as<JsonArray>()) {
      SignalOutputConfig sc;
      sc.signalId    = sig["signalId"] | "";
      sc.enabled     = sig["enabled"]  | false;
      sc.redGpio     = sig["redGpio"]    | (uint8_t)255;
      sc.yellowGpio  = sig["yellowGpio"] | (uint8_t)255;
      sc.greenGpio   = sig["greenGpio"]  | (uint8_t)255;
      sc.commonAnode = sig["commonAnode"] | false;
      if (sc.signalId.length() > 0) {
        config_->signalOutputs.push_back(sc);
      }
    }
    Logger::info("[Config] parsed " + String(config_->signalOutputs.size()) + " signal outputs");
  }

  const bool ok = store_->save(*config_);
  if (ok) {
    config_->lastConfigPushMs += 1;  // push counter — persisted to LittleFS
    // Note: sensors_->reload() intentionally NOT called here.
    // It blocks for hundreds of ms (MCP23017 init) which prevents
    // the MQTT reply from being sent before reboot.
    // The reboot itself will reinitialise everything cleanly.
  }
  return ok;
}

void MqttService::handleMessage(char* topic, byte* payload, unsigned int length) {
  if (config_ == nullptr) return;

  String topicStr(topic);
  String body;
  for (unsigned int i = 0; i < length; ++i) {
    body += static_cast<char>(payload[i]);
  }

  Logger::info("[MQTT] msg topic=" + topicStr + " len=" + String(length));

  // ── Signal aspect messages: {prefix}/signal/{signalId}/aspect ──────────
  if (topicStr.indexOf("/signal/") >= 0 && topicStr.endsWith("/aspect")) {
    if (signalOut_ != nullptr) {
      JsonDocument doc;
      if (deserializeJson(doc, body) == DeserializationError::Ok) {
        String signalId = doc["signalId"] | "";
        String aspect = doc["aspect"] | "";
        if (signalId.length() > 0 && aspect.length() > 0) {
          signalOut_->setAspect(signalId, aspect);
        }
      }
    }
    return;
  }

  // ── Node command messages: {prefix}/node/{nodeId}/cmd/... ──────────────

  if (topicStr.endsWith("/cmd/ping")) {
    JsonDocument doc;
    doc["ok"] = true;
    doc["nodeId"] = config_->nodeId;
    doc["tsMs"] = millis();
    publishCommandReply("ping", doc, false);
    return;
  }

  if (topicStr.endsWith("/cmd/state/sync") || topicStr.endsWith("/cmd/state-sync")) {
    if (sensors_ != nullptr) {
      sensors_->publishAllSensorStates(config_->nodeId, "cmd-state-sync");
    }
    JsonDocument doc;
    doc["ok"] = true;
    doc["nodeId"] = config_->nodeId;
    doc["type"] = "state-sync";
    doc["tsMs"] = millis();
    publishCommandReply("state", doc, false);
    return;
  }

  if (topicStr.endsWith("/cmd/reboot")) {
    JsonDocument doc;
    doc["ok"] = true;
    doc["nodeId"] = config_->nodeId;
    doc["action"] = "rebooting";
    publishCommandReply("reboot", doc, false);
    delay(250);
    ESP.restart();
    return;
  }

  if (topicStr.endsWith("/cmd/config/set")) {
    Logger::info("[MQTT] config/set received, payload length=" + String(length));
    const bool ok = applyConfigPayload(body);
    Logger::info("[MQTT] config apply result: " + String(ok ? "OK — rebooting" : "FAILED"));

    // Send reply FIRST, then pump loop to ensure delivery, then reboot.
    // Must happen before sensors_->reload() blocks the MQTT stack.
    JsonDocument doc;
    doc["ok"] = ok;
    doc["nodeId"] = config_->nodeId;
    doc["rebootRequired"] = ok;
    publishCommandReply("config", doc, false);
    Logger::info("[MQTT] reply sent — pumping loop for delivery");

    // Pump MQTT loop for 3s to ensure reply reaches broker before disconnect
    unsigned long start = millis();
    while (millis() - start < 3000) {
      client_.loop();
      delay(10);
    }

    if (ok) {
      client_.disconnect();
      delay(200);
      ESP.restart();
    }
    return;
  }
}