#pragma once
#include <stdint.h>

// Shared geometry for drawing and hit testing. Keep the navbar at 30 px.
namespace BambuCameraControls {
struct Rect {
  int x, y, width, height;
  constexpr bool contains(int px, int py) const {
    return px >= x && px < x + width && py >= y && py < y + height;
  }
};
constexpr Rect camera{354, 1, 112, 28};
constexpr Rect cameraTouch{354, 0, 112, 34};
constexpr Rect light{500, 58, 128, 44};
constexpr Rect back{500, 112, 128, 44};
enum Button : uint8_t { none, open, toggleLight, close };
constexpr Button hit(bool overlayOpen, bool active, int x, int y) {
  if (!overlayOpen) return cameraTouch.contains(x, y) ? open : none;
  if (light.contains(x, y) && active) return toggleLight;
  if (back.contains(x, y)) return close;
  return none;
}
constexpr bool validRelease(Button down, Button up, uint32_t heldMs, int dx, int dy) {
  return down != none && down == up && heldMs <= 1400 &&
         dx >= -32 && dx <= 32 && dy >= -32 && dy <= 32;
}
}
