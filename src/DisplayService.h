#pragma once

#include <Arduino.h>
#include <TFT_eSPI.h>
#include "ConfigTypes.h"

class DisplayService {
public:
  void begin();
  void showBoot();
  void showConfig(const NodeConfig& config, const NodeStatus& status);
  void showNormal(const NodeConfig& config, const NodeStatus& status);

private:
  TFT_eSPI display_;
};