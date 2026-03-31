// Archived from config.h — r228

// ── Home Assistant panel ─────────────────────────────────────────────
#define HA_BADGE_COUNT               4
#define HA_CONTROL_COUNT             4
#define HA_POLL_INTERVAL_MS          30000UL
#define HA_RETRY_INTERVAL_MS          8000UL
#define HA_SERVICE_REFETCH_DELAY_MS    500UL
#define HA_HTTP_TIMEOUT_MS            4000

#define HA_CTRL_OFF    0
#define HA_CTRL_TOGGLE 1
#define HA_CTRL_BUTTON 2

struct HaBadgeConfig {
  char entityId[64];
  char label[24];
  char unit[16];        // override unit (empty = use API value)
};

struct HaControlConfig {
  uint8_t type;         // HA_CTRL_OFF / HA_CTRL_TOGGLE / HA_CTRL_BUTTON
  char label[24];
  char entityId[64];
  char serviceDomain[24];  // "homeassistant" for toggle, "scene" etc. for button
  char service[24];        // "toggle", "turn_on", etc.
};

struct HaConfig {
  char url[80];
  char token[256];
  HaBadgeConfig    badges[HA_BADGE_COUNT];
  HaControlConfig  controls[HA_CONTROL_COUNT];
  bool configured;      // true iff url and token are non-empty
};
