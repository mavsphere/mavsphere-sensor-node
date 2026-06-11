#include "WifiService.h"
#include "Logger.h"

void WifiService::begin(const WifiConfig& config, const String& nodeId, bool forceConfigMode) {
  config_ = config;
  nodeId_ = nodeId;

  if (forceConfigMode) {
    Logger::info("Forced config mode requested at boot");
    startAccessPoint();
    return;
  }

  WiFi.mode(WIFI_MODE_STA);
  WiFi.setHostname(config_.hostname.c_str());
  startStation();
}

void WifiService::startStation() {
  apMode_ = false;
  connectStartMs_ = millis();

  if (config_.ssid.isEmpty()) {
    Logger::warn("No WiFi configured, starting AP mode");
    startAccessPoint();
    return;
  }

  Logger::info("Connecting WiFi to " + config_.ssid);
  WiFi.mode(WIFI_MODE_STA);
  WiFi.setHostname(config_.hostname.c_str());
  WiFi.begin(config_.ssid.c_str(), config_.password.c_str());
}

void WifiService::startAccessPoint() {
  apMode_ = true;
  connectStartMs_ = 0;
  lastRetryMs_ = 0;

  WiFi.disconnect(true, true);
  delay(100);

  apSsid_ = "MavSphere-" + nodeId_;

  WiFi.mode(WIFI_MODE_AP);
  WiFi.softAP(apSsid_.c_str(), apPassword_.c_str());

  Logger::info("AP mode started: " + apSsid_ + " / " + apPassword_);
  Logger::info("AP IP: " + WiFi.softAPIP().toString());
}

void WifiService::loop() {
  if (apMode_) return;
  if (config_.ssid.isEmpty()) return;
  if (WiFi.status() == WL_CONNECTED) return;

  const unsigned long now = millis();

  if (now - lastRetryMs_ > 10000) {
    lastRetryMs_ = now;
    Logger::warn("WiFi disconnected, retrying STA connection");
    WiFi.disconnect();
    WiFi.begin(config_.ssid.c_str(), config_.password.c_str());
  }
}

bool WifiService::isConnected() const {
  return !apMode_ && WiFi.status() == WL_CONNECTED;
}

bool WifiService::isApMode() const {
  return apMode_;
}

String WifiService::ipAddress() const {
  return apMode_ ? WiFi.softAPIP().toString() : WiFi.localIP().toString();
}

long WifiService::rssi() const {
  return isConnected() ? WiFi.RSSI() : 0;
}

String WifiService::accessPointSsid() const {
  return apSsid_;
}

String WifiService::accessPointPassword() const {
  return apPassword_;
}