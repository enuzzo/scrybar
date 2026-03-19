import Combine
import Foundation

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

    private let discovery = ScryBarDiscovery()
    private let client = ScryBarClient()
    private let systemProvider: any NowPlayingProviding = FallbackNowPlayingProvider([
        SystemNowPlayingProvider(),
        TidalNowPlayingProvider(),
    ])
    private let musicProvider = MusicNowPlayingProvider()
    private let mockProvider = MockNowPlayingProvider()
    private var pollTask: Task<Void, Never>?

    init() {
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
        startPolling()
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

    func rescan() {
        discovery.start()
    }

    func setAutoSend(_ enabled: Bool) {
        autoSendEnabled = enabled
        UserDefaults.standard.set(enabled, forKey: "autoSendEnabled")
    }

    func saveManualTarget() {
        UserDefaults.standard.set(manualHost, forKey: "manualHost")
        UserDefaults.standard.set(manualPort, forKey: "manualPort")
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

    func sendNow() {
        saveManualTarget()
        guard let endpoint = selectedEndpoint else {
            lastSendStatus = "Select a discovered ScryBar or enter a manual host/IP."
            return
        }

        let payload = currentPayload
        lastSendStatus = "Sending to \(endpoint.host):\(endpoint.port)…"

        Task {
            do {
                try await client.send(payload, to: endpoint)
                await MainActor.run {
                    self.lastSendStatus = "Sent to \(endpoint.host):\(endpoint.port) at \(DateFormatter.shortTime.string(from: .now))"
                }
            } catch {
                await MainActor.run {
                    self.lastSendStatus = "Send failed: \(error.localizedDescription)"
                }
            }
        }
    }

    private var lastSendSucceeded = true
    private var sendInFlight = false

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
        let payload = currentPayload
        sendInFlight = true

        Task {
            do {
                try await client.send(payload, to: endpoint)
                await MainActor.run {
                    self.lastSendSucceeded = true
                    self.sendInFlight = false
                }
            } catch {
                await MainActor.run {
                    self.lastSendSucceeded = false
                    self.sendInFlight = false
                }
            }
        }
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
