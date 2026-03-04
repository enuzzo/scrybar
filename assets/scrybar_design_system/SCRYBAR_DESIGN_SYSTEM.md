# ScryBar Design System v1.2

> A comprehensive dark-themed design system for ESP32 web configuration interfaces.
> Inspired by premium fintech dashboard aesthetics. Optimized for readability,
> accessibility on small-to-medium displays, and a professional, futuristic feel.

## 0. Theming System (v1.2 update)

`index.html` now includes a top theme selector that switches runtime presets by writing:

```html
<html data-theme="...">
```

Theme presets are defined in `scrybar.css` as token overrides:

- `:root, :root[data-theme="scrybar-default"]` (baseline)
- `:root[data-theme="cyberpunk-2077"]` (monospaced terminal style)
- `:root[data-theme="toxic-candy"]` (neon magenta + acid green accents, candy sci-fi finish)

Cyberpunk v1.1 styling details (inspired by augmented geometry patterns):

- 45deg cyber corners on selected surfaces/controls with closed top-left/bottom-right chamfers (`clip-path: polygon(...)`)
- Layered border/inlay feel via `::before`/`::after` overlays on panels
- Corner marker accents and scanline overlays bound to cyberpunk tokens
- Includes ScryBar web FX Grid animation demo at the bottom of `index.html` (theme-reactive colors/treatment)
- Kept ScryBar nomenclature intact (no class/token renames)
- Reference studied: `https://unpkg.com/augmented-ui@2/augmented-ui.css`

Toxic Candy v1.2 styling details:

- Accent pair: magenta (`--accent-primary`) + fluo green (`--accent-secondary`)
- New type mood via `Delius Unicase` + `Space Mono` token stacks (`Chakra Petch` fallback)
- Rounded “candy shell” surfaces with neon glows and high-contrast states
- FX Grid token overrides for magenta/green reactive animation
- Keeps same component nomenclature and responsive behavior

The theming layer covers:

- Colors and gradients
- Typography families/sizes
- Component dimensions (`--btn-height-*`, `--input-height-*`, avatar/toggle sizes)
- Stroke and border widths (`--stroke-width-*`)
- Glow/shadow/focus tokens
- Surface overlays/dividers/skeleton shimmer tokens
- Select dropdown contrast tokens (`--select-option-*`) for clearer hover/selected states
- Custom dropdown component (`.scry-dropdown`) with explicit selected/hover states and keyboard interaction
- Web FX animation tokens (`--fx-grid-*`) for grid/horizon/scanline/vignette
- Responsive behavior for every theme preset (desktop + mobile)

To add a new theme, duplicate the cyberpunk block and override only variables:

```css
:root[data-theme="my-theme-name"] {
  --font-family: ...;
  --accent-primary: ...;
  --stroke-width-ui: ...;
  --btn-height-md: ...;
  --shadow-glow-sm: ...;
}
```

**A project by Netmilk Studio sagl — 2026 — Monteggio Plains**
© 2026 Netmilk Studio sagl. All rights reserved.

### Brand Assets

| Asset | URL |
|-------|-----|
| **Studio Logo** | `https://netmi.lk/wp-content/uploads/2024/10/netmilk.svg` |
| **Logo Format** | SVG, light-on-dark optimized |
| **Logo Placement** | Centered hero (large) + footer attribution |
| **Logo Size** | Hero: responsive, target `>=280px` width on desktop. Footer: `130px` width |

---

## 1. FOUNDATIONS

### 1.1 Design Philosophy

ScryBar is a **dark-first** design system built for embedded device configuration UIs.
It prioritizes clarity, hierarchy, and a premium feel while remaining lightweight
enough for ESP32-hosted web interfaces. The aesthetic is: **Deep Space Control Panel** —
calm, precise, authoritative.

### 1.2 Typography

**Font System:** token-driven, with per-theme font families and explicit fallback stacks.

```
@import url('https://fonts.googleapis.com/css2?family=Montserrat:wght@300;400;500;600;700;800&display=swap');
@import url('https://fonts.googleapis.com/css2?family=Chakra+Petch:wght@400;500;600;700&family=Delius+Unicase:wght@400;700&family=Space+Mono:wght@400;700&display=swap');
```

Theme font map:

| Theme | `--font-family` | `--font-mono` | ESP32 UI font mapping |
|------|------------------|---------------|-----------------------|
| `scrybar-default` | Montserrat stack | Space Mono stack | LVGL Montserrat built-ins |
| `cyberpunk-2077` | Space Mono terminal stack | Space Mono terminal stack | `scry_font_space_mono_*` |
| `toxic-candy` | Delius Unicase stack (Chakra fallback) | Space Mono stack | `scry_font_delius_unicase_*` |

ESP32 font assets (generated with `lv_font_conv`, static TTF, non-variable):

- `assets/fonts/SpaceMono-Regular.ttf`
- `assets/fonts/DeliusUnicase-Regular.ttf`
- `src/fonts/scry_font_space_mono_*.c`
- `src/fonts/scry_font_delius_unicase_*.c`

| Token                  | Weight | Size   | Line-Height | Letter-Spacing | Usage                        |
|------------------------|--------|--------|-------------|----------------|------------------------------|
| `--font-display-xl`    | 800    | 36px   | 1.2         | -0.02em        | Page hero numbers            |
| `--font-display-lg`    | 700    | 28px   | 1.25        | -0.01em        | Section titles               |
| `--font-heading-xl`    | 700    | 24px   | 1.3         | -0.01em        | Card main headings           |
| `--font-heading-lg`    | 600    | 20px   | 1.35        | 0              | Panel headings               |
| `--font-heading-md`    | 600    | 18px   | 1.4         | 0              | Sub-section headings         |
| `--font-heading-sm`    | 600    | 16px   | 1.4         | 0              | Widget titles                |
| `--font-body-lg`       | 500    | 16px   | 1.5         | 0              | Primary body text            |
| `--font-body-md`       | 400    | 14px   | 1.5         | 0.01em         | Standard body text           |
| `--font-body-sm`       | 400    | 13px   | 1.5         | 0.01em         | Secondary info               |
| `--font-caption`       | 500    | 12px   | 1.4         | 0.02em         | Labels, badges, metadata     |
| `--font-overline`      | 600    | 11px   | 1.3         | 0.08em         | Overline text (uppercase)    |
| `--font-micro`         | 500    | 10px   | 1.3         | 0.04em         | Tiny labels, status dots     |

### 1.3 Color Palette

#### Core Backgrounds (Dark Hierarchy)
| Token                     | Hex       | Usage                                    |
|---------------------------|-----------|------------------------------------------|
| `--bg-deepest`            | #070D2D   | Page/app background                      |
| `--bg-deep`               | #0B1437   | Sidebar, main canvas                     |
| `--bg-surface`            | #111C44   | Cards, panels, elevated surfaces         |
| `--bg-surface-hover`      | #162052   | Card hover state                         |
| `--bg-elevated`           | #1A2558   | Raised elements, dropdowns               |
| `--bg-elevated-hover`     | #1E2D66   | Hover on elevated                        |
| `--bg-input`              | #0B1437   | Input field backgrounds                  |
| `--bg-input-focus`        | #111C44   | Input focus state                        |
| `--bg-overlay`            | rgba(7,13,45,0.85) | Modal overlays                  |

#### Text Colors
| Token                     | Hex                  | Usage                             |
|---------------------------|----------------------|-----------------------------------|
| `--text-primary`          | #FFFFFF              | Main headings, primary content    |
| `--text-secondary`        | #A3AED0              | Body text, descriptions           |
| `--text-tertiary`         | #707EAE              | Metadata, timestamps, hints       |
| `--text-muted`            | #4A5568              | Disabled states, placeholders     |
| `--text-inverse`          | #0B1437              | Text on light/accent backgrounds  |

#### Accent Colors (Brand)
| Token                     | Hex       | Usage                                    |
|---------------------------|-----------|------------------------------------------|
| `--accent-primary`        | #7551FF   | Primary buttons, active states, links    |
| `--accent-primary-hover`  | #8B6FFF   | Hover state                              |
| `--accent-primary-active` | #6441E0   | Active/pressed state                     |
| `--accent-primary-subtle` | rgba(117,81,255,0.12) | Subtle backgrounds          |
| `--accent-secondary`      | #39B8FF   | Secondary accents, info indicators       |
| `--accent-secondary-hover`| #5DC8FF   | Hover state                              |
| `--accent-secondary-subtle`| rgba(57,184,255,0.12)| Subtle backgrounds          |

#### Semantic Colors
| Token                     | Hex       | Usage                                    |
|---------------------------|-----------|------------------------------------------|
| `--color-success`         | #01B574   | Positive indicators, approved states     |
| `--color-success-hover`   | #02D68A   | Hover                                    |
| `--color-success-subtle`  | rgba(1,181,116,0.12) | Subtle backgrounds          |
| `--color-warning`         | #FFB547   | Caution indicators, pending states       |
| `--color-warning-hover`   | #FFC76B   | Hover                                    |
| `--color-warning-subtle`  | rgba(255,181,71,0.12)| Subtle backgrounds          |
| `--color-danger`          | #EE5D50   | Error states, destructive actions        |
| `--color-danger-hover`    | #F17A70   | Hover                                    |
| `--color-danger-subtle`   | rgba(238,93,80,0.12) | Subtle backgrounds          |
| `--color-info`            | #39B8FF   | Informational                            |
| `--color-info-subtle`     | rgba(57,184,255,0.12)| Subtle backgrounds          |

#### Gradient Definitions
| Token                         | Value                                              | Usage                        |
|-------------------------------|-----------------------------------------------------|------------------------------|
| `--gradient-brand`            | linear-gradient(135deg, #868CFF 0%, #4318FF 100%)  | Primary brand gradient       |
| `--gradient-card`             | linear-gradient(135deg, #7551FF 0%, #39B8FF 100%)  | Feature card highlights      |
| `--gradient-success`          | linear-gradient(135deg, #01B574 0%, #39B8FF 100%)  | Positive metric gradients    |
| `--gradient-warm`             | linear-gradient(135deg, #FFB547 0%, #EE5D50 100%)  | Warm accents                 |
| `--gradient-surface`          | linear-gradient(135deg, #111C44 0%, #1A2558 100%)  | Subtle card gradients        |
| `--gradient-glow-purple`      | radial-gradient(circle, rgba(117,81,255,0.3) 0%, transparent 70%) | Glow effects  |
| `--gradient-glow-blue`        | radial-gradient(circle, rgba(57,184,255,0.2) 0%, transparent 70%) | Glow effects  |
| `--gradient-sidebar`          | linear-gradient(180deg, #111C44 0%, #0B1437 100%)  | Sidebar background           |

### 1.4 Spacing Scale

Based on a 4px grid system:

| Token         | Value  | Usage                                     |
|---------------|--------|-------------------------------------------|
| `--space-0`   | 0px    | Reset                                     |
| `--space-1`   | 4px    | Tightest spacing, icon gaps               |
| `--space-2`   | 8px    | Inner padding tight, inline gaps          |
| `--space-3`   | 12px   | Standard inner padding                    |
| `--space-4`   | 16px   | Default component padding                 |
| `--space-5`   | 20px   | Card inner padding                        |
| `--space-6`   | 24px   | Section spacing                           |
| `--space-8`   | 32px   | Large section gaps                        |
| `--space-10`  | 40px   | Page section margins                      |
| `--space-12`  | 48px   | Major layout gaps                         |
| `--space-16`  | 64px   | Hero spacing                              |

### 1.5 Border Radius

| Token              | Value  | Usage                                     |
|--------------------|--------|-------------------------------------------|
| `--radius-xs`      | 4px    | Badges, tiny elements                     |
| `--radius-sm`      | 8px    | Buttons, inputs, small cards              |
| `--radius-md`      | 12px   | Standard cards, dropdowns                 |
| `--radius-lg`      | 16px   | Large cards, panels                       |
| `--radius-xl`      | 20px   | Feature cards, hero elements              |
| `--radius-2xl`     | 24px   | Modal containers                          |
| `--radius-full`    | 9999px | Pills, avatars, circular elements         |

### 1.6 Shadows & Elevation

| Token                | Value                                                        | Usage               |
|----------------------|--------------------------------------------------------------|----------------------|
| `--shadow-sm`        | 0 2px 8px rgba(0,0,0,0.2)                                   | Subtle lift          |
| `--shadow-md`        | 0 4px 16px rgba(0,0,0,0.25)                                 | Cards, dropdowns     |
| `--shadow-lg`        | 0 8px 32px rgba(0,0,0,0.35)                                 | Modals, popovers     |
| `--shadow-xl`        | 0 16px 48px rgba(0,0,0,0.4)                                 | Large overlays       |
| `--shadow-glow-sm`   | 0 0 20px rgba(117,81,255,0.15)                               | Subtle purple glow   |
| `--shadow-glow-md`   | 0 0 40px rgba(117,81,255,0.2)                                | Medium purple glow   |
| `--shadow-glow-blue` | 0 0 30px rgba(57,184,255,0.15)                               | Blue glow accent     |
| `--shadow-inset`     | inset 0 2px 4px rgba(0,0,0,0.2)                             | Pressed/inset states |

### 1.7 Borders

| Token                    | Value                            | Usage                       |
|--------------------------|----------------------------------|-----------------------------|
| `--border-subtle`        | 1px solid rgba(255,255,255,0.06) | Card borders, dividers      |
| `--border-default`       | 1px solid rgba(255,255,255,0.1)  | Input borders, separators   |
| `--border-strong`        | 1px solid rgba(255,255,255,0.18) | Focused inputs, emphasis    |
| `--border-accent`        | 1px solid #7551FF                | Active/selected states      |
| `--border-success`       | 1px solid #01B574                | Success indicators          |
| `--border-danger`        | 1px solid #EE5D50                | Error states                |

### 1.8 Transitions & Animations

| Token                    | Value                              | Usage                       |
|--------------------------|------------------------------------|-----------------------------|
| `--transition-fast`      | 150ms cubic-bezier(0.4,0,0.2,1)   | Hover states, toggles       |
| `--transition-base`      | 250ms cubic-bezier(0.4,0,0.2,1)   | General transitions         |
| `--transition-slow`      | 400ms cubic-bezier(0.4,0,0.2,1)   | Page transitions, modals    |
| `--transition-spring`    | 500ms cubic-bezier(0.34,1.56,0.64,1) | Bouncy interactions     |

### 1.9 Z-Index Scale

| Token            | Value | Usage                                       |
|------------------|-------|---------------------------------------------|
| `--z-base`       | 0     | Default stacking                            |
| `--z-raised`     | 10    | Cards, raised elements                      |
| `--z-dropdown`   | 100   | Dropdowns, popovers                         |
| `--z-sticky`     | 200   | Sticky headers, sidebars                    |
| `--z-overlay`    | 300   | Backdrop overlays                           |
| `--z-modal`      | 400   | Modal dialogs                               |
| `--z-toast`      | 500   | Toast notifications                         |
| `--z-tooltip`    | 600   | Tooltips                                    |

---

## 2. LAYOUT SYSTEM

### 2.1 Page Structure

```
┌──────────────────────────────────────────────────┐
│ .scry-app                                        │
│ ┌────┬───────────────────────────────────────┐   │
│ │    │ .scry-topbar                          │   │
│ │ S  ├───────────────────────────────────────┤   │
│ │ I  │ .scry-content                         │   │
│ │ D  │  ┌─────────────┐ ┌─────────────┐     │   │
│ │ E  │  │ .scry-card  │ │ .scry-card  │     │   │
│ │ B  │  └─────────────┘ └─────────────┘     │   │
│ │ A  │  ┌───────────────────────────────┐   │   │
│ │ R  │  │ .scry-card                    │   │   │
│ │    │  └───────────────────────────────┘   │   │
│ └────┴───────────────────────────────────────┘   │
└──────────────────────────────────────────────────┘
```

- **Sidebar**: Fixed left, 100px wide (collapsed) or 260px (expanded)
- **Topbar**: Sticky, 72px height
- **Content**: Fluid, with 24px padding, grid-based
- **Right Panel** (optional): 320px fixed, for secondary info

### 2.2 Grid

12-column CSS Grid with 24px gap:

```css
.scry-grid { display: grid; grid-template-columns: repeat(12, 1fr); gap: 24px; }
.scry-col-1  { grid-column: span 1; }
.scry-col-2  { grid-column: span 2; }
...
.scry-col-12 { grid-column: span 12; }
```

### 2.3 Container Widths

| Token                 | Value   | Usage                        |
|-----------------------|---------|------------------------------|
| `--container-sm`      | 640px   | Narrow content               |
| `--container-md`      | 960px   | Standard content             |
| `--container-lg`      | 1200px  | Wide content                 |
| `--container-xl`      | 1440px  | Full-width content           |

---

## 3. COMPONENTS

### 3.1 Buttons

#### Variants
| Variant      | Background              | Text Color      | Border                  |
|--------------|-------------------------|-----------------|-------------------------|
| Primary      | `--accent-primary`      | #FFFFFF         | none                    |
| Secondary    | transparent             | `--text-secondary` | `--border-default`   |
| Ghost        | transparent             | `--text-secondary` | none                 |
| Success      | `--color-success`       | #FFFFFF         | none                    |
| Danger       | `--color-danger`        | #FFFFFF         | none                    |
| Outline      | transparent             | `--accent-primary` | `--border-accent`    |

#### Sizes
| Size    | Height | Padding (H)  | Font Size | Radius          |
|---------|--------|---------------|-----------|-----------------|
| XS      | 28px   | 10px          | 11px      | `--radius-xs`   |
| SM      | 34px   | 14px          | 13px      | `--radius-sm`   |
| MD      | 40px   | 18px          | 14px      | `--radius-sm`   |
| LG      | 48px   | 24px          | 16px      | `--radius-md`   |
| XL      | 56px   | 32px          | 16px      | `--radius-md`   |

#### States
- **Default**: Base styling
- **Hover**: Lighten bg 10%, subtle lift shadow
- **Active**: Darken bg 5%, inset shadow
- **Focus**: 2px ring with `--accent-primary` at 40% opacity, 2px offset
- **Disabled**: 40% opacity, cursor not-allowed
- **Loading**: Spinner icon, text hidden, same dimensions

### 3.2 Stat Cards

Horizontal arrangement of key metrics. Each card:
- Background: `--bg-surface` or subtle gradient variant
- Padding: 20px
- Border-radius: `--radius-lg`
- Border: `--border-subtle`
- Contains: overline label, large number (display-xl weight 700), trend indicator

### 3.3 Input Fields

| Property     | Value                                       |
|--------------|---------------------------------------------|
| Height       | 44px (MD), 38px (SM), 52px (LG)            |
| Background   | `--bg-input`                                |
| Border       | `--border-default`                          |
| Border Focus | `--border-accent`                           |
| Radius       | `--radius-sm`                               |
| Padding      | 0 16px                                      |
| Font         | body-md                                     |
| Text Color   | `--text-primary`                            |
| Placeholder  | `--text-muted`                              |

### 3.4 Cards

| Property     | Value                                       |
|--------------|---------------------------------------------|
| Background   | `--bg-surface`                              |
| Border       | `--border-subtle`                           |
| Radius       | `--radius-lg`                               |
| Padding      | 24px                                        |
| Shadow       | `--shadow-sm`                               |
| Hover        | `--bg-surface-hover`, `--shadow-md`         |

### 3.5 Table

| Element       | Style                                       |
|---------------|---------------------------------------------|
| Header BG     | transparent                                 |
| Header Text   | `--text-tertiary`, font-overline, uppercase |
| Row BG        | transparent                                 |
| Row Hover     | `--bg-surface-hover`                        |
| Row Border    | `--border-subtle` bottom                    |
| Cell Padding  | 14px 16px                                   |
| Cell Font     | body-md                                     |

### 3.6 Badges / Tags

| Variant   | Background                    | Text Color            | Radius            |
|-----------|-------------------------------|-----------------------|-------------------|
| Default   | `--bg-elevated`               | `--text-secondary`    | `--radius-full`   |
| Success   | `--color-success-subtle`      | `--color-success`     | `--radius-full`   |
| Warning   | `--color-warning-subtle`      | `--color-warning`     | `--radius-full`   |
| Danger    | `--color-danger-subtle`       | `--color-danger`      | `--radius-full`   |
| Info      | `--color-info-subtle`         | `--color-info`        | `--radius-full`   |
| Brand     | `--accent-primary-subtle`     | `--accent-primary`    | `--radius-full`   |

Height: 24px, Padding: 0 10px, Font: caption

### 3.7 Sidebar Navigation

| Element         | Style                                     |
|-----------------|-------------------------------------------|
| Width Collapsed | 100px                                     |
| Width Expanded  | 260px                                     |
| BG              | `--gradient-sidebar`                      |
| Border Right    | `--border-subtle`                         |
| Nav Item Height | 48px                                      |
| Nav Icon Size   | 22px                                      |
| Active BG       | `--accent-primary-subtle`                 |
| Active Border-L | 3px solid `--accent-primary`              |
| Active Text     | `--text-primary`                          |
| Inactive Text   | `--text-tertiary`                         |
| Hover BG        | rgba(255,255,255,0.04)                    |

### 3.8 Toggle / Switch

| Property    | Value                                       |
|-------------|---------------------------------------------|
| Width       | 44px                                        |
| Height      | 24px                                        |
| BG Off      | `--bg-elevated`                             |
| BG On       | `--accent-primary`                          |
| Knob Size   | 18px                                        |
| Knob Color  | #FFFFFF                                     |
| Radius      | `--radius-full`                             |
| Transition  | `--transition-base`                         |

### 3.9 Select / Dropdown

Same styling as Input Fields. Dropdown panel:
- BG: `--bg-elevated`
- Border: `--border-default`
- Radius: `--radius-md`
- Shadow: `--shadow-lg`
- Item height: 40px
- Item hover: `--bg-elevated-hover`
- Item active: `--accent-primary-subtle` with accent text

### 3.10 Tabs

| Element     | Style                                       |
|-------------|---------------------------------------------|
| Container   | `--bg-surface`, `--radius-full`, padding 4px|
| Tab Height  | 36px                                        |
| Tab Radius  | `--radius-full`                             |
| Active BG   | `--accent-primary`                          |
| Active Text | #FFFFFF                                     |
| Default BG  | transparent                                 |
| Default Text| `--text-tertiary`                           |
| Font        | body-sm, weight 600                         |

### 3.11 Progress Bar

| Property     | Value                                       |
|--------------|---------------------------------------------|
| Height       | 8px (SM), 12px (MD), 16px (LG)             |
| BG Track     | `--bg-elevated`                             |
| BG Fill      | `--gradient-brand` or semantic color        |
| Radius       | `--radius-full`                             |
| Transition   | width `--transition-slow`                   |

### 3.12 Toast / Notification

| Property     | Value                                       |
|--------------|---------------------------------------------|
| BG           | `--bg-elevated`                             |
| Border       | `--border-default`                          |
| Border-left  | 4px solid (semantic color)                  |
| Radius       | `--radius-md`                               |
| Shadow       | `--shadow-lg`                               |
| Padding      | 16px 20px                                   |
| Max-width    | 420px                                       |

### 3.13 Avatar

| Size   | Dimensions | Font Size (initials) | Radius            |
|--------|------------|----------------------|-------------------|
| XS     | 28px       | 11px                 | `--radius-full`   |
| SM     | 36px       | 13px                 | `--radius-full`   |
| MD     | 44px       | 16px                 | `--radius-full`   |
| LG     | 56px       | 20px                 | `--radius-full`   |
| XL     | 72px       | 24px                 | `--radius-full`   |

### 3.14 Tooltip

| Property     | Value                                       |
|--------------|---------------------------------------------|
| BG           | `--bg-elevated`                             |
| Text         | `--text-primary`                            |
| Padding      | 8px 12px                                    |
| Radius       | `--radius-sm`                               |
| Shadow       | `--shadow-md`                               |
| Font         | caption                                     |
| Arrow        | 6px CSS triangle                            |

### 3.15 Modal / Dialog

| Property     | Value                                       |
|--------------|---------------------------------------------|
| Overlay BG   | `--bg-overlay`                              |
| Container BG | `--bg-surface`                              |
| Radius       | `--radius-2xl`                              |
| Shadow       | `--shadow-xl`                               |
| Max-width    | 560px (SM), 720px (MD), 960px (LG)         |
| Padding      | 32px                                        |
| Header       | heading-lg, border-bottom subtle            |
| Footer       | border-top subtle, flex end, gap 12px       |

### 3.16 Checkbox & Radio

| Property     | Value                                       |
|--------------|---------------------------------------------|
| Size         | 20px                                        |
| BG Off       | `--bg-input`                                |
| Border Off   | `--border-default`                          |
| BG On        | `--accent-primary`                          |
| Checkmark    | #FFFFFF, 2px stroke                         |
| Radius CB    | `--radius-xs`                               |
| Radius Radio | `--radius-full`                             |

### 3.17 Textarea

Same as Input but:
- Min-height: 120px
- Padding: 14px 16px
- Resize: vertical

### 3.18 List Items (History/Feed)

| Property         | Value                                    |
|------------------|------------------------------------------|
| Padding          | 16px                                     |
| Border-bottom    | `--border-subtle`                        |
| Hover BG         | `--bg-surface-hover`                     |
| Avatar           | SM size, left-aligned                    |
| Title            | body-md, weight 600, `--text-primary`    |
| Subtitle         | body-sm, `--text-tertiary`               |
| Timestamp        | caption, `--text-tertiary`, right-aligned|

### 3.19 Separator / Divider

| Variant      | Style                                      |
|--------------|--------------------------------------------|
| Horizontal   | 1px solid rgba(255,255,255,0.06), my 16px  |
| Vertical     | 1px solid rgba(255,255,255,0.06), mx 16px  |
| Dashed       | 1px dashed rgba(255,255,255,0.1)           |

### 3.20 Empty State

- Centered layout
- Icon: 64px, `--text-muted`
- Heading: heading-md, `--text-secondary`
- Description: body-sm, `--text-tertiary`
- CTA: Primary button, SM or MD

### 3.21 Loading / Skeleton

- BG: `--bg-elevated`
- Radius: matches target component
- Animation: shimmer pulse (opacity 0.3 to 0.6 to 0.3, 1.8s ease infinite)
- Gradient overlay: linear-gradient(90deg, transparent, rgba(255,255,255,0.04), transparent)

### 3.22 Slider / Range

| Property      | Value                                      |
|---------------|--------------------------------------------|
| Track Height  | 6px                                        |
| Track BG      | `--bg-elevated`                            |
| Fill BG       | `--accent-primary`                         |
| Thumb Size    | 18px                                       |
| Thumb BG      | #FFFFFF                                    |
| Thumb Shadow  | `--shadow-sm`                              |
| Radius        | `--radius-full`                            |

### 3.23 Breadcrumb

- Font: body-sm
- Separator: `/` or `›` in `--text-muted`
- Link: `--text-tertiary`, hover `--text-primary`
- Active: `--text-primary`, weight 600

### 3.24 Alert / Banner

| Variant  | Border-left Color  | Icon Color        | BG                        |
|----------|--------------------|-------------------|---------------------------|
| Info     | `--color-info`     | `--color-info`    | `--color-info-subtle`     |
| Success  | `--color-success`  | `--color-success` | `--color-success-subtle`  |
| Warning  | `--color-warning`  | `--color-warning` | `--color-warning-subtle`  |
| Danger   | `--color-danger`   | `--color-danger`  | `--color-danger-subtle`   |

Padding: 16px 20px, Radius: `--radius-md`, border-left: 4px solid

### 3.25 Stats for Nerds (ESP32 System Panel)

A dense, monospaced, terminal-aesthetic panel displaying real-time ESP32 system metrics. Inspired by "unix porn" / htop / neofetch style. Uses small text, ASCII-art dividers, and a dark deepest background for maximum nerd appeal.

**Container** — `.scry-nerds`
| Property     | Value                                          |
|--------------|-------------------------------------------------|
| Background   | `--bg-deepest` (#070D2D)                        |
| Border       | 1px solid `rgba(117,81,255,0.15)`               |
| Radius       | `--radius-lg`                                   |
| Padding      | 20px 24px                                       |
| Font         | `--font-mono`, 12px, weight 400                 |
| Inner glow   | `box-shadow: inset 0 0 60px rgba(117,81,255,0.03)` |

**Header** — `.scry-nerds__header`
- ASCII-art title: `═══ SYSTEM STATS ═══`
- Font: 11px, weight 700, `--accent-primary`, letter-spacing 0.12em, uppercase

**Row** — `.scry-nerds__row`
- Flex: label left (muted `#4A5568`) → value right (white or semantic)
- Dense height: ~22px, no extra spacing
- Semantic coloring: green=OK, yellow=warn, red=critical, cyan=info

**Divider** — `.scry-nerds__divider`
- ASCII: `── ── ── ── ── ──` in `rgba(112,126,174,0.2)`, margin 6px 0

**ASCII bars** for memory/flash:
- `[████████░░] 78%` using `--color-success`/`--color-warning`/`--color-danger` thresholds

**Typical fields**: CPU Freq, CPU Temp, Free Heap, Used Flash, PSRAM, WiFi SSID, RSSI, IP, MAC, Uptime, NTP, Sensor status, Last fetch timestamps, FW version, SDK version

---

## 4. ICONOGRAPHY

Use **Lucide Icons** or **Phosphor Icons** (outline style, 1.5px stroke).
- Default size: 20px
- Sidebar icons: 22px
- Stat card icons: 24px
- Feature icons: 28px
- Color: inherits text color or explicit semantic color

---

## 5. ESP32 SPECIFIC PATTERNS

### 5.1 Status Indicators

For showing ESP32 system status (WiFi, memory, uptime):
- Use dot indicator (8px circle) + label
- Green: Connected/OK, Yellow: Warning, Red: Error, Blue: Processing
- Pulse animation for "connecting" states

### 5.2 Configuration Forms

- Group related settings in Cards
- Use clear section headings (heading-md)
- Inline help text under inputs (caption, --text-tertiary)
- Save buttons always visible (sticky footer or card footer)
- Show "unsaved changes" indicator (warning badge)

### 5.3 Data Display

- System metrics: Stat Cards layout for dashboard overview
- **Stats for Nerds**: Use `.scry-nerds` panel (section 3.25) for dense, terminal-style system diagnostics — CPU, memory, network, sensors, fetch history. Monospaced, ASCII-art aesthetic.
- RSS feeds: List Items pattern
- Logs: Monospace font (system), in a scrollable container with --bg-deepest
- JSON: Syntax-highlighted code blocks

---

## 6. RESPONSIVE BREAKPOINTS

| Token         | Value   | Usage                     |
|---------------|---------|---------------------------|
| `--bp-sm`     | 640px   | Mobile landscape          |
| `--bp-md`     | 768px   | Tablet portrait           |
| `--bp-lg`     | 1024px  | Tablet landscape / small desktop |
| `--bp-xl`     | 1280px  | Desktop                   |
| `--bp-2xl`    | 1536px  | Large desktop             |

---

## 7. ACCESSIBILITY

- All interactive elements must have visible focus states (2px ring)
- Minimum touch target: 44px × 44px
- Color contrast: WCAG AA for body text (4.5:1), AAA preferred for headings
- Reduced motion: respect `prefers-reduced-motion` media query
- Semantic HTML: proper heading hierarchy, ARIA labels where needed

---

*© 2026 Netmilk Studio sagl — Monteggio Plains. All rights reserved.*
