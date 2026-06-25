# MavSphere Sensor Node (ESP32)

## Overview

This project is an **ESP32-based sensor node** for MavSphere layouts.

It provides:

* 16 individually opto-isolated field sensor inputs (block detectors, IR beams, reed switches, etc.)
* 16 driven output channels (signal LEDs and low-current accessories)
* Optional RFID reader support (up to 3 local RC522 readers)
* Local configuration via Wi-Fi Access Point (AP mode)
* Runtime communication via MQTT (through the **layout agent**)
* On-device display (T-Display)
* Persistent configuration using LittleFS with **versioning + backup + recovery**

The firmware targets the **Rev D Rail I/O Node PCB**. See [`docs/hardware/`](./docs/hardware/) for the hardware reference and full design specification.

---

# Architecture

```
[Sensors + RFID + Signal LEDs]
            ↓
       ESP32 Node
            ↓ (MQTT)
       Layout Agent
            ↓
          Backend
            ↓
            UI
```

### Key rule:

👉 **The node NEVER talks directly to the backend**
👉 All communication goes via the **layout agent**

---

# Features

## 1. Bootstrap / Config Mode

The node enters **AP config mode** when:

* No Wi-Fi SSID configured
* No layout-agent (MQTT host) configured
* Config button held during boot

### AP details

* SSID: `MavSphere-<nodeId>`
* Password: `configureme`
* IP: `192.168.4.1`

User connects via phone/laptop and configures:

* Wi-Fi credentials
* Layout agent IP/hostname
* MQTT settings

---

## 2. Normal Mode

When configured:

* Connects to Wi-Fi (STA mode)
* Connects to layout agent via MQTT
* Publishes:

  * heartbeat
  * node status
  * sensor events
  * RFID events

---

## 3. Network Resilience

If network drops:

* **DOES NOT enter AP mode**
* Retries Wi-Fi every 10s
* Retries MQTT every ~5s
* Automatically resumes operation when restored

---

## 4. Display (T-Display)

Shows:

* Boot screen
* AP config mode (SSID + IP)
* Normal mode (IP, MQTT status, sensors)
* Network retry states

---

## 5. Configuration System (Bulletproof)

Stored in:

```
/config.json
/config.bak.json
```

### Includes:

* Versioning (`"version": 1`)
* Validation before save
* Backup before overwrite
* Recovery from backup if corrupted
* Default regeneration if both invalid

### Load order:

1. `/config.json`
2. `/config.bak.json`
3. Defaults (then saved)

---

# Hardware

The firmware targets the **Rev D Rail I/O Node PCB**. Full details are in [`docs/hardware/HARDWARE_REFERENCE.md`](./docs/hardware/HARDWARE_REFERENCE.md).

## Supported boards

### ESP32 (working)

* LilyGO T-Display (ST7789) — **mandatory MCU module for Rev D**

### ESP32-S3 (scaffolded)

* T-Display S3 (requires correct TFT config)

---

## GPIO usage (Rev D — fixed)

### SPI buses

| Bus | Purpose | SCK | MOSI | CS | DC | RST |
|-----|---------|-----|------|----|----|-----|
| VSPI | TFT display | 18 | 19 | 5 | 16 | 23 |

| Bus | Purpose | SCK | MOSI | MISO | SS/CS |
|-----|---------|-----|------|------|-------|
| HSPI | RFID readers | 32 | 26 | 13 | See below |

### RFID (HSPI) — individual SS and shared RST

| Signal | Pin |
| ------ | --- |
| SS — Reader 1 | 17 |
| SS — Reader 2 | 25 |
| SS — Reader 3 | 2  |
| RST (shared)  | 33 |

### I2C (MCP23017 expanders)

| Signal | Pin |
|--------|-----|
| SDA | 21 |
| SCL | 22 |
| MCP23017 interrupt | 27 |

---

## Important constraints

Avoid:

* GPIO 6–11 → Flash (DO NOT USE)
* GPIO 0 → boot strap
* GPIO 1/3 → serial

---

# Build & Flash

## 1. Build firmware

```bash
pio run -e esp32_tdisplay
```

---

## 2. Upload firmware

```bash
pio run -e esp32_tdisplay -t upload
```

👉 This **does NOT erase config**

---

## 3. Upload filesystem (config)

```bash
pio run -e esp32_tdisplay -t uploadfs
```

👉 This **overwrites `/config.json`**

---

## 4. Monitor serial

```bash
pio device monitor -b 115200
```

---

# Flash Scenarios

## Safe update (normal dev)

```bash
pio run -t upload
```

✔ keeps config

---

## Full reset (wipe everything)

```bash
pio run -t erase
pio run -t upload
pio run -t uploadfs
```

✔ wipes config + firmware

---

## Reset config only

```bash
pio run -t uploadfs
```

✔ replaces config only

---

# Linux Serial Fix (IMPORTANT)

If upload fails with:

```
Permission denied /dev/ttyACM0
```

Run:

```bash
sudo usermod -aG dialout $USER
```

Then **log out and back in**

---

If port busy:

```bash
sudo systemctl stop ModemManager
sudo systemctl disable ModemManager
```

---

# Configuration File

Example:

```json
{
  "version": 1,
  "nodeId": "node01",
  "wifi": {
    "ssid": "YOUR_WIFI",
    "password": "YOUR_PASS",
    "dhcp": true,
    "hostname": "mavsphere-node"
  },
  "mqtt": {
    "enabled": true,
    "host": "192.168.1.50",
    "port": 1883,
    "topicPrefix": "layout"
  }
}
```

---

# MQTT Topics

Example:

```
layout/node01/heartbeat
layout/node01/status
layout/node01/sensor/B1_OCC
layout/node01/rfid/reader1
```

---

# Sensor Behaviour

* Debounced inputs
* Edge-triggered publishing
* No buffering (missed events during outage are lost)

---

# Boot Modes

| Condition    | Mode           |
| ------------ | -------------- |
| No config    | AP mode        |
| Button held  | AP mode        |
| Config valid | Normal mode    |
| Wi-Fi lost   | Retry (NOT AP) |

---

# Troubleshooting

## Black screen

Cause:

* TFT config mismatch

Fix:

* Ensure correct `User_Setup_TDisplay.h`
* Ensure correct SPI pins

---

## Reboot loop (WDT)

Cause:

* blocking code
* display init issues

Fix:

* comment out display init
* verify boot stability

---

## Garbage serial output

Cause:

* wrong baud

Fix:

```bash
pio device monitor -b 115200
```

---

## Stuck in AP mode

Check:

* `wifi.ssid` not empty
* `mqtt.host` not empty

---

## Config lost

Cause:

* `erase_flash`
* `uploadfs`
* partition change

---

# Recommended Next Improvements

* Offline event buffering
* Config UI validation
* OTA firmware update
* Display status states:

  * CONFIG MODE
  * WIFI RETRY
  * MQTT RETRY
  * RUNNING

---

# Summary

This node is now:

* resilient to network loss
* safe against config corruption
* easy to configure via AP
* correctly routed through layout agent
* ready for scaling across multiple layouts


---

# Factory Reset & Config Button

## Overview

The sensor node includes a **multi-function hardware button** used during boot for:

* entering configuration (AP/bootstrap mode)
* performing a full factory reset

The behaviour depends on **how long the button is held during power-on/reset**.

---

## Button Behaviour Summary

| Button Action (at boot) | Result                  |
| ----------------------- | ----------------------- |
| Not pressed             | Normal startup          |
| Pressed briefly (<5s)   | Enter AP / config mode  |
| Held ≥ 5 seconds        | Factory reset + AP mode |

---

## 1. Normal Boot (no button)

* Node loads config from LittleFS
* Connects to Wi-Fi
* Connects to layout agent (MQTT)
* Begins normal operation:

  * heartbeat
  * status
  * sensor events
  * RFID events

---

## 2. Config / Bootstrap Mode

### Trigger

* Button held at boot (released before 5 seconds), OR
* No Wi-Fi configured, OR
* No layout-agent host configured

### Behaviour

* Starts Wi-Fi Access Point:

```text
SSID: MavSphere-<nodeId>
Password: configureme
IP: 192.168.4.1
```

* Display shows:

  * AP SSID
  * IP address
  * config instructions

### Purpose

Allows user to configure:

* Wi-Fi credentials
* layout agent IP/hostname
* MQTT settings

---

## 3. Factory Reset

### Trigger

Hold the config button **for at least 5 seconds during boot**

### What happens

1. Deletes config files:

   ```text
   /config.json
   /config.bak.json
   ```

2. Regenerates default config

3. Saves fresh config + backup

4. Forces AP/bootstrap mode

---

## Result after reset

* Wi-Fi config is cleared
* MQTT / layout-agent config is cleared
* Sensors revert to default mapping
* Node enters AP config mode

---

## Default config after reset

* Wi-Fi: not configured
* MQTT host: empty
* Node ID: `node01`
* Sensors: default pins (from BoardPins)

---

## Recovery Flow (Field Use)

If a node becomes unreachable:

1. Power cycle device
2. Hold config button
3. Wait ~5 seconds
4. Release button
5. Connect to:

```text
SSID: MavSphere-node01
Password: configureme
```

6. Open:

```text
http://192.168.4.1
```

7. Reconfigure node

---

## Important Behaviour Rules

### Factory reset ONLY happens at boot

* Cannot be triggered during runtime
* Prevents accidental resets

---

### Network loss does NOT trigger reset

If Wi-Fi or MQTT drops:

* Node stays in normal mode
* Retries connection automatically
* Does NOT enter AP mode

---

## Timing Details

```text
0s → boot starts
<5s hold → config mode
≥5s hold → factory reset
```

---

## Future Enhancement (optional)

Recommended improvement:

### On-screen reset countdown

Example:

```text
Hold for reset...
5
4
3
2
1
RESETTING...
```

This improves usability in field deployments.

---

## Developer Notes

### Reset implementation

Handled in `main.cpp`:

```cpp
if (button held >= 5000ms)
  → factoryResetRequested = true
else if (button held)
  → forceConfigMode = true
```

### Reset execution

Handled in `App::begin()`:

```cpp
if (factoryResetRequested) {
  store_.factoryReset();
}
```

---

## Safety Design

This implementation ensures:

* No accidental resets during runtime
* Config corruption is recoverable
* Field devices can always be recovered without reflashing
* Backup config protects against partial writes

---

## Summary

The button provides a **safe and deterministic control mechanism**:

* Short hold → reconfigure
* Long hold → full reset
* No hold → normal operation

This makes the node suitable for unattended deployment and easy recovery in the field.

---

## Contributing

See [CONTRIBUTING.md](./CONTRIBUTING.md) for the contribution workflow and licence terms.

The sensor node firmware is MIT licensed with no restrictions.
