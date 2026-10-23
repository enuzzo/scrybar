#include "../src/mac_stats_format.h"
#include <cassert>
#include <cstring>
#include <limits>
int main() {
  char out[24];
  MacStatsFormat::rate(out, sizeof(out), 0, true); assert(std::strcmp(out, "0 B/s") == 0);
  MacStatsFormat::rate(out, sizeof(out), 0, false); assert(std::strcmp(out, "--") == 0);
  MacStatsFormat::rate(out, sizeof(out), 12500000, true); assert(std::strcmp(out, "12.5 MB/s") == 0);
  MacStatsFormat::rate(out, sizeof(out), 999999, true); assert(std::strcmp(out, "1.0 MB/s") == 0);
  MacStatsFormat::rate(out, sizeof(out), -1, true); assert(std::strcmp(out, "--") == 0);
  assert(!MacStatsFormat::validRate(std::numeric_limits<double>::infinity()));
  assert(!MacStatsFormat::validRate(std::numeric_limits<double>::quiet_NaN()));
  assert(!MacStatsFormat::validRate(1e13));
  assert(MacStatsFormat::validRate(1e12));
}
