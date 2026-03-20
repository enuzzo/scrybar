# ScryBar Landing Page

Dev preview of the landing page — Vite + React, pre-SSG migration.

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
| Build | Vite 6 |
| UI | React 19 |
| Styling | Vanilla CSS (tokens + component files) |
| Fonts | Google Fonts async, system fallback |

## Project structure

```
web/
  index.html            # entry point
  vite.config.js        # dev server config
  package.json
  public/
    manifest-full.json  # ESP Web Tools manifest (full flash)
    manifest-lite.json  # ESP Web Tools manifest (lite flash)
  src/
    main.jsx            # React mount
    App.jsx             # root component
    data.js             # theme screenshots, language data
    components/
      DeviceMockup.jsx  # 3D hover + glare effect
      ThemeSelector.jsx # theme pill buttons
      LanguagePreview.jsx
      Features.jsx
      FlashSection.jsx
      Specs.jsx
      Footer.jsx
    styles/
      tokens.css        # design tokens (colors, fonts, spacing)
      global.css        # base styles, animated background
      device.css        # device mockup + glare
      sections.css      # feature grid, flash, specs, footer
```

## Next steps

Migration to static/SSG (Astro or pre-render) for:
- Zero JS runtime — current React bundle is ~140KB gzipped for static content
- Perfect SEO — server-rendered HTML, meta tags, structured data
- Lighthouse 100 across the board
