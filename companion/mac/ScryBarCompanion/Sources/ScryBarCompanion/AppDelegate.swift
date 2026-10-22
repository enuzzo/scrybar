import AppKit
import SwiftUI

@MainActor
final class AppDelegate: NSObject, NSApplicationDelegate, NSWindowDelegate {
    let model = AppModel()
    private var cameraWindow: NSWindow?
    private var statusItem: NSStatusItem!
    private var networkStatusItem: NSStatusItem!
    private let popover = NSPopover()
    private let popoverLayout = PopoverLayoutModel()
    private var observation: NSKeyValueObservation?

    func applicationDidFinishLaunching(_ notification: Notification) {
        model.openCameraWindow = { [weak self] in self?.showCamera() }
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

    private func showCamera() {
        if cameraWindow == nil {
            let window = NSWindow(contentRect: NSRect(x: 0, y: 0, width: 640, height: 450),
                                  styleMask: [.titled, .closable, .miniaturizable, .resizable],
                                  backing: .buffered, defer: false)
            window.title = "Bambu Camera"
            window.isReleasedWhenClosed = false
            window.contentMinSize = NSSize(width: 480, height: 340)
            window.contentViewController = NSHostingController(rootView: BambuCameraView(camera: model.camera))
            window.delegate = self
            window.center()
            cameraWindow = window
        }
        popover.performClose(nil)
        cameraWindow?.makeKeyAndOrderFront(nil)
        NSApp.activate(ignoringOtherApps: true)
        model.camera.viewOnMac(true)
    }

    func windowWillClose(_ notification: Notification) {
        if let window = notification.object as? NSWindow, window === cameraWindow {
            model.camera.viewOnMac(false)
        }
    }

    func applicationShouldTerminate(_ sender: NSApplication) -> NSApplication.TerminateReply {
        Task { @MainActor in
            let safe = await model.camera.prepareToQuit()
            if !safe {
                let alert = NSAlert()
                alert.messageText = "Printer light-off could not be confirmed"
                alert.informativeText = "Keep Companion open to retry, or quit with light cleanup saved for the next launch."
                alert.addButton(withTitle: "Keep open")
                alert.addButton(withTitle: "Quit anyway")
                sender.reply(toApplicationShouldTerminate: alert.runModal() == .alertSecondButtonReturn)
            } else { sender.reply(toApplicationShouldTerminate: true) }
        }
        return .terminateLater
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

        networkStatusItem = NSStatusBar.system.statusItem(withLength: 94)
        if let networkButton = networkStatusItem.button {
            networkButton.cell?.wraps = true
            networkButton.cell?.usesSingleLineMode = false
            networkButton.target = self
            networkButton.action = #selector(statusItemClicked(_:))
            networkButton.sendAction(on: [.leftMouseUp, .rightMouseUp])
            networkButton.setAccessibilityLabel("ScryBar network activity")
        }
    }

    @objc private func statusItemClicked(_ sender: NSStatusBarButton) {
        guard let event = NSApp.currentEvent else { return }
        if event.type == .rightMouseUp {
            showContextMenu(for: sender)
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

    private func showContextMenu(for sender: NSStatusBarButton) {
        let menu = NSMenu()
        menu.addItem(withTitle: "Settings…", action: #selector(contextOpenSettings), keyEquivalent: ",")
        menu.addItem(.separator())
        menu.addItem(withTitle: "Quit ScryBar Companion", action: #selector(NSApplication.terminate(_:)), keyEquivalent: "q")
        let item = sender === networkStatusItem.button ? networkStatusItem! : statusItem!
        item.menu = menu
        item.button?.performClick(nil)
        // Remove menu after display so left-click works again
        DispatchQueue.main.async {
            item.menu = nil
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
        updateNetworkTitle()
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

    private func updateNetworkTitle() {
        guard let button = networkStatusItem.button else { return }
        networkStatusItem.isVisible = UserDefaults.standard.object(forKey: "network.showInMenuBar") as? Bool ?? true
        let stale = model.latestMacStats.map { Date.now.timeIntervalSince($0.updatedAt) > 15 } ?? true
        let down = NetworkRateFormatter.string(stale ? nil : model.latestNetworkStats?.downloadBytesPerSec)
        let up = NetworkRateFormatter.string(stale ? nil : model.latestNetworkStats?.uploadBytesPerSec)
        let paragraph = NSMutableParagraphStyle()
        paragraph.alignment = .left
        paragraph.minimumLineHeight = 10
        paragraph.maximumLineHeight = 10
        button.attributedTitle = NSAttributedString(
            string: "↓ \(down)\n↑ \(up)",
            attributes: [.font: NSFont.monospacedDigitSystemFont(ofSize: 9.5, weight: .medium), .paragraphStyle: paragraph]
        )
        let leader = model.latestMacStats?.networkApp
        button.toolTip = "Download \(down) · Upload \(up)" + (stale ? " · Waiting for fresh data" : leader.map { " · \($0)" } ?? "")
        button.setAccessibilityValue(button.toolTip)
    }
}
