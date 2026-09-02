import AppKit
import Combine
import Foundation

enum BambuConnectionPhase: Equatable {
    case notConfigured
    case connecting
    case live
    case failed

    var title: String {
        switch self {
        case .notConfigured: return "Setup required"
        case .connecting: return "Connecting…"
        case .live: return "Connected"
        case .failed: return "Connection failed"
        }
    }

    init(status: String) {
        let normalized = status.lowercased()
        if normalized == "printer status live" {
            self = .live
        } else if normalized.contains("failed") || normalized.contains("rejected") ||
                    normalized.contains("closed") || normalized.contains("retrying") {
            self = .failed
        } else if normalized.contains("add printer") || normalized == "not configured" {
            self = .notConfigured
        } else {
            self = .connecting
        }
    }
}

@MainActor
final class AppModel: ObservableObject {
    @Published var providerKind: ProviderKind = .system
    @Published var discoveryStatus = "Starting discovery…"
    @Published var discoveredEndpoints: [ScryBarEndpoint] = []
    @Published var selectedDiscoveredEndpointID: UUID?
    @Published var manualHost = UserDefaults.standard.string(forKey: "manualHost") ?? ""
    @Published var manualPort = UserDefaults.standard.string(forKey: "manualPort") ?? "8080"
    @Published var currentPayload = NowPlayingPayload.placeholder(source: ProviderKind.system.rawValue)
    @Published var autoSendEnabled: Bool = {
        if UserDefaults.standard.object(forKey: "autoSendEnabled") == nil { return true }
        return UserDefaults.standard.bool(forKey: "autoSendEnabled")
    }()
    @Published var showSettingsOnOpen = false
    @Published var lastSendStatus = "Idle"
    @Published var latestMacStats: MacStatsPayload? = nil
    @Published var bambuHost = UserDefaults.standard.string(forKey: "bambuHost") ?? ""
    @Published var bambuSerial = UserDefaults.standard.string(forKey: "bambuSerial") ?? ""
    @Published var bambuPrinterName = UserDefaults.standard.string(forKey: "bambuPrinterName") ?? ""
    @Published var bambuAccessCode = ""
    @Published var bambuConnectionStatus = "Not configured"
    @Published var bambuConnectionPhase: BambuConnectionPhase = .notConfigured
    @Published var currentBambu = BambuPrinterPayload()
    @Published var discoveredBambuPrinters: [BambuDiscoveredPrinter] = []
    @Published var selectedBambuPrinterID: String?
    @Published var bambuDiscoveryStatus = "Printer search has not started."
    @Published var isBambuScanning = false
    @Published var bambuSoundsEnabled: Bool = {
        if UserDefaults.standard.object(forKey: "bambuSoundsEnabled") == nil { return true }
        return UserDefaults.standard.bool(forKey: "bambuSoundsEnabled")
    }()
    @Published private(set) var lastSuccessfulTarget: String?
    @Published private(set) var lastSuccessfulSendAt: Date?
    let macmonAvailable: Bool = {
        FileManager.default.isExecutableFile(atPath: "/opt/homebrew/bin/macmon") ||
        FileManager.default.isExecutableFile(atPath: "/usr/local/bin/macmon")
    }()

    private let discovery = ScryBarDiscovery()
    private let client = ScryBarClient()
    private let systemProvider: any NowPlayingProviding = FallbackNowPlayingProvider([
        SystemNowPlayingProvider(),
        TidalNowPlayingProvider(),
    ])
    private let musicProvider = MusicNowPlayingProvider()
    private let mockProvider = MockNowPlayingProvider()
    private let macStatsProvider = MacStatsProvider()
    private let bambuMonitor = BambuPrinterMonitor()
    private let bambuDiscovery = BambuPrinterDiscovery()
    private var pollTask: Task<Void, Never>?
    private var macStatsPollTask: Task<Void, Never>?
    private var bambuBaselineReceived = false

    init() {
        bambuAccessCode = BambuKeychain.loadAccessCode(serial: bambuSerial, allowLegacyFallback: true)
        discovery.onStatus = { [weak self] status in
            Task { @MainActor in self?.discoveryStatus = status }
        }
        discovery.onEndpoints = { [weak self] endpoints in
            Task { @MainActor in
                self?.discoveredEndpoints = endpoints
                if let current = self?.selectedDiscoveredEndpointID,
                   endpoints.contains(where: { $0.id == current }) {
                    return
                }
                self?.selectedDiscoveredEndpointID = endpoints.first?.id
            }
        }
        discovery.start()
        bambuDiscovery.onStatus = { [weak self] status in
            Task { @MainActor in self?.bambuDiscoveryStatus = status }
        }
        bambuDiscovery.onScanningChanged = { [weak self] scanning in
            Task { @MainActor in self?.isBambuScanning = scanning }
        }
        bambuDiscovery.onPrinters = { [weak self] printers in
            Task { @MainActor in self?.receiveDiscoveredBambuPrinters(printers) }
        }
        scanForBambuPrinters()
        bambuMonitor.onStatus = { [weak self] status in
            Task { @MainActor in self?.receiveBambuConnectionStatus(status) }
        }
        bambuMonitor.onPayload = { [weak self] payload in
            Task { @MainActor in self?.receiveBambu(payload) }
        }
        startBambuMonitor()
        startPolling()
        startMacStatsPolling()
    }

    var selectedEndpoint: ScryBarEndpoint? {
        if let selectedDiscoveredEndpointID,
           let endpoint = discoveredEndpoints.first(where: { $0.id == selectedDiscoveredEndpointID }) {
            return endpoint
        }

        let trimmedHost = manualHost.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !trimmedHost.isEmpty else { return nil }
        let port = Int(manualPort) ?? 8080
        return ScryBarEndpoint(name: "Manual Target", host: trimmedHost, port: port, source: .manual)
    }

    var selectedEndpointIsConnected: Bool {
        guard let endpoint = selectedEndpoint,
              let lastSuccessfulSendAt,
              Date().timeIntervalSince(lastSuccessfulSendAt) < 8 else { return false }
        return lastSuccessfulTarget == endpointKey(endpoint)
    }

    func rescan() {
        discovery.start()
    }

    func scanForBambuPrinters() {
        bambuDiscovery.start()
    }

    func selectBambuPrinter(id: String?) {
        selectedBambuPrinterID = id
        guard let id, let printer = discoveredBambuPrinters.first(where: { $0.id == id }) else { return }
        let previousHost = bambuHost
        bambuHost = printer.host
        if !printer.serial.isEmpty {
            bambuSerial = printer.serial.uppercased()
        }
        if !printer.name.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty {
            bambuPrinterName = printer.name.trimmingCharacters(in: .whitespacesAndNewlines)
        } else if bambuPrinterName.isEmpty, printer.model != "Bambu Lab printer" {
            bambuPrinterName = printer.model
        }
        // A discovery response can first arrive without a serial and then be
        // enriched by a second SSDP announcement. Keep a code the user has
        // already entered unless Keychain has a printer-specific value.
        let storedAccessCode = BambuKeychain.loadAccessCode(serial: bambuSerial)
        if !storedAccessCode.isEmpty {
            bambuAccessCode = storedAccessCode
        }
        UserDefaults.standard.set(bambuHost, forKey: "bambuHost")
        UserDefaults.standard.set(bambuSerial, forKey: "bambuSerial")
        UserDefaults.standard.set(bambuPrinterName, forKey: "bambuPrinterName")
        if bambuAccessCode.isEmpty || bambuSerial.isEmpty {
            bambuConnectionStatus = "Printer selected. Add its serial and LAN access code."
            bambuConnectionPhase = .notConfigured
        } else {
            let configuration = BambuPrinterConfiguration(
                host: bambuHost,
                serial: bambuSerial,
                accessCode: bambuAccessCode
            )
            bambuBaselineReceived = false
            currentBambu.connected = false
            bambuConnectionPhase = .connecting
            bambuConnectionStatus = previousHost == bambuHost
                ? "Printer selected. Connecting securely…"
                : "Printer address updated. Reconnecting securely…"
            bambuMonitor.start(configuration: configuration)
        }
    }

    /// Clean shutdown for app termination: stop polling and terminate the
    /// long-lived macmon subprocess so it can't be orphaned.
    func shutdown() {
        pollTask?.cancel()
        macStatsPollTask?.cancel()
        macStatsProvider.stop()
        bambuDiscovery.stop()
        bambuMonitor.stop()
    }

    func setAutoSend(_ enabled: Bool) {
        autoSendEnabled = enabled
        UserDefaults.standard.set(enabled, forKey: "autoSendEnabled")
    }

    func saveManualTarget() {
        UserDefaults.standard.set(manualHost, forKey: "manualHost")
        UserDefaults.standard.set(manualPort, forKey: "manualPort")
    }

    func saveBambuSettings() {
        let configuration = BambuPrinterConfiguration(
            host: bambuHost.trimmingCharacters(in: .whitespacesAndNewlines),
            serial: bambuSerial.trimmingCharacters(in: .whitespacesAndNewlines).uppercased(),
            accessCode: bambuAccessCode
        )
        guard configuration.isReady else {
            bambuConnectionPhase = .notConfigured
            bambuConnectionStatus = "Add printer IP, serial and LAN access code."
            return
        }

        bambuHost = configuration.host
        bambuSerial = configuration.serial
        bambuBaselineReceived = false
        currentBambu.connected = false
        bambuConnectionPhase = .connecting
        bambuConnectionStatus = "Saving securely…"

        // Return to the run loop before Keychain access so the button and
        // status row acknowledge the click immediately.
        Task { @MainActor [weak self] in
            await Task.yield()
            guard let self else { return }
            UserDefaults.standard.set(configuration.host, forKey: "bambuHost")
            UserDefaults.standard.set(configuration.serial, forKey: "bambuSerial")
            UserDefaults.standard.set(self.bambuPrinterName, forKey: "bambuPrinterName")
            BambuKeychain.saveAccessCode(configuration.accessCode, serial: configuration.serial)
            self.bambuConnectionStatus = "Settings saved. Connecting securely…"
            self.bambuMonitor.start(configuration: configuration)
        }
    }

    private func receiveBambuConnectionStatus(_ status: String) {
        bambuConnectionStatus = status
        bambuConnectionPhase = BambuConnectionPhase(status: status)
    }

    private func receiveDiscoveredBambuPrinters(_ printers: [BambuDiscoveredPrinter]) {
        discoveredBambuPrinters = printers
        if let selectedBambuPrinterID,
           let current = printers.first(where: { $0.id == selectedBambuPrinterID }) {
            if current.host != bambuHost ||
                (!current.serial.isEmpty && current.serial.caseInsensitiveCompare(bambuSerial) != .orderedSame) ||
                (!current.name.isEmpty && current.name != bambuPrinterName) {
                selectBambuPrinter(id: current.id)
            }
            return
        }

        if let match = printers.first(where: {
            (!$0.serial.isEmpty && $0.serial.caseInsensitiveCompare(bambuSerial) == .orderedSame) ||
            $0.host == bambuHost
        }) {
            // The stable id may change from the provisional host to the
            // printer serial as richer discovery data arrives. Re-select the
            // enriched record so host and serial are persisted together.
            selectBambuPrinter(id: match.id)
            return
        }

        if printers.count == 1 {
            selectBambuPrinter(id: printers[0].id)
        } else {
            selectedBambuPrinterID = nil
        }
    }

    func setBambuSoundsEnabled(_ enabled: Bool) {
        bambuSoundsEnabled = enabled
        UserDefaults.standard.set(enabled, forKey: "bambuSoundsEnabled")
    }

    private func startBambuMonitor() {
        let configuration = BambuPrinterConfiguration(
            host: bambuHost,
            serial: bambuSerial,
            accessCode: bambuAccessCode
        )
        bambuMonitor.start(configuration: configuration)
    }

    private func receiveBambu(_ payload: BambuPrinterPayload) {
        let previous = currentBambu
        currentBambu = payload

        if payload.connected {
            if bambuBaselineReceived {
                playBambuSoundIfNeeded(from: previous, to: payload)
            } else {
                bambuBaselineReceived = true
            }
        }

        guard autoSendEnabled, let endpoint = selectedEndpoint else { return }
        Task {
            try? await client.sendBambu(payload, to: endpoint)
        }
    }

    private func playBambuSoundIfNeeded(from previous: BambuPrinterPayload, to current: BambuPrinterPayload) {
        guard bambuSoundsEnabled else { return }
        let soundName: String?
        if !previous.isPrinting && current.isPrinting {
            soundName = "Glass"
        } else if !previous.isPausedOrBlocked && current.isPausedOrBlocked {
            soundName = "Basso"
        } else if !previous.isFinished && current.isFinished {
            soundName = "Hero"
        } else {
            soundName = nil
        }
        if let soundName {
            NSSound(named: NSSound.Name(soundName))?.play()
        }
    }

    func nextMockTrack() {
        mockProvider.next()
        Task { await refreshPayload() }
    }

    func refreshPayload() async {
        // Snapshot providers off the main thread to avoid blocking UI
        // (MediaRemoteBridge uses semaphores that block the calling thread)
        let kind = providerKind
        let provider: any NowPlayingProviding
        switch kind {
        case .system: provider = systemProvider
        case .mock: provider = mockProvider
        case .music: provider = musicProvider
        }

        let payload: NowPlayingPayload? = await Task.detached(priority: .userInitiated) {
            provider.snapshot()
        }.value

        if let payload {
            currentPayload = payload
            lastSendStatus = "Payload updated at \(DateFormatter.shortTime.string(from: payload.updatedAt))"
        } else {
            currentPayload = NowPlayingPayload.placeholder(source: kind.rawValue)
            switch kind {
            case .system:
                lastSendStatus = "No system now-playing session is currently exposed by macOS."
            case .music:
                lastSendStatus = "Music.app not running, not playing, or automation permission not granted yet."
            case .mock:
                lastSendStatus = "Mock provider has no payload."
            }
        }
    }

    private var lastSendSucceeded = false
    private var sendInFlight = false

    // MARK: Mac Stats polling

    private var macStatsSendInFlight = false

    private func startMacStatsPolling() {
        macStatsPollTask?.cancel()
        macStatsPollTask = Task { [weak self] in
            while !Task.isCancelled {
                guard let self else { return }
                // Sample on a detached thread (IOKit + syscalls may block)
                let payload: MacStatsPayload = await Task.detached(priority: .background) {
                    self.macStatsProvider.snapshot()
                }.value

                // Store locally for popover display
                await MainActor.run { self.latestMacStats = payload }

                if self.autoSendEnabled, !self.macStatsSendInFlight,
                   let endpoint = self.selectedEndpoint {
                    self.macStatsSendInFlight = true
                    Task {
                        do {
                            try await self.client.sendMacStats(payload, to: endpoint)
                        } catch {
                            // Silently suppress — mac stats is best-effort
                        }
                        await MainActor.run { self.macStatsSendInFlight = false }
                    }
                }
                try? await Task.sleep(for: .seconds(5))
            }
        }
    }

    private func startPolling() {
        pollTask?.cancel()
        pollTask = Task { [weak self] in
            while !Task.isCancelled {
                guard let self else { return }
                await self.refreshPayload()
                if self.autoSendEnabled, !self.sendInFlight {
                    self.autoSend()
                }
                try? await Task.sleep(for: .seconds(lastSendSucceeded ? 1 : 5))
            }
        }
    }

    private func autoSend() {
        guard let endpoint = selectedEndpoint else { return }
        let target = endpointKey(endpoint)
        let payload = currentPayload
        sendInFlight = true

        Task {
            do {
                try await client.send(payload, to: endpoint)
                await MainActor.run {
                    self.lastSendSucceeded = true
                    self.lastSuccessfulTarget = target
                    self.lastSuccessfulSendAt = .now
                    self.sendInFlight = false
                }
            } catch {
                await MainActor.run {
                    self.lastSendSucceeded = false
                    if self.lastSuccessfulTarget == target {
                        self.lastSuccessfulTarget = nil
                        self.lastSuccessfulSendAt = nil
                    }
                    self.sendInFlight = false
                }
            }
        }
    }

    private func endpointKey(_ endpoint: ScryBarEndpoint) -> String {
        "\(endpoint.host):\(endpoint.port)"
    }
}

private extension DateFormatter {
    static let shortTime: DateFormatter = {
        let formatter = DateFormatter()
        formatter.timeStyle = .medium
        formatter.dateStyle = .none
        return formatter
    }()
}
