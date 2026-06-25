# MavSphere Rail I/O Node — Hardware Reference

**Rev D | May 2026**

This document is the hardware reference for the Rev D Rail I/O Node PCB — the board the firmware in this repository is written for. If you are building a compatible board, designing a derivative, or contributing firmware, start here.

---

## Overview

The Rail I/O Node is an ESP32-based board for model railway layout automation. It sits at the bottom of the MavSphere stack:

```
Backend (Spring Boot)
        ↓
  Layout Agent (Go)
        ↓  (MQTT over Wi-Fi)
  Rail I/O Node (ESP32)
        ↓
  Field wiring: sensors, RFID readers, signal LEDs
```

The node never communicates with the backend directly — all messages go via the layout agent.

---

## What Rev D Provides

| Bank | Channels | Type |
|------|----------|------|
| Inputs | 16 | Individually opto-isolated field sensor inputs |
| Outputs | 16 | Driven output channels for signal LEDs and accessories |

The input and output banks are electrically separate. The board is not a universal mixed-channel design. This is intentional — it reduces miswiring risk and simplifies protection.

---

## Mandatory MCU Module

**LilyGO TTGO T-Display ESP32** (1.14-inch ST7789 TFT, standard 2-row pin header).

This is the only supported MCU module for Rev D. The firmware targets its specific pin assignments. Substitutes are not supported without a firmware change.

---

## GPIO Allocation (Fixed)

The following GPIO assignments are confirmed and locked. PCB routing must match these exactly.

### SPI Buses

| Bus | Purpose | SCK | MOSI | CS | DC | RST | Notes |
|-----|---------|-----|------|----|----|-----|-------|
| VSPI | TFT display | GPIO 18 | GPIO 19 | GPIO 5 | GPIO 16 | GPIO 23 | Fixed to T-Display wiring |
| HSPI | RFID readers | GPIO 32 | GPIO 26 | GPIO 13 | See below | 3 readers, individual SS lines |

RFID (HSPI) individual SS and shared RST:

| Signal | GPIO |
|--------|------|
| SS — Reader 1 | 17 |
| SS — Reader 2 | 25 |
| SS — Reader 3 | 2 |
| RST (shared) | 33 |

The two SPI buses are independent. **No bus sharing between display and RFID is permitted.**

### I2C Bus (MCP23017 expanders)

| Signal | GPIO |
|--------|------|
| SDA | 21 |
| SCL | 22 |
| MCP23017 interrupt | 27 |

### Pins to Avoid

| GPIO range | Reason |
|------------|--------|
| 6–11 | Internal flash — do not use |
| 0 | Boot strap |
| 1, 3 | UART TX/RX |

---

## I/O Expansion (MCP23017)

The 16 inputs and 16 outputs are managed via I2C GPIO expanders rather than direct ESP32 GPIO.

- Minimum 2× MCP23017: one dedicated to the input bank, one to the output bank.
- I2C address configuration via solder jumpers or DIP switch per expander.
- Both INTA and INTB from each expander should be routed to ESP32 interrupt-capable GPIO where practical.
- I2C pull-ups (4.7 kΩ typical) fitted on the board.

---

## Input System

### Isolation

All 16 field inputs are individually opto-isolated (PC817 or equivalent). Isolation protects the MCU from voltage differences, noise, and ground loops introduced by field wiring.

### Polarity Support

The board must handle sensors that drive their signal either HIGH or LOW:

| Sensor behaviour | Electrical meaning |
|------------------|--------------------|
| Active-LOW / sinking | Sensor pulls SIG to GND when active (open collector / open drain) |
| Active-HIGH / sourcing | Sensor drives SIG to field voltage when active |
| Dry contact / reed / relay | Passive contact closure — board provides wetting current |
| Externally powered digital | Sensor has its own supply, returns a digital signal |

The firmware also supports per-input logical inversion, so triggered/occupied state is normalised regardless of electrical polarity.

### Field Input Connector Pinout

Each of the 16 input channels has a 3-pin connector (pluggable screw terminal, 3.5 mm or 5.08 mm pitch):

| Pin | Label |
|-----|-------|
| 1 | GND_FIELD |
| 2 | +5V_FIELD |
| 3 | SIG_IN |

The 3-pin arrangement lets sensors such as IR break beams be powered and read over a single cable run back to the node.

---

## Output System

### Driver

Outputs must not be driven directly from ESP32 GPIO. They are driven via MCP23017 outputs with appropriate driver ICs (ULN2803A or equivalent low-side driver is confirmed acceptable for signal LED loads). Outputs are suitable for common-anode 3-aspect signal heads and similar low-current accessories.

### LED Current Limiting

Every output channel includes a current-limiting resistor. Default fitted value targets realistic model signal brightness at 5 V — approximately 1 kΩ per LED is the design starting point. PCB silkscreen and documentation must state whether onboard resistors are fitted.

### Signal Wiring Example (Common-Anode 3-Aspect Head)

```
+5V_SIGNAL ──── Common anode
                  ├── Red cathode ── R ── Output N    (driver pulls LOW to light)
                  ├── Yellow cathode ── R ── Output N+1
                  └── Green cathode ── R ── Output N+2
```

One 3-aspect signal head consumes three output channels.

### Field Output Connector

Outputs are exposed via serviceable connectors (pluggable screw terminals), clearly labelled with channel number and polarity. Grouped signal-head connectors (+5V common, RED, YELLOW, GREEN) are the preferred arrangement.

---

## RFID Support

Up to 3 local MFRC522 (RC522) readers. PN532 using the same SPI pinout is also acceptable.

### RFID Connector (per reader)

JST-XH 2.54 mm pitch, 7-pin. Pin order:

| Pin | Signal |
|-----|--------|
| 1 | VCC 3.3 V |
| 2 | GND |
| 3 | SCK |
| 4 | MOSI |
| 5 | MISO |
| 6 | SS (individual per reader) |
| 7 | RST (shared across all 3 readers) |

**IRQ is not used by the firmware and must not be connected or included on the connector.**

RFID cables should be kept short and local to the reader. RFID is not suitable for long field wiring runs around a layout — use additional distributed nodes for wider coverage.

---

## Power System

| Stage | Requirement |
|-------|-------------|
| Input | 9–24 V DC via screw terminal |
| Reverse polarity protection | PMOS ideal diode style preferred |
| Overcurrent | Resettable polyfuse, 1 A minimum |
| Transient protection | TVS diode across input |
| 5 V regulated | Switching buck converter; supplies sensors, outputs, 5 V logic |
| 3.3 V regulated | LDO from 5 V rail; supplies ESP32, expanders, RFID |

### Field Power Budget

| Load | Budget |
|------|--------|
| 16 sensors at ~20–50 mA each | 500–800 mA |
| Signal LEDs (16 outputs) | Designer-documented per channel and per bank |

Field sensor power and output power should be on separately protected branches where practical, so a field wiring short does not reset the ESP32.

---

## Display and User Interface

The T-Display's built-in 1.14-inch ST7789 TFT is used for:
- Boot screen
- AP / config mode display (SSID + IP)
- Normal operating status (IP, MQTT state, sensor activity)

### Config Button

| Boot action | Result |
|-------------|--------|
| Not pressed | Normal startup |
| Brief press (< 5 s) | Enter AP / config mode |
| Hold ≥ 5 s | Factory reset + AP mode |

A second button may be included for future use.

---

## Status LEDs

| LED | Type |
|-----|------|
| Power | Always-on |
| Status / activity | Firmware-controlled (heartbeat, MQTT status) |
| Fault / error | Reserved GPIO |

---

## PCB Layout Requirements

| Requirement | Detail |
|-------------|--------|
| Mounting | 4× M3 mounting holes for open standoff mounting to baseboard surface |
| Board size | Target ~100 × 100 mm; larger acceptable if needed for connectors |
| Zone separation | Dirty field input zone / output driver zone / power conversion zone / clean MCU logic zone / ESP32 antenna keep-out |
| Antenna keep-out | No copper or ground plane under ESP32 module antenna area |
| Manufacture | Designed for JLCPCB / PCBWay, parts from LCSC where practical |

No enclosure is required or supplied. Standoff mounting to a flat baseboard surface is standard practice for this application.

---

## Firmware Interface Summary

The firmware runs on the Arduino framework via PlatformIO. Config is stored in LittleFS as versioned JSON.

Each input channel is configured with: `id`, `name`, `type`, `expanderAddress`, `port/pin`, `enabled`, `electricalPolarity`, `invert`, `debounceMs`.

Each output channel is configured with: `id`, `name`, `type`, `expanderAddress`, `port/pin`, `enabled`, `defaultState`, `activeLow`, optional `signalAspect` mapping.

RFID configured with: `readerType`, `busType`, and the SPI pin assignments listed above.

MQTT topics include: sensor state, heartbeat, node status, RFID tag reads, command replies. The node subscribes to command topics for config push, ping, reboot, and output state changes.

---

## Block Occupancy Detection

Block occupancy is handled by **external dedicated detector modules** (NCE BD20, Digitrax BD4N, and similar), not by onboard current sensing. This is a confirmed architectural decision. External detectors output a clean digital signal that connects directly to the opto-isolated field inputs. There is no direct connection to the DCC track bus on the Rail I/O Node board.

---

## Future Considerations (Rev E+)

The following are recorded for planning purposes — none are Rev D requirements:

- **More channels**: Preferred scaling strategy is additional distributed nodes rather than higher channel count per board. The MCP23017 address space does allow 32+32 in a future revision if demand warrants it.
- **Servo outputs**: PWM headers for semaphore arms, level crossing barriers, turntable indexing (PCA9685 I2C driver is a candidate).
- **Audio trigger outputs**: DFPlayer Mini-compatible trigger for station announcements, crossing bells, etc.
- **DCC accessory decoder input**: Direct DCC accessory address listening without requiring a MQTT message from the layout agent.
- **Dedicated RFID node variant**: Specialised node with multiple RFID reader connectors and short local cable runs for denser read-point coverage.

---

## Full Design Specification

The complete hardware requirements specification (Rev D v3.2) used to commission the PCB design is in this directory:

[`RAIL_IO_NODE_REV_D_REQUIREMENTS.md`](./RAIL_IO_NODE_REV_D_REQUIREMENTS.md)

That document includes the full formal requirements table (MUST / SHOULD / MAY), connector and protection specifications, manufacturing guidance, and contractor deliverables. It is included here for transparency and to support anyone building a compatible or derivative board.
