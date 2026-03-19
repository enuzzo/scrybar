#!/usr/bin/env python3
"""Migrate ScryBar web UI to Vibemilk DS subset.

Replaces custom CSS with vibemilk bridge + component subset.
Removes: animations, FX grid, backdrop-filter, Font Awesome.
Adds: vm-* classes, unicode section icons, clean token-based styles.

Run from project root:
    python3 tools/migrate_webui_vibemilk.py
"""
import re, sys, os

INO = os.path.join(os.path.dirname(__file__), '..', 'scrybar.ino')

def main():
    with open(INO, 'r') as f:
        src = f.read()

    # --- Safety check ---
    if 'vm-wrap' in src:
        print("SKIP: already migrated (vm-wrap found)")
        sys.exit(0)

    original_len = len(src)

    # ===== 1. REMOVE FONT AWESOME CDN =====
    src = src.replace(
        """  html += F("<link rel='stylesheet' href='https://cdnjs.cloudflare.com/ajax/libs/font-awesome/6.7.2/css/all.min.css' media='print' onload=\\"this.media='all'\\">");\n""",
        ""
    )
    # Remove the noscript FA fallback (inside the noscript tag)
    src = src.replace(
        """<link rel='stylesheet' href='https://cdnjs.cloudflare.com/ajax/libs/font-awesome/6.7.2/css/all.min.css'>""",
        ""
    )

    # ===== 2. SIMPLIFY GOOGLE FONTS (keep only weights we actually use) =====
    src = src.replace(
        "family=Chakra+Petch:wght@400;500;600;700&family=Delius+Unicase:wght@400;700&family=IBM+Plex+Mono:wght@400;500;600;700&family=Montserrat:wght@400;500;600;700;800&family=Space+Mono:wght@400;700",
        "family=Montserrat:wght@400;500;600;700&family=Space+Mono:wght@400;700&family=Chakra+Petch:wght@400;600&family=IBM+Plex+Mono:wght@400;600"
    )

    # ===== 3. REDUCE RESERVE SIZE =====
    src = src.replace("html.reserve(32000);", "html.reserve(22000);")

    # ===== 4. REPLACE ENTIRE CSS BLOCK =====
    # Old CSS: from '*{box-sizing' through the brutalist override block
    css_start_marker = '  html += F("*{box-sizing:border-box}'
    css_end_marker = "border-radius:0!important}\");\n"

    css_start = src.index(css_start_marker)
    css_end = src.index(css_end_marker, css_start) + len(css_end_marker)

    NEW_CSS = r'''  // ── Vibemilk DS subset: bridge + reset + components ──
  html += F("*,*::before,*::after{box-sizing:border-box;margin:0;padding:0}");
  // Bridge: map firmware tokens → vibemilk standard names
  html += F(":root{--font-family:var(--font-main);--text-primary:var(--txt);--text-secondary:var(--txt2);--text-tertiary:var(--txt3);--accent-primary:var(--acc1);--accent-secondary:var(--acc2);--bg-input:var(--bg-deep);--bg-elevated:var(--bg-surface);--stroke:var(--line);--stroke-soft:var(--line-soft);--shadow-sm:0 2px 8px rgba(0,0,0,.25);--shadow-md:0 4px 16px rgba(0,0,0,.3);--r-sm:8px;--r-md:12px;--r-lg:14px;--focus-ring:0 0 0 3px rgba(57,184,255,.18)}");
  // Base
  html += F("body{font-family:var(--font-family);font-size:14px;font-weight:400;line-height:1.5;color:var(--text-secondary);background:var(--bg-deepest);-webkit-font-smoothing:antialiased}");
  html += F("a{color:var(--accent-primary);text-decoration:none}::selection{background:rgba(57,184,255,.24);color:var(--text-primary)}");
  // Layout
  html += F(".vm-wrap{max-width:780px;margin:0 auto;padding:20px 16px 32px}");
  // Card
  html += F(".vm-card{background:var(--bg-surface);border:1px solid var(--stroke-soft);border-radius:var(--r-lg);padding:20px;margin-bottom:16px}");
  html += F(".vm-card__hd{display:flex;align-items:center;gap:8px;margin-bottom:16px;font-size:13px;font-weight:700;letter-spacing:.06em;text-transform:uppercase;color:var(--text-primary)}");
  html += F(".vm-card__hd .vm-badge{margin-left:auto;text-transform:none;letter-spacing:0}");
  html += F(".vm-card--inner{background:rgba(255,255,255,.03);border:1px solid var(--stroke-soft);border-radius:var(--r-md);padding:16px}");
  // Buttons
  html += F(".vm-btn{display:inline-flex;align-items:center;justify-content:center;gap:6px;font-family:var(--font-family);font-weight:600;border:0;cursor:pointer;transition:all .15s ease;white-space:nowrap;font-size:13px;height:40px;padding:0 18px;border-radius:var(--r-sm)}");
  html += F(".vm-btn--sm{height:34px;padding:0 12px;font-size:12px;border-radius:6px}");
  html += F(".vm-btn--primary{background:var(--accent-primary);color:#fff}.vm-btn--primary:hover{filter:brightness(1.15);box-shadow:var(--shadow-sm)}");
  html += F(".vm-btn--secondary{background:var(--bg-elevated);color:var(--text-secondary);border:1px solid var(--stroke)}.vm-btn--secondary:hover{color:var(--text-primary);border-color:var(--accent-secondary)}");
  html += F(".vm-btn--danger{background:rgba(238,93,80,.12);color:#f26a5e;border:1px solid rgba(238,93,80,.3)}.vm-btn--danger:hover{background:rgba(238,93,80,.22)}");
  html += F(".vm-btn--warn{background:rgba(117,81,255,.12);color:#b8a8ff;border:1px solid rgba(117,81,255,.35)}.vm-btn--warn:hover{background:rgba(117,81,255,.22)}");
  html += F(".vm-btn:disabled{opacity:.4;cursor:not-allowed;pointer-events:none}");
  // Forms
  html += F(".vm-input,.vm-select{width:100%;height:44px;padding:0 16px;font-family:var(--font-family);font-size:14px;font-weight:500;color:var(--text-primary);background:var(--bg-input);border:1px solid var(--stroke);border-radius:var(--r-sm);outline:none;transition:border-color .15s ease;margin:0 0 4px}");
  html += F(".vm-input:focus,.vm-select:focus{border-color:var(--accent-secondary);box-shadow:var(--focus-ring)}");
  html += F(".vm-input::placeholder{color:var(--text-tertiary)}");
  html += F(".vm-select{cursor:pointer;appearance:none;padding-right:40px;background-image:linear-gradient(45deg,transparent 50%,var(--text-tertiary) 50%),linear-gradient(135deg,var(--text-tertiary) 50%,transparent 50%);background-repeat:no-repeat;background-size:6px 6px,6px 6px;background-position:calc(100% - 18px) 52%,calc(100% - 13px) 52%}");
  html += F(".vm-label{display:block;font-size:11px;font-weight:600;letter-spacing:.06em;text-transform:uppercase;color:var(--text-tertiary);margin:0 0 6px}");
  html += F(".vm-help{font-size:12px;color:var(--text-tertiary);line-height:1.45;margin:6px 0 0}");
  // Badge
  html += F(".vm-badge{display:inline-flex;align-items:center;gap:4px;height:22px;padding:0 10px;border-radius:999px;font-size:11px;font-weight:600}");
  html += F(".vm-badge--brand{background:rgba(117,81,255,.14);color:var(--accent-primary)}.vm-badge--info{background:rgba(57,184,255,.14);color:var(--accent-secondary)}");
  html += F(".pill{display:inline-block;padding:4px 10px;border-radius:999px;background:rgba(57,184,255,.12);color:var(--accent-secondary);font-size:11px;font-weight:700}");
  // Alert
  html += F(".vm-alert{padding:12px 16px;border-radius:var(--r-md);border-left:4px solid #01B574;background:var(--okbg);color:#c9fce9;font-weight:600;font-size:13px}");
  html += F(".vm-toast-fixed{position:fixed;top:12px;left:50%;transform:translateX(-50%);width:min(94vw,680px);z-index:9999;box-shadow:var(--shadow-md)}");
  html += F(".msg{margin:0 0 12px;padding:10px 12px;border-radius:var(--r-md);border:1px solid rgba(1,181,116,.45);background:var(--okbg);color:#c9fce9;font-weight:600}");
  html += F(".panel{background:transparent;border:0;padding:0}");
  // Hero (keeps existing class names, restyled with tokens)
  html += F(".hero{background:0;border:0;border-radius:0;padding:0;margin-bottom:14px}");
  html += F(".hero-top-card{border:1px solid var(--stroke-soft);border-radius:var(--r-lg);padding:14px;background:var(--bg-surface)}");
  html += F(".hero-top{display:flex;align-items:flex-start;justify-content:space-between;gap:14px;flex-wrap:wrap}.hero-left{min-width:290px;flex:1 1 560px}");
  html += F(".logo{height:56px;display:block;object-fit:contain}.hero-right{display:grid;gap:8px;justify-items:end}");
  html += F(".release-box{display:grid;grid-template-columns:auto auto;gap:4px 12px;padding:8px 10px;border-radius:var(--r-sm);border:1px solid var(--stroke-soft);background:rgba(255,255,255,.03);font:600 11px var(--font-mono)}");
  html += F(".release-box .k{color:var(--accent-secondary);text-transform:uppercase;letter-spacing:.08em}.release-box .v{color:var(--text-primary);letter-spacing:.01em}");
  html += F(".hero-copy{margin-top:10px;border:1px solid var(--stroke-soft);border-radius:var(--r-md);padding:12px;background:rgba(255,255,255,.02)}");
  html += F(".lede{margin:0;color:var(--text-secondary);font-size:13px;line-height:1.46}");
  // Grid
  html += F(".vm-grid{display:grid;grid-template-columns:1fr 1fr;gap:12px}");
  // Views
  html += F(".vm-views{display:grid;grid-template-columns:repeat(auto-fit,minmax(180px,1fr));gap:10px}");
  html += F(".vm-view{display:flex;gap:10px;align-items:flex-start;padding:12px;border:1px solid var(--stroke-soft);border-radius:var(--r-md);background:rgba(255,255,255,.02)}");
  html += F(".vm-view input[type=checkbox]{width:18px;height:18px;margin:2px 0 0;accent-color:var(--accent-secondary);flex:0 0 auto}");
  html += F(".vm-view__copy{display:grid;gap:3px}.vm-view__copy strong{font-size:13px;color:var(--text-primary)}.vm-view__copy small{color:var(--text-tertiary);line-height:1.35;font-size:12px}");
  html += F(".vm-view--fixed{border-style:dashed}.vm-view--off{opacity:.55}");
  // WiFi
  html += F(".vm-secret{display:flex;gap:8px;align-items:stretch;margin:0 0 4px}.vm-secret .vm-input{margin:0}");
  html += F(".vm-setup-grid{display:grid;grid-template-columns:auto 1fr;gap:12px;align-items:center;margin-top:10px}");
  html += F(".vm-setup-qr{width:138px;height:138px;border:1px solid var(--stroke);background:#fff;padding:6px;border-radius:var(--r-sm);display:block}");
  html += F(".vm-setup-url{font:600 13px var(--font-mono);word-break:break-all;color:var(--text-primary)}");
  // RSS
  html += F(".vm-rss-composer{display:grid;grid-template-columns:1fr 1.9fr .55fr auto auto;gap:10px;align-items:end;margin-top:4px}");
  html += F(".vm-rss-list{display:grid;gap:8px;margin-top:10px}");
  html += F(".rss-row{display:grid;grid-template-columns:1fr auto;gap:8px;align-items:center;border:1px solid var(--stroke-soft);border-left:3px solid var(--accent-primary);border-radius:var(--r-md);padding:12px;background:rgba(255,255,255,.02)}");
  html += F(".rss-title{display:flex;align-items:center;gap:6px;font-size:14px;font-weight:700;color:var(--text-primary);margin:0 0 2px}");
  html += F(".rss-meta{font-size:12px;color:var(--text-tertiary);margin:0;word-break:break-all}");
  html += F(".rss-chip{display:inline-block;margin-left:7px;padding:2px 8px;border-radius:999px;background:rgba(57,184,255,.14);color:var(--accent-secondary);font-size:11px;font-weight:600}");
  html += F(".rss-actions{display:flex;gap:6px;flex-wrap:wrap;justify-content:flex-end}");
  html += F(".rss-status{margin:6px 0 2px;color:var(--text-tertiary);font-size:12px;min-height:16px}");
  html += F(".rss-empty{padding:12px;border:1px dashed var(--stroke);border-radius:var(--r-md);color:var(--text-tertiary);font-size:12px;background:rgba(255,255,255,.02)}");
  html += F(".hidden{display:none}");
  // System info
  html += F(".vm-kv{font-size:13px;line-height:1.7}.vm-kv small{color:var(--text-tertiary)}.vm-kv code{color:var(--text-primary);font-family:var(--font-mono);font-size:12px}");
  // Footer
  html += F(".vm-footer{margin-top:20px;padding:14px 0 4px;border-top:1px solid var(--stroke-soft);font-size:12px;color:var(--text-tertiary);line-height:1.5}.vm-footer strong{color:var(--text-secondary)}.vm-footer a{color:var(--accent-secondary)}");
  // Actions
  html += F(".vm-actions{display:flex;gap:10px;flex-wrap:wrap;margin-top:16px}");
  // API note
  html += F(".vm-api-note{margin-top:12px;padding:10px 12px;border-radius:var(--r-sm);background:rgba(57,184,255,.06);border:1px solid var(--stroke-soft);font-size:12px;color:var(--text-tertiary)}.vm-api-note code{color:var(--text-secondary)}");
  // Password field mono
  html += F("#wifi_new_password{font-family:var(--font-mono),ui-monospace,SFMono-Regular,Menlo,Monaco,Consolas,monospace;letter-spacing:.02em}");
  // Geo status
  html += F(".geo-status{margin:2px 0 8px;color:var(--text-tertiary);font-size:12px;min-height:16px}");
  // Responsive
  html += F("@media(max-width:768px){.vm-grid{grid-template-columns:1fr}.vm-rss-composer{grid-template-columns:1fr}.hero-top{flex-wrap:wrap}.hero-right{width:100%;justify-items:start}.vm-actions{flex-direction:column}.vm-actions .vm-btn{width:100%;justify-content:center}.logo{height:48px}}");
  html += F("small{color:var(--text-tertiary)}code{color:var(--text-secondary)}");
'''
    src = src[:css_start] + NEW_CSS + src[css_end:]

    # ===== 5. REMOVE FX GRID DIVS from body =====
    src = src.replace(
        "'><div class='fx-grid'><div class='fx-grid__plane'></div><div class='fx-grid__glow'></div><div class='fx-grid__horizon'></div><div class='fx-grid__scanline'></div><div class='fx-grid__vline'></div><div class='fx-grid__vline'></div><div class='fx-grid__vline'></div><div class='fx-grid__vignette'></div></div><main class='wrap'>",
        "'><main class='vm-wrap'>"
    )

    # ===== 6. HERO — just remove FA icons, keep structure =====
    src = src.replace("<i class='fa-regular fa-calendar'></i> last release", "release")
    src = src.replace("<i class='fa-solid fa-code-branch'></i> version", "version")

    # ===== 7. STATUS MESSAGE — restyle =====
    src = src.replace("class='msg fixed-top'", "class='vm-alert vm-toast-fixed'")
    # Also add a simple .panel style (keep the class for now)
    src = src.replace("class='panel'", "class='panel'")

    # ===== 8. REPLACE SECTION HEADERS (FA icons → unicode) =====
    fa_replacements = [
        # Theme
        ("<i class='fa-solid fa-palette'></i>Visual Theme",
         "\xF0\x9F\x8E\xA8 Visual Theme"),
        # Views
        ("<i class='fa-solid fa-table-cells-large'></i>Views",
         "Views"),
        # WiFi
        ("<i class='fa-solid fa-wifi'></i>Wi-Fi Known Networks",
         "Wi-Fi Known Networks"),
        # Language
        ("<i class='fa-solid fa-language'></i>System Language",
         "\xF0\x9F\x8C\x90 System Language"),
        # Wikipedia
        ("<i class='fa-solid fa-book-open'></i>Wikipedia Language",
         "\xF0\x9F\x93\x96 Wikipedia Language"),
        # Weather
        ("<i class='fa-solid fa-location-dot'></i>Weather &amp; Location",
         "\xF0\x9F\x93\x8D Weather &amp; Location"),
        # RSS
        ("<i class='fa-solid fa-square-rss'></i>RSS Feed Builder",
         "\xF0\x9F\x93\xA1 RSS Feed Builder"),
        # System info
        ("<i class='fa-solid fa-microchip'></i>System Info",
         "\xE2\x9A\x99 System Info"),
    ]
    for old, new in fa_replacements:
        src = src.replace(old, new)

    # ===== 9. REPLACE SECTION CONTAINERS =====
    # .sec → .vm-card (class only)
    src = src.replace("class='sec'", "class='vm-card'")
    # .sec h2 → .vm-card__hd  (update the tag usage in HTML)
    # Actually the h2 already has inline text, just need to update the parent container class

    # ===== 10. REPLACE FORM CONTROL CLASSES =====
    # Key labels: .key → .vm-label
    src = src.replace("class='key'", "class='vm-label'")
    # Hint paragraphs: .hint → .vm-help
    src = src.replace("class='hint'", "class='vm-help'")
    # Grids: .grid2 → .vm-grid
    src = src.replace("class='grid2'", "class='vm-grid'")

    # Input/select: add vm-input/vm-select classes
    # The existing HTML uses bare <input> and <select> — need to add classes
    # For selects with name= attribute (form selects)
    src = src.replace("<select name='ui_theme'>", "<select class='vm-select' name='ui_theme'>")
    src = src.replace("<select name='wifi_pref_ssid'>", "<select class='vm-select' name='wifi_pref_ssid'>")
    src = src.replace("<select name='wifi_setup_mode'>", "<select class='vm-select' name='wifi_setup_mode'>")
    src = src.replace("<select name='wc_lang'>", "<select class='vm-select' name='wc_lang'>")
    src = src.replace("<select name='wiki_lang'>", "<select class='vm-select' name='wiki_lang'>")
    src = src.replace("<select id='wifi_scan_results'>", "<select class='vm-select' id='wifi_scan_results'>")
    # Inputs with id= or name=
    src = src.replace("<input id='geo_query'", "<input class='vm-input' id='geo_query'")
    src = src.replace("<input id='weather_city'", "<input class='vm-input' id='weather_city'")
    src = src.replace("<input id='weather_lat'", "<input class='vm-input' id='weather_lat'")
    src = src.replace("<input id='weather_lon'", "<input class='vm-input' id='weather_lon'")
    src = src.replace("<input id='wifi_new_ssid'", "<input class='vm-input' id='wifi_new_ssid'")
    src = src.replace("<input id='wifi_new_password'", "<input class='vm-input' id='wifi_new_password'")
    src = src.replace("<input id='rss_name'", "<input class='vm-input' id='rss_name'")
    src = src.replace("<input id='rss_url'", "<input class='vm-input' id='rss_url'")
    src = src.replace("<input id='rss_max'", "<input class='vm-input' id='rss_max'")

    # ===== 11. REPLACE BUTTON CLASSES =====
    # Primary submit buttons
    src = src.replace("class='btn primary' type='submit'",
                      "class='vm-btn vm-btn--primary' type='submit'")
    src = src.replace("class='btn primary' type='button'",
                      "class='vm-btn vm-btn--primary' type='button'")
    # Ghost/secondary buttons
    src = src.replace("class='btn ghost' type='submit'",
                      "class='vm-btn vm-btn--secondary' type='submit'")
    src = src.replace("class='btn ghost' type='button'",
                      "class='vm-btn vm-btn--secondary' type='button'")
    src = src.replace("class='btn ghost sm' type='button'",
                      "class='vm-btn vm-btn--sm vm-btn--secondary' type='button'")
    src = src.replace("class='btn ghost sm secret-toggle' type='button'",
                      "class='vm-btn vm-btn--sm vm-btn--secondary' type='button'")

    # ===== 12. REPLACE FA ICONS IN BUTTONS/HINTS =====
    # Button icons
    src = src.replace("<i class='fa-solid fa-floppy-disk'></i>Save Config",
                      "Save Config")
    src = src.replace("<i class='fa-solid fa-rotate-right'></i>Force Weather + RSS + Wiki Reload",
                      "Force Reload")
    src = src.replace("<i class='fa-solid fa-circle-plus'></i>Add",
                      "+ Add")
    src = src.replace("<i class='fa-solid fa-broom'></i>Reset",
                      "Reset")
    src = src.replace("<i class='fa-solid fa-tower-cell'></i> Scan networks",
                      "Scan networks")
    # Hint icons (just drop them)
    src = src.replace("<i class='fa-solid fa-bolt'></i> ", "")
    src = src.replace("<i class='fa-solid fa-hand-pointer'></i> ", "")
    src = src.replace("<i class='fa-solid fa-circle-info'></i> ", "")
    src = src.replace("<i class='fa-solid fa-floppy-disk'></i> ", "")
    src = src.replace("<i class='fa-solid fa-satellite-dish'></i> ", "")
    src = src.replace("<i class='fa-solid fa-mobile-screen-button'></i> ", "")
    src = src.replace("<i class='fa-solid fa-terminal'></i> ", "")
    # Pill FA icons
    src = src.replace("<i class='fa-solid fa-rss'></i> ", "")

    # ===== 13. REPLACE VIEW CARD CLASSES =====
    src = src.replace("class='view-grid'", "class='vm-views'")
    src = src.replace("class='view-card'", "class='vm-view'")
    src = src.replace("class='view-card fixed'", "class='vm-view vm-view--fixed'")
    src = src.replace("class='view-card disabled'", "class='vm-view vm-view--off'")
    # The class is assembled conditionally for disabled:
    src = src.replace("class='view-copy'", "class='vm-view__copy'")

    # ===== 14. REPLACE INNER CARD CLASS =====
    # .card used inside sections (not the JS-created rss-row)
    # These are the static <div class='card'> inside sections
    # Be careful: only replace the CSS class, not the JS references
    src = src.replace("<div class='card'>", "<div class='vm-card--inner'>")

    # ===== 15. REPLACE RSS/COMPOSER CLASSES =====
    src = src.replace("class='rss-composer'", "class='vm-rss-composer'")
    src = src.replace("class='rss-list'", "class='vm-rss-list'")
    # Keep rss-row, rss-title, rss-meta, rss-chip, rss-actions — JS creates these
    src = src.replace("class='rss-status'", "class='rss-status'")  # keep
    src = src.replace("class='rss-empty'", "class='rss-empty'")  # keep

    # ===== 16. REPLACE ACTIONS/FOOTER =====
    src = src.replace("class='btns'", "class='vm-actions'")
    src = src.replace("class='site-footer'", "class='vm-footer'")
    src = src.replace("class='api-note'", "class='vm-api-note'")

    # ===== 17. REPLACE WIFI CLASSES =====
    src = src.replace("class='secret-input-wrap'", "class='vm-secret'")
    src = src.replace("class='setup-quick'", "class='vm-setup-grid'")
    src = src.replace("class='setup-qr'", "class='vm-setup-qr'")
    src = src.replace("class='setup-url'", "class='vm-setup-url'")

    # ===== 18. REPLACE SYSTEM INFO CLASSES =====
    # .card inside system info → .vm-card--inner + .vm-kv
    # These were already replaced by step 14. Just need to add vm-kv.
    # The system info cards use <small>key:</small><code>value</code> pattern
    # which matches .vm-kv styling

    # ===== 19. UPDATE PILL CLASS =====
    src = src.replace("class='pill'", "class='vm-badge vm-badge--info'")
    src = src.replace("class='badge-soon'", "class='vm-badge vm-badge--brand'")

    # ===== 20. UPDATE PASSWORD TOGGLE IN JS =====
    # Old: wifiPwdToggle.innerHTML=visible?"<i class='fa-solid fa-eye-slash'></i>":"<i class='fa-solid fa-eye'></i>"
    src = src.replace(
        """visible?\"<i class='fa-solid fa-eye-slash'></i>\":\"<i class='fa-solid fa-eye'></i>\"""",
        """visible?'Hide':'Show'"""
    )
    # Old: wifiPwdToggle.title=visible?'Hide password':'Show password'
    # Keep this — it's fine

    # ===== 21. UPDATE JS-CREATED BUTTON CLASSES =====
    src = src.replace("bEdit.className='btn sm warn'",
                      "bEdit.className='vm-btn vm-btn--sm vm-btn--warn'")
    src = src.replace("bDel.className='btn sm danger'",
                      "bDel.className='vm-btn vm-btn--sm vm-btn--danger'")
    # RSS add/clear button innerHTML (remove FA icons)
    src = src.replace("""rssAdd.innerHTML=\"<i class='fa-solid fa-circle-plus'></i>Add\"""",
                      """rssAdd.textContent='+ Add'""")
    src = src.replace("""rssAdd.innerHTML=\"<i class='fa-solid fa-floppy-disk'></i>Update\"""",
                      """rssAdd.textContent='Update'""")
    # RSS title icon in JS
    src = src.replace("""t.innerHTML=\"<i class='fa-solid fa-signal'></i>\"""",
                      """t.textContent=''""")
    # Edit/delete button text
    src = src.replace("""bEdit.innerHTML=\"<i class='fa-solid fa-pen-to-square'></i>Edit\"""",
                      """bEdit.textContent='Edit'""")
    src = src.replace("""bDel.innerHTML=\"<i class='fa-solid fa-trash-can'></i>Delete\"""",
                      """bDel.textContent='Delete'""")
    # RSS composer clear
    src = src.replace("""rssAdd.innerHTML=\"<i class='fa-solid fa-circle-plus'></i>Add\";setRssStatus""",
                      """rssAdd.textContent='+ Add';setRssStatus""")

    # ===== 22. FIX RSS COUNT PILL IN JS =====
    src = src.replace(
        """rssPill.innerHTML=\"<i class='fa-solid fa-rss'></i> RSS feeds \"+feeds.length+'/5'""",
        """rssPill.textContent='RSS feeds '+feeds.length+'/5'"""
    )

    # ===== 23. CLOSING TAGS — keep as-is (section+main both need closing) =====

    # ===== 24. PILL icon in RSS chip (JS-created) =====
    src = src.replace("chip.className='rss-chip'", "chip.className='rss-chip'")  # keep — styled in CSS

    # ===== 25. FIX view-card class for DOOM (conditional) =====
    # The DOOM view card is built with string concat:
    # html += F("<label class='view-card"); if (!doomFeatureAvailable) html += F(" disabled");
    # After our replacement it's: <label class='vm-view ...
    # Need to fix the disabled append:
    src = src.replace(
        """html += F(\"<label class='vm-view\");\n    if (!doomFeatureAvailable) html += F(\" disabled\");""",
        """html += F(\"<label class='vm-view\");\n    if (!doomFeatureAvailable) html += F(\" vm-view--off\");"""
    )

    # ===== DONE — WRITE =====
    new_len = len(src)
    saved = original_len - new_len
    print(f"Original: {original_len} bytes")
    print(f"New:      {new_len} bytes")
    print(f"Saved:    {saved} bytes ({saved*100//original_len}%)")

    with open(INO, 'w') as f:
        f.write(src)
    print("✓ scrybar.ino updated successfully")

if __name__ == '__main__':
    main()
