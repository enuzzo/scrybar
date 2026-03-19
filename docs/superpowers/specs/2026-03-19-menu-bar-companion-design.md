# ScryBar Companion: Menu Bar App Redesign

Date: 2026-03-19

## Goal

Transform the ScryBar macOS companion from a windowed dashboard app into a lightweight menu bar utility. The app should feel like "open and it works" — clean now-playing display, minimal chrome, no exposed plumbing.

## Current State

- Standard `WindowGroup` app (min 1120x780)
- `NavigationSplitView` with sidebar (connection) + detail (dashboard)
- Exposes raw JSON payload, provider picker, manual host/port, metrics grid, discovery list
- 10 Swift source files, ~2800 lines total
- No menu bar or `NSStatusItem` integration

## Architecture

### App Lifecycle

- **No Dock icon**: set `LSUIElement = true` via Info.plist key (`INFOPLIST_KEY_LSUIElement: YES` in project.yml build settings)
- **No window**: remove `WindowGroup` entirely
- **Menu bar only**: `NSStatusItem` + `NSPopover` hosted via `AppDelegate`
- **Quit behavior**: Cmd+Q or right-click menu bar icon > "Quit ScryBar Companion" to exit. Clicking away from popover just hides it.
- **Launch at login**: out of scope for this iteration

### Entry Point Change

Replace the current SwiftUI `App` struct with an `NSApplicationDelegate`-based setup.

**Mechanism**: keep `ScryBarCompanionApp.swift` as a thin `@main` struct conforming to `App`, but with an empty `Scene` body (using `Settings {}`). Attach `AppDelegate` via `@NSApplicationDelegateAdaptor`. This avoids `main.swift` complexity and works cleanly with both SPM and xcodegen.

```swift
@main
struct ScryBarCompanionApp: App {
    @NSApplicationDelegateAdaptor(AppDelegate.self) var appDelegate
    var body: some Scene {
        Settings { EmptyView() }
    }
}
```

```
ScryBarCompanionApp (@main, SwiftUI App)
  └── AppDelegate (@NSApplicationDelegateAdaptor)
        ├── NSStatusItem (menu bar icon + track title)
        ├── NSPopover → PopoverView (SwiftUI, hosted via NSHostingController)
        └── AppModel (shared @ObservableObject)
```

**Swift 6 concurrency**: `AppDelegate` must be `@MainActor final class AppDelegate: NSObject, NSApplicationDelegate` since it owns `NSStatusItem` and `NSPopover` (AppKit main-thread types).

The `AppDelegate` owns:
- `NSStatusItem` creation and updates
- `NSPopover` lifecycle (show/hide on click)
- Right-click context menu (Quit, Settings)
- `AppModel` instance passed as environment object to SwiftUI views
- Main menu with Quit item (ensures Cmd+Q works even when popover is dismissed)

### Menu Bar Item

- **Icon**: SF Symbol `waveform` (monochrome, adapts to menu bar appearance)
- **Title**: current track name, truncated to ~30 characters with ellipsis
- **Updates**: `AppModel.currentPayload` changes drive title updates via observation
- **When nothing is playing**: icon only, no title text (or "—")

### Popover View

Compact popover, approximately 320px wide x 260px tall. Dark theme.

```
┌─────────────────────────────────────┐
│                                     │
│  ┌────────┐  Nanai                  │
│  │        │  Mala Rodríguez         │
│  │  ART   │  Malamarismo            │
│  │        │                         │
│  └────────┘  ▶ Playing  ● ScryBar   │
│                                     │
│  ━━━━━━━━━━━━━━━━━━━━━━━━━━  1:24   │
│                                     │
│  [ Send Now ]               [ ⚙ ]   │
└─────────────────────────────────────┘
```

**Layout details:**

- **Artwork**: 64x64px, rounded corners (8px radius). Falls back to a subtle music note icon when no artwork is available.
- **Track info**: title (15pt semibold), artist (13pt medium, secondary color), album (12pt, tertiary color). Each line truncated to 1 line with ellipsis.
- **Status row**: play/pause indicator + connection status. Connection shows green dot + "ScryBar" when a device is discovered, gray dot + "No device" otherwise.
- **Progress bar**: thin horizontal bar showing elapsed/duration. Elapsed time label right-aligned. Displays the last polled value (1-second resolution from `AppModel` polling). No client-side interpolation — the 1Hz refresh is smooth enough for a status utility.
- **Bottom bar**: "Send Now" button (compact, accent color) + gear icon button (opens settings).
- **Auto-send**: enabled by default in `AppModel`. No toggle in the main popover view — it just works.

### Settings Panel

Clicking the gear icon in the popover transitions to a settings view (same popover, content swap via navigation):

```
┌─────────────────────────────────────┐
│  ← Back                 Settings    │
│─────────────────────────────────────│
│                                     │
│  Provider     [ System ▾ ]          │
│                                     │
│  Target       [ ScryBar DB1C ▾ ]    │
│               scrybar-db1c:8080     │
│                                     │
│  ─────────────────────────────────  │
│  Manual Host  [ ______________ ]    │
│  Port         [ 8080           ]    │
│                                     │
│  Auto-send    [  ON  ]              │
│                                     │
│  ─────────────────────────────────  │
│  v0.1.2 (3)                         │
└─────────────────────────────────────┘
```

**Settings fields:**

- **Provider**: picker (System / Music.app / Mock). Default: System.
- **Target**: dropdown of discovered ScryBars. Auto-selects first discovered device.
- **Manual Host/Port**: text fields for manual targeting. Shown via a `DisclosureGroup("Manual Target")` below the Target picker, collapsed by default.
- **Auto-send toggle**: on/off.
- **Version badge**: small monospaced label at bottom.

### Right-Click Menu

Right-clicking the menu bar icon shows a native `NSMenu`:

- "Send Now" — triggers immediate send
- Separator
- "Settings..." — opens popover to settings panel
- Separator
- "Quit ScryBar Companion" — `NSApp.terminate(nil)`

## Files Plan

### New files

| File | Purpose |
|------|---------|
| `AppDelegate.swift` | `NSApplicationDelegate`, owns status item + popover + AppModel |
| `PopoverView.swift` | Compact now-playing SwiftUI view |
| `SettingsView.swift` | Settings panel SwiftUI view |

### Modified files

| File | Change |
|------|--------|
| `ScryBarCompanionApp.swift` | Replace body with `@NSApplicationDelegateAdaptor` + empty `Settings` scene (see Entry Point Change) |
| `CompanionTheme.swift` | Keep palette and typography. Remove unused dashboard-specific components (DashboardCard, MetricGrid modifiers). Add popover-specific layout constants. |
| `AppModel.swift` | Set `autoSendEnabled = true` by default (persisted to `UserDefaults` key `autoSendEnabled`). Add observation hook for menu bar title updates. |
| `project.yml` | Bump version to 0.2.0 (4) — minor version bump intentional for architectural overhaul. Add `INFOPLIST_KEY_LSUIElement: YES`. |

### Removed files

| File | Reason |
|------|--------|
| `ContentView.swift` | NavigationSplitView shell — replaced by popover |
| `ConnectionSidebarView.swift` | Sidebar — functionality moved to SettingsView |
| `NowPlayingDashboardView.swift` | Dashboard — replaced by PopoverView |

### Untouched files

| File | Reason |
|------|--------|
| `Providers.swift` | Core provider logic — no UI changes needed |
| `Models.swift` | Data structures — unchanged |
| `ScryBarClient.swift` | HTTP transport — unchanged |
| `ScryBarDiscovery.swift` | Bonjour discovery — unchanged |

## Behavior Details

### Popover show/hide

- **Left-click** on menu bar icon: toggle popover (show/hide)
- **Right-click** on menu bar icon: show context menu
- **Click outside popover**: popover hides (standard `NSPopover` behavior with `.transient` behavior)
- **Cmd+Q**: quit app. `AppDelegate` creates a minimal `NSMenu` as the app's main menu with a Quit item (shortcut Cmd+Q) so the shortcut works even when the popover is dismissed. This is required because `LSUIElement` apps have no default application menu.

### Auto-send default

`autoSendEnabled` defaults to `true`. When a ScryBar device is discovered, the companion immediately starts sending now-playing updates every polling cycle (1 second). The user doesn't need to press anything.

### Connection status

- Discovery runs continuously in the background (same as today)
- The popover shows a simple dot indicator: green = device found, gray = no device
- If no device is found, "Send Now" is disabled but the app otherwise works normally

### Artwork loading

Two artwork sources exist in the current payload:

1. **`artworkURL`** (String?) — either an HTTPS URL (TIDAL cover art from `resources.tidal.com`) or a `file://` URL pointing to a temp file written by `SystemNowPlayingProvider`.
2. **`artworkRGB565B64`** (String?) — base64-encoded RGB565 pixel data (150x150), used for firmware transmission but not for display.

**PopoverView strategy**: use `AsyncImage(url:)` for HTTPS artwork URLs. For `file://` URLs (SystemNowPlayingProvider temp files), load via `NSImage(contentsOf:)` and wrap in SwiftUI `Image(nsImage:)`. When neither is available, show a placeholder SF Symbol (`music.note`).

- Artwork displayed at 64x64 with 8px corner radius
- Cached implicitly by `artworkID` (only reloads when the ID changes, tracked via `.task(id: artworkURL)`)
- `artworkRGB565B64` is not used in the popover — it's only relevant for firmware payloads

## Success Criteria

1. App launches into menu bar with no Dock icon
2. Menu bar shows waveform icon + current track title
3. Clicking icon shows compact now-playing popover with artwork, metadata, and status
4. Gear icon opens settings panel for provider/target/auto-send configuration
5. Closing popover hides it; only Cmd+Q or "Quit" actually quits
6. Auto-send works by default when a ScryBar is discovered
7. App builds successfully with `swift build` (compile check) and `xcodebuild` (full app bundle with LSUIElement behavior)

## Out of Scope

- Launch at login
- Keyboard shortcuts beyond Cmd+Q
- Notification center integration
- Multiple simultaneous ScryBar targets
- Light mode (stays dark-only via `.preferredColorScheme(.dark)` on the popover's root view)
