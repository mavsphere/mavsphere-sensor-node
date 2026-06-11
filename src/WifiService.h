#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include "ConfigTypes.h"

class WifiService {
public:
  void begin(const WifiConfig& config, const String& nodeId, bool forceConfigMode = false);
  void loop();
  bool isConnected() const;
  bool isApMode() const;
  String ipAddress() const;
  long rssi() const;

  String accessPointSsid() const;
  String accessPointPassword() const;

private:
  WifiConfig config_;
  String nodeId_;
  bool apMode_ = false;
  unsigned long connectStartMs_ = 0;
  unsigned long lastRetryMs_ = 0;
  String apSsid_;
  String apPassword_ = "configureme";

  void startStation();
  void startAccessPoint();
};