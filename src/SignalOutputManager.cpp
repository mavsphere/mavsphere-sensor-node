#include "SignalOutputManager.h"
#include "Logger.h"
#include <ArduinoJson.h>

void SignalOutputManager::begin(const NodeConfig& config, Adafruit_MCP23X17* mcp) {
    mcp_ = mcp;
    signals_.clear();

    if (config.signalOutputs.empty()) return;

    if (mcp_ == nullptr) {
        Logger::warn("SignalOutputManager: MCP23017 not available — signal outputs disabled");
        // Still populate signals_ so statusJson() can report configured signals
        for (const auto& sc : config.signalOutputs) {
            if (!sc.enabled || sc.signalId.isEmpty()) continue;
            RuntimeSignal rs;
            rs.config = sc;
            rs.currentAspect = "DARK";  // not driven — MCP unavailable
            signals_.push_back(rs);
        }
        return;
    }

    for (const auto& sc : config.signalOutputs) {
        if (!sc.enabled || sc.signalId.isEmpty()) continue;

        RuntimeSignal rs;
        rs.config = sc;
        rs.currentAspect = "RED";  // fail-safe default

        // Configure MCP23017 pins as outputs
        if (sc.redGpio    != 255) mcp_->pinMode(sc.redGpio,    OUTPUT);
        if (sc.yellowGpio != 255) mcp_->pinMode(sc.yellowGpio, OUTPUT);
        if (sc.greenGpio  != 255) mcp_->pinMode(sc.greenGpio,  OUTPUT);

        signals_.push_back(rs);
        driveGpios(signals_.back());

        Logger::info("Signal output ready: " + sc.signalId +
                     " R=MCP" + String(sc.redGpio) +
                     " Y=MCP" + String(sc.yellowGpio) +
                     " G=MCP" + String(sc.greenGpio));
    }
}

void SignalOutputManager::setAspect(const String& signalId, const String& aspect) {
    for (auto& sig : signals_) {
        if (sig.config.signalId == signalId) {
            if (sig.currentAspect != aspect) {
                sig.currentAspect = aspect;
                driveGpios(sig);
                Logger::info("Signal " + signalId + " → " + aspect);
            }
            return;
        }
    }
    // Signal not managed by this node — ignore silently
}

void SignalOutputManager::driveGpios(const RuntimeSignal& sig) {
    if (mcp_ == nullptr) return;
    bool isRed    = (sig.currentAspect == "RED");
    bool isYellow = (sig.currentAspect == "YELLOW");
    bool isGreen  = (sig.currentAspect == "GREEN");
    bool ca = sig.config.commonAnode;

    setPin(sig.config.redGpio,    isRed,    ca);
    setPin(sig.config.yellowGpio, isYellow, ca);
    setPin(sig.config.greenGpio,  isGreen,  ca);
}

void SignalOutputManager::setPin(uint8_t gpio, bool on, bool commonAnode) {
    if (gpio == 255 || mcp_ == nullptr) return;
    // Common anode: LOW = on, HIGH = off
    // Common cathode: HIGH = on, LOW = off
    mcp_->digitalWrite(gpio, (on ^ commonAnode) ? HIGH : LOW);
}

String SignalOutputManager::statusJson() const {
    JsonDocument doc;
    auto arr = doc["signals"].to<JsonArray>();
    for (const auto& sig : signals_) {
        auto o = arr.add<JsonObject>();
        o["signalId"]   = sig.config.signalId;
        o["aspect"]     = sig.currentAspect;
        o["redMcpPin"]    = sig.config.redGpio;
        o["yellowMcpPin"] = sig.config.yellowGpio;
        o["greenMcpPin"]  = sig.config.greenGpio;
        o["mcpReady"]   = (mcp_ != nullptr);
    }
    String out;
    serializeJson(doc, out);
    return out;
}
