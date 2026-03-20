# ScryBar Landing Page

Static landing page — Astro SSG, zero JS runtime.

## Quick start

```bash
cd web
npm install
npm run dev
```

Opens on `http://localhost:5173`.

## What's here

- **Hero** — tagline, badges, CTA buttons (Flash Firmware / Explore Features)
- **Device mockup** — real screenshots, theme selector, hover scale + glass glare
- **Language preview** — rotating word-clock sentences in 13 languages
- **Features grid** — highlights with icons
- **Flash section** — ESP Web Tools integration (manifests in `public/`)
- **Specs + Footer**

## Stack

| Layer | Tech |
|-------|------|
| Build | Astro 5 (SSG) |
| UI | Astro components (`.astro`) |
| Interactivity | Vanilla JS inlined (~1.5 KB) |
| Styling | Vanilla CSS (tokens + component files) |
| Fonts | Google Fonts async, system fallback |

## Project structure

```
web/
  astro.config.mjs      # Astro config (port 5173)
  package.json
  public/
    manifest-full.json   # ESP Web Tools manifest (full flash)
    manifest-lite.json   # ESP Web Tools manifest (lite flash)
  src/
    layouts/
      Layout.astro       # HTML shell, CSS imports, font loading
    pages/
      index.astro        # main page composition + scroll-reveal
    components/
      Nav.astro          # top navigation bar
      DeviceMockup.astro # 3D hover + glare effect
      ThemeSelector.astro # theme pill buttons
      LanguagePreview.astro # cycling word-clock sentences
      Features.astro     # feature grid
      FlashSection.astro # ESP Web Tools flash buttons
      Specs.astro        # hardware specs table
      Footer.astro       # footer
    styles/
      tokens.css         # design tokens (5 themes, colors, fonts)
      global.css         # base styles, animated background
      device.css         # device mockup + glare
      sections.css       # feature grid, flash, specs, footer
    assets/
      screenshots/       # device display captures (640x172)
```

## Theme system

Five themes, unified typography (Syne + Montserrat everywhere). Theme switch
is instant with smooth CSS color transitions (~0.45s) — no flash, no layout
shift. Only colors, borders, shadows, and glows crossfade.

Themes: `scrybar-default`, `cyberpunk-2077`, `toxic-candy`, `tokyo-transit`,
`minimal-brutalist-mono`.

## Build output

```bash
npm run build   # → dist/ (static HTML + CSS, ~10.5 KB gzipped, no JS files)
npm run preview # serve the built site locally
```
