#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include "ConfigTypes.h"

class ConfigStore {
public:
  bool begin();
  bool load(NodeConfig& config);
  bool save(const NodeConfig& config);
  bool factoryReset();
  void applyDefaults(NodeConfig& config);

  static String sensorTypeToString(SensorType type);
  static SensorType sensorTypeFromString(const String& value);

private:
  static constexpr const char* CONFIG_PATH = "/config.json";
  static constexpr const char* BACKUP_PATH = "/config.bak.json";
  static constexpr int CONFIG_VERSION = 1;

  bool loadFromPath(const char* path, NodeConfig& config);
  bool writeToPath(const char* path, const NodeConfig& config);
  bool copyFile(const char* fromPath, const char* toPath);
  bool removeIfExists(const char* path);
  bool validateConfig(const NodeConfig& config, String* error = nullptr);
  void populateJson(JsonDocument& doc, const NodeConfig& config);
  bool parseJson(JsonDocument& doc, NodeConfig& config, String* error = nullptr);
};