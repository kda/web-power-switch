#include "trim.h"

static const char WHITESPACE[] = " \r\n\t\v";

std::string ltrim(const std::string& input) {
  auto front = input.find_first_not_of(WHITESPACE);
  if (front != std::string::npos) {
    return input.substr(front);
  }
  return input;
}

std::string rtrim(const std::string& input) {
  auto back = input.find_last_not_of(WHITESPACE);
  if (back != std::string::npos) {
    return input.substr(0, back + 1);
  }
  return input;
}

std::string trim(const std::string& input) {
  return ltrim(rtrim(input));
}
