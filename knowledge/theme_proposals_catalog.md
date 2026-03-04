# ScryBar Theme Proposals Catalog

Living document for visual theme exploration.

Goal: keep a durable backlog of theme ideas that can be implemented first in the design system, then migrated to firmware + web UI runtime theming.

Last update: 2026-03-04

---

## 1) Working Rules

- Keep existing ScryBar naming/token contract intact (no token or class renames).
- Flow is always:
  1. Design system prototype (`assets/scrybar_design_system/`)
  2. Validation pass
  3. Firmware + web UI token port (`kUiThemes[]` and web CSS vars)
- Fonts must be static TTF files (non-variable).
- New themes should define both:
  - Web token overrides (`UiThemeWebTokens` equivalent)
  - LVGL token overrides (`UiThemeLvglTokens` equivalent)

---

## 2) Token Contract (do not rename)

Minimum token group to fill for each new theme:

- `--bg-deepest`, `--bg-deep`, `--bg-surface`, `--bg-elevated`
- `--text-primary`, `--text-secondary`, `--text-tertiary`
- `--accent-primary`, `--accent-primary-hover`, `--accent-primary-active`
- `--accent-secondary`, `--accent-secondary-hover`
- `--color-success`, `--color-warning`, `--color-danger`, `--color-info`
- `--stroke-width-ui`, `--stroke-width-strong`
- `--shadow-sm`, `--shadow-md`, `--shadow-glow-sm`, `--shadow-glow-md`
- `--font-family`, `--font-mono`

Optional but recommended for themed personality:

- `--gradient-brand`, `--gradient-card`, `--gradient-surface`
- `--fx-grid-*` tokens for animated section coherence
- chamfer/corner tokens for sci-fi or geometric styles

---

## 3) Font Sourcing Policy (static only)

For every theme:

1. Pick Google Font family with static TTF.
2. Track two links:
   - Google Fonts specimen page
   - Google Fonts repo folder (for reproducible TTF source)
3. Store selected TTF under `assets/fonts/`.
4. Convert with `lv_font_conv --no-compress` into `src/fonts/` in needed sizes.

Baseline references:

- Google Fonts: <https://fonts.google.com/>
- Google Fonts repo: <https://github.com/google/fonts>

---

## 4) Theme Matrix (18 candidates)

| # | Theme ID (proposal) | Mood | Accent Pair | Recommended Font |
|---|---|---|---|---|
| 1 | `toxic-candy-v2` | playful, neon | `#ff2fd1` + `#86ff00` | Delius Unicase |
| 2 | `geocities-turbo` | retro web, fun | `#00a2ff` + `#ff00a8` | VT323 |
| 3 | `y2k-chrome-pop` | glossy future-retro | `#7df9ff` + `#ff6ec7` | Audiowide |
| 4 | `vapor-mallsoft` | dreamy nostalgic | `#ff77c8` + `#6cf6ff` | Monoton |
| 5 | `arcade-operator` | arcade control UI | `#00ff66` + `#ff004d` | Press Start 2P |
| 6 | `synthwave-sunset` | neon outrun | `#ff4d6d` + `#00f5ff` | Michroma |
| 7 | `frutiger-aero-reboot` | clean glossy optimism | `#4ac0ff` + `#67e8b4` | Varela Round |
| 8 | `solarpunk-grid` | eco-tech hopeful | `#2e7d32` + `#f6b93b` | Rajdhani |
| 9 | `neo-brutal-control` | bold high contrast | `#ff3b30` + `#2d7dff` | Archivo Black |
|10 | `bauhaus-signal` | geometric modernist | `#d90429` + `#ffbe0b` | Jura |
|11 | `art-deco-console` | luxe geometric | `#d4af37` + `#5de2e7` | Cinzel |
|12 | `noir-crt` | analog mystery | `#f7f2e8` + `#4affd0` | Special Elite |
|13 | `industrial-amber` | rugged instrument panel | `#ffbf00` + `#5d6d7e` | Teko |
|14 | `deep-ocean-sonar` | calm technical | `#14b8a6` + `#7dd3fc` | Share Tech Mono |
|15 | `desert-radar` | warm tactical | `#ff7b00` + `#00c2a8` | Quantico |
|16 | `hologram-museum` | elegant sci-fi | `#b8b8ff` + `#7afcff` | Syncopate |
|17 | `retro-terminal-olive` | vintage terminal | `#c7d36f` + `#7f8c4b` | Space Mono |
|18 | `sakura-circuit` | candy-tech chic | `#ff5fa2` + `#8ef9f3` | Poiret One |

### Implemented and removed from proposal queue

- `tokyo-transit` moved to active design-system + firmware/web runtime themes on 2026-03-04.
- `minimal-brutalist-mono` moved to active design-system + firmware/web runtime themes on 2026-03-04.

---

## 5) Detailed Theme Cards

Each card includes:
- Palette seed
- Token mapping hint
- Typography package
- Visual language guide
- Research references

### 5.1 `toxic-candy-v2`

- Vibe: hyper-pop control panel, playful but readable.
- Seed palette:
  - bg deepest `#120014`
  - bg surface `#2a0b34`
  - text primary `#fff4fb`
  - accent A `#ff2fd1`
  - accent B `#86ff00`
- Token mapping hint:
  - `--accent-primary: #ff2fd1`
  - `--accent-secondary: #86ff00`
  - success can reuse accent B for coherent "fluo positive" language.
- Typography:
  - primary: Delius Unicase
  - mono: Space Mono
- Font references:
  - <https://fonts.google.com/specimen/Delius+Unicase>
  - <https://github.com/google/fonts/tree/main/ofl/deliusunicase>
- Study references:
  - <https://en.wikipedia.org/wiki/Vaporwave>
  - <https://en.wikipedia.org/wiki/Neon_art>

### 5.2 `geocities-turbo`

- Vibe: late 90s / early 2000 web nostalgia, loud and fun.
- Seed palette:
  - bg deepest `#001133`
  - bg surface `#003366`
  - text primary `#eaf6ff`
  - accent A `#00a2ff`
  - accent B `#ff00a8`
- Token mapping hint:
  - use tiled subtle backgrounds only in decorative layers, never under body text.
- Typography:
  - primary + mono: VT323
- Font references:
  - <https://fonts.google.com/specimen/VT323>
  - <https://github.com/google/fonts/tree/main/ofl/vt323>
- Study references:
  - <https://en.wikipedia.org/wiki/GeoCities>
  - <https://www.cameronsworld.net/>
  - <https://geocities.restorativland.org/>

### 5.3 `y2k-chrome-pop`

- Vibe: polished metallic Y2K interface with glossy accents.
- Seed palette:
  - bg deepest `#0e1020`
  - bg surface `#1b2142`
  - text primary `#eef3ff`
  - accent A `#7df9ff`
  - accent B `#ff6ec7`
- Typography:
  - primary: Audiowide
  - mono: Space Mono
- Font references:
  - <https://fonts.google.com/specimen/Audiowide>
  - <https://github.com/google/fonts/tree/main/ofl/audiowide>
- Study references:
  - <https://en.wikipedia.org/wiki/Y2K_aesthetic>
  - <https://en.wikipedia.org/wiki/Web_2.0>

### 5.4 `vapor-mallsoft`

- Vibe: pastel neon nostalgia, slow and dreamy.
- Seed palette:
  - bg deepest `#1b1030`
  - bg surface `#2d1a4d`
  - text primary `#ffeefd`
  - accent A `#ff77c8`
  - accent B `#6cf6ff`
- Typography:
  - primary: Monoton (display) + fallback Varela Round for body readability
  - mono: Space Mono
- Font references:
  - <https://fonts.google.com/specimen/Monoton>
  - <https://github.com/google/fonts/tree/main/ofl/monoton>
- Study references:
  - <https://en.wikipedia.org/wiki/Mallsoft>
  - <https://en.wikipedia.org/wiki/Vaporwave>

### 5.5 `arcade-operator`

- Vibe: arcade cabinet HUD, energetic.
- Seed palette:
  - bg deepest `#0b0f1a`
  - bg surface `#121a29`
  - text primary `#f7f7f7`
  - accent A `#00ff66`
  - accent B `#ff004d`
- Typography:
  - primary: Press Start 2P (sparingly)
  - body fallback: Space Mono
  - mono: Space Mono
- Font references:
  - <https://fonts.google.com/specimen/Press+Start+2P>
  - <https://github.com/google/fonts/tree/main/ofl/pressstart2p>
- Study references:
  - <https://en.wikipedia.org/wiki/Pixel_art>
  - <https://en.wikipedia.org/wiki/Arcade_game>

### 5.6 `synthwave-sunset`

- Vibe: neon highway, strong gradients and horizon glow.
- Seed palette:
  - bg deepest `#120824`
  - bg surface `#22153b`
  - text primary `#fdf0ff`
  - accent A `#ff4d6d`
  - accent B `#00f5ff`
- Typography:
  - primary: Michroma
  - mono: Space Mono
- Font references:
  - <https://fonts.google.com/specimen/Michroma>
  - <https://github.com/google/fonts/tree/main/ofl/michroma>
- Study references:
  - <https://en.wikipedia.org/wiki/Synthwave>
  - <https://en.wikipedia.org/wiki/Out_Run>

### 5.7 `frutiger-aero-reboot`

- Vibe: optimistic glossy UI, readable and airy.
- Seed palette:
  - bg deepest `#dff4ff`
  - bg surface `#f4fbff`
  - text primary `#0a355a`
  - accent A `#4ac0ff`
  - accent B `#67e8b4`
- Token note:
  - This is one of the few light-first proposals. Weather card will naturally fit icon pack.
- Typography:
  - primary: Varela Round
  - mono: Space Mono
- Font references:
  - <https://fonts.google.com/specimen/Varela+Round>
  - <https://github.com/google/fonts/tree/main/ofl/varelaround>
- Study references:
  - <https://en.wikipedia.org/wiki/Frutiger_Aero>

### 5.8 `solarpunk-grid`

- Vibe: ecological future + technical instrumentation.
- Seed palette:
  - bg deepest `#f0ffe8`
  - bg surface `#e6f7da`
  - text primary `#1b5e20`
  - accent A `#2e7d32`
  - accent B `#f6b93b`
- Typography:
  - primary: Rajdhani
  - mono: Space Mono
- Font references:
  - <https://fonts.google.com/specimen/Rajdhani>
  - <https://github.com/google/fonts/tree/main/ofl/rajdhani>
- Study references:
  - <https://en.wikipedia.org/wiki/Solarpunk>

### 5.9 `neo-brutal-control`

- Vibe: loud, blocky, high contrast "poster UI".
- Seed palette:
  - bg deepest `#fffdf8`
  - bg surface `#ffffff`
  - text primary `#111111`
  - accent A `#ff3b30`
  - accent B `#2d7dff`
- Typography:
  - primary: Archivo Black
  - mono: IBM Plex Mono
- Font references:
  - <https://fonts.google.com/specimen/Archivo+Black>
  - <https://github.com/google/fonts/tree/main/ofl/archivoblack>
- Study references:
  - <https://blog.logrocket.com/ux-design/web-brutalism/>

### 5.10 `bauhaus-signal`

- Vibe: geometric modernism with primary/secondary color logic.
- Seed palette:
  - bg deepest `#fff7e6`
  - bg surface `#fff2d1`
  - text primary `#1d3557`
  - accent A `#d90429`
  - accent B `#ffbe0b`
- Typography:
  - primary: Jura
  - mono: Space Mono
- Font references:
  - <https://fonts.google.com/specimen/Jura>
  - <https://github.com/google/fonts/tree/main/ofl/jura>
- Study references:
  - <https://en.wikipedia.org/wiki/Bauhaus>
  - <https://en.wikipedia.org/wiki/De_Stijl>

### 5.11 `art-deco-console`

- Vibe: luxury geometry, elegant lines and metallic accents.
- Seed palette:
  - bg deepest `#0f1014`
  - bg surface `#1a1d24`
  - text primary `#f5f1e8`
  - accent A `#d4af37`
  - accent B `#5de2e7`
- Typography:
  - primary: Cinzel
  - mono: Space Mono
- Font references:
  - <https://fonts.google.com/specimen/Cinzel>
  - <https://github.com/google/fonts/tree/main/ofl/cinzel>
- Study references:
  - <https://en.wikipedia.org/wiki/Art_Deco>

### 5.12 `noir-crt`

- Vibe: detective terminal, warm paper + phosphor glow.
- Seed palette:
  - bg deepest `#090909`
  - bg surface `#161616`
  - text primary `#f7f2e8`
  - accent A `#4affd0`
  - accent B `#ffb347`
- Typography:
  - primary: Special Elite
  - mono: Space Mono
- Font references:
  - <https://fonts.google.com/specimen/Special+Elite>
  - <https://github.com/google/fonts/tree/main/ofl/specialelite>
- Study references:
  - <https://en.wikipedia.org/wiki/Cathode-ray_tube>
  - <https://en.wikipedia.org/wiki/Film_noir>

### 5.13 `industrial-amber`

- Vibe: instrumentation panel in factory/lab tools.
- Seed palette:
  - bg deepest `#101418`
  - bg surface `#1a2229`
  - text primary `#f2f2f2`
  - accent A `#ffbf00`
  - accent B `#5d6d7e`
- Typography:
  - primary: Teko
  - mono: Space Mono
- Font references:
  - <https://fonts.google.com/specimen/Teko>
  - <https://github.com/google/fonts/tree/main/ofl/teko>
- Study references:
  - <https://material.io/design/color/the-color-system.html>

### 5.14 `deep-ocean-sonar`

- Vibe: deep-sea technical interface, calm and precise.
- Seed palette:
  - bg deepest `#031b2f`
  - bg surface `#0d2740`
  - text primary `#e2f3ff`
  - accent A `#14b8a6`
  - accent B `#7dd3fc`
- Typography:
  - primary + mono: Share Tech Mono
- Font references:
  - <https://fonts.google.com/specimen/Share+Tech+Mono>
  - <https://github.com/google/fonts/tree/main/ofl/sharetechmono>
- Study references:
  - <https://en.wikipedia.org/wiki/Sonar>

### 5.15 `desert-radar`

- Vibe: warm tactical HUD, sunset radar tones.
- Seed palette:
  - bg deepest `#23160f`
  - bg surface `#3a2416`
  - text primary `#ffe8d0`
  - accent A `#ff7b00`
  - accent B `#00c2a8`
- Typography:
  - primary: Quantico
  - mono: Space Mono
- Font references:
  - <https://fonts.google.com/specimen/Quantico>
  - <https://github.com/google/fonts/tree/main/ofl/quantico>
- Study references:
  - <https://en.wikipedia.org/wiki/Head-up_display>

### 5.16 `hologram-museum`

- Vibe: curated futuristic exhibition.
- Seed palette:
  - bg deepest `#161925`
  - bg surface `#1f2433`
  - text primary `#f5f5ff`
  - accent A `#b8b8ff`
  - accent B `#7afcff`
- Typography:
  - primary: Syncopate
  - mono: Space Mono
- Font references:
  - <https://fonts.google.com/specimen/Syncopate>
  - <https://github.com/google/fonts/tree/main/ofl/syncopate>
- Study references:
  - <https://en.wikipedia.org/wiki/Holography>

### 5.17 `retro-terminal-olive`

- Vibe: classic terminal monitor, military-olive phosphor.
- Seed palette:
  - bg deepest `#10160f`
  - bg surface `#1a2318`
  - text primary `#edf2d0`
  - accent A `#c7d36f`
  - accent B `#7f8c4b`
- Typography:
  - primary + mono: Space Mono
- Font references:
  - <https://fonts.google.com/specimen/Space+Mono>
  - <https://github.com/google/fonts/tree/main/ofl/spacemono>
- Study references:
  - <https://en.wikipedia.org/wiki/Monochrome_monitor>

### 5.18 `sakura-circuit`

- Vibe: soft fashion-tech, bright and elegant.
- Seed palette:
  - bg deepest `#1a0f1f`
  - bg surface `#2a1630`
  - text primary `#ffe8f4`
  - accent A `#ff5fa2`
  - accent B `#8ef9f3`
- Typography:
  - primary: Poiret One
  - mono: Space Mono
- Font references:
  - <https://fonts.google.com/specimen/Poiret+One>
  - <https://github.com/google/fonts/tree/main/ofl/poiretone>
- Study references:
  - <https://en.wikipedia.org/wiki/Kawaii>
  - <https://en.wikipedia.org/wiki/Cyberpunk>

---

## 6) Recommended Next Batch (Design System first)

Priority order for the next implementation cycle:

1. `frutiger-aero-reboot`
2. `geocities-turbo`
3. `neo-brutal-control`

Reason:

- Strong visual diversity versus existing 3 themes.
- Good validation of both light and dark theme behavior.
- Useful stress test for token consistency, readability, and weather card fallback rules.

---

## 7) Ready-to-Use Theme Intake Checklist

Before moving a proposal from catalog to implementation:

1. Confirm final palette (5 core colors + semantic colors).
2. Confirm static font package and fallback stack.
3. Define edge geometry: rounded vs chamfered vs square.
4. Define hover/focus/active contrast for dropdown and buttons.
5. Validate responsive layout in design system (desktop + mobile).
6. Validate weather panel readability rule.
7. Port to firmware/web runtime tokens.
8. Capture screenshots for README previews if approved.
