// Content negotiation for the two representations embedded in flash.
#include "../src/settings_assets/encoding.h"
#include <cassert>

int main() {
  using SettingsAssets::chooseEncoding;
  using SettingsAssets::Encoding;
  assert(chooseEncoding("") == Encoding::identity);
  assert(chooseEncoding("br, deflate") == Encoding::identity);
  assert(chooseEncoding("br, GZip") == Encoding::gzip);
  assert(chooseEncoding("gzip; q=0, *;q=1") == Encoding::identity);
  assert(chooseEncoding("gzip; q=0.4, identity; q=0.8") == Encoding::identity);
  assert(chooseEncoding("gzip; q=0.500, identity; q=0.5") == Encoding::gzip);
  assert(chooseEncoding("identity;q=0, *;q=0.1") == Encoding::gzip);
  assert(chooseEncoding("*;q=0, gzip;q=1") == Encoding::gzip);
  assert(chooseEncoding("identity;q=0, gzip;q=0") == Encoding::unavailable);
  assert(chooseEncoding("*;q=0") == Encoding::unavailable);
  assert(chooseEncoding("gzip;q=1.5") == Encoding::identity);
}
