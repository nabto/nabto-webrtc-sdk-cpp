#pragma once

#include <random>
#include <sstream>

namespace nabto {
namespace webrtc {
namespace util {

/**
 * Generate a UUID v4 string using the std random generator.
 */
static std::string generate_uuid_v4() {
  std::random_device rd;
  std::mt19937 gen(rd());
  // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
  std::uniform_int_distribution<> dis(0, 15);
  // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
  std::uniform_int_distribution<> dis2(8, 11);

  std::stringstream ss;
  ss << std::hex;
  // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
  for (int i = 0; i < 8; i++) {
    ss << dis(gen);
  }
  ss << "-";
  // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
  for (int i = 0; i < 4; i++) {
    ss << dis(gen);
  }
  ss << "-4";
  // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
  for (int i = 0; i < 3; i++) {
    ss << dis(gen);
  }
  ss << "-";
  ss << dis2(gen);
  // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
  for (int i = 0; i < 3; i++) {
    ss << dis(gen);
  }
  ss << "-";
  // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
  for (int i = 0; i < 12; i++) {
    ss << dis(gen);
  };
  return ss.str();
}

}  // namespace util
}  // namespace webrtc
}  // namespace nabto
