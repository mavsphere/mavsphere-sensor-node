#include "App.h"
#include "Logger.h"

bool App::begin(bool forceConfigMode, bool factoryResetRequested) {
  Serial.begin(115200);
  delay(300);
  Logger::info("MavSphere ESP32 Sensor Node booting");

  display_.begin();
  display_.showBoot();

  if (!store_.begin()) {
    Logger::error("Config store init failed");
    return false;
  }

  if (factoryResetRequested) {
    Logger::warn("Performing factory reset");
    if (!store_.factoryReset()) {
      Logger::error("Factory reset failed");
    }
  }

  if (!store_.load(config_)) {
    Logger::error("Failed to load config");
    return false;
  }

  const bool wifiMissing = config_.wifi.ssid.isEmpty();
  const bool agentMissing = config_.mqtt.host.isEmpty();
  const bool forceBootstrap = forceConfigMode || wifiMissing || agentMissing;

  if (wifiMissing) {
    Logger::warn("No Wi-Fi SSID configured, forcing config mode");
  }

  if (agentMissing) {
    Logger::warn("No layout-agent configured, forcing config mode");
  }

  if (forceConfigMode) {
    Logger::info("Forced config mode requested at boot");
  }

  wifi_.begin(config_.wifi, config_.nodeId, forceBootstrap);

  // Bring MQTT object online before sensors so sensor events can publish once
  // connected, but initialise sensors before signal outputs so the MCP23017 is
  // available to both input sensors and signal LEDs.
  mqtt_.begin(config_, &store_, &sensors_, &signals_);
  sensors_.begin(config_, &mqtt_);
  signals_.begin(config_, sensors_.getMcp());

  web_.begin(&config_, &store_, &wifi_, &mqtt_, &sensors_, &signals_);

  updateDisplay();
  return true;
}

NodeStatus App::buildStatus() {
  NodeStatus s;
  s.wifiConnected = wifi_.isConnected();
  s.apMode = wifi_.isApMode();
  s.mqttConnected = mqtt_.isConnected();
  s.ipAddress = wifi_.ipAddress();
  s.rssi = wifi_.rssi();
  s.uptimeMs = millis();
  return s;
}

void App::loop() {
  wifi_.loop();

  if (!wifi_.isApMode()) {
    mqtt_.loop(wifi_.isConnected());
    sensors_.loop(config_.nodeId);

    if (millis() - lastStatusMs_ > 30000) {
      mqtt_.publishStatus(buildStatus(), config_.nodeId);
      lastStatusMs_ = millis();
    }

    if (millis() - lastHeartbeatMs_ > 10000) {
      mqtt_.publishHeartbeat(buildStatus(), config_.nodeId);
      lastHeartbeatMs_ = millis();
    }
  }

  web_.loop();

  if (millis() - lastDisplayMs_ > 1000) {
    updateDisplay();
    lastDisplayMs_ = millis();
  }
}

void App::updateDisplay() {
  NodeStatus status = buildStatus();

  String key = String(status.apMode ? "AP" : "RUN") + "|" +
               status.ipAddress + "|" +
               config_.mqtt.host + "|" +
               String(status.mqttConnected ? "1" : "0");

  if (key == lastDisplayKey_) {
    return;
  }

  lastDisplayKey_ = key;

if (status.apMode) {
  display_.showConfig(config_, status);
} else {
  display_.showNormal(config_, status);
}

// TFT_eSPI uses the global SPI bus. The RC522 library also uses the global SPI
// object, but on remapped pins. After any display draw, restore/re-prime the
// RFID reader so PICC_IsNewCardPresent() keeps working.
sensors_.restoreRfidAfterExternalSpiUse();
}