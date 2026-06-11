#pragma once
#include <Arduino.h>
#include <vector>

class Logger {
public:
  static constexpr int MAX_LOG_LINES = 40;

  static void info(const String& msg);
  static void warn(const String& msg);
  static void error(const String& msg);

  static String getLogsJson();

private:
  static void append(const String& line);
  static std::vector<String> lines_;
};
