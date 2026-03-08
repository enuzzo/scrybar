#pragma once
#include <stdint.h>
#include <string.h>

// Metadata estratti dal record SAUCE (opzionale, tutti i campi sono NUL-padded)
struct SauceRecord {
  char title[36];   // 35 chars + NUL
  char author[21];  // 20 chars + NUL
  char group[21];   // 20 chars + NUL
  char year[5];     // 4 chars + NUL
  uint16_t width;   // colonne ANSI (tipicamente 80)
  uint16_t height;  // righe ANSI
  bool valid;
};

// Estrae il record SAUCE dalla fine del file .ANS.
// Restituisce true se trovato. data/len = intero file.
static bool parseSauceRecord(const uint8_t *data, size_t len, SauceRecord &out) {
  out = {};
  if (len < 128) return false;
  const uint8_t *rec = data + len - 128;
  if (memcmp(rec, "SAUCE00", 7) != 0) return false;
  strncpy(out.title,  (const char*)(rec + 7),  35); out.title[35]  = 0;
  strncpy(out.author, (const char*)(rec + 42), 20); out.author[20] = 0;
  strncpy(out.group,  (const char*)(rec + 62), 20); out.group[20]  = 0;
  strncpy(out.year,   (const char*)(rec + 82), 4);  out.year[4]    = 0;
  out.width  = (uint16_t)(rec[96] | (rec[97] << 8));
  out.height = (uint16_t)(rec[98] | (rec[99] << 8));
  for (int i = 34; i >= 0 && out.title[i]  == ' '; --i) out.title[i]  = 0;
  for (int i = 19; i >= 0 && out.author[i] == ' '; --i) out.author[i] = 0;
  for (int i = 19; i >= 0 && out.group[i]  == ' '; --i) out.group[i]  = 0;
  out.valid = true;
  return true;
}

// Stato del parser durante il rendering
struct AnsiParserState {
  int col = 0;
  int row = 0;
  uint8_t fg = 7;
  uint8_t bg = 0;
  bool bold = false;
  bool pendingWrap = false; // deferred wrap: set after filling last column

  void reset() { col=0; row=0; fg=7; bg=0; bold=false; pendingWrap=false; }
  uint8_t fgResolved() const { return bold ? (fg | 8) : fg; }
};

// ANSI SGR codes 30-37/40-47 map to CGA indices in a non-sequential order.
// ANSI: Black=0, Red=1, Green=2, Yellow=3, Blue=4, Magenta=5, Cyan=6, White=7
// CGA:  Black=0, Blue=1, Green=2, Cyan=3, Red=4, Magenta=5, Brown=6, LGray=7
static const uint8_t kAnsiToCga[8] = {0, 4, 2, 6, 1, 5, 3, 7};

// Legge una sequenza CSI (buffer punta a '[' dopo ESC). Modifica state.
// Ritorna puntatore al byte DOPO la lettera finale.
static const uint8_t* parseAnsiCsi(const uint8_t *buf, size_t remaining,
                                    AnsiParserState &st) {
  size_t i = 0;
  // Skip private parameter prefix (?, <, =, >)
  if (i < remaining && (buf[i] == '?' || buf[i] == '<' || buf[i] == '=' || buf[i] == '>')) {
    ++i;
  }
  int params[8] = {0};
  int nparams = 0;
  while (i < remaining && nparams < 8) {
    uint8_t c = buf[i++];
    if (c >= '0' && c <= '9') {
      params[nparams] = params[nparams] * 10 + (c - '0');
    } else if (c == ';') {
      ++nparams;
    } else if (c >= 0x20 && c <= 0x2F) {
      // Intermediate byte — consume and keep looking for final byte
    } else {
      if (nparams < 8) ++nparams;
      if (c == 'm') {
        if (nparams == 0 || (nparams == 1 && params[0] == 0)) {
          st.fg = 7; st.bg = 0; st.bold = false;
        } else {
          for (int p = 0; p < nparams; ++p) {
            int v = params[p];
            if (v == 0)                { st.fg=7; st.bg=0; st.bold=false; }
            else if (v == 1)           { st.bold = true; }
            else if (v == 5)           { st.bold = true; } // blink → bright
            else if (v >= 30 && v <= 37)   { st.fg = kAnsiToCga[v - 30]; }
            else if (v >= 40 && v <= 47)   { st.bg = kAnsiToCga[v - 40]; }
            else if (v >= 90 && v <= 97)   { st.fg = (uint8_t)(kAnsiToCga[v - 90] + 8); }
            else if (v >= 100 && v <= 107) { st.bg = (uint8_t)(kAnsiToCga[v - 100] + 8); }
          }
        }
      } else if (c == 'H' || c == 'f') {
        st.row = (nparams >= 1 && params[0] > 0) ? params[0] - 1 : 0;
        st.col = (nparams >= 2 && params[1] > 0) ? params[1] - 1 : 0;
        st.pendingWrap = false;
      } else if (c == 'A') { st.row -= (nparams >= 1 && params[0] > 0) ? params[0] : 1; st.pendingWrap = false; }
      else if  (c == 'B') { st.row += (nparams >= 1 && params[0] > 0) ? params[0] : 1; st.pendingWrap = false; }
      else if  (c == 'C') { st.col += (nparams >= 1 && params[0] > 0) ? params[0] : 1; st.pendingWrap = false; }
      else if  (c == 'D') { st.col -= (nparams >= 1 && params[0] > 0) ? params[0] : 1; st.pendingWrap = false; }
      // Ignora: J, K, s, u, etc.
      break;
    }
  }
  return buf + i;
}
