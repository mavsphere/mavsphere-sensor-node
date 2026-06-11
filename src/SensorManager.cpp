#include "SensorManager.h"
#include "MqttService.h"
#include "ConfigStore.h"
#include "Logger.h"
#include <ArduinoJson.h>

namespace {
// RC522 readers are momentary detectors. Poll frequently so fast train/tag
// passes are not missed, but do not block the rest of the node.
constexpr unsigned long RFID_POLL_INTERVAL_MS = 20;
constexpr unsigned long RFID_REPRIME_INTERVAL_MS = 2000;
constexpr unsigned long RFID_MIN_REPUBLISH_MS = 200;

constexpr uint32_t RFID_SPI_CLOCK_HZ = 4000000;

// RC522 register addresses and command constants. Register values are the
// logical RC522 register numbers; the SPI read/write address byte is derived in
// rfidReadRegister/rfidWriteRegister.
constexpr uint8_t CommandReg      = 0x01;
constexpr uint8_t DivIrqReg       = 0x05;
constexpr uint8_t ComIrqReg       = 0x04;
constexpr uint8_t ErrorReg        = 0x06;
constexpr uint8_t Status2Reg      = 0x08;
constexpr uint8_t FIFODataReg     = 0x09;
constexpr uint8_t FIFOLevelReg    = 0x0A;
constexpr uint8_t ControlReg      = 0x0C;
constexpr uint8_t BitFramingReg   = 0x0D;
constexpr uint8_t CollReg         = 0x0E;
constexpr uint8_t ModeReg         = 0x11;
constexpr uint8_t TxModeReg       = 0x12;
constexpr uint8_t RxModeReg       = 0x13;
constexpr uint8_t TxControlReg    = 0x14;
constexpr uint8_t TxASKReg        = 0x15;
constexpr uint8_t ModWidthReg     = 0x24;
constexpr uint8_t RFCfgReg        = 0x26;
constexpr uint8_t TModeReg        = 0x2A;
constexpr uint8_t TPrescalerReg   = 0x2B;
constexpr uint8_t TReloadRegH     = 0x2C;
constexpr uint8_t TReloadRegL     = 0x2D;
constexpr uint8_t VersionReg      = 0x37;
constexpr uint8_t CRCResultRegH   = 0x21;
constexpr uint8_t CRCResultRegL   = 0x22;

constexpr uint8_t PCD_Idle        = 0x00;
constexpr uint8_t PCD_CalcCRC     = 0x03;
constexpr uint8_t PCD_Transceive  = 0x0C;
constexpr uint8_t PCD_SoftReset   = 0x0F;

constexpr uint8_t PICC_CMD_REQA    = 0x26;
constexpr uint8_t PICC_CMD_CT      = 0x88;
constexpr uint8_t PICC_CMD_SEL_CL1 = 0x93;
constexpr uint8_t PICC_CMD_HLTA    = 0x50;

#ifndef MAVSPHERE_RFID_SPI_BUS
#define MAVSPHERE_RFID_SPI_BUS HSPI
#endif
}

SensorManager::SensorManager() {}

SensorManager::~SensorManager() {
  if (rfidSpi_) {
    delete rfidSpi_;
    rfidSpi_ = nullptr;
  }
}

void SensorManager::begin(const NodeConfig& config, MqttService* mqtt) {
  beginI2CBus();
  reload(config, mqtt);
}

void SensorManager::reload(const NodeConfig& config, MqttService* mqtt) {
  config_ = config;
  mqtt_   = mqtt;

  sensors_.clear();
  for (const auto& s : config_.sensors) {
    if (!s.enabled || s.gpio == 255 || s.type == SensorType::None) continue;
    RuntimeSensor rs;
    rs.config = s;
    sensors_.push_back(rs);
  }

  beginMcp23017();
  beginRfid();
}

// ── Start the I2C bus for MCP23017 ───────────────────────────────────────────
void SensorManager::beginI2CBus() {
  Wire.begin(BOARD_I2C_SDA_PIN, BOARD_I2C_SCL_PIN);
  Logger::info("I2C bus started on SDA=" + String(BOARD_I2C_SDA_PIN)
               + " SCL=" + String(BOARD_I2C_SCL_PIN));
}

// ── Initialise MCP23017 and configure sensor pins as inputs with pull-ups ─────
void SensorManager::beginMcp23017() {
  mcpStarted_ = false;

  if (!mcp_.begin_I2C(BOARD_MCP23017_I2C_ADDR, &Wire)) {
    Logger::info("MCP23017 NOT found at 0x" + String(BOARD_MCP23017_I2C_ADDR, HEX));
    return;
  }

  pinMode(BOARD_MCP23017_INTA_PIN, INPUT_PULLUP);

  // Mark as started before taking initial readings so readRaw(...) uses the MCP23017.
  // Otherwise startup state may be initialised as CLEAR for every sensor.
  mcpStarted_ = true;

  for (auto& s : sensors_) {
    mcp_.pinMode(s.config.gpio, INPUT_PULLUP);
    s.lastRawState = readRaw(s);
    s.stableState  = s.lastRawState;
    s.lastEdgeMs   = millis();
    Logger::info("Sensor ready: " + s.config.id + " MCP pin=" + String(s.config.gpio));
  }

  mcp_.setupInterrupts(true /*mirror*/, false /*openDrain*/, LOW /*polarity*/);
  for (auto& s : sensors_) {
    mcp_.setupInterruptPin(s.config.gpio, CHANGE);
  }

  Logger::info("MCP23017 ready at 0x" + String(BOARD_MCP23017_I2C_ADDR, HEX));
}

void SensorManager::restoreRfidAfterExternalSpiUse() {
  // No-op by design. RFID now uses its own SPIClass/HSPI instance, so TFT_eSPI
  // display updates no longer need to restore the global SPI bus for RFID.
}

void SensorManager::beginRfidBus(const RfidReaderConfig& firstReader) {
  if (!rfidSpi_) {
    rfidSpi_ = new SPIClass(MAVSPHERE_RFID_SPI_BUS);
  }

  rfidSpi_->begin(firstReader.sckPin,
                  firstReader.misoPin,
                  firstReader.mosiPin,
                  firstReader.ssPin);
  rfidBusStarted_ = true;
}

void SensorManager::rfidDeselectAll() {
  for (const auto& reader : rfidReaders_) {
    if (reader.config.ssPin != 255) {
      pinMode(reader.config.ssPin, OUTPUT);
      digitalWrite(reader.config.ssPin, HIGH);
    }
  }
}

void SensorManager::rfidSelect(const RuntimeRfidReader& reader) {
  rfidDeselectAll();
  digitalWrite(reader.config.ssPin, LOW);
}

void SensorManager::rfidDeselect(const RuntimeRfidReader& reader) {
  digitalWrite(reader.config.ssPin, HIGH);
}

// ── Minimal dedicated-SPI RC522 driver helpers ───────────────────────────────
void SensorManager::rfidWriteRegister(const RuntimeRfidReader& reader, uint8_t reg, uint8_t value) {
  if (!rfidSpi_) return;
  rfidSpi_->beginTransaction(SPISettings(RFID_SPI_CLOCK_HZ, MSBFIRST, SPI_MODE0));
  rfidSelect(reader);
  rfidSpi_->transfer((reg << 1) & 0x7E);
  rfidSpi_->transfer(value);
  rfidDeselect(reader);
  rfidSpi_->endTransaction();
}

uint8_t SensorManager::rfidReadRegister(const RuntimeRfidReader& reader, uint8_t reg) {
  if (!rfidSpi_) return 0x00;
  rfidSpi_->beginTransaction(SPISettings(RFID_SPI_CLOCK_HZ, MSBFIRST, SPI_MODE0));
  rfidSelect(reader);
  rfidSpi_->transfer(0x80 | ((reg << 1) & 0x7E));
  uint8_t value = rfidSpi_->transfer(0x00);
  rfidDeselect(reader);
  rfidSpi_->endTransaction();
  return value;
}

void SensorManager::rfidSetRegisterBitMask(const RuntimeRfidReader& reader, uint8_t reg, uint8_t mask) {
  rfidWriteRegister(reader, reg, rfidReadRegister(reader, reg) | mask);
}

void SensorManager::rfidClearRegisterBitMask(const RuntimeRfidReader& reader, uint8_t reg, uint8_t mask) {
  rfidWriteRegister(reader, reg, rfidReadRegister(reader, reg) & (~mask));
}

void SensorManager::rfidReset(const RuntimeRfidReader& reader) {
  rfidWriteRegister(reader, CommandReg, PCD_SoftReset);
  delay(50);

  for (uint8_t i = 0; i < 10; ++i) {
    if ((rfidReadRegister(reader, CommandReg) & (1 << 4)) == 0) return;
    delay(10);
  }
}

void SensorManager::rfidAntennaOn(const RuntimeRfidReader& reader) {
  uint8_t value = rfidReadRegister(reader, TxControlReg);
  if ((value & 0x03) != 0x03) {
    rfidWriteRegister(reader, TxControlReg, value | 0x03);
  }
}

void SensorManager::rfidSetMaxGain(const RuntimeRfidReader& reader) {
  rfidClearRegisterBitMask(reader, RFCfgReg, 0x70);
  rfidSetRegisterBitMask(reader, RFCfgReg, 0x70);
}

void SensorManager::primeRfidReader(RuntimeRfidReader& reader, bool logRestore) {
  if (!rfidSpi_) return;

  rfidReset(reader);
  rfidWriteRegister(reader, TxModeReg, 0x00);
  rfidWriteRegister(reader, RxModeReg, 0x00);
  rfidWriteRegister(reader, ModWidthReg, 0x26);
  rfidWriteRegister(reader, TModeReg, 0x80);
  rfidWriteRegister(reader, TPrescalerReg, 0xA9);
  rfidWriteRegister(reader, TReloadRegH, 0x03);
  rfidWriteRegister(reader, TReloadRegL, 0xE8);
  rfidWriteRegister(reader, TxASKReg, 0x40);
  rfidWriteRegister(reader, ModeReg, 0x3D);
  rfidSetMaxGain(reader);
  rfidAntennaOn(reader);

  if (logRestore) {
    uint8_t version = rfidReadRegister(reader, VersionReg);
    Logger::info("[rfid] reader re-primed id=" + reader.config.id + " VersionReg=" + versionHex(version));
  }
}

bool SensorManager::rfidCalculateCrc(const RuntimeRfidReader& reader, const uint8_t* data, uint8_t len, uint8_t* result) {
  if (!rfidSpi_ || !data || !result) return false;

  rfidWriteRegister(reader, CommandReg, PCD_Idle);
  rfidWriteRegister(reader, DivIrqReg, 0x04);
  rfidSetRegisterBitMask(reader, FIFOLevelReg, 0x80);

  for (uint8_t i = 0; i < len; ++i) {
    rfidWriteRegister(reader, FIFODataReg, data[i]);
  }

  rfidWriteRegister(reader, CommandReg, PCD_CalcCRC);

  for (uint16_t i = 0; i < 5000; ++i) {
    uint8_t n = rfidReadRegister(reader, DivIrqReg);
    if (n & 0x04) {
      rfidWriteRegister(reader, CommandReg, PCD_Idle);
      result[0] = rfidReadRegister(reader, CRCResultRegL);
      result[1] = rfidReadRegister(reader, CRCResultRegH);
      return true;
    }
    delayMicroseconds(1);
  }

  rfidWriteRegister(reader, CommandReg, PCD_Idle);
  return false;
}

bool SensorManager::rfidTransceive(const RuntimeRfidReader& reader,
                                   const uint8_t* sendData, uint8_t sendLen,
                                   uint8_t* backData, uint8_t* backLen,
                                   uint8_t* validBits, uint8_t txLastBits) {
  if (!rfidSpi_ || !sendData || sendLen == 0) return false;

  uint8_t waitIRq = 0x30; // RxIRq | IdleIRq

  rfidWriteRegister(reader, CommandReg, PCD_Idle);
  rfidWriteRegister(reader, ComIrqReg, 0x7F);
  rfidSetRegisterBitMask(reader, FIFOLevelReg, 0x80);

  for (uint8_t i = 0; i < sendLen; ++i) {
    rfidWriteRegister(reader, FIFODataReg, sendData[i]);
  }

  rfidWriteRegister(reader, BitFramingReg, txLastBits & 0x07);
  rfidWriteRegister(reader, CommandReg, PCD_Transceive);
  rfidSetRegisterBitMask(reader, BitFramingReg, 0x80);

  bool completed = false;
  for (uint16_t i = 0; i < 5000; ++i) {
    uint8_t n = rfidReadRegister(reader, ComIrqReg);
    if (n & waitIRq) {
      completed = true;
      break;
    }
    if (n & 0x01) {
      break;
    }
    delayMicroseconds(1);
  }

  rfidClearRegisterBitMask(reader, BitFramingReg, 0x80);

  if (!completed) return false;

  uint8_t error = rfidReadRegister(reader, ErrorReg);
  if (error & 0x13) {
    return false;
  }

  if (backData && backLen) {
    uint8_t fifoLevel = rfidReadRegister(reader, FIFOLevelReg);
    if (fifoLevel > *backLen) {
      return false;
    }
    *backLen = fifoLevel;
    for (uint8_t i = 0; i < fifoLevel; ++i) {
      backData[i] = rfidReadRegister(reader, FIFODataReg);
    }

    if (validBits) {
      *validBits = rfidReadRegister(reader, ControlReg) & 0x07;
    }
  }

  return true;
}

bool SensorManager::rfidRequestA(const RuntimeRfidReader& reader) {
  uint8_t atqa[2] = {0, 0};
  uint8_t atqaLen = sizeof(atqa);
  uint8_t validBits = 0;
  if (!rfidTransceive(reader, &PICC_CMD_REQA, 1, atqa, &atqaLen, &validBits, 7)) {
    return false;
  }
  return atqaLen == 2;
}

bool SensorManager::rfidReadUid(RuntimeRfidReader& reader, String& tag) {
  tag = "";
  rfidClearRegisterBitMask(reader, CollReg, 0x80);

  uint8_t cmd[] = { PICC_CMD_SEL_CL1, 0x20 };
  uint8_t buffer[10] = {0};
  uint8_t bufferLen = sizeof(buffer);
  uint8_t validBits = 0;

  if (!rfidTransceive(reader, cmd, sizeof(cmd), buffer, &bufferLen, &validBits, 0)) {
    return false;
  }

  if (bufferLen < 5) {
    return false;
  }

  uint8_t bcc = buffer[0] ^ buffer[1] ^ buffer[2] ^ buffer[3];
  if (bcc != buffer[4]) {
    Logger::warn("[rfid] UID BCC check failed reader=" + reader.config.id);
    return false;
  }

  uint8_t start = 0;
  uint8_t count = 4;

  if (buffer[0] == PICC_CMD_CT) {
    start = 1;
    count = 3;
  }

  for (uint8_t i = 0; i < count; ++i) {
    if (i) tag += ":";
    uint8_t value = buffer[start + i];
    if (value < 0x10) tag += "0";
    tag += String(value, HEX);
  }
  tag.toUpperCase();
  return tag.length() > 0;
}

void SensorManager::rfidHaltA(const RuntimeRfidReader& reader) {
  uint8_t buffer[4] = { PICC_CMD_HLTA, 0x00, 0x00, 0x00 };
  if (rfidCalculateCrc(reader, buffer, 2, &buffer[2])) {
    uint8_t back[1];
    uint8_t backLen = sizeof(back);
    rfidTransceive(reader, buffer, sizeof(buffer), back, &backLen, nullptr, 0);
  }
}

void SensorManager::rfidStopCrypto(const RuntimeRfidReader& reader) {
  rfidClearRegisterBitMask(reader, Status2Reg, 0x08);
}

// ── Initialise configured RC522 RFID readers ─────────────────────────────────
void SensorManager::beginRfid() {
  rfidReaders_.clear();
  rfidBusStarted_ = false;

  std::vector<RfidReaderConfig> configured;
  if (!config_.rfidReaders.empty()) {
    configured = config_.rfidReaders;
  } else if (config_.rfid.enabled) {
    configured.push_back(config_.rfid);
  }

  for (auto& rc : configured) {
    if (rfidReaders_.size() >= BOARD_RFID_MAX_READERS) {
      Logger::warn("[rfid] ignoring reader beyond firmware max " + String(BOARD_RFID_MAX_READERS));
      break;
    }
    if (!rc.enabled) continue;
    if (rc.id.isEmpty()) {
      Logger::warn("[rfid] skipping configured reader with empty id");
      continue;
    }

    RuntimeRfidReader rr;
    rr.config = rc;
    rr.config.sckPin = rc.sckPin == 255 ? BOARD_RFID_SCK_PIN : rc.sckPin;
    rr.config.misoPin = rc.misoPin == 255 ? BOARD_RFID_MISO_PIN : rc.misoPin;
    rr.config.mosiPin = rc.mosiPin == 255 ? BOARD_RFID_MOSI_PIN : rc.mosiPin;
    rr.config.duplicateSuppressMs = rc.duplicateSuppressMs == 0 ? RFID_MIN_REPUBLISH_MS : rc.duplicateSuppressMs;
    rr.configured = true;
    rfidReaders_.push_back(rr);
  }

  if (rfidReaders_.empty()) {
    Logger::info("[rfid] disabled in config");
    return;
  }

  // Prepare SS/RST pins before starting the shared bus. All SS pins idle HIGH.
  for (auto& reader : rfidReaders_) {
    pinMode(reader.config.ssPin, OUTPUT);
    digitalWrite(reader.config.ssPin, HIGH);
    if (reader.config.rstPin != 255) {
      pinMode(reader.config.rstPin, OUTPUT);
      digitalWrite(reader.config.rstPin, HIGH);
    }
  }

  beginRfidBus(rfidReaders_[0].config);

  Logger::info("[rfid] HSPI bus ready sck=" + String(rfidReaders_[0].config.sckPin)
               + " miso=" + String(rfidReaders_[0].config.misoPin)
               + " mosi=" + String(rfidReaders_[0].config.mosiPin)
               + " readers=" + String(rfidReaders_.size()));

  for (auto& reader : rfidReaders_) {
    Logger::info("[rfid] init reader=" + reader.config.id
                 + " ss=" + String(reader.config.ssPin)
                 + " rst=" + String(reader.config.rstPin));

    primeRfidReader(reader, false);
    reader.versionReg = rfidReadRegister(reader, VersionReg);

    Logger::info("[rfid] reader=" + reader.config.id + " RC522 VersionReg=" + versionHex(reader.versionReg));

    if (reader.versionReg == 0x00 || reader.versionReg == 0xFF) {
      reader.lastError = "RC522 not detected";
      Logger::warn("[rfid] reader not detected id=" + reader.config.id
                   + "; continuing without this reader");
      continue;
    }

    reader.detected = true;
    reader.ready = true;
    Logger::info("[rfid] reader ready id=" + reader.config.id + " version=" + versionHex(reader.versionReg));
  }
}

// ── Read a single sensor via MCP23017 ────────────────────────────────────────
bool SensorManager::readRaw(RuntimeSensor& s) {
  bool value = mcpStarted_ ? (mcp_.digitalRead(s.config.gpio) == HIGH) : false;
  if (s.config.invert) value = !value;
  return value;
}

void SensorManager::publishAllSensorStates(const String& nodeId, const String& reason) {
  if (mqtt_ == nullptr) return;

  unsigned int count = 0;
  const unsigned long now = millis();

  if (mcpStarted_) {
    for (auto& s : sensors_) {
      const bool raw = readRaw(s);
      s.lastRawState = raw;
      s.stableState = raw;
      s.lastEdgeMs = now;
      mqtt_->publishSensorEvent(nodeId, s.config, s.stableState);
      count++;
    }
  } else {
    // If the MCP is unavailable, still publish the last known configured state
    // as CLEAR so the backend does not keep stale occupied blocks forever.
    for (auto& s : sensors_) {
      s.lastRawState = false;
      s.stableState = false;
      s.lastEdgeMs = now;
      mqtt_->publishSensorEvent(nodeId, s.config, false);
      count++;
    }
  }

  Logger::info("[sensor-sync] published " + String(count) + " sensor state(s) reason=" + reason);
}

void SensorManager::pollRfidReader(const String& nodeId, RuntimeRfidReader& reader, unsigned long now) {
  if (!reader.ready || !rfidBusStarted_ || !rfidSpi_) return;
  if (now - reader.lastCheckMs < RFID_POLL_INTERVAL_MS) return;
  reader.lastCheckMs = now;

  if (now - reader.lastReprimeMs > RFID_REPRIME_INTERVAL_MS) {
    reader.lastReprimeMs = now;
    primeRfidReader(reader, false);
  }

  if (!rfidRequestA(reader)) {
    return;
  }

  String tag;
  if (!rfidReadUid(reader, tag)) {
    if (now - reader.lastUidFailLogMs > 1000) {
      reader.lastUidFailLogMs = now;
      Logger::warn("[rfid] card present but UID read failed reader=" + reader.config.id);
    }
    rfidStopCrypto(reader);
    return;
  }

  const uint32_t minRepublishMs = reader.config.duplicateSuppressMs > 0
      ? reader.config.duplicateSuppressMs
      : RFID_MIN_REPUBLISH_MS;
  const bool publish = (tag != reader.lastTag) || (now - reader.lastTagMs >= minRepublishMs);
  if (publish) {
    reader.lastTag = tag;
    reader.lastTagMs = now;
    if (mqtt_) mqtt_->publishRfidEvent(nodeId, reader.config.id, tag);
    Logger::info("RFID tag " + reader.config.id + " -> " + tag);
  }

  rfidHaltA(reader);
  rfidStopCrypto(reader);
}

// ── Main loop ─────────────────────────────────────────────────────────────────
void SensorManager::loop(const String& nodeId) {
  const unsigned long now = millis();

  // RFID first: tag reads are short, momentary events and should not be delayed
  // behind ordinary MCP/I2C sensor polling.
  for (auto& reader : rfidReaders_) {
    pollRfidReader(nodeId, reader, now);
  }

  if (mcpStarted_) {
    for (auto& s : sensors_) {
      const bool raw = readRaw(s);
      if (raw != s.lastRawState) {
        s.lastRawState = raw;
        s.lastEdgeMs   = now;
      }
      if ((now - s.lastEdgeMs) >= s.config.debounceMs && s.stableState != raw) {
        s.stableState = raw;
        if (mqtt_) mqtt_->publishSensorEvent(nodeId, s.config, s.stableState);
        Logger::info("Sensor " + s.config.id + " -> " + (s.stableState ? "ACTIVE" : "CLEAR"));
      }
    }
  }
}

String SensorManager::versionHex(uint8_t version) const {
  char buf[5];
  snprintf(buf, sizeof(buf), "0x%02X", version);
  return String(buf);
}

String SensorManager::rfidVersionHex() const {
  for (const auto& r : rfidReaders_) {
    if (r.configured) return versionHex(r.versionReg);
  }
  return "0x00";
}

bool SensorManager::isRfidConfigured() const {
  for (const auto& r : rfidReaders_) if (r.configured) return true;
  return false;
}

bool SensorManager::isRfidDetected() const {
  for (const auto& r : rfidReaders_) if (r.detected) return true;
  return false;
}

bool SensorManager::isRfidReady() const {
  for (const auto& r : rfidReaders_) if (r.ready) return true;
  return false;
}

String SensorManager::rfidReaderId() const {
  String out;
  for (const auto& r : rfidReaders_) {
    if (!r.configured) continue;
    if (out.length() > 0) out += ",";
    out += r.config.id;
  }
  return out;
}

String SensorManager::rfidLastError() const {
  String out;
  for (const auto& r : rfidReaders_) {
    if (r.lastError.length() == 0) continue;
    if (out.length() > 0) out += "; ";
    out += r.config.id + ": " + r.lastError;
  }
  return out;
}

String SensorManager::rfidLastTag() const {
  for (const auto& r : rfidReaders_) {
    if (r.lastTag.length() > 0) return r.lastTag;
  }
  return "";
}

String SensorManager::statusJson() const {
  JsonDocument doc;
  auto arr = doc["sensors"].to<JsonArray>();
  for (const auto& s : sensors_) {
    auto o = arr.add<JsonObject>();
    o["id"]     = s.config.id;
    o["name"]   = s.config.name;
    o["type"]   = ConfigStore::sensorTypeToString(s.config.type);
    o["mcpPin"] = s.config.gpio;
    o["active"] = s.stableState;
  }

  doc["rfidEnabled"] = isRfidConfigured();          // legacy/compat
  doc["rfidConfigured"] = isRfidConfigured();
  doc["rfidDetected"] = isRfidDetected();
  doc["rfidReady"] = isRfidReady();
  doc["rfidReaderId"] = rfidReaderId();
  doc["rfidVersionReg"] = rfidVersionHex();
  const String err = rfidLastError();
  if (err.length() > 0) {
    doc["rfidLastError"] = err;
  }
  doc["rfidLastTag"] = rfidLastTag();

  auto readers = doc["rfidReaders"].to<JsonArray>();
  for (const auto& r : rfidReaders_) {
    auto o = readers.add<JsonObject>();
    o["id"] = r.config.id;
    o["enabled"] = r.config.enabled;
    o["configured"] = r.configured;
    o["detected"] = r.detected;
    o["ready"] = r.ready;
    o["ssPin"] = r.config.ssPin;
    o["rstPin"] = r.config.rstPin;
    o["versionReg"] = versionHex(r.versionReg);
    if (r.lastError.length() > 0) o["lastError"] = r.lastError;
    if (r.lastTag.length() > 0) o["lastTag"] = r.lastTag;
  }

  String out;
  serializeJson(doc, out);
  return out;
}
