#ifndef LV_CONF_H
#define LV_CONF_H

/* Keep this file minimal: lv_conf_internal.h will fill all missing options. */

#define LV_COLOR_DEPTH 16
#define LV_COLOR_16_SWAP 1

/* Enable anti-aliasing for better text/shape quality. */
#define LV_ANTIALIAS 1

/* Font set for this UI pass (larger Montserrat for readability on 640x172). */
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_18 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_22 1
#define LV_FONT_MONTSERRAT_24 1
#define LV_FONT_MONTSERRAT_28 1
#define LV_FONT_MONTSERRAT_30 1
#define LV_FONT_MONTSERRAT_32 1
#define LV_FONT_MONTSERRAT_36 1
#define LV_FONT_MONTSERRAT_38 1
#define LV_FONT_UNSCII_8 1
#define LV_FONT_UNSCII_16 1

#define LV_FONT_DEFAULT &lv_font_montserrat_20

/* Extra widgets */
#define LV_USE_QRCODE 1
#define LV_USE_PNG 0
#define LV_USE_SJPG 0

#endif /*LV_CONF_H*/
