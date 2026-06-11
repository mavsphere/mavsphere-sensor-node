#include "WebUi.h"
#include "Logger.h"
#include "ConfigStore.h"
#include "WifiService.h"
#include "MqttService.h"
#include "SensorManager.h"
#include "SignalOutputManager.h"
#include "BoardPins.h"
#include <ArduinoJson.h>
#include <LittleFS.h>

void WebUi::begin(NodeConfig* config, ConfigStore* store, WifiService* wifi, MqttService* mqtt, SensorManager* sensors, SignalOutputManager* signalOut) {
  config_ = config;
  store_ = store;
  wifi_ = wifi;
  mqtt_ = mqtt;
  sensors_ = sensors;
  signalOut_ = signalOut;
  setupRoutes();
  server_.begin();
}

void WebUi::loop() { server_.handleClient(); }

String WebUi::getStatusJson() const {
  JsonDocument doc;
  doc["nodeId"] = config_->nodeId;
  doc["wifiConnected"] = wifi_->isConnected();
  doc["apMode"] = wifi_->isApMode();
  doc["mqttConnected"] = mqtt_->isConnected();
  doc["ipAddress"] = wifi_->ipAddress();
  doc["rssi"] = wifi_->rssi();
  doc["uptimeMs"] = millis();
  doc["version"] = MAVSPHERE_SENSOR_NODE_VERSION;
  doc["configPushCount"] = config_->lastConfigPushMs;  // number of times config pushed
  doc["configuredSensors"] = (int)config_->sensors.size();
  doc["configuredSignals"] = (int)config_->signalOutputs.size();
  doc["mqttHost"] = config_->mqtt.host;
  doc["mqttPort"] = config_->mqtt.port;
  JsonDocument sens;
  deserializeJson(sens, sensors_->statusJson());
  doc["live"] = sens.as<JsonObject>();

  JsonObject live = sens.as<JsonObject>();
  if (!live.isNull()) {
    doc["rfidConfigured"] = live["rfidConfigured"] | false;
    doc["rfidEnabled"] = live["rfidEnabled"] | false;
    doc["rfidDetected"] = live["rfidDetected"] | false;
    doc["rfidReady"] = live["rfidReady"] | false;
    doc["rfidReaderId"] = live["rfidReaderId"] | "";
    doc["rfidVersionReg"] = live["rfidVersionReg"] | "0x00";
    doc["rfidLastTag"] = live["rfidLastTag"] | "";
    doc["rfidLastError"] = live["rfidLastError"] | "";
  }

  if (signalOut_ != nullptr) {
    JsonDocument sigs;
    deserializeJson(sigs, signalOut_->statusJson());
    if (sigs.containsKey("signals")) {
      doc["signals"] = sigs["signals"];
    }
  }
  String out;
  serializeJson(doc, out);
  return out;
}

String WebUi::getConfigJson(bool redactPasswords) const {
  JsonDocument doc;
  doc["nodeId"] = config_->nodeId;
  doc["wifi"]["ssid"] = config_->wifi.ssid;
  doc["wifi"]["password"] = redactPasswords && !config_->wifi.password.isEmpty() ? "***" : config_->wifi.password;
  doc["wifi"]["hostname"] = config_->wifi.hostname;
  doc["mqtt"]["enabled"] = config_->mqtt.enabled;
  doc["mqtt"]["host"] = config_->mqtt.host;
  doc["mqtt"]["port"] = config_->mqtt.port;
  doc["mqtt"]["username"] = config_->mqtt.username;
  doc["mqtt"]["password"] = redactPasswords && !config_->mqtt.password.isEmpty() ? "***" : config_->mqtt.password;
  doc["mqtt"]["clientId"] = config_->mqtt.clientId;
  doc["mqtt"]["topicPrefix"] = config_->mqtt.topicPrefix;

  auto arr = doc["sensors"].to<JsonArray>();
  for (const auto& s : config_->sensors) {
    auto o = arr.add<JsonObject>();
    o["id"] = s.id;
    o["name"] = s.name;
    o["type"] = ConfigStore::sensorTypeToString(s.type);
    o["gpio"] = s.gpio;
    o["enabled"] = s.enabled;
    o["invert"] = s.invert;
    o["debounceMs"] = s.debounceMs;
  }

  doc["rfid"]["enabled"] = config_->rfid.enabled;
  doc["rfid"]["id"] = config_->rfid.id;
  doc["rfid"]["ssPin"] = config_->rfid.ssPin;
  doc["rfid"]["rstPin"] = config_->rfid.rstPin;
  doc["rfid"]["sckPin"] = config_->rfid.sckPin;
  doc["rfid"]["misoPin"] = config_->rfid.misoPin;
  doc["rfid"]["mosiPin"] = config_->rfid.mosiPin;
  doc["rfid"]["duplicateSuppressMs"] = config_->rfid.duplicateSuppressMs;
  auto readers = doc["rfid"]["readers"].to<JsonArray>();
  for (const auto& r : config_->rfidReaders) {
    auto o = readers.add<JsonObject>();
    o["enabled"] = r.enabled;
    o["id"] = r.id;
    o["sensorId"] = r.id;
    o["ssPin"] = r.ssPin;
    o["rstPin"] = r.rstPin;
    o["sckPin"] = r.sckPin;
    o["misoPin"] = r.misoPin;
    o["mosiPin"] = r.mosiPin;
    o["duplicateSuppressMs"] = r.duplicateSuppressMs;
  }

  String out;
  serializeJson(doc, out);
  return out;
}

void WebUi::setupRoutes() {
  server_.on("/", HTTP_GET, [this]() {
    File f = LittleFS.open("/index.html", "r");
    if (!f) {
      server_.send(500, "text/plain", "index.html missing");
      return;
    }
    server_.streamFile(f, "text/html");
    f.close();
  });

  server_.on("/favicon.ico", HTTP_GET, [this]() {
    server_.send(404, "text/plain", "Not found");
  });

  server_.on("/api/status", HTTP_GET, [this]() {
    server_.send(200, "application/json", getStatusJson());
  });

  server_.on("/api/config", HTTP_GET, [this]() {
    server_.send(200, "application/json", getConfigJson(true));
  });

  server_.on("/api/live", HTTP_GET, [this]() {
    server_.send(200, "application/json", sensors_->statusJson());
  });

  server_.on("/api/logs", HTTP_GET, [this]() {
    server_.send(200, "application/json", Logger::getLogsJson());
  });

  server_.on("/api/reboot", HTTP_POST, [this]() {
    server_.send(200, "application/json", "{\"ok\":true}");
    delay(250);
    ESP.restart();
  });

  server_.on("/api/config", HTTP_POST, [this]() {
    if (!server_.hasArg("plain")) {
      server_.send(400, "application/json", "{\"error\":\"missing body\"}");
      return;
    }

    JsonDocument doc;
    if (deserializeJson(doc, server_.arg("plain"))) {
      server_.send(400, "application/json", "{\"error\":\"invalid json\"}");
      return;
    }

    config_->nodeId = doc["nodeId"] | config_->nodeId;
    // Only update ssid/host if non-empty — prevents wipe if form field was blank on save
    const String newSsid = doc["wifi"]["ssid"] | "";
    if (newSsid.length() > 0) config_->wifi.ssid = newSsid;
    if (String(doc["wifi"]["password"] | "") != "***") {
      config_->wifi.password = doc["wifi"]["password"] | config_->wifi.password;
    }
    config_->wifi.hostname = doc["wifi"]["hostname"] | config_->wifi.hostname;

    config_->mqtt.enabled = doc["mqtt"]["enabled"] | config_->mqtt.enabled;
    // Only update host if non-empty — prevents accidental wipe if form field was blank
    const String newMqttHost = doc["mqtt"]["host"] | "";
    if (newMqttHost.length() > 0) config_->mqtt.host = newMqttHost;
    config_->mqtt.port = doc["mqtt"]["port"] | config_->mqtt.port;
    config_->mqtt.username = doc["mqtt"]["username"] | config_->mqtt.username;
    if (String(doc["mqtt"]["password"] | "") != "***") {
      config_->mqtt.password = doc["mqtt"]["password"] | config_->mqtt.password;
    }
    config_->mqtt.clientId = doc["mqtt"]["clientId"] | config_->mqtt.clientId;
    config_->mqtt.topicPrefix = doc["mqtt"]["topicPrefix"] | config_->mqtt.topicPrefix;

    config_->sensors.clear();
    for (JsonObject s : doc["sensors"].as<JsonArray>()) {
      SensorConfig sc;
      sc.id = s["id"] | "";
      sc.name = s["name"] | sc.id;
      sc.type = ConfigStore::sensorTypeFromString(s["type"] | "DISABLED");
      sc.gpio = s["gpio"] | 255;
      sc.enabled = s["enabled"] | false;
      sc.invert = s["invert"] | false;
      sc.debounceMs = s["debounceMs"] | 30;
      config_->sensors.push_back(sc);
    }

    config_->rfid.enabled = doc["rfid"]["enabled"] | config_->rfid.enabled;
    config_->rfid.id = doc["rfid"]["id"] | config_->rfid.id;
    config_->rfid.ssPin = doc["rfid"]["ssPin"] | config_->rfid.ssPin;
    config_->rfid.rstPin = doc["rfid"]["rstPin"] | config_->rfid.rstPin;
    config_->rfid.sckPin = doc["rfid"]["sckPin"] | config_->rfid.sckPin;
    config_->rfid.misoPin = doc["rfid"]["misoPin"] | config_->rfid.misoPin;
    config_->rfid.mosiPin = doc["rfid"]["mosiPin"] | config_->rfid.mosiPin;
    config_->rfid.duplicateSuppressMs = doc["rfid"]["duplicateSuppressMs"] | config_->rfid.duplicateSuppressMs;

    config_->rfidReaders.clear();
    if (doc["rfid"]["readers"].is<JsonArray>()) {
      size_t idx = 0;
      for (JsonObject r : doc["rfid"]["readers"].as<JsonArray>()) {
        if (idx >= BOARD_RFID_MAX_READERS) break;
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
        rc.duplicateSuppressMs = r["duplicateSuppressMs"] | config_->rfid.duplicateSuppressMs;
        if (idx == 0) { rc.ssPin = r["ssPin"] | BOARD_RFID1_SS_PIN; rc.rstPin = r["rstPin"] | BOARD_RFID1_RST_PIN; }
#if BOARD_RFID_MAX_READERS >= 2
        if (idx == 1) { rc.ssPin = r["ssPin"] | BOARD_RFID2_SS_PIN; rc.rstPin = r["rstPin"] | BOARD_RFID2_RST_PIN; }
#endif
#if BOARD_RFID_MAX_READERS >= 3
        if (idx == 2) { rc.ssPin = r["ssPin"] | BOARD_RFID3_SS_PIN; rc.rstPin = r["rstPin"] | BOARD_RFID3_RST_PIN; }
#endif
        config_->rfidReaders.push_back(rc);
        if (idx == 0) config_->rfid = rc;
        idx++;
      }
    } else if (config_->rfid.enabled) {
      config_->rfidReaders.push_back(config_->rfid);
    }

    const bool ok = store_->save(*config_);
    if (ok) {
      Logger::info("Config saved OK — ssid=" + config_->wifi.ssid + " mqttHost=" + config_->mqtt.host);
    } else {
      Logger::error("Config save FAILED");
    }
    server_.send(
      ok ? 200 : 500,
      "application/json",
      ok ? "{\"saved\":true,\"rebooting\":true}" : "{\"saved\":false}"
    );
    if (ok) {
      delay(300);
      ESP.restart();
    }
  });

  server_.onNotFound([this]() {
    server_.send(404, "application/json", "{\"error\":\"not found\"}");
  });
}