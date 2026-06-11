#include "Logger.h"
#include <ArduinoJson.h>

std::vector<String> Logger::lines_;

void Logger::append(const String& line) {
  lines_.push_back(line);
  if ((int)lines_.size() > MAX_LOG_LINES) {
    lines_.erase(lines_.begin());
  }
}

void Logger::info(const String& msg) {
  String line = "[INFO] " + msg;
  Serial.println(line);
  append(line);
}

void Logger::warn(const String& msg) {
  String line = "[WARN] " + msg;
  Serial.println(line);
  append(line);
}

void Logger::error(const String& msg) {
  String line = "[ERROR] " + msg;
  Serial.println(line);
  append(line);
}

String Logger::getLogsJson() {
  JsonDocument doc;
  auto arr = doc.to<JsonArray>();
  for (const auto& l : lines_) {
    arr.add(l);
  }
  String out;
  serializeJson(doc, out);
  return out;
}
