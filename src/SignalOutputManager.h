#pragma once

#include <Arduino.h>
#include <vector>
#include <Adafruit_MCP23X17.h>
#include "ConfigTypes.h"

/**
 * Drives physical signal LEDs via MCP23017 GPIO expander.
 *
 * Each signal has 3 MCP23017 output pins (red, yellow, green).
 * Pin numbers in config are MCP23017 port pins (GPA0-GPA7 = 0-7, GPB0-GPB7 = 8-15).
 * Supports common-anode signals (LOW = on) via the commonAnode config flag.
 * Default state on startup is RED (fail-safe).
 */
class SignalOutputManager {
public:
    void begin(const NodeConfig& config, Adafruit_MCP23X17* mcp = nullptr);
    void setAspect(const String& signalId, const String& aspect);
    String statusJson() const;

private:
    struct RuntimeSignal {
        SignalOutputConfig config;
        String currentAspect = "RED";
    };

    std::vector<RuntimeSignal> signals_;
    Adafruit_MCP23X17* mcp_ = nullptr;

    void driveGpios(const RuntimeSignal& sig);
    void setPin(uint8_t gpio, bool on, bool commonAnode);
};
