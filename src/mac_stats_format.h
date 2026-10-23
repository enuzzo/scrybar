#pragma once
#include <cmath>
#include <cstdio>
#include <cstddef>

namespace MacStatsFormat {
inline bool validRate(double bytesPerSecond) {
  return std::isfinite(bytesPerSecond) && bytesPerSecond >= 0 && bytesPerSecond <= 1e12;
}
inline void rate(char *out, size_t capacity, double bytesPerSecond, bool known) {
  if (!known || !validRate(bytesPerSecond)) { std::snprintf(out, capacity, "--"); return; }
  const char *units[] = {"B/s", "kB/s", "MB/s", "GB/s", "TB/s"};
  unsigned unit = 0;
  while (bytesPerSecond >= 999.95 && unit < 4) { bytesPerSecond /= 1000; ++unit; }
  if (!unit) std::snprintf(out, capacity, "%.0f %s", bytesPerSecond, units[unit]);
  else std::snprintf(out, capacity, "%.1f %s", bytesPerSecond, units[unit]);
}
}
