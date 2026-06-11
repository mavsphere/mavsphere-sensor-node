#pragma once

#include <WebServer.h>
#include "ConfigTypes.h"
class ConfigStore;
class WifiService;
class MqttService;
class SensorManager;
class SignalOutputManager;

class WebUi {
public:
  void begin(NodeConfig* config, ConfigStore* store, WifiService* wifi, MqttService* mqtt, SensorManager* sensors, SignalOutputManager* signalOut = nullptr);
  void loop();
private:
  WebServer server_{80};
  NodeConfig* config_ = nullptr;
  ConfigStore* store_ = nullptr;
  WifiService* wifi_ = nullptr;
  MqttService* mqtt_ = nullptr;
  SensorManager* sensors_ = nullptr;
  SignalOutputManager* signalOut_ = nullptr;
  void setupRoutes();
  String getConfigJson(bool redactPasswords) const;
  String getStatusJson() const;
};
