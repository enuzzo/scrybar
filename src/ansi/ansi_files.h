#pragma once
#include "5m_legends_ans.h"
#include "avg_fire_ans.h"
#include "mt_blad_ans.h"
#include "ra_devil_ans.h"

struct AnsiFileEntry {
  const uint8_t *data;
  size_t         len;
  const char    *filename;
};

static const AnsiFileEntry kAnsiFiles[] = {
  { f_5m_legends_ans, f_5m_legends_ans_len, "5m-legends" },
  { avg_fire_ans,     avg_fire_ans_len,     "avg-fire"   },
  { mt_blad_ans,      mt_blad_ans_len,      "mt-blad"    },
  { ra_devil_ans,     ra_devil_ans_len,     "ra-devil"   },
};
static constexpr uint8_t kAnsiFileCount = sizeof(kAnsiFiles) / sizeof(kAnsiFiles[0]);
