#include <Arduino.h>
#include "App.h"
#include "BoardPins.h"

App app;

static constexpr unsigned long FACTORY_RESET_HOLD_MS = 5000;

void setup() {
  pinMode(BOARD_CONFIG_BUTTON_PIN, INPUT_PULLUP);

  bool forceConfigMode = false;
  bool factoryResetRequested = false;

  if (digitalRead(BOARD_CONFIG_BUTTON_PIN) == LOW) {
    unsigned long startMs = millis();

    while (digitalRead(BOARD_CONFIG_BUTTON_PIN) == LOW) {
      delay(20);

      if (millis() - startMs >= FACTORY_RESET_HOLD_MS) {
        factoryResetRequested = true;
        break;
      }
    }

    if (!factoryResetRequested) {
      forceConfigMode = true;
    }
  }

  app.begin(forceConfigMode, factoryResetRequested);
}

void loop() {
  app.loop();
  delay(5);
}