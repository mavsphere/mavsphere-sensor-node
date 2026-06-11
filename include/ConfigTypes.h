#pragma once

#include <Arduino.h>
#include <vector>

enum class SensorType {
  None,
  DigitalOccupancy,
  IrBreakBeam
};

struct WifiConfig {
  String ssid;
  String password;
  bool dhcp = true;
  String hostname = "mavsphere-sensor-node";
};

struct MqttConfig {
  String host;
  uint16_t port = 1883;
  String username;
  String password;
  String clientId;
  String topicPrefix = "layout";
  bool enabled = true;
};

struct SensorConfig {
  String id;
  String name;
  SensorType type = SensorType::None;
  uint8_t gpio = 255;   // MCP23017 pin index: 0-7 = GPA0-GPA7, 8-15 = GPB0-GPB7
  bool enabled = false;
  bool invert = false;
  uint16_t debounceMs = 30;
};

// RC522 RFID over dedicated HSPI (second SPI bus, separate from display SPI).
// All readers on a node share SCK/MISO/MOSI. Each reader must have a unique
// SS/CS pin. RST may be shared between readers.
struct RfidReaderConfig {
  bool enabled = false;
  String id = "reader1";
  uint8_t ssPin   = 17;
  uint8_t rstPin  = 33;
  uint8_t sckPin  = 32;
  uint8_t misoPin = 13;
  uint8_t mosiPin = 26;
  uint32_t duplicateSuppressMs = 3000;
};

struct SignalOutputConfig {
  bool enabled = false;
  String signalId;           // matches backend signal ID (e.g. "SIG_B2_HOME")
  uint8_t redGpio = 255;
  uint8_t yellowGpio = 255;
  uint8_t greenGpio = 255;
  bool commonAnode = false;  // if true, LOW = LED on
};

struct NodeConfig {
  String nodeId = "node-01";
  WifiConfig wifi;
  MqttConfig mqtt;
  std::vector<SensorConfig> sensors;   // empty by default — push config from MavSphere

  // Legacy/single-reader RFID config. Kept for backwards compatibility with
  // existing stored node configs and simple/manual config pages.
  RfidReaderConfig rfid;

  // Preferred multi-reader config. Up to BOARD_RFID_MAX_READERS readers are
  // initialised by the firmware. Existing backend pushes using rfid.readers[]
  // map into this vector.
  std::vector<RfidReaderConfig> rfidReaders;

  std::vector<SignalOutputConfig> signalOutputs;
  unsigned long lastConfigPushMs = 0;  // millis() when last config was pushed via MQTT
};

struct NodeStatus {
  bool wifiConnected = false;
  bool apMode = false;
  bool mqttConnected = false;
  String ipAddress;
  long rssi = 0;
  unsigned long uptimeMs = 0;
  String version = MAVSPHERE_SENSOR_NODE_VERSION;
};
