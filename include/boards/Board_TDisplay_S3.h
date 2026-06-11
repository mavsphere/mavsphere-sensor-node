#pragma once

#define BOARD_DISPLAY_NAME "T-Display S3"

// ── Fixed onboard hardware pins (DO NOT CHANGE) ──────────────────────────────
#define BOARD_CONFIG_BUTTON_PIN      0   // Onboard button (GPIO0)
#define BOARD_TFT_BACKLIGHT_PIN     15   // S3 backlight is GPIO15, not GPIO4

// ── TFT display pins (I8080 parallel interface — NOT SPI on the S3) ──────────
// The S3 display is board-specific/parallel in this project. This S3 profile is
// still theoretical/untested for the MAVsphere sensor node.

// ── I2C bus (MCP23017 expander) ──────────────────────────────────────────────
// The S3 has a STEMMA QT/Qwiic connector for I2C as well as header pins.
#define BOARD_I2C_SDA_PIN           43
#define BOARD_I2C_SCL_PIN           44

// ── MCP23017 GPIO expander ───────────────────────────────────────────────────
#define BOARD_MCP23017_I2C_ADDR     0x20
#define BOARD_MCP23017_INTA_PIN      1

// ── RC522 RFID on dedicated SPI bus — UNTESTED/THEORETICAL on S3 ─────────────
// These values are best-effort defaults only. Confirm against the exact LilyGO
// T-Display S3 variant before relying on them. If these pins conflict on your
// board, prefer moving RFID to PN532/I2C or override the pins in config.
#define BOARD_RFID_MAX_READERS       3

#define BOARD_RFID_SCK_PIN          12
#define BOARD_RFID_MOSI_PIN         11
#define BOARD_RFID_MISO_PIN         13
#define BOARD_RFID_RST_PIN           9

#define BOARD_RFID1_SS_PIN          10
#define BOARD_RFID1_RST_PIN         BOARD_RFID_RST_PIN

#define BOARD_RFID2_SS_PIN           8
#define BOARD_RFID2_RST_PIN         BOARD_RFID_RST_PIN

#define BOARD_RFID3_SS_PIN           7
#define BOARD_RFID3_RST_PIN         BOARD_RFID_RST_PIN

// Legacy aliases for older single-reader config/code paths.
#define BOARD_RFID_SS_PIN           BOARD_RFID1_SS_PIN
