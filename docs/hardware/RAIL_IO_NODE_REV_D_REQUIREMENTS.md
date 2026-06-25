# MavSphere Rail I/O Node — Hardware Requirements Specification

**Rev D | Document Version 3.2 | May 2026**

This is the hardware requirements specification used to commission the Rev D Rail I/O Node PCB design. It is included in this repository for transparency and to support anyone building a compatible or derivative board.

For a summary of the key facts a firmware contributor or board builder needs, see [HARDWARE_REFERENCE.md](./HARDWARE_REFERENCE.md).

---

## Revision Summary

| Revision | Changes |
|----------|---------|
| Rev B | Original dedicated sensor-node: 16 opto-isolated field sensor inputs. |
| Rev C | Updated to Rail I/O Node: 16 dedicated opto-isolated sensor inputs plus 16 dedicated LED/output driver channels. Added active-HIGH/active-LOW sensor handling and clarified RFID scaling strategy. |
| Rev D | Confirmed LilyGO TTGO T-Display ESP32 as mandatory MCU module. RFID reader count increased to 3. SPI bus architecture confirmed: display on VSPI, RFID on dedicated HSPI; GPIO allocation locked. RFID connector type updated to JST-XH 2.54 mm 7-pin. v3.1: Added Section 20 Future Considerations. v3.2: Block occupancy detection via external detectors only confirmed as architectural decision (not future consideration). |

---

## Requirement Levels

| Level | Meaning |
|-------|---------|
| MUST | Mandatory. |
| SHOULD | Strongly recommended; omit only with documented justification. |
| MAY | Optional enhancement; include if cost and complexity allow. |

---

## 1. Purpose and Scope

The MavSphere Rail I/O Node Rev D is an ESP32-based distributed interface board for model railway layout automation. It replaces a breadboard/dev-board prototype and provides robust field wiring, electrical protection, sensor input isolation, and driven layout outputs suitable for signal LEDs and similar low-current accessories.

Rev D provides two separate banks:

- 16 dedicated, opto-isolated field sensor inputs.
- 16 dedicated, driven output channels for signal LEDs and other low-current layout outputs.

The electrical design keeps the input and output banks separate. The board must not rely on a single pin being both an isolated input and a driven output.

### 1.1 System Context

The node sits at the bottom of a layered architecture:

- **Backend (Spring Boot)** — owns the layout model, train roster, routes, signals, and system authority.
- **Layout Agent (Go)** — bridges backend, DCC-EX command station, MQTT sensor/output nodes, and cameras.
- **ESP32 Rail I/O Nodes** — provide physical sensor inputs and signal/accessory outputs over MQTT.
- **Local MQTT Broker** — connects nodes to the layout agent.

The current firmware is working on real hardware with TFT display, AP bootstrap, Wi-Fi, MQTT, sensors, RFID, and config push/reply. Rev D requires firmware expansion for MCP23017-based input and output banks.

### 1.2 Operating Environment

The board operates in a model railway / DCC environment. The design must prioritise robustness and noise immunity over minimal cost.

- Electrically noisy DCC track signals and accessory wiring.
- Field wiring runs of up to approximately 4 metres between sensors and the node board.
- Mixed sensor types: dry contact, reed switch, relay contact, open collector/open drain, active-HIGH, and active-LOW.
- Multiple LED signal heads, typically common-anode, with red/yellow/green aspects.
- Installation under or beside layout baseboards, potentially in enclosed spaces.

---

## 2. MCU and Processing

| ID | Level | Requirement |
|----|-------|-------------|
| MCU-01 | MUST | Use a certified ESP32 module (e.g. ESP32-WROOM-32 or ESP32-WROVER). Must not require custom RF design. |
| MCU-02 | MUST | Support Wi-Fi 2.4 GHz for MQTT communication to the local broker. |
| MCU-03 | MUST | Provide UART programming/debug access via header. |
| MCU-04 | MUST | Provide I2C bus for MCP23017 GPIO expanders and optional I2C peripherals. |
| MCU-05 | MUST | Provide two independent SPI buses: VSPI for the TFT display (GPIO 5/18/19/23) and HSPI for RFID readers (GPIO 32/26/13). Bus allocation is fixed by firmware; the PCB must route accordingly. No SPI bus sharing between display and RFID is required or permitted. |
| MCU-06 | MUST | Support interrupt-capable GPIO pins for expander interrupt lines. |
| MCU-07 | SHOULD | Support OTA firmware updates with suitable flash size and partition scheme. |
| MCU-08 | MUST | Include a socket/header compatible with the LilyGO TTGO T-Display ESP32 (1.14-inch ST7789 display, standard 2-row pin header). The T-Display is the mandatory MCU module; existing firmware targets this module and its pin assignments are fixed. An equivalent substitute is not acceptable without explicit approval. |

---

## 3. I/O Architecture

Rev D uses dedicated input and output banks rather than a universal mixed-channel circuit. This reduces miswiring risk, simplifies protection, and makes the board easier to manufacture and support.

| ID | Level | Requirement |
|----|-------|-------------|
| IO-01 | MUST | Provide 16 dedicated field sensor input channels. |
| IO-02 | MUST | Provide 16 dedicated driven output channels. |
| IO-03 | MUST | One 3-aspect LED signal consumes three output channels: red, yellow, and green. |
| IO-04 | MUST | Inputs and outputs must be clearly separated and labelled on the PCB silkscreen. |
| IO-05 | SHOULD | The firmware/UI should expose the node as having 16 input channels and 16 output channels, each with configurable semantic purpose. |
| IO-06 | SHOULD | Use 2× MCP23017 minimum: one expander for the 16 inputs and one for the 16 outputs. |

---

## 4. Input System

### 4.1 Input Capacity and Isolation

| ID | Level | Requirement |
|----|-------|-------------|
| INP-01 | MUST | Provide 16 field sensor inputs minimum. |
| INP-02 | MUST | All 16 field inputs must be individually opto-isolated, using PC817 or a more suitable equivalent. |
| INP-03 | MUST | Isolation must protect the MCU from voltage differences, noise, and ground loops from field wiring. |
| INP-04 | MUST | Each input channel must include a current-limiting resistor on the optocoupler LED side. |
| INP-05 | MUST | Each input channel must support active-LOW/sinking sensor outputs and active-HIGH/sourcing sensor outputs, either by a universal bidirectional input circuit or per-channel polarity selection. |
| INP-06 | MUST | Firmware must support per-input logical inversion so that occupied/triggered state can be normalised regardless of electrical polarity. |
| INP-07 | SHOULD | Each input channel should include optional RC filtering, populated or DNP as needed. |
| INP-08 | SHOULD | Input stage should tolerate cable lengths up to approximately 4 metres. |
| INP-09 | SHOULD | Include ESD/TVS protection on external field input connectors. |

### 4.2 Active-HIGH and Active-LOW Sensor Handling

The board must handle sensors that drive their signal either HIGH or LOW. This is an electrical requirement, not only a firmware inversion requirement.

| Sensor behaviour | Electrical meaning | Required board support |
|------------------|--------------------|------------------------|
| Active-LOW / sinking output | Sensor pulls SIG to GND when active. Common for open collector/open drain. | Input circuit must allow current from +5V_FIELD through optocoupler LED/resistor into the sensor output. |
| Active-HIGH / sourcing output | Sensor drives SIG to +5V or another valid field voltage when active. | Input circuit must allow current from sensor SIG through optocoupler LED/resistor to GND_FIELD. |
| Dry contact / reed / relay | Passive contact closes between two terminals. | Board must provide safe wetting current through the optocoupler input. |
| Externally powered digital output | Sensor has its own supply and returns a digital signal. | Input must tolerate the specified signal voltage and maintain isolation. |

Acceptable implementation approaches:

- **Preferred**: a universal isolated input circuit that works for both active-HIGH and active-LOW without user jumpers.
- **Acceptable**: per-channel jumper, solder bridge, or DIP switch selecting SOURCE vs SINK mode.
- **Acceptable**: two clearly documented input classes if a single universal circuit compromises robustness.
- **Not acceptable**: relying only on firmware inversion when the optocoupler input circuit can only be electrically triggered by one polarity.

### 4.3 Input Types Supported

| ID | Level | Requirement |
|----|-------|-------------|
| INP-10 | MUST | Support dry contact / switch closure inputs (reed switches, microswitches, relay contacts). |
| INP-11 | MUST | Support open collector / open drain sensor outputs, common on block occupancy detectors. |
| INP-12 | MUST | Support externally powered digital outputs where the sensor has its own supply. |
| INP-13 | MUST | Support 5 V powered sensor outputs referenced to the board-supplied 5 V field rail. |
| INP-14 | SHOULD | Support 12 V signal inputs via appropriate conditioning or documented jumper/resistor configuration. |
| INP-15 | MAY | If a single universal input circuit cannot cleanly handle all types, the designer may propose two input classes. |

### 4.4 Field Input Connectors

| ID | Level | Requirement |
|----|-------|-------------|
| CON-IN-01 | MUST | Each field input must have a screw terminal, pluggable screw terminal, or similarly serviceable connector on the field-wiring side. |
| CON-IN-02 | MUST | Each input connector must provide 3 pins: GND_FIELD, +5V_FIELD, SIG_IN. |
| CON-IN-03 | MUST | Connectors must be clearly labelled with channel number and pin functions on PCB silkscreen. |
| CON-IN-04 | SHOULD | Use pluggable screw terminal blocks, 3.5 mm or 5.08 mm pitch, so field wiring can be disconnected without a soldering iron. |
| CON-IN-05 | MAY | Group input connectors into banks (e.g. 2 banks of 8) if this improves PCB layout. |

The 3-pin arrangement allows sensors such as IR break beams to be powered and read over one cable run back to the node.

---

## 5. Output System

### 5.1 Output Capacity and Driver Type

| ID | Level | Requirement |
|----|-------|-------------|
| OUT-01 | MUST | Provide 16 driven output channels minimum. |
| OUT-02 | MUST | Outputs must be suitable for railway signal LEDs and similar low-current accessory outputs. |
| OUT-03 | MUST | Outputs must not be driven directly from ESP32 GPIO. Use MCP23017 outputs with appropriate driver ICs or transistor/MOSFET drivers. |
| OUT-04 | SHOULD | Use low-side switching outputs suitable for common-anode LED signal heads. ULN2803A or equivalent 8-channel low-side driver is acceptable. |
| OUT-05 | SHOULD | Support 5 V LED/signal power as standard. |
| OUT-06 | MAY | Support 12 V LED/accessory output power if practical and clearly documented. |
| OUT-07 | MUST | Output current limits must be documented per channel and per bank. |

### 5.2 LED Current Limiting

| ID | Level | Requirement |
|----|-------|-------------|
| LED-OUT-01 | MUST | Every LED output path must include current limiting — either onboard resistor footprints or a clearly specified requirement for external resistors. |
| LED-OUT-02 | SHOULD | Provide resistor footprints per output channel so common model railway signal LEDs can be wired directly without extra inline resistors. |
| LED-OUT-03 | SHOULD | Default fitted LED resistor value should target realistic model signal brightness. 1 kΩ per LED is a suitable starting point for 5 V supply, subject to designer validation. |
| LED-OUT-04 | MUST | PCB silkscreen and documentation must state whether output channels include onboard resistors. |

### 5.3 Output Connectors

| ID | Level | Requirement |
|----|-------|-------------|
| CON-OUT-01 | MUST | Provide serviceable output connectors for 16 output channels. |
| CON-OUT-02 | MUST | Output connectors must be clearly labelled with output channel numbers and polarity. |
| CON-OUT-03 | SHOULD | Provide grouped signal-head connectors where practical (e.g. +5V common, RED, YELLOW, GREEN). |
| CON-OUT-04 | SHOULD | Also provide individual output channel access or a clearly documented mapping from grouped signal connectors to output channel numbers. |
| CON-OUT-05 | MAY | Group outputs into banks of 8 or signal-head groups depending on board layout. |

### 5.4 Example Signal Wiring

For common-anode 3-aspect signal heads, the preferred output model is low-side switching:

- +5V_FIELD or +5V_SIGNAL connects to the common anode of the signal head.
- Red cathode connects through a current-limiting resistor to output channel N.
- Yellow cathode connects through a current-limiting resistor to output channel N+1.
- Green cathode connects through a current-limiting resistor to output channel N+2.
- The output driver pulls the selected channel LOW to turn that LED on.

---

## 6. Sensor Support

### 6.1 Block Occupancy Detectors

| ID | Level | Requirement |
|----|-------|-------------|
| SEN-01 | MUST | Support connection to block detectors with dry contact, relay, open collector/open drain, active-LOW, or active-HIGH outputs. |
| SEN-02 | SHOULD | Allow detectors that require external power to be supplied from the board 5 V field rail where compatible. |
| SEN-03 | SHOULD | Not assume all detectors are passive or track-powered. Some may need 5 V or 12 V. |
| SEN-04 | MUST | Block detector signal inputs must pass through the isolated input stage, not direct to MCU or expander GPIO. |

### 6.2 IR Break Beam Sensors

| ID | Level | Requirement |
|----|-------|-------------|
| SEN-10 | MUST | Support IR break beam sensors via the standard 3-pin field connector: GND_FIELD, +5V_FIELD, SIG_IN. |
| SEN-11 | MUST | Beam sensor signals must pass through the opto-isolated input stage, not direct to GPIO. |
| SEN-12 | MUST | The board 5 V field supply must support up to 16 sensors at approximately 20 mA per sensor pair. |
| SEN-13 | MUST | Support either active-HIGH or active-LOW beam receiver outputs by hardware polarity handling and firmware inversion. |

### 6.3 Generic Digital Inputs

| ID | Level | Requirement |
|----|-------|-------------|
| SEN-20 | MUST | Support generic digital inputs: reed switches, microswitches, push buttons, relay outputs. |
| SEN-21 | SHOULD | Internal firmware representation should normalise triggered/occupied state regardless of electrical polarity. |
| SEN-22 | SHOULD | Input debounce should be firmware-configurable per channel. |

---

## 7. RFID Support

RFID is used for train identification and location confirmation. RFID readers are local to the node with short cable runs. SPI and I2C are not suitable for multi-metre field wiring around a layout without additional bus engineering.

The RFID SPI bus allocation and GPIO pin assignment is confirmed and fixed:

- HSPI bus: SCK=GPIO32, MOSI=GPIO26, MISO=GPIO13, shared RST=GPIO33
- Individual SS lines: Reader 1 SS=GPIO17, Reader 2 SS=GPIO25, Reader 3 SS=GPIO2
- T-Display VSPI bus (GPIO 5/18/19/23) is entirely separate
- MCP23017 interrupt line: GPIO27
- I2C bus: GPIO21/22

| ID | Level | Requirement |
|----|-------|-------------|
| RFID-01 | MUST | Provide connector support for exactly 3 local RFID reader modules. MFRC522 (RC522) is the confirmed reader type. PN532 using the same SPI pinout is also acceptable. |
| RFID-02 | MUST | RFID connectors must be JST-XH 2.54 mm pitch, 7-pin, one per reader. |
| RFID-03 | MUST | RFID connections are local only. Not intended for long field runs. |
| RFID-04 | MUST | Each RFID connector must provide 7 pins in this order: VCC 3.3 V, GND, SCK, MOSI, MISO, SS (individual per reader), RST (shared). IRQ is not required and must not be included on the connector. |
| RFID-05 | MUST | Support all 3 RFID readers simultaneously on HSPI with individual SS lines and shared RST. Confirmed GPIO: SCK=32, MOSI=26, MISO=13, RST=33, SS1=17, SS2=25, SS3=2. |
| RFID-06 | SHOULD | PCB routing must respect the confirmed bus allocation. No bus sharing between display and RFID. |
| RFID-07 | MUST | RST pin must be connected (shared line, GPIO 33). IRQ pin is not used and must not be connected. |

### 7.1 RFID Scaling Strategy

For more than 3 local RFID read points, a dedicated RFID node or multiple distributed nodes is preferred over extending this board with long SPI/I2C cables.

| ID | Level | Requirement |
|----|-------|-------------|
| RFID-S-01 | SHOULD | For more than 1–2 local RFID readers per physical location, use a specialised RFID node or multiple small distributed nodes. |
| RFID-S-02 | SHOULD | RFID reader cable length should remain short and local. The node reports reads over MQTT/Wi-Fi. |
| RFID-S-03 | SHOULD | A future RFID Node variant may provide dedicated reader connectors, reader power management, and mounting provisions close to track/tag locations. |
| RFID-S-04 | MAY | Consider RS-485 or CAN only if Wi-Fi/MQTT is unsuitable for a specific installation. |

---

## 8. I/O Expansion

| ID | Level | Requirement |
|----|-------|-------------|
| EXP-01 | MUST | Use I2C GPIO expanders (MCP23017) rather than consuming most ESP32 GPIO directly. |
| EXP-02 | MUST | Support 16 isolated inputs via the expander system. |
| EXP-03 | MUST | Support 16 driven outputs via the expander system and driver stage. |
| EXP-04 | MUST | Expander interrupt outputs must be connected to ESP32 interrupt-capable GPIO pins. |
| EXP-05 | MUST | Include I2C address configuration (solder jumpers or DIP switch) for each expander. |
| EXP-06 | SHOULD | Use 2× MCP23017: one for the input bank, one for the output bank. |
| EXP-07 | SHOULD | Route both INTA and INTB from each expander to the ESP32 where practical. |
| EXP-08 | SHOULD | Include I2C pull-ups on the board, 4.7 kΩ typical. Only one set per bus. |
| EXP-09 | MAY | Provide a future expansion header for additional I2C devices. |

---

## 9. Power System

### 9.1 Input Power

| ID | Level | Requirement |
|----|-------|-------------|
| PWR-01 | MUST | Accept 9–24 V DC input via screw terminal connector. |
| PWR-02 | MUST | Include reverse polarity protection. PMOS ideal diode style preferred; Schottky acceptable for prototype. |
| PWR-03 | MUST | Include overcurrent protection — resettable polyfuse 1 A minimum. |
| PWR-04 | MUST | Include input transient protection (TVS diode across input). |

### 9.2 Regulation

| ID | Level | Requirement |
|----|-------|-------------|
| PWR-10 | MUST | Generate a regulated 5 V rail for field sensor power, LED signal power, and 5 V logic/accessories. |
| PWR-11 | MUST | Generate a regulated 3.3 V rail for ESP32, expanders, RFID modules, and 3.3 V logic. |
| PWR-12 | SHOULD | Use a switching buck converter for the 5 V stage for efficiency at higher input voltages. |
| PWR-13 | SHOULD | Use a low-noise LDO for the 3.3 V stage fed from the 5 V rail, subject to thermal validation. |
| PWR-14 | MUST | Include adequate decoupling capacitors near all IC power pins. |

### 9.3 Field Sensor and Output Power

| ID | Level | Requirement |
|----|-------|-------------|
| PWR-20 | MUST | Provide a protected 5 V field supply rail distributed to all 16 input connectors for powering sensors. |
| PWR-21 | MUST | Field 5 V rail must support up to 16 sensors at approximately 20–50 mA each (budget 500–800 mA). |
| PWR-22 | MUST | Provide power capacity for output LEDs/signals in addition to field sensors. Designer must document assumed LED current and total output-bank current. |
| PWR-23 | SHOULD | Include short-circuit protection on field 5 V and signal/output 5 V rails. |
| PWR-24 | SHOULD | Field power should be separated into protected branches so a field wiring short does not reset the ESP32. |
| PWR-25 | MAY | Provide selectable 3.3 V field power option by jumper for sensors that require 3.3 V. Default should remain 5 V. |

---

## 10. Display and User Interface

| ID | Level | Requirement |
|----|-------|-------------|
| DIS-01 | SHOULD | Include a connector or header for the TTGO T-Display 1.14-inch 135×240 TFT (or compatible). |
| DIS-02 | SHOULD | Include a user button for entering AP/config mode (short press) and factory reset (long press ≥ 5 s). |
| DIS-03 | MAY | Include a second button for future use. |

---

## 11. Communications and Debug

| ID | Level | Requirement |
|----|-------|-------------|
| COM-01 | MUST | Primary communication is Wi-Fi via ESP32, using MQTT over Wi-Fi to the local broker. |
| COM-02 | MUST | Include UART programming/debug header: TX, RX, GND, 3.3 V, EN, IO0. |
| COM-03 | MUST | Include Reset and Boot buttons or header access to EN and GPIO0. |
| COM-04 | SHOULD | Headers should be labelled with clear silkscreen markings. |
| COM-05 | MAY | Include provision for RS-485 or CAN transceiver in a future revision. Route spare GPIO if practical but do not populate unless required. |

---

## 12. Status Indicators

| ID | Level | Requirement |
|----|-------|-------------|
| LED-01 | MUST | Include a power LED. |
| LED-02 | MUST | Include a firmware-controllable status/activity LED for heartbeat, MQTT status, etc. |
| LED-03 | SHOULD | Include a network/Wi-Fi status LED or use the status LED with defined blink patterns. |
| LED-04 | SHOULD | Include a fault/error LED or reserved GPIO for one. |
| LED-05 | MAY | Include per-input activity LEDs or grouped diagnostic indication. |
| LED-06 | MAY | Include per-output diagnostic LEDs only if they do not confuse actual signal output state and do not significantly increase cost/board size. |

---

## 13. Mechanical and PCB Layout

| ID | Level | Requirement |
|----|-------|-------------|
| PCB-01 | MUST | Include 4× M3 mounting holes for enclosure or baseboard mounting. |
| PCB-02 | MUST | PCB layout must clearly separate: dirty field input zone, output driver zone, power conversion zone, clean logic zone, and ESP32 antenna keep-out zone. |
| PCB-03 | MUST | Respect ESP32 module antenna keep-out area — no copper/ground under antenna as required by module guidance. |
| PCB-04 | MUST | Use a solid ground plane where compatible with isolation boundaries. |
| PCB-05 | SHOULD | Target board size approximately 100 × 100 mm; larger is acceptable if needed for serviceable wiring. |
| PCB-06 | SHOULD | Use 4-layer PCB if it significantly improves noise, power, or RF performance. 2-layer is acceptable if well designed. |
| PCB-07 | SHOULD | All connectors should be clearly labelled with channel numbers and pin functions. |
| PCB-08 | SHOULD | Board should be designed for standard PCB assembly — JLCPCB/PCBWay compatible. |
| PCB-09 | MAY | Include board version and project name on silkscreen. |
| PCB-10 | MUST | Board must be designed for open standoff mounting. No enclosure is required or supplied. The four M3 mounting holes allow the board to be mounted on customer-supplied standoffs to any flat baseboard surface. |
| PCB-11 | MAY | If the board is ever sold as a product subject to CE/UKCA marking, a cover panel or DIN rail mounting option should be considered. This is a future product decision; not required for Rev D. |

---

## 14. Reliability and Protection

| ID | Level | Requirement |
|----|-------|-------------|
| REL-01 | MUST | All external connectors must have appropriate ESD/TVS protection. |
| REL-02 | MUST | Input stage must tolerate reasonable field miswiring. Reversed polarity on a sensor connection should not damage the board. |
| REL-03 | MUST | Output stage must tolerate LED wiring mistakes as far as practical, including short-circuit protection at bank/rail level. |
| REL-04 | MUST | Design must tolerate the noisy DCC electrical environment without false sensor triggers. |
| REL-05 | SHOULD | Include adequate input filtering to prevent noise-induced false triggers on long cable runs. |
| REL-06 | SHOULD | Power stage should handle brief input overvoltage events gracefully. |
| REL-07 | SHOULD | The design should fail safe: loss of MCU control should not randomly energise signal outputs. |

---

## 15. Firmware Interface Notes

- Firmware is Arduino-framework on ESP32 using PlatformIO.
- Config is stored in LittleFS with versioned JSON format.
- Inputs configured per channel with: `id`, `name`, `type`, `expanderAddress`, `port/pin`, `enabled`, `electricalPolarity`/`sourceSinkMode`, `invert`, `debounceMs`.
- Outputs configured per channel with: `id`, `name`, `type`, `expanderAddress`, `port/pin`, `enabled`, `defaultState`, `activeLow`, optional `signalAspect` mapping.
- RFID configured with: reader type, bus type, `ssPin`/`rstPin`/`sckPin`/`misoPin`/`mosiPin`.
- MCP23017 support must include interrupt handling for input changes.
- MQTT topics include: sensor state, heartbeat, status, RFID tag reads, and command replies.
- Node subscribes to command topics for: config push, ping, reboot, and output state changes.
- AP/bootstrap mode triggered by boot button; factory reset by long press (≥ 5 s).

---

## 16. Open Design Decisions

### 16.1 Input Polarity Implementation

The PCB designer must propose how active-HIGH and active-LOW field sensors will be supported. Acceptable approaches are described in Section 4.2. The selected approach must be robust and easy to install.

### 16.2 Output Driver Selection

The designer must propose the output driver approach. ULN2803A-style low-side drivers are acceptable for LED signal outputs. MOSFET drivers may be preferable for higher current or lower voltage drop requirements.

### 16.3 Connector Strategy

The designer should propose whether outputs are best exposed as individual channels, grouped signal-head connectors, or both. Serviceability and clear field wiring must be prioritised.

### 16.4 SPI Bus Allocation (Resolved)

This decision is closed. The firmware uses two independent SPI peripherals: VSPI for the T-Display (GPIO 5/18/19/23) and HSPI for the three RFID readers (GPIO 32/26/13). This has been validated on the working breadboard prototype with all three readers and the display operating simultaneously. The PCB must route to these specific GPIOs.

### 16.5 Field Power Strategy

The designer should decide whether sensor and output power share one protected 5 V rail or are split into banks. Banked power is preferred if it materially improves resilience without excessive complexity.

---

## 17. PCB Deliverables

| ID | Level | Requirement |
|----|-------|-------------|
| DEL-01 | MUST | Complete KiCad schematic files. |
| DEL-02 | MUST | PCB layout ready for manufacture. |
| DEL-03 | MUST | Bill of Materials with supplier part numbers (LCSC/JLCPCB preferred). |
| DEL-04 | MUST | Gerber files for PCB fabrication. |
| DEL-05 | MUST | Pick and place files for assembly. |
| DEL-06 | SHOULD | Design notes documenting all decisions and trade-offs, especially input polarity and output driver choices. |
| DEL-07 | SHOULD | Assembly drawing / reference designator map. |
| DEL-08 | MAY | 3D model of the assembled board. |

---

## 18. Budget and Manufacturing Guidance

Target unit cost for assembled boards:

| Batch size | Target unit cost |
|------------|-----------------|
| Prototype, 5–10 units | GBP 30–60 per board |
| Small batch, 50–100 units | GBP 18–35 per board |

Design should target JLCPCB or PCBWay for prototype manufacturing with parts sourced primarily from LCSC where practical.

---

## 19. Critical Design Note

This board operates in a noisy model railway / DCC environment with potentially long field wiring runs and mixed sensor/output types. All field sensor inputs **must** be opto-isolated. Output channels **must** be properly driven and current-limited. PCB layout **must** separate noisy input wiring, output drivers, power conversion, and clean MCU logic. The design must prioritise reliability, protection, serviceability, and clean field installation over minimalism.

---

## 20. Future Considerations (Rev E and Beyond)

None of these are requirements for Rev D. They are recorded to inform future revision planning and to ensure Rev D does not inadvertently close off these options.

### 20.1 I/O Channel Count — More Channels vs More Nodes

The preferred scaling strategy is additional distributed nodes rather than higher channel count per board. 16+16 is sufficient for a substantial layout section (a typical station throat, fiddle yard, or scenic section). Multiple smaller nodes mean shorter field cable runs, improving noise immunity. The MCP23017 I2C address space supports up to 8 expanders, so 32+32 channels is achievable in a future revision without changing the fundamental architecture if demand warrants it.

### 20.2 DCC Accessory Decoder Input

A future revision could include the ability to listen to DCC accessory addresses directly on the track bus, allowing the node to react to DCC commands without a MQTT message from the layout agent.

### 20.3 Servo Outputs

PWM-capable servo output headers (3-pin: GND, 5 V, PWM signal) driven from spare ESP32 PWM-capable GPIO or a PCA9685 I2C PWM driver would enable semaphore signal arms, level crossing barriers, and turntable indexing. Considered a strong candidate for a future specialist variant or Rev E.

### 20.4 Audio Trigger Outputs

A trigger output compatible with modules such as the DFPlayer Mini (UART or simple trigger input) would allow the node to fire audio cues (station announcements, crossing bells, locomotive whistle sounds) in response to sensor events or MQTT commands.

### 20.5 Block Occupancy Detection — Architectural Decision

Block occupancy detection is handled by **external dedicated detector modules** rather than onboard current sensing circuitry. This is a confirmed architectural decision. External detectors (NCE BD20, Digitrax BD4N, and similar) are widely owned by the target market, proven reliable, and output a clean digital signal that connects directly to the opto-isolated field inputs. This approach keeps the I/O node board focused, avoids any direct connection to the DCC track bus, and gives users freedom to choose whichever detector suits their layout. Onboard current sensing will not be added to the general-purpose Rail I/O Node in any future revision. A dedicated occupancy detector node may be considered as a separate product if demand warrants it.
