#pragma once

#if defined(BOARD_TDISPLAY_ESP32)
  #include "boards/Board_TDisplay_ESP32.h"
#elif defined(BOARD_TDISPLAY_S3)
  #include "boards/Board_TDisplay_S3.h"
#else
  #error "No board profile selected. Define BOARD_TDISPLAY_ESP32 or BOARD_TDISPLAY_S3 in platformio.ini"
#endif