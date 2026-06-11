#pragma once

#define BOARD_DISPLAY_NAME "T-Display ESP32"

// ── Fixed onboard hardware pins (DO NOT CHANGE) ──────────────────────────────
#define BOARD_CONFIG_BUTTON_PIN     35   // Onboard button (input-only)
#define BOARD_TFT_BACKLIGHT_PIN      4   // TFT backlight

// ── TFT display SPI pins (fixed by hardware — DO NOT USE for anything else) ──
// GPIO  4  = TFT_BL   (backlight PWM)
// GPIO  5  = TFT_CS
// GPIO 16  = TFT_DC
// GPIO 18  = TFT_SCLK
// GPIO 19  = TFT_MOSI
// GPIO 23  = TFT_RST

// ── I2C bus (MCP23017 expander) ──────────────────────────────────────────────
#define BOARD_I2C_SDA_PIN           21
#define BOARD_I2C_SCL_PIN           22

// ── MCP23017 GPIO expander ───────────────────────────────────────────────────
// Wire A0, A1, A2 all to GND  ->  I2C address = 0x20
// Wire RESET to 3.3V
#define BOARD_MCP23017_I2C_ADDR     0x20
#define BOARD_MCP23017_INTA_PIN     27   // Exposed on header; reserved for MCP interrupt

// ── RC522 RFID on dedicated HSPI bus ─────────────────────────────────────────
// These pins are deliberately separate from the display SPI pins above.
// All RC522 readers share SCK/MOSI/MISO and each reader gets its own SS/CS.
// RST is shared by default to save GPIOs.
#define BOARD_RFID_MAX_READERS       3

#define BOARD_RFID_SCK_PIN          32
#define BOARD_RFID_MOSI_PIN         26
#define BOARD_RFID_MISO_PIN         13
#define BOARD_RFID_RST_PIN          33

#define BOARD_RFID1_SS_PIN          17
#define BOARD_RFID1_RST_PIN         BOARD_RFID_RST_PIN

#define BOARD_RFID2_SS_PIN          25
#define BOARD_RFID2_RST_PIN         BOARD_RFID_RST_PIN

#define BOARD_RFID3_SS_PIN           2
#define BOARD_RFID3_RST_PIN         BOARD_RFID_RST_PIN

// Legacy aliases for older single-reader config/code paths.
#define BOARD_RFID_SS_PIN           BOARD_RFID1_SS_PIN
