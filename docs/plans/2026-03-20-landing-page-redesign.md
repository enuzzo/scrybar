# ScryBar Landing Page — Redesign Brief

## What This Document Is

A comprehensive brief for the next session/agent to redesign the ScryBar public-facing landing page. This replaces the current minimal `docs/flasher/index.html` with a polished, interactive product page.

---

## Vision

The landing page is the **first thing** someone sees when they discover ScryBar. It should communicate what ScryBar is, look stunning, let people flash firmware directly from the browser, and — most importantly — let them **try the themes live** on an interactive device mockup before flashing.

Think: Apple product page meets developer tool. Dark, elegant, interactive. Not a docs page with a button.

## Core Concept: Interactive Theme Preview

The hero feature is an **isometric or 3D rendering of the ScryBar hardware** (the Waveshare 3.49" bar-shaped touchscreen) that updates in real-time as the user switches themes.

### How It Works

1. User sees a beautiful 3D/isometric illustration of the physical ScryBar device
2. Below or beside it, **5 theme selector buttons** (one per theme)
3. Clicking a theme updates the device mockup's screen to show that theme's color palette applied to a representative UI (e.g., the word clock HOME view or weather panel)
4. The page's own CSS also subtly adapts to echo the selected theme (background tints, accent colors)
5. **Language selector** — switching language updates the word clock sentence on the mockup in real-time (e.g., "Sono le undici e ventitré" → "It's eleven twenty-three" → "tlhIngan Hol: cha'maH wej")
6. The visitor gets a **live preview of what their ScryBar will look and feel like** before they even flash it — themes + language, all interactive
7. Below: firmware flash buttons (ESP Web Tools, already working), feature list, specs

The key insight from the user: *"chi visita il sito per flashare e scoprire il progetto, può anche vedere come funzionerebbe in realtime, cambiando effetti, cambiando lingua"* — the page IS the product demo.

### The 5 Themes

| ID | Display Name | Vibe |
|---|---|---|
| `scrybar-default` | ScryBar Default | Deep navy blues, purple/cyan accents, clean and modern |
| `cyberpunk-2077` | Cyberpunk 2077 | Hot yellow on dark, neon grid, tech dystopia |
| `toxic-candy` | Toxic Candy | Acid green/magenta on black, rave/candy aesthetic |
| `tokyo-transit` | Tokyo Transit | Warm neutrals, Japanese transit signage, calm and precise |
| `minimal-brutalist-mono` | Minimal Brutalist Mono | Pure black/white, monospace, no decoration, raw |

Each theme has existing screenshots in `assets/readme_previews/home_weather_*.png` that can be used as the "screen content" inside the device mockup.

### Device Mockup Options (in order of preference)

1. **CSS 3D transform** — Pure CSS isometric box with a screen area showing theme screenshots. No dependencies, lightweight, impressive. Rotate on hover.
2. **Three.js / React Three Fiber** — Actual 3D model of the bar, rotate with mouse/gyroscope. Heavier but stunning.
3. **SVG isometric illustration** — Hand-crafted SVG with a `<foreignObject>` or `<image>` swap for the screen content. Clean, scalable, fast.
4. **Flat mockup with perspective** — CSS `perspective` + `rotateY` on a div shaped like the bar. Simplest viable option.

The physical device is a **long narrow bar** (roughly 15:1 aspect ratio), 172px tall by 640px wide in its display area. Think of it like a desk-mounted LED strip with a screen. USB-C on the left, speaker grille on top.

## Tech Stack

| Layer | Choice | Why |
|---|---|---|
| Build | **Vite** | Fast dev, good React support, static output for GitHub Pages |
| Framework | **React** (or Preact for size) | Component state for theme switching, smooth transitions |
| Styling | **Vibemilk DS v3** | Our own design system, already supports all 5 themes via CSS custom properties |
| 3D (optional) | CSS 3D transforms or React Three Fiber | Interactive device mockup |
| Flashing | **ESP Web Tools** (`esp-web-tools@10`) | Already working, Web Serial API, zero backend |
| Deploy | **GitHub Pages** via `actions/deploy-pages` | Already configured, CI builds firmware binaries |

### Vibemilk Design System

The complete Vibemilk DS is available at:
- **Source:** `assets/Vibemilk - Netmilk Design System/` (in this repo)
- **Live:** [netmilk.ch/vibemilk](https://netmilk.ch/vibemilk)
- **What it provides:** CSS custom properties, component classes (`vm-card`, `vm-btn`, `vm-input`, `vm-badge`, etc.), dark theme tokens, responsive utilities
- **Key:** The DS was built FOR ScryBar originally, then extracted as a general DS. It's native here.

## Page Structure

```
[HERO]
  - ScryBar logo/wordmark
  - One-liner tagline
  - Interactive 3D/isometric device mockup
  - Theme selector (5 buttons/pills)
  - "Flash Now" CTA button (scrolls to flash section)

[FEATURES]
  - Grid of feature cards with icons
  - Word clock (13 languages), Weather, RSS, Wikipedia, DOOM
  - Swipe navigation, 5 themes, LAN web config
  - Now Playing (macOS companion)

[FLASH FIRMWARE]
  - Two variant cards (Full with DOOM, Lite without)
  - ESP Web Tools install buttons
  - Post-flash setup instructions (WiFi AP, config URL)
  - Browser compatibility note (Chrome/Edge only)

[SPECS]
  - Hardware: Waveshare ESP32-S3 3.49" touch bar
  - Display: 640x172 IPS LCD, AXS15231B
  - Connectivity: WiFi 2.4GHz, USB-C
  - Sensors: IMU (QMI8658), battery monitor
  - Storage: 16MB flash, OPI PSRAM

[FOOTER]
  - Netmilk Studio credit
  - GitHub link
  - MIT License
```

## Existing Flasher Infrastructure

The web flasher pipeline is already built and merged (`feature/web-flasher` branch):

- **`docs/flasher/index.html`** — Current minimal flasher page (to be replaced by the new landing page)
- **`docs/flasher/manifest-full.json`** / **`manifest-lite.json`** — ESP Web Tools manifests pointing to firmware binaries
- **`.github/workflows/build-firmware.yml`** — CI workflow that builds Full + Lite variants and deploys to GitHub Pages
- **`.github/ci-secrets.h`** — Dummy secrets for CI compilation

The new landing page must integrate with this existing infrastructure. The ESP Web Tools install buttons reference manifest JSON files at relative paths.

### ESP Web Tools Usage

```html
<script type="module"
  src="https://unpkg.com/esp-web-tools@10/dist/web/install-button.js?module"></script>

<esp-web-install-button manifest="manifest-full.json">
  <button slot="activate">Install Full Firmware</button>
</esp-web-install-button>
```

The manifests and firmware binaries are assembled at CI time (never committed to git) and deployed to GitHub Pages. The landing page just needs to reference the manifests by relative path.

## Design Principles

1. **Dark by default** — ScryBar is a dark-themed product. The page should feel like the device.
2. **Interactive, not static** — The theme switcher is the hero interaction. Make it feel alive.
3. **Fast** — No heavy frameworks loading 2MB of JS. Vite tree-shakes, Preact is 3KB.
4. **Offline-resilient** — If deployed on GitHub Pages and user has no internet... well, they need internet for Web Serial anyway. But keep CDN deps minimal.
5. **Mobile-first** — Most people will discover this on their phone. The 3D mockup should work with touch (swipe to rotate?) and the flash section should clearly say "use desktop Chrome".
6. **Product quality** — This is the public face of the project. Every pixel matters.

## What NOT to Do

- No Lorem Ipsum — real copy only
- No generic "hero with gradient" templates
- No Bootstrap/Tailwind — use Vibemilk DS
- No heavy animation libraries (GSAP etc.) — CSS transitions + transforms are enough
- No server-side anything — pure static deploy to GitHub Pages

## Reference: ScryBar Copy

**Tagline options:**
- "Your desk knows things now."
- "A mass of sensors, pixels, and unresolved ambition, pretending to be furniture."
- "Time, weather, news, and a talking oracle. Everything you could faster check on your phone, but won't."

**Product description:**
ScryBar is an ESP32-S3 powered desk companion with a 3.49" IPS touchscreen. Word clock in 13 languages, live weather, RSS feed deck with QR codes, Wikipedia explorer, and DOOM — all navigable by swipe. Five visual themes, LAN web config, macOS Now Playing companion. Repository-wide licensing is under review because bundled components and assets have their own terms.

**13 Languages:** Italian, English, French, German, Spanish, Portuguese, Latin, Esperanto, Klingon, 1337 Speak, Shakespearean, Valley Girl, Bellazio (Italian Gen Z)

## Files to Deliver

When implementing, the new page should live in a Vite project structure:

```
docs/flasher/           (or web/ — discuss with user)
  src/
    main.jsx            # Entry point
    App.jsx             # Root component
    components/
      Hero.jsx          # Logo + tagline + device mockup
      DeviceMockup.jsx  # 3D/isometric ScryBar with theme-reactive screen
      ThemeSelector.jsx # 5 theme buttons
      Features.jsx      # Feature grid
      FlashSection.jsx  # ESP Web Tools integration
      Specs.jsx         # Hardware specs
      Footer.jsx        # Credits
    styles/
      vibemilk.css      # DS subset (or import from assets/)
      themes.css        # 5 theme token sets for page-level theming
    assets/
      screenshots/      # Theme preview images for device mockup
  index.html
  vite.config.js
  package.json
```

The Vite build output (`dist/`) gets deployed to GitHub Pages. The CI workflow needs updating to:
1. Build the Vite project
2. Copy firmware manifests + binaries into the dist output
3. Deploy the combined output

## Priority

This is the **next major project** after the firmware web UI vibemilk migration (completed r198). The user wants it to be impressive — this is the public showcase of ScryBar.

---

*Document created 2026-03-20 to preserve session context for the next working session.*
