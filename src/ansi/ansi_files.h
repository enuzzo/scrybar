#pragma once

// ANSI art file registry — loaded from FAT filesystem at runtime.
// Raw .ans files live on the FAT partition under /ansi/ (provisioned via
// tools/provision_ansi.py). The font bitmap is still compiled in.

#include "font_ibmvga8x16.h"

struct AnsiFileEntry {
  const char *fatPath;   // path on FAT filesystem (e.g. "/ansi/45-fire.ans")
  const char *filename;  // display label
};

static const AnsiFileEntry kAnsiFiles[] = {
  { "/ansi/45-fire.ans",                      "45-fire" },
  { "/ansi/5m-legends.ans",                   "5m-legends" },
  { "/ansi/6733-blocktronics-memberlist.ans",  "6733-blocktronics-memberlist" },
  { "/ansi/67c21-04.ans",                     "67c21-04" },
  { "/ansi/at-totoro.ans",                    "at-totoro" },
  { "/ansi/avg-fire.ans",                     "avg-fire" },
  { "/ansi/avg-hvns.ans",                     "avg-hvns" },
  { "/ansi/avg-terminator2.ans",              "avg-terminator2" },
  { "/ansi/escape-revengeday.ans",            "escape-revengeday" },
  { "/ansi/ev-legnd.ans",                     "ev-legnd" },
  { "/ansi/fil-g0nk.ans",                     "fil-g0nk" },
  { "/ansi/fil-gaul.ans",                     "fil-gaul" },
  { "/ansi/id-final.ans",                     "id-final" },
  { "/ansi/id-lgnd.ans",                      "id-lgnd" },
  { "/ansi/misfit-blocked.ans",               "misfit-blocked" },
  { "/ansi/mt-blad.ans",                      "mt-blad" },
  { "/ansi/n-maniac.ans",                     "n-maniac" },
  { "/ansi/ra-devil.ans",                     "ra-devil" },
  { "/ansi/tcf-mina.ans",                     "tcf-mina" },
  { "/ansi/tg-blcko.ans",                     "tg-blcko" },
  { "/ansi/tg-evil.ans",                      "tg-evil" },
  { "/ansi/tg-sab.ans",                       "tg-sab" },
  { "/ansi/tg-zom.ans",                       "tg-zom" },
  { "/ansi/tnt-tr0n.ans",                     "tnt-tr0n" },
  { "/ansi/tnt-vn0m.ans",                     "tnt-vn0m" },
  { "/ansi/us-4c1d.ans",                      "us-4c1d" },
  { "/ansi/us-impre.ans",                     "us-impre" },
};
static constexpr uint8_t kAnsiFileCount = 27;
