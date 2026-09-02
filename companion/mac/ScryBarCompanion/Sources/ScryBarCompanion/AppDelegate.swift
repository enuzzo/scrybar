import AppKit
import SwiftUI

@MainActor
final class AppDelegate: NSObject, NSApplicationDelegate {
    let model = AppModel()
    private var statusItem: NSStatusItem!
    private let popover = NSPopover()
    private let popoverLayout = PopoverLayoutModel()
    private var observation: NSKeyValueObservation?

    func applicationDidFinishLaunching(_ notification: Notification) {
        let opensBambuSetup = ProcessInfo.processInfo.arguments.contains("--bambu-setup")
        let opensPopover = opensBambuSetup || ProcessInfo.processInfo.arguments.contains("--open")
        model.showSettingsOnOpen = opensBambuSetup
        setupMainMenu()
        setupStatusItem()
        setupPopover()
        startTitleUpdates()

        if opensPopover {
            popover.behavior = .applicationDefined
            NSApp.activate(ignoringOtherApps: true)
            DispatchQueue.main.asyncAfter(deadline: .now() + 0.5) { [weak self] in
                guard let self, let button = self.statusItem.button else { return }
                self.togglePopover(button)
            }
        }
    }

    func applicationWillTerminate(_ notification: Notification) {
        model.shutdown()
    }

    // MARK: - Main Menu (enables Cmd+Q for LSUIElement apps)

    private func setupMainMenu() {
        let mainMenu = NSMenu()

        let appMenuItem = NSMenuItem()
        let appMenu = NSMenu()
        appMenu.addItem(withTitle: "Quit ScryBar Companion", action: #selector(NSApplication.terminate(_:)), keyEquivalent: "q")
        appMenuItem.submenu = appMenu
        mainMenu.addItem(appMenuItem)

        // A menu-bar-only app still needs the standard Edit menu so keyboard
        // commands are routed to the first responder. Without it, contextual
        // Paste works in text fields but Command-V does not.
        let editMenuItem = NSMenuItem()
        let editMenu = NSMenu(title: "Edit")
        editMenu.addItem(withTitle: "Undo", action: Selector(("undo:")), keyEquivalent: "z")
        editMenu.addItem(withTitle: "Redo", action: Selector(("redo:")), keyEquivalent: "Z")
        editMenu.addItem(.separator())
        editMenu.addItem(withTitle: "Cut", action: #selector(NSText.cut(_:)), keyEquivalent: "x")
        editMenu.addItem(withTitle: "Copy", action: #selector(NSText.copy(_:)), keyEquivalent: "c")
        editMenu.addItem(withTitle: "Paste", action: #selector(NSText.paste(_:)), keyEquivalent: "v")
        editMenu.addItem(withTitle: "Select All", action: #selector(NSText.selectAll(_:)), keyEquivalent: "a")
        editMenuItem.submenu = editMenu
        mainMenu.addItem(editMenuItem)

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
            popoverLayout.maximumHeight = maximumPopoverHeight(relativeTo: sender)
            popover.show(relativeTo: sender.bounds, of: sender, preferredEdge: .minY)
            popover.contentViewController?.view.window?.makeKey()
        }
    }

    // MARK: - Context Menu

    private func showContextMenu() {
        let menu = NSMenu()
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

    @objc private func contextOpenSettings() {
        model.showSettingsOnOpen = true
        if let button = statusItem.button {
            togglePopover(button)
        }
    }

    // MARK: - Popover

    private func setupPopover() {
        popover.contentSize = NSSize(width: 360, height: 400)
        popoverLayout.maximumHeight = max(
            PopoverHeightPolicy.minimumHeight,
            (NSScreen.main?.visibleFrame.height ?? 656) - 16
        )
        popover.behavior = .transient
        popover.animates = false
        let hostingController = NSHostingController(
            rootView: PopoverContentView(layout: popoverLayout) { [weak self] size in
                guard let self else { return }
                let target = NSSize(width: size.width, height: size.height)
                if self.popover.contentSize != target {
                    self.popover.contentSize = target
                }
            }
                .environmentObject(model)
        )
        // Force layout now so first open is instant
        hostingController.view.layoutSubtreeIfNeeded()
        popover.contentViewController = hostingController
    }

    private func maximumPopoverHeight(relativeTo anchor: NSView) -> CGFloat {
        guard let screen = anchor.window?.screen ?? NSScreen.main else { return 640 }
        // Menu-bar item windows do not expose a stable frame origin across
        // displays and macOS versions. On some screens `frame.minY` resolves
        // near zero, which collapsed the popover to the 320 pt minimum even
        // when hundreds of points were still available below the menu bar.
        // `visibleFrame` already excludes the menu bar and Dock, so it is the
        // correct source of truth for a downward-opening status-item popover.
        return PopoverHeightPolicy.maximumHeight(
            visibleFrameHeight: screen.visibleFrame.height
        )
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
