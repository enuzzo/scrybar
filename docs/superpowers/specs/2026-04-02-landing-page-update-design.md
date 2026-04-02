# Landing Page Update — Design Spec

**Date:** 2026-04-02
**Scope:** Content fixes + design improvements (no full redesign)

## Problem

The landing page has stale/incorrect content and is missing sections that would improve the user journey:

1. **ViewShowcase** shows 3 views (RSS, Wiki, Now Playing) but title says "Six views" — missing Transit Board screenshot. DOOM is excluded because it is disabled in the web-flash firmware build.
2. **Features** subtitle says "Six swipeable views" but lists 8 feature cards — the count is misleading
3. **No "How It Works"** section for new users
4. **No Companion App** section despite DMG v0.2.0 being available
5. **Nav** missing Views link
6. **Dead asset** `doom_bunker_console.png` still in repo (DOOM removed from web-flash firmware)

## Design

### Page Structure (new order)

```
Hero
  Nav (+ Views link)
  Badges, headline, lede, CTAs
  "Explore Features" CTA → keep pointing to #features (Features is the capability overview)
  DeviceMockup + ThemeSelector + LanguagePreview

Views  ← moved BEFORE Features (show the product first)
  4 cards: RSS, Wikipedia, Transit Board, Now Playing
  Title: non-numeric to avoid count trap — "Views worth swiping for"
  (HOME/clock and INFO are not showcase-worthy — they are the default experience)

Features
  8 cards (unchanged count): Word Clock, Weather, RSS, Wiki, Transit, Now Playing, 7 Themes, LAN Config
  Subtitle: generic, no numeric view count — "Swipe through clock, news, transit, and more — on one desk companion."

How It Works (NEW)
  4 horizontal steps with emoji icons:
    1. Plug In — Connect USB-C
    2. Flash — One click from your browser
    3. Connect — Device creates WiFi AP, open config page
    4. Enjoy — Clock, weather, news, all running

Companion App (NEW)
  Single card with:
    - Title: "macOS Companion"
    - Description: menu bar app pushing Now Playing to device
    - Badges: "macOS" + "v0.2.0"
    - Download: link to repo file `companion/mac/ScryBarCompanion-0.2.0.dmg` via GitHub raw URL
    - Note about album art, progress, source detection

Flash (unchanged structure, content already correct)

Specs (unchanged)

Footer (unchanged)
```

### Content Changes

| Component | Current | Updated |
|-----------|---------|---------|
| ViewShowcase h2 | "Six views, one gesture away" | "Views worth swiping for" |
| ViewShowcase views array | 3 items (RSS, Wiki, NP) | 4 items (+Transit Board) |
| ViewShowcase subtitle | mentions "clock, news, Wikipedia, Now Playing, and live transit departures" | "Swipe left or right to switch between news, Wikipedia, transit departures, and Now Playing." |
| Features subtitle | "Six swipeable views. Thirteen languages." | "Swipe through clock, news, transit, and more — on one desk companion." |
| Features cards | 8 cards | 8 cards (unchanged): Word Clock, Weather, RSS, Wiki, Transit Board, Now Playing, 7 Themes, LAN Config |
| Nav links | Features, Flash, Specs, GitHub | Features, Views, Flash, Specs, GitHub |
| Meta description | ok as-is | no changes needed |
| index.astro section order | Features → ViewShowcase → Flash → Specs | ViewShowcase → Features → HowItWorks → CompanionApp → Flash → Specs |

### New Components

#### HowItWorks.astro

4-step horizontal layout. Each step:
- Emoji icon (large)
- Step number label
- Title (bold)
- One-line description

Style: reuse `vm-card` pattern. CSS grid: `grid-template-columns: repeat(4, 1fr)`.
Breakpoints: at 900px → `repeat(2, 1fr)` (2x2 grid), at 640px → `1fr` (single column).

#### CompanionApp.astro

Single centered card:
- Section label "Companion" (matches existing `section-label` pattern)
- h2 title + subtitle
- `vm-card` with description, badges (`vm-badge--brand` for macOS, `vm-badge--info` for v0.2.0), and download CTA (`vm-btn--primary`)
- Download URL: `https://github.com/enuzzo/scrybar/raw/main/companion/mac/ScryBarCompanion-0.2.0.dmg`

### Assets

| Action | File |
|--------|------|
| KEEP | `web/src/assets/screenshots/transit_board.png` (already captured — London Liverpool Street) |
| DELETE | `web/src/assets/screenshots/doom_bunker_console.png` |
| KEEP | `wiki_stream.png` (will recapture in English+cyberpunk later when API rate limit clears) |
| KEEP | All `home_weather_*.png` theme screenshots |
| KEEP | `info_panel.png`, `now_playing.png`, `aux_rss.png` |

### What We Are NOT Doing

- No CSS framework changes (keep custom CSS + Vibemilk tokens)
- No new animations or effects beyond entrance/reveal already in place
- No React/JS framework additions (stay pure Astro SSG)
- No HA section (archived from firmware since r228)
- No DOOM in showcase (disabled in web-flash firmware)
- No full visual redesign of existing components
- No changes to FlashSection, Specs, DeviceMockup, ThemeSelector, LanguagePreview, Footer internals

## Files to Create/Modify

| File | Action |
|------|--------|
| `web/src/pages/index.astro` | Reorder sections, add HowItWorks + CompanionApp imports |
| `web/src/components/ViewShowcase.astro` | Add Transit, fix title/subtitle, remove TODO comment |
| `web/src/components/Features.astro` | Fix subtitle copy |
| `web/src/components/Nav.astro` | Add Views link |
| `web/src/components/HowItWorks.astro` | NEW — 4-step setup guide |
| `web/src/components/CompanionApp.astro` | NEW — macOS companion section |
| `web/src/styles/tokens.css` | Add styles for new components if needed (prefer reusing existing classes) |
| `web/src/assets/screenshots/doom_bunker_console.png` | DELETE |
