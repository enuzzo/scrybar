#pragma once

#include <Preferences.h>
#include <nvs.h>

// Preferences::putString returns zero for both a committed empty value and a
// failure. RSS, Wi-Fi and transit fields can legitimately be cleared.
class ConfigPreferences : public Preferences {
 public:
  bool writeString(const char *key, const char *value) {
    return _started && !_readOnly &&
           nvs_set_str(_handle, key, value) == ESP_OK &&
           nvs_commit(_handle) == ESP_OK;
  }
};
