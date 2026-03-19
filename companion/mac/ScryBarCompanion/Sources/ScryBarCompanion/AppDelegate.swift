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