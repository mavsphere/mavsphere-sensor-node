#pragma once

#include <Arduino.h>
#include <vector>
#include <SPI.h>               // RC522 RFID over a dedicated ESP32 SPI bus
#include <Wire.h>
#include <Adafruit_MCP23X17.h> // MCP23017 GPIO expander over I2C
#include "BoardPins.h"
#include "ConfigTypes.h"

class MqttService;

struct RuntimeSensor {
  SensorConfig config;
  bool stableState = false;
  bool lastRawState = false;
  unsigned long lastEdgeMs = 0;
};

struct RuntimeRfidReader {
  RfidReaderConfig config;
  bool configured = false;
  bool detected = false;
  bool ready = false;
  uint8_t versionReg = 0x00;
  String lastError;
  String lastTag;
  unsigned long lastTagMs = 0;
  unsigned long lastCheckMs = 0;
  unsigned long lastReprimeMs = 0;
  unsigned long lastUidFailLogMs = 0;
};

class SensorManager {
public:
  SensorManager();
  ~SensorManager();

  void begin(const NodeConfig& config, MqttService* mqtt);
  void reload(const NodeConfig& config, MqttService* mqtt);
  void loop(const String& nodeId);
  void publishAllSensorStates(const String& nodeId, const String& reason);
  String statusJson() const;

  bool isRfidConfigured() const;
  bool isRfidDetected() const;
  bool isRfidReady() const;
  String rfidReaderId() const;
  String rfidLastError() const;
  String rfidVersionHex() const;
  String rfidLastTag() const;

  // Kept for App.cpp compatibility. RFID now uses a dedicated SPIClass/HSPI
  // instance, so display/TFT SPI activity no longer needs to restore RFID.
  void restoreRfidAfterExternalSpiUse();

private:
  std::vector<RuntimeSensor> sensors_;
  NodeConfig config_;
  MqttService* mqtt_ = nullptr;

  // MCP23017 expander on I2C — reads all sensor inputs
  Adafruit_MCP23X17 mcp_;
  bool mcpStarted_ = false;
public:
  Adafruit_MCP23X17* getMcp() { return mcpStarted_ ? &mcp_ : nullptr; }
private:

  // RC522 RFID — all readers share one dedicated ESP32 SPI bus. Each reader
  // has a unique SS/CS pin. RST may be shared.
  SPIClass* rfidSpi_ = nullptr;
  std::vector<RuntimeRfidReader> rfidReaders_;
  bool rfidBusStarted_ = false;

  void beginI2CBus();
  void beginMcp23017();
  void beginRfid();
  void beginRfidBus(const RfidReaderConfig& firstReader);
  void pollRfidReader(const String& nodeId, RuntimeRfidReader& reader, unsigned long now);

  // Minimal RC522 driver using the dedicated SPIClass above. This avoids the
  // MFRC522 library's global SPI usage conflicting with TFT_eSPI.
  void rfidSelect(const RuntimeRfidReader& reader);
  void rfidDeselect(const RuntimeRfidReader& reader);
  void rfidDeselectAll();
  void rfidWriteRegister(const RuntimeRfidReader& reader, uint8_t reg, uint8_t value);
  uint8_t rfidReadRegister(const RuntimeRfidReader& reader, uint8_t reg);
  void rfidSetRegisterBitMask(const RuntimeRfidReader& reader, uint8_t reg, uint8_t mask);
  void rfidClearRegisterBitMask(const RuntimeRfidReader& reader, uint8_t reg, uint8_t mask);
  void rfidReset(const RuntimeRfidReader& reader);
  void rfidAntennaOn(const RuntimeRfidReader& reader);
  void rfidSetMaxGain(const RuntimeRfidReader& reader);
  void primeRfidReader(RuntimeRfidReader& reader, bool logRestore);
  bool rfidCalculateCrc(const RuntimeRfidReader& reader, const uint8_t* data, uint8_t len, uint8_t* result);
  bool rfidTransceive(const RuntimeRfidReader& reader,
                      const uint8_t* sendData, uint8_t sendLen,
                      uint8_t* backData, uint8_t* backLen,
                      uint8_t* validBits = nullptr, uint8_t txLastBits = 0);
  bool rfidRequestA(const RuntimeRfidReader& reader);
  bool rfidReadUid(RuntimeRfidReader& reader, String& tag);
  void rfidHaltA(const RuntimeRfidReader& reader);
  void rfidStopCrypto(const RuntimeRfidReader& reader);

  String versionHex(uint8_t version) const;
  bool readRaw(RuntimeSensor& s);  // non-const: MCP23017 digitalRead() is not const
};
