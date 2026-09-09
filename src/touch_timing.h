#pragma once
#include <stdint.h>

namespace TouchTiming {
// Bridge short gaps between controller reports without depending on loop load.
constexpr bool released(uint32_t now, uint32_t lastContact) {
  return uint32_t(now - lastContact) >= 70;
}
constexpr uint32_t contactDuration(uint32_t start, uint32_t lastContact) {
  return uint32_t(lastContact - start);
}
}
