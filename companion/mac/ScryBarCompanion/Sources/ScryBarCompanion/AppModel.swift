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

enum BambuRecoveryScanPolicy {
    static func shouldStart(
        status: String,
        isScanning: Bool,
        lastStartedAt: Date?,
        now: Date,
        minimumInterval: TimeInterval = 15
    ) -> Bool {
        guard !isScanning else { return false }
        if let lastStartedAt, now.timeIntervalSince(lastStartedAt) < minimumInterval {
            return false
        }

        let normalized = status.lowercased()
        guard !normalized.contains("rejected") && !normalized.contains("authentication failed") else {
            return false
        }
        return normalized.contains("connection failed") ||
            normalized.contains("connection closed") ||
            normalized.contains("mqtt receive failed") ||
            normalized.contains("printer closed the connection")
    }
}

@MainActor
final class AppModel: ObservableObject {
    @Published var providerKind: ProviderKind = .system
    @Published var discoveryStatus = "Starting discovery…"
    @Published var discoveredEndpoints: [ScryBarEndpoint] = []
    @Published private(set) var displayTargets: [DisplayTarget] = []
    @Published private(set) var displayStates: [String: DisplayDeliveryState] = [:]
    private var displayStore = DisplayTargetStore()
    private let delivery = DisplayDelivery()
    let yakamoz = YakamozPush()   // optional studio TV destination (off until configured)
    @Published var yakamozStatus = "Off"
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
    @Published var latestNetworkStats: NetworkStatsSnapshot? = nil
    @Published var networkScope = NetworkScope.stored() {
        didSet {
            guard oldValue != networkScope else { return }
            UserDefaults.standard.set(networkScope.rawValue, forKey: "network.scope")
            latestNetworkStats = nil
            // Immediately clear the previous scope locally and on displays.
            if let stats = latestMacStats {
                let cleared = stats.withNetwork(.init(downloadBytesPerSec: nil, uploadBytesPerSec: nil,
                    apps: [], attributionAvailable: false, attributionMessage: "Changing network scope…",
                    scope: networkScope), hostName: macStatsHostName)
                latestMacStats = cleared
                if autoSendEnabled { delivery.send(stats: cleared) }
            }
        }
    }
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
    let macmonAvailable: Bool = {
        FileManager.default.isExecutableFile(atPath: "/opt/homebrew/bin/macmon") ||
        FileManager.default.isExecutableFile(atPath: "/usr/local/bin/macmon")
    }()

    var openCameraWindow: (() -> Void)?
    var closePopover: (() -> Void)?
    let camera = BambuCameraModel()
    private var activeBambuConfiguration = BambuPrinterConfiguration(host: "", serial: "", accessCode: "")

    private let discovery = ScryBarDiscovery()
    private let systemProvider: any NowPlayingProviding = FallbackNowPlayingProvider([
        SystemNowPlayingProvider(),
        TidalNowPlayingProvider(),
    ])
    private let musicProvider = MusicNowPlayingProvider()
    private let mockProvider = MockNowPlayingProvider()
    private let macStatsProvider = MacStatsProvider()
    private let networkStatsProvider = NetworkStatsProvider()
    private let macStatsHostName = Host.current().localizedName
    private let bambuMonitor = BambuPrinterMonitor()
    private let bambuDiscovery = BambuPrinterDiscovery()
    private var pollTask: Task<Void, Never>?
    private var macStatsPollTask: Task<Void, Never>?
    private var bambuBaselineReceived = false
    private var lastBambuDiscoveryStartedAt: Date?
    private var requestedKeychainSerial: String?
    private var loadingBambuAccessCode = false

    init() {
        discovery.onStatus = { [weak self] status in
            Task { @MainActor in self?.discoveryStatus = status }
        }
        displayTargets = displayStore.targets
        delivery.onChange = { [weak self] states in self?.displayStates = states }
        yakamoz.onStatus = { [weak self] status in self?.yakamozStatus = status }
        syncDisplayTargets()
        discovery.onEndpoints = { [weak self] endpoints in
            Task { @MainActor in
                guard let self else { return }
                self.discoveredEndpoints = endpoints
                self.displayStore.merge(endpoints)
                self.syncDisplayTargets()
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
        camera.configuration = { [weak self] in
            self?.activeBambuConfiguration ?? BambuPrinterConfiguration(host: "", serial: "", accessCode: "")
        }
        camera.endpoints = { [weak self] in
            guard let self else { return [] }
            return self.enabledEndpoints.filter { self.displayStates[$0.stableKey]?.capabilities?.camera == true }.map {
                var target = $0
                target.product = self.displayStates[$0.stableKey]?.capabilities?.product ?? target.product
                return target
            }
        }
        camera.bodyLimit = { [weak self] endpoint in self?.displayStates[endpoint.stableKey]?.capabilities?.maxBodyBytes ?? 98304 }
        camera.setLightWithDeadline = { [weak self] enabled, serial, deadline in
            self?.bambuMonitor.setCameraLight(enabled, serial: serial, expiresAt: deadline)
        }
        bambuMonitor.onLight = { [weak self] serial, on in
            Task { @MainActor in self?.camera.receiveLight(serial: serial, on: on) }
        }
        camera.startPolling()
        startBambuMonitor()
        startPolling()
        startMacStatsPolling()
        loadStoredBambuAccessCode(allowLegacyFallback: true)
    }

    var enabledEndpoints: [ScryBarEndpoint] {
        let endpoints = displayTargets.filter(\.enabled).map(\.endpoint)
        var addresses = Set<String>()
        return endpoints.filter { addresses.insert($0.addressKey).inserted }
    }
    var selectedEndpoint: ScryBarEndpoint? { enabledEndpoints.first }
    var selectedEndpointIsConnected: Bool { enabledEndpoints.contains { displayStates[$0.stableKey]?.connected() == true } }
    var connectedDisplayCount: Int { enabledEndpoints.filter { displayStates[$0.stableKey]?.connected() == true }.count }
    func setDisplayEnabled(_ enabled: Bool, id: String) {
        displayStore.setEnabled(enabled, id: id)
        syncDisplayTargets()
    }
    private func syncDisplayTargets() {
        displayTargets = displayStore.targets
        delivery.select(autoSendEnabled ? enabledEndpoints : [])
        camera.targetsDidChange()
    }

    func rescan() {
        discovery.start()
    }

    func scanForBambuPrinters() {
        lastBambuDiscoveryStartedAt = .now
        isBambuScanning = true
        bambuDiscovery.start()
    }

    func selectBambuPrinter(id: String?) {
        guard let id, let printer = discoveredBambuPrinters.first(where: { $0.id == id }) else {
            selectedBambuPrinterID = id
            return
        }
        guard canSwitchBambuPrinter(to: printer.serial.isEmpty ? bambuSerial : printer.serial.uppercased()) else { return }
        selectedBambuPrinterID = id
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
        // A Keychain consent prompt must not block discovery or the main UI.
        if activeBambuConfiguration.serial != bambuSerial && !activeBambuConfiguration.serial.isEmpty {
            bambuAccessCode = ""
        }
        loadStoredBambuAccessCode()
        UserDefaults.standard.set(bambuHost, forKey: "bambuHost")
        UserDefaults.standard.set(bambuSerial, forKey: "bambuSerial")
        UserDefaults.standard.set(bambuPrinterName, forKey: "bambuPrinterName")
        if loadingBambuAccessCode { return }
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
            connectBambu(configuration: configuration)
        }
    }

    /// Clean shutdown for app termination: stop polling and terminate the
    /// long-lived macmon subprocess so it can't be orphaned.
    func shutdown() {
        NativeMediaRemote.shared.stopAndWait()
        camera.shutdown()
        delivery.stop()
        discovery.stop()
        pollTask?.cancel()
        macStatsPollTask?.cancel()
        macStatsProvider.stop()
        networkStatsProvider.stop()
        bambuDiscovery.stop()
        bambuMonitor.stop()
    }

    func setAutoSend(_ enabled: Bool) {
        autoSendEnabled = enabled
        UserDefaults.standard.set(enabled, forKey: "autoSendEnabled")
        syncDisplayTargets()
    }

    func saveManualTarget() {
        UserDefaults.standard.set(manualHost, forKey: "manualHost")
        UserDefaults.standard.set(manualPort, forKey: "manualPort")
        displayStore.addManual(host: manualHost.trimmingCharacters(in: .whitespacesAndNewlines), port: Int(manualPort) ?? 8080)
        syncDisplayTargets()
    }

    func saveBambuSettings() {
        loadingBambuAccessCode = false
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

        guard canSwitchBambuPrinter(to: configuration.serial) else { return }
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
            self.connectBambu(configuration: configuration)
        }
    }

    private func loadStoredBambuAccessCode(allowLegacyFallback: Bool = false) {
        let serial = bambuSerial
        guard !serial.isEmpty, bambuAccessCode.isEmpty, requestedKeychainSerial != serial else { return }
        requestedKeychainSerial = serial
        loadingBambuAccessCode = true
        bambuConnectionStatus = "Reading saved printer access. Unlock your Mac if prompted."
        bambuConnectionPhase = .connecting
        Task { [weak self] in
            let code = await Task.detached(priority: .userInitiated) {
                BambuKeychain.loadAccessCode(serial: serial, allowLegacyFallback: allowLegacyFallback)
            }.value
            guard let self, self.bambuSerial == serial else { return }
            self.loadingBambuAccessCode = false
            guard self.bambuAccessCode.isEmpty else { return }
            guard !code.isEmpty else {
                self.bambuConnectionStatus = "Saved access unavailable. Enter the printer LAN access code to reconnect."
                self.bambuConnectionPhase = .notConfigured
                return
            }
            self.bambuAccessCode = code
            self.startBambuMonitor()
        }
    }

    private func receiveBambuConnectionStatus(_ status: String) {
        guard !loadingBambuAccessCode else { return }
        bambuConnectionStatus = status
        bambuConnectionPhase = BambuConnectionPhase(status: status)
        if BambuRecoveryScanPolicy.shouldStart(
            status: status,
            isScanning: isBambuScanning,
            lastStartedAt: lastBambuDiscoveryStartedAt,
            now: .now
        ) {
            bambuDiscoveryStatus = "Connection lost. Looking for the saved printer…"
            scanForBambuPrinters()
        }
    }

    private func receiveDiscoveredBambuPrinters(_ printers: [BambuDiscoveredPrinter]) {
        discoveredBambuPrinters = printers
        guard let preferred = BambuPrinterSelectionPolicy.preferredPrinter(
            in: printers,
            savedSerial: bambuSerial,
            savedName: bambuPrinterName,
            savedHost: bambuHost,
            selectedID: selectedBambuPrinterID
        ) else {
            selectedBambuPrinterID = nil
            return
        }

        let selectionChanged = selectedBambuPrinterID != preferred.id
        let addressChanged = preferred.host.caseInsensitiveCompare(bambuHost) != .orderedSame
        let identityChanged = !preferred.serial.isEmpty &&
            preferred.serial.caseInsensitiveCompare(bambuSerial) != .orderedSame
        let nameChanged = !preferred.name.isEmpty && preferred.name != bambuPrinterName

        if selectionChanged || addressChanged || identityChanged || nameChanged {
            // A serial-bearing SSDP result deliberately replaces a provisional
            // or stale IP selection and restarts MQTT on the current address.
            selectBambuPrinter(id: preferred.id)
        }
    }

    func setBambuSoundsEnabled(_ enabled: Bool) {
        bambuSoundsEnabled = enabled
        UserDefaults.standard.set(enabled, forKey: "bambuSoundsEnabled")
    }

    private func canSwitchBambuPrinter(to serial: String) -> Bool {
        if !activeBambuConfiguration.serial.isEmpty,
           activeBambuConfiguration.serial != serial,
           camera.running || camera.lightPending {
            bambuConnectionStatus = "Close camera and wait for light-off before switching printers"
            return false
        }
        return true
    }

    private func connectBambu(configuration: BambuPrinterConfiguration) {
        guard canSwitchBambuPrinter(to: configuration.serial) else { return }
        activeBambuConfiguration = configuration
        bambuMonitor.start(configuration: configuration)
    }

    private func startBambuMonitor() {
        let configuration = BambuPrinterConfiguration(
            host: bambuHost,
            serial: bambuSerial,
            accessCode: bambuAccessCode
        )
        connectBambu(configuration: configuration)
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

        guard autoSendEnabled else { return }
        delivery.send(bambu: payload)
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

        // Second check for every provider (Music.app, TIDAL, mock): nothing outside the policy reaches a display.
        let allowed = payload.map { SourcePolicy.current.allows(bundleID: SourcePolicy.bundleID(of: $0)) } ?? false
        if let payload, allowed {
            currentPayload = payload
            lastSendStatus = "Payload updated at \(DateFormatter.shortTime.string(from: payload.updatedAt))"
        } else if payload != nil || SourcePolicyLog.shared.lastHidden != nil {
            currentPayload = NowPlayingPayload.placeholder(source: kind.rawValue)
            lastSendStatus = "Hidden for privacy: the playing app is not an allowed music source."
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

    private func startMacStatsPolling() {
        macStatsPollTask?.cancel()
        macStatsPollTask = Task { [weak self] in
            while !Task.isCancelled {
                guard let self else { return }
                // Sample on a detached thread (IOKit + syscalls may block)
                let scope = self.networkScope
                let sample = await Task.detached(priority: .background) {
                    let network = self.networkStatsProvider.snapshot(scope: scope)
                    let stats = self.macStatsProvider.snapshot().withNetwork(
                        network, hostName: self.macStatsHostName
                    )
                    return (stats, network)
                }.value
                guard !Task.isCancelled else { return }
                guard self.networkScope == scope else { continue }
                self.latestMacStats = sample.0
                self.latestNetworkStats = sample.1

                if self.autoSendEnabled { self.delivery.send(stats: sample.0) }
                try? await Task.sleep(for: .seconds(1))
            }
        }
    }

    private func startPolling() {
        pollTask?.cancel()
        pollTask = Task { [weak self] in
            while !Task.isCancelled {
                guard let self else { return }
                await self.refreshPayload()
                if self.autoSendEnabled {
                    self.autoSend()
                }
                try? await Task.sleep(for: .seconds(1))
            }
        }
    }

    private func autoSend() {
        delivery.send(nowPlaying: currentPayload, bambu: currentBambu)
        yakamoz.send(currentPayload)
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
