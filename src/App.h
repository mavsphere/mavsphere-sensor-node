#pragma once

#include <Arduino.h>
#include "ConfigTypes.h"
#include "ConfigStore.h"
#include "DisplayService.h"
#include "WifiService.h"
#include "MqttService.h"
#include "SensorManager.h"
#include "SignalOutputManager.h"
#include "WebUi.h"

class App {
public:
  bool begin(bool forceConfigMode, bool factoryResetRequested);
  void loop();

private:
  NodeStatus buildStatus();
  void updateDisplay();

  NodeConfig config_;
  ConfigStore store_;
  DisplayService display_;
  WifiService wifi_;
  MqttService mqtt_;
  SensorManager sensors_;
  SignalOutputManager signals_;
  WebUi web_;

  unsigned long lastStatusMs_ = 0;
  unsigned long lastHeartbeatMs_ = 0;
  unsigned long lastDisplayMs_ = 0;
  String lastDisplayKey_;
};