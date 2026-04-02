# Landing Page Update — Design Spec

**Date:** 2026-04-02
**Scope:** Content fixes + design improvements (no full redesign)

## Problem

The landing page has stale/incorrect content and is missing sections that would improve the user journey:

1. **ViewShowcase** shows 3 views (RSS, Wiki, Now Playing) but title says "Six views" — missing Transit Board screenshot
2. **Features** subtitle says "Six swipeable views" but lists 8 cards
3. **Home Assistant** was archived from firmware (r228) but memory still references it — must NOT be advertised
4. **No "How It Works"** section for new users
5. **No Companion App** section despite DMG being available
6. **Nav** missing Views link
7. **Dead asset** `doom_bunker_console.png` still in repo

## Design

### Page Structure (new order)

```
Hero
  Nav (+ Views link)
  Badges, headline, lede, CTAs
  DeviceMockup + ThemeSelector + LanguagePreview

Views  ← moved BEFORE Features (show the product first)
  4 cards: RSS, Wikipedia, Transit Board, Now Playing
  Title: "Four views, one gesture away"

Features
  7 cards (removed HA, kept: Word Clock, Weather, RSS, Wiki, Transit, Now Playing, 7 Themes, LAN Config → 8 total, but subtitle is generic)
  Subtitle: "Swipe through clock, news, transit, and more — on one desk companion."

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
    - Download button (GitHub release or repo link)
    - Note about album art, progress, source detection

Flash (unchanged structure, content already correct)

Specs (unchanged)

Footer (unchanged)
```

### Content Changes

| Component | Current | Updated |
|-----------|---------|---------|
| ViewShowcase h2 | "Six views, one gesture away" | "Four views, one gesture away" |
| ViewShowcase views array | 3 items (RSS, Wiki, NP) | 4 items (+Transit Board) |
| ViewShowcase subtitle | mentions "clock, news, Wikipedia, Now Playing, and live transit departures" | "Swipe left or right to switch between news, Wikipedia, transit departures, and Now Playing." |
| Features subtitle | "Six swipeable views. Thirteen languages." | "Swipe through clock, news, transit, and more — on one desk companion." |
| Features cards | 8 cards (includes HA implicitly via themes/config) | 8 cards: Word Clock, Weather, RSS, Wiki, Transit Board, Now Playing, 7 Themes, LAN Config |
| Nav links | Features, Flash, Specs, GitHub | Features, Views, Flash, Specs, GitHub |
| Meta description | already mentions transit, ok | remove any HA mention if present |
| index.astro section order | Features → Views → Flash → Specs | Views → Features → HowItWorks → Companion → Flash → Specs |

### New Components

#### HowItWorks.astro

4-step horizontal layout (2x2 on mobile). Each step:
- Emoji icon (large)
- Step number label
- Title (bold)
- One-line description

Style: reuse existing `vm-card` pattern, `features-grid`-like layout but 4 columns.

#### CompanionApp.astro

Single centered card with:
- Left: text content (title, description, badges, download CTA)
- Style: similar to `flash-card` but with companion-specific content
- Download links to GitHub release or repo DMG path

### Assets

| Action | File |
|--------|------|
| ADD | `web/src/assets/screenshots/transit_board.png` (already captured — London Liverpool Street) |
| DELETE | `web/src/assets/screenshots/doom_bunker_console.png` |
| KEEP | `wiki_stream.png` (will recapture in English+cyberpunk later when API rate limit clears) |
| KEEP | All `home_weather_*.png` theme screenshots |
| KEEP | `info_panel.png`, `now_playing.png`, `aux_rss.png` |

### What We Are NOT Doing

- No CSS framework changes (keep custom CSS + Vibemilk tokens)
- No new animations or effects
- No React/JS framework additions (stay pure Astro SSG)
- No HA section (archived from firmware)
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
| `web/src/styles/tokens.css` | Add styles for HowItWorks + CompanionApp (minimal, reuse existing patterns) |
| `web/src/assets/screenshots/transit_board.png` | Already in place |
| `web/src/assets/screenshots/doom_bunker_console.png` | DELETE |
