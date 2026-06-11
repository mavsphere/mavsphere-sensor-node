#include "ConfigStore.h"
#include "Logger.h"
#include "BoardPins.h"

bool ConfigStore::begin() {
  if (!LittleFS.begin()) {
    Logger::error("LittleFS mount failed");
    return false;
  }
  return true;
}

void ConfigStore::applyDefaults(NodeConfig& config) {
  config.nodeId = "node-01";

  config.wifi.ssid = "";
  config.wifi.password = "";
  config.wifi.dhcp = true;
  config.wifi.hostname = "mavsphere-sensor-node";

  config.mqtt.enabled = true;
  config.mqtt.host = "";
  config.mqtt.port = 1883;
  config.mqtt.username = "";
  config.mqtt.password = "";
  config.mqtt.clientId = config.nodeId;
  config.mqtt.topicPrefix = "layout";

  // No default sensors — push config from MavSphere HW Config tab
  // or configure manually via the node web UI.
  config.sensors.clear();

  config.rfid.enabled = false;
  config.rfid.id = "reader1";
  config.rfid.ssPin = BOARD_RFID1_SS_PIN;
  config.rfid.rstPin = BOARD_RFID1_RST_PIN;
  config.rfid.sckPin = BOARD_RFID_SCK_PIN;
  config.rfid.misoPin = BOARD_RFID_MISO_PIN;
  config.rfid.mosiPin = BOARD_RFID_MOSI_PIN;
  config.rfid.duplicateSuppressMs = 3000;

  config.rfidReaders.clear();
  for (uint8_t i = 0; i < BOARD_RFID_MAX_READERS; ++i) {
    RfidReaderConfig r;
    r.enabled = false;
    r.id = "reader" + String(i + 1);
    r.sckPin = BOARD_RFID_SCK_PIN;
    r.misoPin = BOARD_RFID_MISO_PIN;
    r.mosiPin = BOARD_RFID_MOSI_PIN;
    r.duplicateSuppressMs = 200;

    if (i == 0) { r.ssPin = BOARD_RFID1_SS_PIN; r.rstPin = BOARD_RFID1_RST_PIN; }
#if BOARD_RFID_MAX_READERS >= 2
    if (i == 1) { r.ssPin = BOARD_RFID2_SS_PIN; r.rstPin = BOARD_RFID2_RST_PIN; }
#endif
#if BOARD_RFID_MAX_READERS >= 3
    if (i == 2) { r.ssPin = BOARD_RFID3_SS_PIN; r.rstPin = BOARD_RFID3_RST_PIN; }
#endif
    config.rfidReaders.push_back(r);
  }
}

String ConfigStore::sensorTypeToString(SensorType type) {
  switch (type) {
    case SensorType::DigitalOccupancy: return "DIGITAL_OCCUPANCY";
    case SensorType::IrBreakBeam: return "IR_BREAK_BEAM";
    default: return "DISABLED";
  }
}

SensorType ConfigStore::sensorTypeFromString(const String& value) {
  if (value == "DIGITAL_OCCUPANCY") return SensorType::DigitalOccupancy;
  if (value == "IR_BREAK_BEAM") return SensorType::IrBreakBeam;
  return SensorType::None;
}

bool ConfigStore::removeIfExists(const char* path) {
  if (LittleFS.exists(path)) {
    return LittleFS.remove(path);
  }
  return true;
}

bool ConfigStore::copyFile(const char* fromPath, const char* toPath) {
  if (!LittleFS.exists(fromPath)) return false;

  File in = LittleFS.open(fromPath, "r");
  if (!in) return false;

  removeIfExists(toPath);
  File out = LittleFS.open(toPath, "w");
  if (!out) {
    in.close();
    return false;
  }

  uint8_t buffer[256];
  while (in.available()) {
    size_t n = in.read(buffer, sizeof(buffer));
    if (n == 0) break;
    if (out.write(buffer, n) != n) {
      in.close();
      out.close();
      return false;
    }
  }

  in.close();
  out.close();
  return true;
}

bool ConfigStore::validateConfig(const NodeConfig& config, String* error) {
  if (config.nodeId.isEmpty()) {
    if (error) *error = "nodeId is empty";
    return false;
  }

  if (config.wifi.hostname.isEmpty()) {
    if (error) *error = "wifi.hostname is empty";
    return false;
  }

  if (config.mqtt.port <= 0 || config.mqtt.port > 65535) {
    if (error) *error = "mqtt.port out of range";
    return false;
  }

  if (config.mqtt.topicPrefix.isEmpty()) {
    if (error) *error = "mqtt.topicPrefix is empty";
    return false;
  }

  for (size_t i = 0; i < config.sensors.size(); ++i) {
    const auto& s = config.sensors[i];
    if (s.id.isEmpty()) {
      if (error) *error = "sensor id is empty";
      return false;
    }
    if (s.enabled && s.gpio > 100) {
      // gpio 255 = unassigned MCP pin, don't block save but warn
      Logger::warn("Sensor " + s.id + " enabled but gpio=" + String(s.gpio) + " looks unassigned");
    }
    if (s.debounceMs < 0) {
      if (error) *error = "sensor debounce invalid";
      return false;
    }
  }

  auto validateRfidReader = [&](const RfidReaderConfig& r, size_t index) -> bool {
    if (!r.enabled) return true;
    if (r.id.isEmpty()) {
      if (error) *error = "rfid reader " + String(index + 1) + " id is empty";
      return false;
    }
    if (r.ssPin == 255) {
      if (error) *error = "rfid reader " + String(index + 1) + " ssPin is unassigned";
      return false;
    }
    return true;
  };

  if (config.rfid.enabled && !validateRfidReader(config.rfid, 0)) {
    return false;
  }

  for (size_t i = 0; i < config.rfidReaders.size(); ++i) {
    if (!validateRfidReader(config.rfidReaders[i], i)) {
      return false;
    }
  }

  return true;
}

void ConfigStore::populateJson(JsonDocument& doc, const NodeConfig& config) {
  doc["version"] = CONFIG_VERSION;
  doc["configPushCount"] = config.lastConfigPushMs;  // reused as push counter

  doc["nodeId"] = config.nodeId;

  doc["wifi"]["ssid"] = config.wifi.ssid;
  doc["wifi"]["password"] = config.wifi.password;
  doc["wifi"]["dhcp"] = config.wifi.dhcp;
  doc["wifi"]["hostname"] = config.wifi.hostname;

  doc["mqtt"]["enabled"] = config.mqtt.enabled;
  doc["mqtt"]["host"] = config.mqtt.host;
  doc["mqtt"]["port"] = config.mqtt.port;
  doc["mqtt"]["username"] = config.mqtt.username;
  doc["mqtt"]["password"] = config.mqtt.password;
  doc["mqtt"]["clientId"] = config.mqtt.clientId;
  doc["mqtt"]["topicPrefix"] = config.mqtt.topicPrefix;

  auto sensors = doc["sensors"].to<JsonArray>();
  for (const auto& s : config.sensors) {
    auto obj = sensors.add<JsonObject>();
    obj["id"] = s.id;
    obj["name"] = s.name;
    obj["type"] = sensorTypeToString(s.type);
    obj["gpio"] = s.gpio;
    obj["enabled"] = s.enabled;
    obj["invert"] = s.invert;
    obj["debounceMs"] = s.debounceMs;
  }

  doc["rfid"]["enabled"] = config.rfid.enabled;
  doc["rfid"]["id"] = config.rfid.id;
  doc["rfid"]["ssPin"] = config.rfid.ssPin;
  doc["rfid"]["rstPin"] = config.rfid.rstPin;
  doc["rfid"]["sckPin"] = config.rfid.sckPin;
  doc["rfid"]["misoPin"] = config.rfid.misoPin;
  doc["rfid"]["mosiPin"] = config.rfid.mosiPin;
  doc["rfid"]["duplicateSuppressMs"] = config.rfid.duplicateSuppressMs;

  auto readers = doc["rfid"]["readers"].to<JsonArray>();
  for (const auto& r : config.rfidReaders) {
    auto obj = readers.add<JsonObject>();
    obj["enabled"] = r.enabled;
    obj["id"] = r.id;
    obj["sensorId"] = r.id;
    obj["ssPin"] = r.ssPin;
    obj["rstPin"] = r.rstPin;
    obj["sckPin"] = r.sckPin;
    obj["misoPin"] = r.misoPin;
    obj["mosiPin"] = r.mosiPin;
    obj["duplicateSuppressMs"] = r.duplicateSuppressMs;
  }

  auto signalArr = doc["signalOutputs"].to<JsonArray>();
  for (const auto& sig : config.signalOutputs) {
    auto obj = signalArr.add<JsonObject>();
    obj["enabled"] = sig.enabled;
    obj["signalId"] = sig.signalId;
    obj["redGpio"] = sig.redGpio;
    obj["yellowGpio"] = sig.yellowGpio;
    obj["greenGpio"] = sig.greenGpio;
    obj["commonAnode"] = sig.commonAnode;
  }
}

bool ConfigStore::parseJson(JsonDocument& doc, NodeConfig& config, String* error) {
  const int version = doc["version"] | 0;
  if (version != CONFIG_VERSION) {
    Logger::warn("Config version mismatch: file=" + String(version) + " expected=" + String(CONFIG_VERSION) + " — migrating");
    // Don't fail — attempt to load what we can. Worst case is missing new fields
    // which will use defaults. This prevents wiping credentials on firmware update.
  }

  config.lastConfigPushMs = doc["configPushCount"] | 0;
  config.nodeId = doc["nodeId"] | config.nodeId;

  config.wifi.ssid = doc["wifi"]["ssid"] | "";
  config.wifi.password = doc["wifi"]["password"] | "";
  config.wifi.dhcp = doc["wifi"]["dhcp"] | true;
  config.wifi.hostname = doc["wifi"]["hostname"] | "mavsphere-sensor-node";

  config.mqtt.enabled = doc["mqtt"]["enabled"] | true;
  config.mqtt.host = doc["mqtt"]["host"] | "";
  config.mqtt.port = doc["mqtt"]["port"] | 1883;
  config.mqtt.username = doc["mqtt"]["username"] | "";
  config.mqtt.password = doc["mqtt"]["password"] | "";
  config.mqtt.clientId = doc["mqtt"]["clientId"] | config.nodeId;
  if (config.mqtt.clientId.isEmpty()) {
    config.mqtt.clientId = config.nodeId;
  }
  config.mqtt.topicPrefix = doc["mqtt"]["topicPrefix"] | "layout";

  config.sensors.clear();
  for (JsonObject s : doc["sensors"].as<JsonArray>()) {
    SensorConfig sc;
    sc.id = s["id"] | "";
    sc.name = s["name"] | sc.id;
    sc.type = sensorTypeFromString(s["type"] | "DISABLED");
    sc.gpio = s["gpio"] | 255;
    sc.enabled = s["enabled"] | false;
    sc.invert = s["invert"] | false;
    sc.debounceMs = s["debounceMs"] | 30;
    config.sensors.push_back(sc);
  }

  config.rfid.enabled = doc["rfid"]["enabled"] | false;
  config.rfid.id = doc["rfid"]["id"] | "reader1";
  config.rfid.ssPin = doc["rfid"]["ssPin"] | BOARD_RFID1_SS_PIN;
  config.rfid.rstPin = doc["rfid"]["rstPin"] | BOARD_RFID1_RST_PIN;
  config.rfid.sckPin = doc["rfid"]["sckPin"] | BOARD_RFID_SCK_PIN;
  config.rfid.misoPin = doc["rfid"]["misoPin"] | BOARD_RFID_MISO_PIN;
  config.rfid.mosiPin = doc["rfid"]["mosiPin"] | BOARD_RFID_MOSI_PIN;
  config.rfid.duplicateSuppressMs = doc["rfid"]["duplicateSuppressMs"] | 200;

  config.rfidReaders.clear();
  if (doc["rfid"]["readers"].is<JsonArray>()) {
    size_t idx = 0;
    for (JsonObject r : doc["rfid"]["readers"].as<JsonArray>()) {
      if (idx >= BOARD_RFID_MAX_READERS) {
        Logger::warn("Ignoring RFID reader beyond firmware max " + String(BOARD_RFID_MAX_READERS));
        break;
      }
      RfidReaderConfig rc;
      rc.enabled = r["enabled"] | config.rfid.enabled;
      if (r["sensorId"].is<const char*>()) {
        rc.id = String(r["sensorId"].as<const char*>());
      } else if (r["id"].is<const char*>()) {
        rc.id = String(r["id"].as<const char*>());
      } else {
        rc.id = String("reader") + String(idx + 1);
      }
      rc.sckPin = r["sckPin"] | config.rfid.sckPin;
      rc.misoPin = r["misoPin"] | config.rfid.misoPin;
      rc.mosiPin = r["mosiPin"] | config.rfid.mosiPin;
      rc.duplicateSuppressMs = r["duplicateSuppressMs"] | config.rfid.duplicateSuppressMs;
      if (idx == 0) { rc.ssPin = r["ssPin"] | BOARD_RFID1_SS_PIN; rc.rstPin = r["rstPin"] | BOARD_RFID1_RST_PIN; }
#if BOARD_RFID_MAX_READERS >= 2
      if (idx == 1) { rc.ssPin = r["ssPin"] | BOARD_RFID2_SS_PIN; rc.rstPin = r["rstPin"] | BOARD_RFID2_RST_PIN; }
#endif
#if BOARD_RFID_MAX_READERS >= 3
      if (idx == 2) { rc.ssPin = r["ssPin"] | BOARD_RFID3_SS_PIN; rc.rstPin = r["rstPin"] | BOARD_RFID3_RST_PIN; }
#endif
      config.rfidReaders.push_back(rc);
      idx++;
    }
  } else if (config.rfid.enabled) {
    config.rfidReaders.push_back(config.rfid);
  }

  config.signalOutputs.clear();
  for (JsonObject sig : doc["signalOutputs"].as<JsonArray>()) {
    SignalOutputConfig sc;
    sc.enabled = sig["enabled"] | false;
    sc.signalId = sig["signalId"] | "";
    sc.redGpio = sig["redGpio"] | 255;
    sc.yellowGpio = sig["yellowGpio"] | 255;
    sc.greenGpio = sig["greenGpio"] | 255;
    sc.commonAnode = sig["commonAnode"] | false;
    config.signalOutputs.push_back(sc);
  }

  return validateConfig(config, error);
}

bool ConfigStore::loadFromPath(const char* path, NodeConfig& config) {
  if (!LittleFS.exists(path)) return false;

  File f = LittleFS.open(path, "r");
  if (!f) return false;

  JsonDocument doc;
  const auto err = deserializeJson(doc, f);
  f.close();

  if (err) {
    Logger::warn(String("Config parse failed for ") + path);
    return false;
  }

  String validationError;
  if (!parseJson(doc, config, &validationError)) {
    Logger::warn(String("Config validation failed for ") + path + ": " + validationError);
    return false;
  }

  return true;
}

bool ConfigStore::writeToPath(const char* path, const NodeConfig& config) {
  JsonDocument doc;
  populateJson(doc, config);

  removeIfExists(path);
  File f = LittleFS.open(path, "w");
  if (!f) return false;

  const size_t written = serializeJsonPretty(doc, f);
  f.close();
  return written > 0;
}

bool ConfigStore::load(NodeConfig& config) {
  if (loadFromPath(CONFIG_PATH, config)) {
    Logger::info("Loaded config from primary");
    return true;
  }

  NodeConfig backupConfig;
  if (loadFromPath(BACKUP_PATH, backupConfig)) {
    Logger::warn("Primary config invalid, recovered from backup");
    config = backupConfig;
    writeToPath(CONFIG_PATH, config);
    return true;
  }

  Logger::warn("No valid config found, applying defaults");
  applyDefaults(config);

  if (!save(config)) {
    Logger::error("Failed to save regenerated default config");
    return false;
  }

  return true;
}

bool ConfigStore::save(const NodeConfig& config) {
  NodeConfig normalized = config;
  if (normalized.mqtt.clientId.isEmpty()) {
    normalized.mqtt.clientId = normalized.nodeId;
  }

  String validationError;
  if (!validateConfig(normalized, &validationError)) {
    Logger::error("Refusing to save invalid config: " + validationError);
    return false;
  }

  if (LittleFS.exists(CONFIG_PATH)) {
    if (!copyFile(CONFIG_PATH, BACKUP_PATH)) {
      Logger::warn("Failed to update backup from primary");
    }
  }

  if (!writeToPath(CONFIG_PATH, normalized)) {
    Logger::error("Failed to write primary config");

    if (LittleFS.exists(BACKUP_PATH)) {
      copyFile(BACKUP_PATH, CONFIG_PATH);
    }
    return false;
  }

  NodeConfig verify;
  if (!loadFromPath(CONFIG_PATH, verify)) {
    Logger::error("Primary config verification failed after save");

    if (LittleFS.exists(BACKUP_PATH)) {
      copyFile(BACKUP_PATH, CONFIG_PATH);
      Logger::warn("Restored backup after failed verification");
    }
    return false;
  }

  if (!copyFile(CONFIG_PATH, BACKUP_PATH)) {
    Logger::warn("Failed to refresh backup after successful save");
  }

  Logger::info("Config saved successfully");
  return true;
}

bool ConfigStore::factoryReset() {
  Logger::warn("Factory reset requested");

  bool okPrimary = removeIfExists(CONFIG_PATH);
  bool okBackup = removeIfExists(BACKUP_PATH);

  if (!okPrimary || !okBackup) {
    Logger::error("Factory reset failed while removing config files");
    return false;
  }

  NodeConfig defaults;
  applyDefaults(defaults);

  if (!save(defaults)) {
    Logger::error("Factory reset failed while writing default config");
    return false;
  }

  Logger::info("Factory reset completed");
  return true;
}