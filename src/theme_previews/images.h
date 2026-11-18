#pragma once

#include <cstdint>

namespace ThemePreviews {
extern "C" {
extern const uint8_t preview_scrybar_default_start[], preview_scrybar_default_end[];
extern const uint8_t preview_cyberpunk_2077_start[], preview_cyberpunk_2077_end[];
extern const uint8_t preview_toxic_candy_start[], preview_toxic_candy_end[];
extern const uint8_t preview_tokyo_transit_start[], preview_tokyo_transit_end[];
extern const uint8_t preview_minimal_brutalist_mono_start[], preview_minimal_brutalist_mono_end[];
extern const uint8_t preview_mint_protocol_start[], preview_mint_protocol_end[];
extern const uint8_t preview_cathode_ray_start[], preview_cathode_ray_end[];
extern const uint8_t preview_letterpress_start[], preview_letterpress_end[];
extern const uint8_t preview_tide_table_start[], preview_tide_table_end[];
}
struct Image {
  const char *id;
  const uint8_t *start;
  const uint8_t *end;
};
inline constexpr Image images[] = {
  {"scrybar-default", preview_scrybar_default_start, preview_scrybar_default_end},
  {"cyberpunk-2077", preview_cyberpunk_2077_start, preview_cyberpunk_2077_end},
  {"toxic-candy", preview_toxic_candy_start, preview_toxic_candy_end},
  {"tokyo-transit", preview_tokyo_transit_start, preview_tokyo_transit_end},
  {"minimal-brutalist-mono", preview_minimal_brutalist_mono_start, preview_minimal_brutalist_mono_end},
  {"mint-protocol", preview_mint_protocol_start, preview_mint_protocol_end},
  {"cathode-ray", preview_cathode_ray_start, preview_cathode_ray_end},
  {"letterpress", preview_letterpress_start, preview_letterpress_end},
  {"tide-table", preview_tide_table_start, preview_tide_table_end},
};
}
