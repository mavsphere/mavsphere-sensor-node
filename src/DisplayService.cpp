#include "DisplayService.h"
#include "BoardPins.h"

void DisplayService::begin() {
  pinMode(BOARD_TFT_BACKLIGHT_PIN, OUTPUT);
  digitalWrite(BOARD_TFT_BACKLIGHT_PIN, HIGH);

  display_.init();
  display_.setRotation(1);
  display_.fillScreen(TFT_BLACK);
  display_.setTextColor(TFT_WHITE, TFT_BLACK);
}

void DisplayService::showBoot() {
  display_.fillScreen(TFT_BLACK);

  display_.setFreeFont(&FreeSansBold12pt7b);
  display_.setTextColor(TFT_GREEN, TFT_BLACK);

  int16_t xCenter = (display_.width() - display_.textWidth("MavSphere")) / 2;
  display_.setCursor(xCenter, 24);
  display_.println("MavSphere");

  xCenter = (display_.width() - display_.textWidth("Sensor Node")) / 2;
  display_.setCursor(xCenter, 52);
  display_.println("Sensor Node");

  display_.setFreeFont(&FreeSans9pt7b);
  display_.setTextColor(TFT_WHITE, TFT_BLACK);
  display_.setCursor(10, 90);
  display_.println("Initializing...");
  display_.setCursor(10, 118);
  display_.println(MAVSPHERE_SENSOR_NODE_VERSION);

  display_.drawRect(0, 0, display_.width(), display_.height(), TFT_GREEN);
  display_.drawLine(0, display_.height() - 2, display_.width(), display_.height() - 2, TFT_RED);
}

void DisplayService::showConfig(const NodeConfig& config, const NodeStatus& status) {
  display_.fillScreen(TFT_BLACK);

  display_.setFreeFont(&FreeSansBold12pt7b);
  display_.setTextColor(TFT_GREEN, TFT_BLACK);

  int16_t xCenter = (display_.width() - display_.textWidth("AP Mode: Configure")) / 2;
  display_.setCursor(xCenter, 24);
  display_.println("AP Mode: Configure");

  display_.setFreeFont(&FreeSans9pt7b);
  display_.setTextColor(TFT_WHITE, TFT_BLACK);

  display_.setCursor(10, 52);
  display_.println("SSID:");
  display_.setCursor(70, 52);
  display_.println("MavSphere-" + config.nodeId);

  display_.setCursor(10, 82);
  display_.println("PW:");
  display_.setCursor(70, 82);
  display_.println("configureme");

  display_.setCursor(10, 112);
  display_.println("IP:");
  display_.setCursor(70, 112);
  display_.println(status.ipAddress);

  display_.drawRect(0, 0, display_.width(), display_.height(), TFT_GREEN);
  display_.drawLine(0, display_.height() - 2, display_.width(), display_.height() - 2, TFT_RED);
}

void DisplayService::showNormal(const NodeConfig& config, const NodeStatus& status) {
  display_.fillScreen(TFT_BLACK);

  display_.setFreeFont(&FreeSansBold12pt7b);
  display_.setTextColor(TFT_GREEN, TFT_BLACK);

  int16_t xCenter = (display_.width() - display_.textWidth("MavSphere")) / 2;
  display_.setCursor(xCenter, 24);
  display_.println("MavSphere");

  display_.setFreeFont(&FreeSans9pt7b);
  display_.setTextColor(TFT_WHITE, TFT_BLACK);

  display_.setCursor(10, 52);
  display_.println("Mode:");
  display_.setCursor(100, 52);
  display_.println("sensor");

  display_.setCursor(10, 82);
  display_.println("IP:");
  display_.setCursor(100, 82);
  display_.println(status.ipAddress);

  display_.setCursor(10, 112);
  display_.println("Agent:");
  display_.setCursor(100, 112);
  display_.println(config.mqtt.host);

  display_.setCursor(10, 142);
  display_.println("MQTT:");
  display_.setCursor(100, 142);
  display_.println(status.mqttConnected ? "OK" : "OFF");

  display_.drawRect(0, 0, display_.width(), display_.height(), TFT_GREEN);
  display_.drawLine(0, display_.height() - 2, display_.width(), display_.height() - 2, TFT_RED);
}