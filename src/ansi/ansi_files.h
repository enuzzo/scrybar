#pragma once
#include "f_45_fire_ans.h"
#include "5m_legends_ans.h"
#include "f_6733_blocktronics_memberlist_ans.h"
#include "f_67c21_04_ans.h"
#include "at_totoro_ans.h"
#include "avg_fire_ans.h"
#include "avg_hvns_ans.h"
#include "avg_terminator2_ans.h"
#include "escape_revengeday_ans.h"
#include "ev_legnd_ans.h"
#include "fil_g0nk_ans.h"
#include "fil_gaul_ans.h"
#include "id_final_ans.h"
#include "id_lgnd_ans.h"
#include "misfit_blocked_ans.h"
#include "mt_blad_ans.h"
#include "n_maniac_ans.h"
#include "ra_devil_ans.h"
#include "tcf_mina_ans.h"
#include "tg_blcko_ans.h"
#include "tg_evil_ans.h"
#include "tg_sab_ans.h"
#include "tg_zom_ans.h"
#include "tnt_tr0n_ans.h"
#include "tnt_vn0m_ans.h"
#include "us_4c1d_ans.h"
#include "us_impre_ans.h"

struct AnsiFileEntry {
  const uint8_t *data;
  size_t         len;
  const char    *filename;
};

static const AnsiFileEntry kAnsiFiles[] = {
  { f_45_fire_ans, f_45_fire_ans_len, "45-fire" },
  { f_5m_legends_ans, f_5m_legends_ans_len, "5m-legends" },
  { f_6733_blocktronics_memberlist_ans, f_6733_blocktronics_memberlist_ans_len, "6733-blocktronics-memberlist" },
  { f_67c21_04_ans, f_67c21_04_ans_len, "67c21-04" },
  { at_totoro_ans, at_totoro_ans_len, "at-totoro" },
  { avg_fire_ans, avg_fire_ans_len, "avg-fire" },
  { avg_hvns_ans, avg_hvns_ans_len, "avg-hvns" },
  { avg_terminator2_ans, avg_terminator2_ans_len, "avg-terminator2" },
  { escape_revengeday_ans, escape_revengeday_ans_len, "escape-revengeday" },
  { ev_legnd_ans, ev_legnd_ans_len, "ev-legnd" },
  { fil_g0nk_ans, fil_g0nk_ans_len, "fil-g0nk" },
  { fil_gaul_ans, fil_gaul_ans_len, "fil-gaul" },
  { id_final_ans, id_final_ans_len, "id-final" },
  { id_lgnd_ans, id_lgnd_ans_len, "id-lgnd" },
  { misfit_blocked_ans, misfit_blocked_ans_len, "misfit-blocked" },
  { mt_blad_ans, mt_blad_ans_len, "mt-blad" },
  { n_maniac_ans, n_maniac_ans_len, "n-maniac" },
  { ra_devil_ans, ra_devil_ans_len, "ra-devil" },
  { tcf_mina_ans, tcf_mina_ans_len, "tcf-mina" },
  { tg_blcko_ans, tg_blcko_ans_len, "tg-blcko" },
  { tg_evil_ans, tg_evil_ans_len, "tg-evil" },
  { tg_sab_ans, tg_sab_ans_len, "tg-sab" },
  { tg_zom_ans, tg_zom_ans_len, "tg-zom" },
  { tnt_tr0n_ans, tnt_tr0n_ans_len, "tnt-tr0n" },
  { tnt_vn0m_ans, tnt_vn0m_ans_len, "tnt-vn0m" },
  { us_4c1d_ans, us_4c1d_ans_len, "us-4c1d" },
  { us_impre_ans, us_impre_ans_len, "us-impre" },
};
static constexpr uint8_t kAnsiFileCount = 27;
