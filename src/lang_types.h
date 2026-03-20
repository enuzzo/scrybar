#pragma once

#include <stdint.h>
#include <time.h>

struct UiStrings; // forward declaration (defined in ui_strings.h)

// Short (category) weather labels — one struct per language
struct WeatherShortLabels {
  const char* clear;
  const char* cloudy;
  const char* overcast;
  const char* fog;
  const char* rain;
  const char* snow;
  const char* storm;
  const char* na;
};

// Detailed WMO UI label index — maps WMO codes to array positions
enum WmoUiIdx : uint8_t {
  WMO_CLEAR, WMO_MAINLY_CLEAR, WMO_PARTLY_CLOUDY, WMO_OVERCAST,
  WMO_FOG, WMO_ICY_FOG,
  WMO_DRIZZLE_L, WMO_DRIZZLE_M, WMO_DRIZZLE_H, WMO_FREEZE_DRIZZLE,
  WMO_RAIN_L, WMO_RAIN_M, WMO_RAIN_H, WMO_FREEZE_RAIN,
  WMO_SNOW_L, WMO_SNOW_M, WMO_SNOW_H, WMO_SNOW_GRAINS,
  WMO_SHOWER_L, WMO_SHOWER_M, WMO_SHOWER_H, WMO_SNOW_SHOWER,
  WMO_THUNDER, WMO_HAIL,
  WMO_UI_COUNT
};

// Language vtable — maps a language code to all its dispatch targets
struct LangVtable {
  const char*               code;
  void                      (*wordClock)(const tm&, char*, size_t);
  const WeatherShortLabels* weatherShort;
  const char* const*        weatherUi;
  const char*               weatherUiNa;
  void                      (*formatDate)(const tm&, char*, size_t);
  const UiStrings*          uiStrings;
};
