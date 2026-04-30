# Web UI — Vibemilk Redesign

> **For agentic workers:** REQUIRED: Use superpowers:executing-plans to implement this plan. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the ScryBar ESP32 web config UI's custom CSS with a vibemilk design system subset — stripping all animations, FX grid, backdrop-filter, and Font Awesome while keeping every form section and JS logic intact.

**Architecture:** A CSS "bridge" maps the firmware's existing `UiThemeWebTokens` variables (`--txt`, `--acc1`, etc.) to vibemilk-standard names (`--text-primary`, `--accent-primary`). Vibemilk component classes (`vm-card`, `vm-btn`, `vm-input`, `vm-select`) replace inline custom CSS. No C++ struct changes needed — the bridge is pure CSS `var()` aliasing.

**Tech Stack:** Arduino/ESP32 C++ (inline HTML generation), Vibemilk DS v3 subset (pure CSS custom properties), vanilla JS (unchanged).

**Metrics:**
- Current output: ~35KB → Target: ~20KB
- Current animations: 5 keyframes → Target: 0
- Current FX divs: 8 → Target: 0
- Font Awesome refs: ~40 `<i class='fa-*'>` → Target: 0 (unicode replacements)

---

### Task 1: CSS Bridge + Vibemilk Subset

**Files:**
- Modify: `scrybar.ino:3882-3919` (`appendWebThemeCssVars`)
- Modify: `scrybar.ino:3975-4020` (inline CSS in `buildWebConfigPage`)

This replaces ALL current CSS with two blocks:
1. Firmware token injection (keep `appendWebThemeCssVars` as-is)
2. Bridge + vibemilk subset (~3KB minified)

- [ ] **Step 1: Replace CSS block (lines 3978-4020)**

Remove:
- All `@keyframes` (gridScroll, gridGlowPulse, scanMove, vlinePulse, savePulse)
- All `.fx-grid*` classes
- All `backdrop-filter` / `-webkit-backdrop-filter` declarations
- All `.hero*`, `.release-box`, `.panel`, `.pill` custom classes
- Entire `body[data-theme='minimal-brutalist-mono']` override block
- All hardcoded color values in CSS (use tokens instead)

Add:
- CSS bridge block mapping `var(--txt)` → `var(--text-primary)`, etc.
- Vibemilk subset: reset, card, button, form, badge, alert, layout utilities
- Minimal responsive: single breakpoint at 768px (was 3 breakpoints)

Key bridge mappings:
```css
--font-family: var(--font-main);
--text-primary: var(--txt);
--text-secondary: var(--txt2);
--text-tertiary: var(--txt3);
--accent-primary: var(--acc1);
--accent-secondary: var(--acc2);
--bg-input: var(--bg-deep);
--bg-input-focus: var(--bg-surface);
--bg-elevated: var(--bg-surface);
--stroke-default: var(--line);
--stroke-subtle: var(--line-soft);
```

- [ ] **Step 2: Verify CSS compiles**

Run: `arduino-cli compile --clean --build-path /tmp/arduino-build-scrybar --fqbn esp32:esp32:esp32s3:UploadSpeed=921600,USBMode=hwcdc,CDCOnBoot=cdc,CPUFreq=240,FlashMode=qio,FlashSize=16M,PartitionScheme=custom,PSRAM=opi <REPO_ROOT>`

Expected: Compiles successfully. No string literal errors.

---

### Task 2: HTML Markup — Header, Hero, Status Message

**Files:**
- Modify: `scrybar.ino:4021-4036` (body tag, FX grid divs, hero, status message)

- [ ] **Step 1: Replace body + FX grid + hero**

Remove:
- `<div class='fx-grid'>` and all 8 child divs
- `.hero-top-card`, `.hero-left`, `.hero-right`, `.hero-copy` wrapper divs
- `.release-box` grid with Font Awesome icons
- Font Awesome `<link>` and `<noscript>` fallback (lines 3972-3974)

Replace with:
- Clean `<body>` with `data-theme` attribute (keep)
- `<main class='vm-wrap'>` (single centered container)
- Hero as `vm-card` with logo + version badge inline
- Release info as `vm-badge` pills instead of grid
- Lede text in a simple `<p>` inside card body

- [ ] **Step 2: Replace status message**

Current: `<p class='msg fixed-top'>` with custom styling
Replace with: `<div class='vm-alert vm-alert--success vm-toast-fixed'>` using vibemilk alert component

---

### Task 3: HTML Markup — All Form Sections

**Files:**
- Modify: `scrybar.ino:4037-4202` (form, all 8 config sections)

- [ ] **Step 1: Replace section containers**

Current: `<div class='sec'><h2><i class='fa-*'></i>Title</h2>`
Replace with: `<section class='vm-card'><header class='vm-card__header'><h2 class='vm-card__title'>EMOJI Title</h2></header><div class='vm-card__body'>`

Icon replacements:
- `fa-palette` → `🎨`
- `fa-table-cells-large` → `📱`
- `fa-wifi` → `📶`
- `fa-language` → `🌐`
- `fa-book-open` → `📖`
- `fa-location-dot` → `📍`
- `fa-square-rss` → `📡`
- `fa-microchip` → `⚙️`

- [ ] **Step 2: Replace form controls**

Current: bare `<input>`, `<select>` with global CSS
Replace with: `<input class='vm-input'>`, `<select class='vm-select'>` with vibemilk classes

Current: `.key` labels → `<label class='vm-label text-overline'>`
Current: `.hint` paragraphs → `<p class='vm-help-text'>`

- [ ] **Step 3: Replace buttons**

Current: `<button class='btn primary'>` / `<button class='btn ghost'>`
Replace with: `<button class='vm-btn vm-btn--md vm-btn--primary'>` / `<button class='vm-btn vm-btn--md vm-btn--secondary'>`

Also update JS-created buttons:
- `bEdit.className='btn sm warn'` → `bEdit.className='vm-btn vm-btn--sm vm-btn--secondary'`
- `bDel.className='btn sm danger'` → `bDel.className='vm-btn vm-btn--sm vm-btn--danger'`
- RSS add/reset buttons
- WiFi scan button

- [ ] **Step 4: Replace pills/badges**

Current: `.pill`, `.rss-chip`, `.badge-soon`
Replace with: `vm-badge vm-badge--brand` / `vm-badge vm-badge--info`

- [ ] **Step 5: Replace view cards (checkboxes)**

Current: `.view-card` with custom checkbox layout
Replace with: `<label class='vm-checkbox'>` wrapper or keep custom but styled with tokens

- [ ] **Step 6: Replace RSS composer + list**

Keep `.rss-composer` grid layout but style with tokens.
Keep `.rss-row` class name (JS creates these) but restyle with vm-card tokens.

---

### Task 4: HTML Markup — System Info, Footer, JS Updates

**Files:**
- Modify: `scrybar.ino:4203-4315` (system info, footer, JS, closing tags)

- [ ] **Step 1: Replace System Info section**

Current: `.grid2 > .card` with `<small>key:</small><code>value</code>` pattern
Replace with: `vm-card` cards using `vm-stat` or simple key-value pairs with `vm-label` / `<code>`

- [ ] **Step 2: Replace footer**

Current: `.site-footer` with inline styles
Replace with: `<footer>` using typography tokens, minimal styling

- [ ] **Step 3: Update JS class references**

In the JavaScript block, update all `className` assignments to use new vm-* classes:
- `row.className='rss-row'` → keep (restyled via CSS)
- `t.className='rss-title'` → keep (restyled via CSS)
- Button classNames → use vm-btn classes
- `chip.className='rss-chip'` → `vm-badge vm-badge--info`
- Badge/icon innerHTML → remove Font Awesome, use unicode

- [ ] **Step 4: Remove Font Awesome icon references from JS**

Current: `bEdit.innerHTML="<i class='fa-solid fa-pen-to-square'></i>Edit"`
Replace with: `bEdit.textContent='Edit'` (or use unicode ✏️)

- [ ] **Step 5: Final compile + upload + verify**

Run compile, upload to device, open web UI in browser.
Verify all 5 themes render correctly.
Verify all form interactions work (WiFi scan, geo search, RSS builder, save).
Verify mobile layout (responsive).

- [ ] **Step 6: Bump version + commit**

Bump `FW_BUILD_TAG` in `config.h` to next r-number.
Bump `FW_RELEASE_DATE`.

```bash
git add scrybar.ino config.h
git commit -m "refactor(web-ui): replace custom CSS with vibemilk DS subset

Strip all animations (5 keyframes), FX grid (8 divs), backdrop-filter,
and Font Awesome dependency. Use vibemilk vm-* classes with CSS bridge
mapping firmware tokens to standard vibemilk variables.

~35KB → ~20KB output. Zero CDN dependencies for functionality."
```

---

## Reference: Unicode Icon Map

| Context | Old (Font Awesome) | New (Unicode) |
|---------|-------------------|---------------|
| Theme section | `fa-palette` | 🎨 |
| Views section | `fa-table-cells-large` | ⊞ |
| WiFi section | `fa-wifi` | ⏣ |
| Language | `fa-language` | 🌐 |
| Wikipedia | `fa-book-open` | 📖 |
| Location | `fa-location-dot` | 📍 |
| RSS | `fa-square-rss` | 📡 |
| System Info | `fa-microchip` | ⚙ |
| Save button | `fa-floppy-disk` | — (text only) |
| Reload button | `fa-rotate-right` | — (text only) |
| Hint icon | `fa-circle-info` | — (drop) |
| Calendar | `fa-calendar` | — (text only) |
| Version | `fa-code-branch` | — (text only) |
| Scan button | `fa-tower-cell` | — (text only) |
| Eye toggle | `fa-eye` / `fa-eye-slash` | 👁 / ⊘ |
| RSS feed icon | `fa-signal` | — (drop) |
| Add | `fa-circle-plus` | + |
| Broom | `fa-broom` | ↺ |
| Edit | `fa-pen-to-square` | — (text only) |
| Delete | `fa-trash-can` | — (text only) |
| Terminal | `fa-solid fa-terminal` | $ |
