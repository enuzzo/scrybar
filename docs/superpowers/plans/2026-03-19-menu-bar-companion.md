# Menu Bar Companion Implementation Plan

> **For agentic workers:** REQUIRED: Use superpowers:subagent-driven-development (if subagents available) or superpowers:executing-plans to implement this plan. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Transform the ScryBar macOS companion from a windowed dashboard into a lightweight menu bar utility with NSPopover.

**Architecture:** `@NSApplicationDelegateAdaptor` wires an `AppDelegate` that owns an `NSStatusItem` (menu bar icon + track title) and an `NSPopover` hosting a compact SwiftUI `PopoverView`. Settings live in a separate SwiftUI view navigated within the same popover. Three old view files are deleted; core logic (AppModel, Providers, Models, Client, Discovery) stays untouched.

**Tech Stack:** Swift 6, SwiftUI, AppKit (NSStatusItem, NSPopover, NSMenu), xcodegen

**Spec:** `docs/superpowers/specs/2026-03-19-menu-bar-companion-design.md`

**Build commands:**
```bash
cd companion/mac/ScryBarCompanion
xcodegen generate
swift build
xcodebuild -project ScryBarCompanion.xcodeproj -scheme ScryBarCompanion -configuration Debug -derivedDataPath /tmp/ScryBarCompanionDerivedData build
```

---

### Task 1: Update project.yml and AppModel defaults

**Files:**
- Modify: `companion/mac/ScryBarCompanion/project.yml`
- Modify: `companion/mac/ScryBarCompanion/Sources/ScryBarCompanion/AppModel.swift`

- [ ] **Step 1: Update project.yml**

In `project.yml`, bump version and add LSUIElement:

```yaml
settings:
  base:
    SWIFT_VERSION: "6.0"
    CURRENT_PROJECT_VERSION: 4
    MARKETING_VERSION: "0.2.0"
    GENERATE_INFOPLIST_FILE: YES
    INFOPLIST_KEY_LSUIElement: YES
    PRODUCT_BUNDLE_IDENTIFIER: com.netmilk.ScryBarCompanion
    PRODUCT_NAME: ScryBarCompanion
    INFOPLIST_KEY_CFBundleDisplayName: ScryBar Companion
    INFOPLIST_KEY_LSApplicationCategoryType: public.app-category.utilities
    CODE_SIGN_STYLE: Automatic
    DEVELOPMENT_TEAM: ""
    LD_RUNPATH_SEARCH_PATHS:
      - $(inherited)
      - "@executable_path/../Frameworks"
```

Changes from current: `CURRENT_PROJECT_VERSION: 3` → `4`, `MARKETING_VERSION: "0.1.2"` → `"0.2.0"`, add `INFOPLIST_KEY_LSUIElement: YES`.

Note: the TIDAL fix already bumped to 0.1.2 (3), so this is the next increment.

- [ ] **Step 2: Update AppModel auto-send default and persistence**

In `AppModel.swift`, change line 13:

From: `@Published var autoSendEnabled = false`

To: `@Published var autoSendEnabled = UserDefaults.standard.object(forKey: "autoSendEnabled") == nil ? true : UserDefaults.standard.bool(forKey: "autoSendEnabled")`

Add a `didSet` observer by converting to a computed-init pattern. Simpler approach — add persistence in the polling toggle. Add this method to `AppModel`:

```swift
func setAutoSend(_ enabled: Bool) {
    autoSendEnabled = enabled
    UserDefaults.standard.set(enabled, forKey: "autoSendEnabled")
}
```

Change the init default:

```swift
@Published var autoSendEnabled: Bool = {
    if UserDefaults.standard.object(forKey: "autoSendEnabled") == nil { return true }
    return UserDefaults.standard.bool(forKey: "autoSendEnabled")
}()
```

Also add a flag for settings-on-open (used by right-click "Settings..." menu item):

```swift
@Published var showSettingsOnOpen = false
```

- [ ] **Step 3: Build to verify changes compile**

Run: `cd companion/mac/ScryBarCompanion && xcodegen generate && swift build`

Expected: Build succeeded

- [ ] **Step 4: Commit**

```bash
git add companion/mac/ScryBarCompanion/project.yml companion/mac/ScryBarCompanion/Sources/ScryBarCompanion/AppModel.swift
git commit -m "feat(companion): bump to v0.2.0, add LSUIElement, default auto-send on"
```

---

### Task 2: Create AppDelegate with NSStatusItem and NSPopover

**Files:**
- Create: `companion/mac/ScryBarCompanion/Sources/ScryBarCompanion/AppDelegate.swift`
- Modify: `companion/mac/ScryBarCompanion/Sources/ScryBarCompanion/ScryBarCompanionApp.swift`

- [ ] **Step 1: Create AppDelegate.swift**

Create `companion/mac/ScryBarCompanion/Sources/ScryBarCompanion/AppDelegate.swift`:

```swift
import AppKit
import SwiftUI

@MainActor
final class AppDelegate: NSObject, NSApplicationDelegate {
    let model = AppModel()
    private var statusItem: NSStatusItem!
    private let popover = NSPopover()
    private var observation: NSKeyValueObservation?

    func applicationDidFinishLaunching(_ notification: Notification) {
        setupMainMenu()
        setupStatusItem()
        setupPopover()
        startTitleUpdates()
    }

    // MARK: - Main Menu (enables Cmd+Q for LSUIElement apps)

    private func setupMainMenu() {
        let mainMenu = NSMenu()
        let appMenuItem = NSMenuItem()
        let appMenu = NSMenu()
        appMenu.addItem(withTitle: "Quit ScryBar Companion", action: #selector(NSApplication.terminate(_:)), keyEquivalent: "q")
        appMenuItem.submenu = appMenu
        mainMenu.addItem(appMenuItem)
        NSApplication.shared.mainMenu = mainMenu
    }

    // MARK: - Status Item

    private func setupStatusItem() {
        statusItem = NSStatusBar.system.statusItem(withLength: NSStatusItem.variableLength)
        guard let button = statusItem.button else { return }

        let image = NSImage(systemSymbolName: "waveform", accessibilityDescription: "ScryBar")
        image?.isTemplate = true
        button.image = image
        button.imagePosition = .imageLeading
        button.target = self
        button.action = #selector(statusItemClicked(_:))
        button.sendAction(on: [.leftMouseUp, .rightMouseUp])
    }

    @objc private func statusItemClicked(_ sender: NSStatusBarButton) {
        guard let event = NSApp.currentEvent else { return }
        if event.type == .rightMouseUp {
            showContextMenu()
        } else {
            togglePopover(sender)
        }
    }

    private func togglePopover(_ sender: NSView) {
        if popover.isShown {
            popover.performClose(nil)
        } else {
            popover.show(relativeTo: sender.bounds, of: sender, preferredEdge: .minY)
            popover.contentViewController?.view.window?.makeKey()
        }
    }

    // MARK: - Context Menu

    private func showContextMenu() {
        let menu = NSMenu()
        menu.addItem(withTitle: "Send Now", action: #selector(contextSendNow), keyEquivalent: "")
        menu.addItem(.separator())
        menu.addItem(withTitle: "Settings…", action: #selector(contextOpenSettings), keyEquivalent: ",")
        menu.addItem(.separator())
        menu.addItem(withTitle: "Quit ScryBar Companion", action: #selector(NSApplication.terminate(_:)), keyEquivalent: "q")
        statusItem.menu = menu
        statusItem.button?.performClick(nil)
        // Remove menu after display so left-click works again
        DispatchQueue.main.async { [weak self] in
            self?.statusItem.menu = nil
        }
    }

    @objc private func contextSendNow() {
        model.sendNow()
    }

    @objc private func contextOpenSettings() {
        model.showSettingsOnOpen = true
        if let button = statusItem.button {
            togglePopover(button)
        }
    }

    // MARK: - Popover

    private func setupPopover() {
        popover.contentSize = NSSize(width: 320, height: 280)
        popover.behavior = .transient
        popover.animates = true
        let hostingController = NSHostingController(
            rootView: PopoverContentView()
                .environmentObject(model)
        )
        popover.contentViewController = hostingController
    }

    // MARK: - Title Updates

    private func startTitleUpdates() {
        // Poll title from model every second (same cadence as AppModel polling)
        Task { [weak self] in
            while !Task.isCancelled {
                guard let self else { return }
                self.updateTitle()
                try? await Task.sleep(for: .seconds(1))
            }
        }
    }

    private func updateTitle() {
        let payload = model.currentPayload
        if payload.isPlaying, payload.title != "—" {
            let maxLen = 30
            let title = payload.title.count > maxLen
                ? String(payload.title.prefix(maxLen - 1)) + "…"
                : payload.title
            statusItem.button?.title = " \(title)"
        } else {
            statusItem.button?.title = ""
        }
    }
}
```

- [ ] **Step 2: Rewrite ScryBarCompanionApp.swift**

Replace the entire content of `ScryBarCompanionApp.swift`:

```swift
import SwiftUI

@main
struct ScryBarCompanionApp: App {
    @NSApplicationDelegateAdaptor(AppDelegate.self) var appDelegate

    var body: some Scene {
        Settings { EmptyView() }
    }
}
```

- [ ] **Step 3: Build to verify AppDelegate compiles**

This will fail because `PopoverContentView` doesn't exist yet. Create a temporary stub to verify the AppDelegate/entry point wiring:

Add to the bottom of `AppDelegate.swift` temporarily:

```swift
struct PopoverContentView: View {
    @EnvironmentObject var model: AppModel
    var body: some View {
        Text("ScryBar").frame(width: 320, height: 280)
    }
}
```

Run: `cd companion/mac/ScryBarCompanion && xcodegen generate && swift build`

Expected: Build succeeded

- [ ] **Step 4: Remove the temporary PopoverContentView stub from AppDelegate.swift**

Delete the `PopoverContentView` struct from the bottom of `AppDelegate.swift`. The real one will be created in Task 3.

- [ ] **Step 5: Commit**

```bash
git add companion/mac/ScryBarCompanion/Sources/ScryBarCompanion/AppDelegate.swift companion/mac/ScryBarCompanion/Sources/ScryBarCompanion/ScryBarCompanionApp.swift
git commit -m "feat(companion): add AppDelegate with NSStatusItem and NSPopover shell"
```

---

### Task 3: Create PopoverView (now-playing display)

**Files:**
- Create: `companion/mac/ScryBarCompanion/Sources/ScryBarCompanion/PopoverView.swift`

- [ ] **Step 1: Create PopoverView.swift**

Create `companion/mac/ScryBarCompanion/Sources/ScryBarCompanion/PopoverView.swift`:

```swift
import AppKit
import SwiftUI

struct PopoverContentView: View {
    @EnvironmentObject var model: AppModel
    @State private var showSettings = false

    var body: some View {
        if showSettings {
            SettingsView(showSettings: $showSettings)
                .environmentObject(model)
        } else {
            NowPlayingPopoverView(showSettings: $showSettings)
                .environmentObject(model)
        }
    }
    .onAppear {
        if model.showSettingsOnOpen {
            model.showSettingsOnOpen = false
            showSettings = true
        }
    }
}

struct NowPlayingPopoverView: View {
    @EnvironmentObject var model: AppModel
    @Binding var showSettings: Bool

    private var payload: NowPlayingPayload { model.currentPayload }
    private var hasDevice: Bool { !model.discoveredEndpoints.isEmpty || model.selectedEndpoint != nil }

    var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            // Artwork + track info
            HStack(alignment: .top, spacing: 12) {
                ArtworkView(artworkURL: payload.artworkURL)
                    .frame(width: 64, height: 64)

                VStack(alignment: .leading, spacing: 3) {
                    Text(payload.title)
                        .font(.system(size: 15, weight: .semibold))
                        .foregroundStyle(CompanionTheme.textPrimary)
                        .lineLimit(1)

                    Text(payload.artist)
                        .font(.system(size: 13, weight: .medium))
                        .foregroundStyle(CompanionTheme.textSecondary)
                        .lineLimit(1)

                    if !payload.album.isEmpty {
                        Text(payload.album)
                            .font(.system(size: 12))
                            .foregroundStyle(CompanionTheme.textTertiary)
                            .lineLimit(1)
                    }

                    Spacer(minLength: 0)

                    // Status row
                    HStack(spacing: 12) {
                        HStack(spacing: 4) {
                            Image(systemName: payload.isPlaying ? "play.fill" : "pause.fill")
                                .font(.system(size: 10))
                            Text(payload.isPlaying ? "Playing" : "Paused")
                                .font(.system(size: 11, weight: .medium))
                        }
                        .foregroundStyle(payload.isPlaying ? CompanionTheme.success : CompanionTheme.textTertiary)

                        HStack(spacing: 4) {
                            Circle()
                                .fill(hasDevice ? CompanionTheme.success : CompanionTheme.textDisabled)
                                .frame(width: 6, height: 6)
                            Text(hasDevice ? "ScryBar" : "No device")
                                .font(.system(size: 11, weight: .medium))
                                .foregroundStyle(CompanionTheme.textTertiary)
                        }
                    }
                }
            }

            // Progress bar
            VStack(spacing: 4) {
                GeometryReader { geometry in
                    ZStack(alignment: .leading) {
                        RoundedRectangle(cornerRadius: 2)
                            .fill(CompanionTheme.border)
                            .frame(height: 3)

                        RoundedRectangle(cornerRadius: 2)
                            .fill(CompanionTheme.accent)
                            .frame(width: progressWidth(totalWidth: geometry.size.width), height: 3)
                    }
                }
                .frame(height: 3)

                HStack {
                    Spacer()
                    Text(formatTime(payload.elapsedSec))
                        .font(.system(size: 10, weight: .medium, design: .monospaced))
                        .foregroundStyle(CompanionTheme.textTertiary)
                }
            }

            // Bottom bar
            HStack {
                Button {
                    model.sendNow()
                } label: {
                    Label("Send Now", systemImage: "paperplane.fill")
                        .font(.system(size: 12, weight: .medium))
                }
                .buttonStyle(CompanionPrimaryButtonStyle())
                .controlSize(.small)
                .disabled(!hasDevice)

                Spacer()

                Button {
                    showSettings = true
                } label: {
                    Image(systemName: "gearshape.fill")
                        .font(.system(size: 14))
                        .foregroundStyle(CompanionTheme.textSecondary)
                }
                .buttonStyle(.plain)
            }
        }
        .padding(16)
        .frame(width: 320)
        .background(CompanionTheme.windowBackground)
        .preferredColorScheme(.dark)
    }

    private func progressWidth(totalWidth: CGFloat) -> CGFloat {
        guard payload.durationSec > 0 else { return 0 }
        let ratio = min(payload.elapsedSec / payload.durationSec, 1.0)
        return totalWidth * CGFloat(ratio)
    }

    private func formatTime(_ seconds: Double) -> String {
        let total = Int(max(seconds, 0))
        let m = total / 60
        let s = total % 60
        return String(format: "%d:%02d", m, s)
    }
}

struct ArtworkView: View {
    let artworkURL: String?
    @State private var image: NSImage?

    var body: some View {
        ZStack {
            RoundedRectangle(cornerRadius: 8, style: .continuous)
                .fill(CompanionTheme.surfaceElevated)

            if let image {
                Image(nsImage: image)
                    .resizable()
                    .scaledToFill()
                    .clipShape(RoundedRectangle(cornerRadius: 8, style: .continuous))
            } else {
                Image(systemName: "music.note")
                    .font(.system(size: 22, weight: .medium))
                    .foregroundStyle(CompanionTheme.textTertiary)
            }
        }
        .overlay(
            RoundedRectangle(cornerRadius: 8, style: .continuous)
                .stroke(CompanionTheme.border, lineWidth: 1)
        )
        .task(id: artworkURL) {
            image = await loadArtwork()
        }
    }

    private func loadArtwork() async -> NSImage? {
        guard let rawURL = artworkURL, !rawURL.isEmpty else { return nil }
        guard let url = URL(string: rawURL) else { return nil }

        if url.isFileURL {
            return NSImage(contentsOf: url)
        }

        guard let scheme = url.scheme?.lowercased(), scheme == "http" || scheme == "https" else {
            return nil
        }

        do {
            let (data, _) = try await URLSession.shared.data(from: url)
            return NSImage(data: data)
        } catch {
            return nil
        }
    }
}
```

- [ ] **Step 2: Build to verify (will fail — SettingsView not yet created)**

Create a temporary stub for `SettingsView` at the bottom of `PopoverView.swift`:

```swift
struct SettingsView: View {
    @Binding var showSettings: Bool
    @EnvironmentObject var model: AppModel
    var body: some View {
        VStack { Text("Settings"); Button("Back") { showSettings = false } }
            .frame(width: 320, height: 280)
            .background(CompanionTheme.windowBackground)
            .preferredColorScheme(.dark)
    }
}
```

Run: `cd companion/mac/ScryBarCompanion && xcodegen generate && swift build`

Expected: Build succeeded

- [ ] **Step 3: Remove the temporary SettingsView stub from PopoverView.swift**

Delete the `SettingsView` struct from the bottom of `PopoverView.swift`. The real one will be created in Task 4.

- [ ] **Step 4: Commit**

```bash
git add companion/mac/ScryBarCompanion/Sources/ScryBarCompanion/PopoverView.swift
git commit -m "feat(companion): add compact now-playing popover view"
```

---

### Task 4: Create SettingsView

**Files:**
- Create: `companion/mac/ScryBarCompanion/Sources/ScryBarCompanion/SettingsView.swift`

- [ ] **Step 1: Create SettingsView.swift**

Create `companion/mac/ScryBarCompanion/Sources/ScryBarCompanion/SettingsView.swift`:

```swift
import SwiftUI

struct SettingsView: View {
    @Binding var showSettings: Bool
    @EnvironmentObject var model: AppModel

    var body: some View {
        VStack(alignment: .leading, spacing: 0) {
            // Header
            HStack {
                Button {
                    showSettings = false
                } label: {
                    HStack(spacing: 4) {
                        Image(systemName: "chevron.left")
                            .font(.system(size: 11, weight: .semibold))
                        Text("Back")
                            .font(.system(size: 13, weight: .medium))
                    }
                }
                .buttonStyle(.plain)
                .foregroundStyle(CompanionTheme.accent)

                Spacer()

                Text("Settings")
                    .font(.system(size: 15, weight: .semibold))
                    .foregroundStyle(CompanionTheme.textPrimary)
            }
            .padding(.bottom, 12)

            Divider().overlay(CompanionTheme.divider)

            ScrollView {
                VStack(alignment: .leading, spacing: 16) {
                    // Provider
                    settingRow("Provider") {
                        Picker("", selection: $model.providerKind) {
                            ForEach(ProviderKind.allCases) { kind in
                                Text(kind.rawValue).tag(kind)
                            }
                        }
                        .labelsHidden()
                        .pickerStyle(.menu)
                    }

                    // Target
                    settingRow("Target") {
                        if model.discoveredEndpoints.isEmpty {
                            Text("No device found")
                                .font(.system(size: 12))
                                .foregroundStyle(CompanionTheme.textTertiary)
                        } else {
                            Picker("", selection: $model.selectedDiscoveredEndpointID) {
                                ForEach(model.discoveredEndpoints) { endpoint in
                                    Text(endpoint.name)
                                        .tag(Optional(endpoint.id))
                                }
                            }
                            .labelsHidden()
                            .pickerStyle(.menu)
                        }
                    }

                    if let endpoint = model.selectedEndpoint {
                        Text("\(endpoint.host):\(endpoint.port)")
                            .font(.system(size: 11, design: .monospaced))
                            .foregroundStyle(CompanionTheme.textTertiary)
                            .padding(.top, -8)
                    }

                    Divider().overlay(CompanionTheme.divider)

                    // Manual target
                    DisclosureGroup("Manual Target") {
                        VStack(alignment: .leading, spacing: 8) {
                            settingRow("Host") {
                                TextField("Hostname or IP", text: $model.manualHost)
                                    .textFieldStyle(.roundedBorder)
                                    .font(.system(size: 12))
                            }
                            settingRow("Port") {
                                TextField("8080", text: $model.manualPort)
                                    .textFieldStyle(.roundedBorder)
                                    .font(.system(size: 12))
                            }
                            Button("Save") {
                                model.saveManualTarget()
                            }
                            .buttonStyle(CompanionSecondaryButtonStyle())
                            .controlSize(.small)
                        }
                        .padding(.top, 4)
                    }
                    .font(.system(size: 12, weight: .medium))
                    .foregroundStyle(CompanionTheme.textSecondary)

                    Divider().overlay(CompanionTheme.divider)

                    // Auto-send
                    settingRow("Auto-send") {
                        Toggle("", isOn: Binding(
                            get: { model.autoSendEnabled },
                            set: { model.setAutoSend($0) }
                        ))
                        .labelsHidden()
                        .toggleStyle(.switch)
                        .controlSize(.small)
                    }

                    Spacer(minLength: 8)

                    // Version badge
                    Text(versionString)
                        .font(.system(size: 11, weight: .medium, design: .monospaced))
                        .foregroundStyle(CompanionTheme.textDisabled)
                }
                .padding(.top, 12)
            }
        }
        .padding(16)
        .frame(width: 320)
        .background(CompanionTheme.windowBackground)
        .preferredColorScheme(.dark)
    }

    private func settingRow<Content: View>(_ label: String, @ViewBuilder content: () -> Content) -> some View {
        HStack(alignment: .center) {
            Text(label)
                .font(.system(size: 13, weight: .medium))
                .foregroundStyle(CompanionTheme.textSecondary)
                .frame(width: 80, alignment: .leading)
            content()
        }
    }

    private var versionString: String {
        let version = Bundle.main.object(forInfoDictionaryKey: "CFBundleShortVersionString") as? String ?? "0.0.0"
        let build = Bundle.main.object(forInfoDictionaryKey: "CFBundleVersion") as? String ?? "0"
        return "v\(version) (\(build))"
    }
}
```

- [ ] **Step 2: Build**

Run: `cd companion/mac/ScryBarCompanion && xcodegen generate && swift build`

Expected: Build succeeded

- [ ] **Step 3: Commit**

```bash
git add companion/mac/ScryBarCompanion/Sources/ScryBarCompanion/SettingsView.swift
git commit -m "feat(companion): add settings panel for provider, target, and auto-send"
```

---

### Task 5: Delete old view files and clean up CompanionTheme

**Files:**
- Delete: `companion/mac/ScryBarCompanion/Sources/ScryBarCompanion/ContentView.swift`
- Delete: `companion/mac/ScryBarCompanion/Sources/ScryBarCompanion/ConnectionSidebarView.swift`
- Delete: `companion/mac/ScryBarCompanion/Sources/ScryBarCompanion/NowPlayingDashboardView.swift`

- [ ] **Step 1: Delete old view files**

```bash
cd companion/mac/ScryBarCompanion
rm Sources/ScryBarCompanion/ContentView.swift
rm Sources/ScryBarCompanion/ConnectionSidebarView.swift
rm Sources/ScryBarCompanion/NowPlayingDashboardView.swift
```

- [ ] **Step 2: Regenerate project and build**

Run: `cd companion/mac/ScryBarCompanion && xcodegen generate && swift build`

Expected: Build succeeded (no references to deleted views remain — `ContentView` was only used in old `ScryBarCompanionApp.swift`, `ConnectionSidebarView` and `NowPlayingDashboardView` were only used in `ContentView`)

- [ ] **Step 3: Verify with xcodebuild too**

Run: `xcodebuild -project ScryBarCompanion.xcodeproj -scheme ScryBarCompanion -configuration Debug -derivedDataPath /tmp/ScryBarCompanionDerivedData build`

Expected: BUILD SUCCEEDED

- [ ] **Step 4: Commit**

```bash
git add -A companion/mac/ScryBarCompanion/
git commit -m "refactor(companion): remove old windowed views, complete menu bar migration"
```

---

### Task 6: Final verification and version commit

**Files:**
- None (verification only)

- [ ] **Step 1: Full clean build**

```bash
cd companion/mac/ScryBarCompanion
xcodegen generate
swift build --build-tests 2>/dev/null; swift build
xcodebuild -project ScryBarCompanion.xcodeproj -scheme ScryBarCompanion -configuration Debug -derivedDataPath /tmp/ScryBarCompanionDerivedData clean build
```

Expected: Both builds succeed

- [ ] **Step 2: Verify file inventory**

Remaining source files should be:
- `ScryBarCompanionApp.swift` (thin entry point)
- `AppDelegate.swift` (status item + popover)
- `PopoverView.swift` (now-playing UI + artwork)
- `SettingsView.swift` (settings panel)
- `AppModel.swift` (state management)
- `CompanionTheme.swift` (design tokens)
- `Models.swift` (data types)
- `Providers.swift` (now-playing providers)
- `ScryBarClient.swift` (HTTP transport)
- `ScryBarDiscovery.swift` (Bonjour)

Run: `ls Sources/ScryBarCompanion/*.swift | wc -l`

Expected: 10

- [ ] **Step 3: Verify success criteria checklist**

Manual checklist (for running the app via xcodebuild output):
1. App launches into menu bar with no Dock icon (LSUIElement)
2. Menu bar shows waveform icon + current track title
3. Left-click shows compact now-playing popover
4. Right-click shows context menu with Send Now and Quit
5. Gear icon navigates to settings panel
6. Back button returns to now-playing view
7. Cmd+Q quits the app
8. Auto-send is enabled by default
