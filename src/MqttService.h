#pragma once

#include <Arduino.h>
#include <PubSubClient.h>
#include <WiFiClient.h>
#include <ArduinoJson.h>
#include "ConfigTypes.h"

class ConfigStore;
class SensorManager;
class SignalOutputManager;

class MqttService {
public:
  void begin(NodeConfig& config, ConfigStore* store = nullptr,
             SensorManager* sensors = nullptr, SignalOutputManager* signalOut = nullptr);
  void loop(bool wifiConnected);
  bool isConnected();

  void publishStatus(const NodeStatus& status, const String& nodeId);
  void publishHeartbeat(const NodeStatus& status, const String& nodeId);
  void publishSensorEvent(const String& nodeId, const SensorConfig& cfg, bool active);
  void publishRfidEvent(const String& nodeId, const String& readerId, const String& tagId);

private:
  WiFiClient wifi_;
  PubSubClient client_{wifi_};
  NodeConfig* config_ = nullptr;
  ConfigStore* store_ = nullptr;
  SensorManager* sensors_ = nullptr;
  SignalOutputManager* signalOut_ = nullptr;
  unsigned long lastConnectAttemptMs_ = 0;

  void connect();
  void subscribeCommands();
  void subscribeSignals();
  bool publishJson(const String& topic, JsonDocument& doc, bool retained = false);

  void handleMessage(char* topic, byte* payload, unsigned int length);
  bool applyConfigPayload(const String& payload);
  void publishCommandReply(const String& suffix, JsonDocument& doc, bool retained = false);

  static MqttService* instance_;
  static void callback(char* topic, byte* payload, unsigned int length);
};