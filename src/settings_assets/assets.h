#pragma once

#include "encoding.h"
#include <cstdint>

namespace SettingsAssets {
extern "C" {
extern const uint8_t settings_css_start[], settings_css_end[];
extern const uint8_t settings_css_gzip_start[], settings_css_gzip_end[];
extern const uint8_t settings_js_start[], settings_js_end[];
extern const uint8_t settings_js_gzip_start[], settings_js_gzip_end[];
}
struct Bytes { const uint8_t *begin, *end; };
struct Asset { const char *contentType; Bytes plain, gzip; };
inline constexpr Asset css{"text/css; charset=utf-8", {settings_css_start, settings_css_end},
                           {settings_css_gzip_start, settings_css_gzip_end}};
inline constexpr Asset js{"text/javascript; charset=utf-8", {settings_js_start, settings_js_end},
                          {settings_js_gzip_start, settings_js_gzip_end}};
}
