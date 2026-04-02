# Landing Page Update — Implementation Plan

> **For agentic workers:** REQUIRED: Use superpowers:subagent-driven-development (if subagents available) or superpowers:executing-plans to implement this plan. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Fix stale content on the ScryBar landing page and add missing sections (How It Works, Companion App) with reordered page flow.

**Architecture:** Astro SSG static site. Each section is a `.astro` component imported into `index.astro`. CSS lives in `web/src/styles/sections.css` (component styles) and `tokens.css` (theme tokens). No JS frameworks — just vanilla `<script>` blocks.

**Tech Stack:** Astro 5.7, custom CSS with Vibemilk design tokens, zero runtime JS dependencies.

**Spec:** `docs/superpowers/specs/2026-04-02-landing-page-update-design.md`

---

## File Map

| File | Action | Responsibility |
|------|--------|---------------|
| `web/src/components/ViewShowcase.astro` | Modify | Add Transit card, fix title/subtitle, remove TODO |
| `web/src/components/Features.astro` | Modify | Fix subtitle copy |
| `web/src/components/Nav.astro` | Modify | Add Views link |
| `web/src/components/HowItWorks.astro` | Create | 4-step setup guide section |
| `web/src/components/CompanionApp.astro` | Create | macOS companion download section |
| `web/src/pages/index.astro` | Modify | Reorder sections, add new imports |
| `web/src/styles/sections.css` | Modify | Add styles for HowItWorks + CompanionApp |
| `web/src/assets/screenshots/doom_bunker_console.png` | Delete | Dead asset |

---

### Task 1: Fix ViewShowcase — add Transit, fix copy

**Files:**
- Modify: `web/src/components/ViewShowcase.astro`

- [ ] **Step 1: Add Transit screenshot import and update views array**

Replace the entire frontmatter block (lines 1-11, including closing `---`) with:

```astro
---
import imgRss from '../assets/screenshots/aux_rss.png';
import imgWiki from '../assets/screenshots/wiki_stream.png';
import imgTransit from '../assets/screenshots/transit_board.png';
import imgNowPlaying from '../assets/screenshots/now_playing.png';
const views = [
  { id: 'rss',     src: imgRss,        label: 'RSS Feeds',           desc: 'Live news with favicons, QR codes for mobile reading. Up to 5 configurable feeds.' },
  { id: 'wiki',    src: imgWiki,       label: 'Wikipedia',            desc: 'Featured articles, On This Day, random discoveries. Swipe through knowledge.' },
  { id: 'transit', src: imgTransit,    label: 'Transit Departures',   desc: 'Live departures worldwide via Transitous. Trains, metro, bus — no API key needed.' },
  { id: 'np',      src: imgNowPlaying, label: 'Now Playing',          desc: 'Album art, progress bar, artist detection. Pushed live from macOS companion.' },
];
---
```

- [ ] **Step 2: Fix section title and subtitle**

Replace the h2 and subtitle (lines 15-19) with:

```html
    <h2>Views worth swiping for</h2>
    <p class="section-sub">
      Swipe left or right to switch between news, Wikipedia,
      transit departures, and Now Playing.
    </p>
```

- [ ] **Step 3: Verify in dev server**

Run: `cd web && npm run dev`
Open: `http://localhost:5173/#views`
Expected: 4 cards in 2x2 grid, Transit card shows London Liverpool Street screenshot, lightbox works with 4 images.

- [ ] **Step 4: Commit**

```bash
git add web/src/components/ViewShowcase.astro
git commit -m "web: fix ViewShowcase — add Transit, non-numeric title, 4 views"
```

---

### Task 2: Fix Features subtitle

**Files:**
- Modify: `web/src/components/Features.astro`

- [ ] **Step 1: Update subtitle copy**

Replace the h2 and section-sub paragraph (lines 16-20) with:

```html
    <h2>Everything on a 3.49" bar</h2>
    <p class="section-sub">
      Swipe through clock, news, transit, and more &mdash;
      on one desk companion. Thirteen languages included.
    </p>
```

- [ ] **Step 2: Commit**

```bash
git add web/src/components/Features.astro
git commit -m "web: fix Features subtitle — generic copy, no numeric view count"
```

---

### Task 3: Add Views link to Nav

**Files:**
- Modify: `web/src/components/Nav.astro`

- [ ] **Step 1: Add Views link after Features**

Replace the `hero-nav-links` div (lines 3-15) with:

```html
  <div class="hero-nav-links">
    <a href="#features">Features</a>
    <a href="#views">Views</a>
    <a href="#flash">Flash</a>
    <a href="#specs">Specs</a>
    <a
      href="https://github.com/enuzzo/scrybar"
      target="_blank"
      rel="noopener noreferrer"
      class="hero-nav-gh"
    >
      GitHub
    </a>
  </div>
```

- [ ] **Step 2: Commit**

```bash
git add web/src/components/Nav.astro
git commit -m "web: add Views link to nav"
```

---

### Task 4: Create HowItWorks component

**Files:**
- Create: `web/src/components/HowItWorks.astro`
- Modify: `web/src/styles/sections.css`

- [ ] **Step 1: Create HowItWorks.astro**

```astro
---
const steps = [
  { num: '1', icon: '\u{1F50C}', title: 'Plug In',  desc: 'Connect USB-C to your device and computer.' },
  { num: '2', icon: '\u{26A1}',  title: 'Flash',    desc: 'One click from your browser. No toolchain, no drivers.' },
  { num: '3', icon: '\u{1F4F6}', title: 'Connect',  desc: 'Device creates a WiFi AP. Open the config page and set up.' },
  { num: '4', icon: '\u{2705}',  title: 'Enjoy',    desc: 'Clock, weather, news, transit — all running on your desk.' },
];
---
<section id="setup">
  <div class="container">
    <span class="section-label">Setup</span>
    <h2>Up and running in minutes</h2>
    <p class="section-sub">
      No soldering, no SDK, no app store. Plug in, flash, configure.
    </p>

    <div class="steps-grid">
      {steps.map((s) => (
        <div class="vm-card step-card">
          <span class="step-icon">{s.icon}</span>
          <span class="step-num">Step {s.num}</span>
          <h3>{s.title}</h3>
          <p>{s.desc}</p>
        </div>
      ))}
    </div>
  </div>
</section>
```

- [ ] **Step 2: Add CSS for steps-grid and step-card to sections.css**

Add before the `/* ============ FLASH ============ */` comment:

```css
/* ============ HOW IT WORKS ============ */
.steps-grid {
  display: grid;
  grid-template-columns: repeat(4, 1fr);
  gap: 16px;
}

.step-card {
  display: flex;
  flex-direction: column;
  gap: 8px;
  padding: 24px 22px;
  text-align: center;
}

.step-icon {
  font-size: 32px;
  line-height: 1;
  margin-bottom: 4px;
}

.step-num {
  font-family: var(--font-mono);
  font-size: 11px;
  font-weight: 600;
  text-transform: uppercase;
  letter-spacing: 0.08em;
  color: var(--accent-primary);
}

.step-card h3 {
  font-size: 15px;
  letter-spacing: -0.01em;
}

.step-card p {
  font-size: 13px;
  color: var(--text-tertiary);
  line-height: 1.55;
  transition: color 0.45s ease;
}
```

- [ ] **Step 3: Add responsive breakpoints**

In the `@media (max-width: 900px)` block, add:

```css
  .steps-grid {
    grid-template-columns: repeat(2, 1fr);
  }
```

In the `@media (max-width: 640px)` block, add:

```css
  .steps-grid {
    grid-template-columns: 1fr;
  }
```

- [ ] **Step 4: Commit**

```bash
git add web/src/components/HowItWorks.astro web/src/styles/sections.css
git commit -m "web: add HowItWorks section — 4-step setup guide"
```

---

### Task 5: Create CompanionApp component

**Files:**
- Create: `web/src/components/CompanionApp.astro`
- Modify: `web/src/styles/sections.css`

- [ ] **Step 1: Create CompanionApp.astro**

```astro
<section id="companion">
  <div class="container">
    <span class="section-label">Companion</span>
    <h2>macOS menu bar app</h2>
    <p class="section-sub">
      Push what's playing to your ScryBar — album art, progress bar,
      and source detection, live from your Mac.
    </p>

    <div class="companion-card-wrap">
      <div class="vm-card companion-card">
        <div class="companion-card__body">
          <div class="companion-badges">
            <span class="vm-badge vm-badge--brand">macOS</span>
            <span class="vm-badge vm-badge--info">v0.2.0</span>
          </div>
          <p class="companion-card__desc">
            A lightweight menu bar app that detects music from Apple Music,
            Spotify, TIDAL, and other sources — then pushes track info and
            album art to your ScryBar over the local network.
          </p>
          <a
            href="https://github.com/enuzzo/scrybar/raw/main/companion/mac/ScryBarCompanion-0.2.0.dmg"
            class="vm-btn vm-btn--primary"
          >
            Download DMG
          </a>
        </div>
      </div>
    </div>
  </div>
</section>
```

- [ ] **Step 2: Add CSS for companion section to sections.css**

Add before the `/* ============ FLASH ============ */` comment (after How It Works styles):

```css
/* ============ COMPANION APP ============ */
.companion-card-wrap {
  max-width: 480px;
  margin: 0 auto;
}

.companion-card {
  padding: 28px;
}

.companion-badges {
  display: flex;
  gap: 8px;
  margin-bottom: 16px;
}

.companion-card__desc {
  font-size: 14px;
  color: var(--text-tertiary);
  line-height: 1.65;
  margin-bottom: 20px;
  transition: color 0.45s ease;
}
```

- [ ] **Step 3: Commit**

```bash
git add web/src/components/CompanionApp.astro web/src/styles/sections.css
git commit -m "web: add CompanionApp section — macOS companion with download"
```

---

### Task 6: Reorder index.astro and wire everything up

**Files:**
- Modify: `web/src/pages/index.astro`

- [ ] **Step 1: Add new imports**

After the ViewShowcase import (line 8), add:

```astro
import HowItWorks from '../components/HowItWorks.astro';
import CompanionApp from '../components/CompanionApp.astro';
```

- [ ] **Step 2: Reorder sections in the content div**

Replace the section order (lines 66-83) with:

```astro
    <div class="reveal">
      <ViewShowcase />
    </div>

    <div class="reveal">
      <Features />
    </div>

    <div class="reveal">
      <HowItWorks />
    </div>

    <div class="reveal">
      <CompanionApp />
    </div>

    <div class="reveal">
      <FlashSection />
    </div>

    <div class="reveal">
      <Specs />
    </div>

    <Footer />
```

- [ ] **Step 3: Verify full page in dev server**

Run: `cd web && npm run dev`
Check:
1. Section order: Hero → Views → Features → How It Works → Companion → Flash → Specs → Footer
2. Nav links work: Features, Views, Flash, Specs all scroll to correct sections
3. Scroll reveal animations fire on each section
4. All 7 themes display correctly (click through ThemeSelector)
5. ViewShowcase lightbox works with 4 images (including Transit)

- [ ] **Step 4: Commit**

```bash
git add web/src/pages/index.astro
git commit -m "web: reorder sections — Views first, add HowItWorks + CompanionApp"
```

---

### Task 7: Delete dead DOOM asset

**Files:**
- Delete: `web/src/assets/screenshots/doom_bunker_console.png`

- [ ] **Step 1: Verify no references to doom_bunker_console.png exist**

Run: `grep -r "doom_bunker_console" web/src/`
Expected: no matches (it was already unused after ViewShowcase was updated)

- [ ] **Step 2: Delete the file**

```bash
git rm web/src/assets/screenshots/doom_bunker_console.png
```

- [ ] **Step 3: Commit**

```bash
git commit -m "web: remove dead doom_bunker_console.png asset"
```

---

### Task 8: Final build verification

- [ ] **Step 1: Run production build**

```bash
cd web && npm run build
```

Expected: clean build, no errors, no warnings about missing assets.

- [ ] **Step 2: Preview production build**

```bash
cd web && npm run preview
```

Open `http://localhost:4321/` and verify all sections render correctly.

- [ ] **Step 3: Check for broken links/images**

Visually confirm:
- Transit screenshot loads in ViewShowcase
- All theme screenshots load in DeviceMockup
- No console errors in browser dev tools
- CompanionApp download link points to valid URL
