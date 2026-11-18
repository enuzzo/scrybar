#pragma once

#include <string_view>

namespace SettingsAssets {
enum class Encoding { identity, gzip, unavailable };

inline std::string_view trim(std::string_view value) {
  const auto first = value.find_first_not_of(" \t");
  if (first == value.npos) return {};
  return value.substr(first, value.find_last_not_of(" \t") - first + 1);
}

inline bool sameToken(std::string_view value, std::string_view lower) {
  if (value.size() != lower.size()) return false;
  for (size_t i = 0; i < lower.size(); ++i) {
    char c = value[i];
    if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
    if (c != lower[i]) return false;
  }
  return true;
}

// Parse HTTP quality values as thousandths without allocating on the ESP32.
inline int quality(std::string_view param) {
  const auto eq = param.find('=');
  if (eq == param.npos || !sameToken(trim(param.substr(0, eq)), "q")) return 0;
  const auto q = trim(param.substr(eq + 1));
  if (q.empty() || (q.front() != '0' && q.front() != '1')) return 0;
  const int whole = q.front() == '1' ? 1000 : 0;
  if (q.size() == 1) return whole;
  if (q[1] != '.' || q.size() > 5) return 0;
  int fraction = 0, scale = 100;
  for (size_t i = 2; i < q.size(); ++i, scale /= 10) {
    if (q[i] < '0' || q[i] > '9') return 0;
    fraction += (q[i] - '0') * scale;
  }
  return whole && fraction ? 0 : whole + fraction;
}

inline Encoding chooseEncoding(std::string_view header) {
  int gzip = -1, identity = -1, wildcard = -1;
  while (!header.empty()) {
    const auto comma = header.find(',');
    const auto item = header.substr(0, comma);
    const auto semi = item.find(';');
    const auto token = trim(item.substr(0, semi));
    const int weight = semi == item.npos ? 1000 : quality(item.substr(semi + 1));
    if (sameToken(token, "gzip")) gzip = weight;
    else if (sameToken(token, "identity")) identity = weight;
    else if (token == "*") wildcard = weight;
    if (comma == header.npos) break;
    header.remove_prefix(comma + 1);
  }
  if (gzip < 0) gzip = wildcard;
  if (gzip > 0 && gzip >= identity) return Encoding::gzip;
  if (identity == 0 || (identity < 0 && wildcard == 0)) return Encoding::unavailable;
  return Encoding::identity;
}
}
