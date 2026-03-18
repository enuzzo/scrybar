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
    @Published var autoSendEnabled = false
    @Published var lastSendStatus = "Idle"

    private let discovery = ScryBarDiscovery()
    private let client = ScryBarClient()
    private let systemProvider = SystemNowPlayingProvider()
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
        refreshPayload()
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

    func saveManualTarget() {
        UserDefaults.standard.set(manualHost, forKey: "manualHost")
        UserDefaults.standard.set(manualPort, forKey: "manualPort")
    }

    func nextMockTrack() {
        mockProvider.next()
        refreshPayload()
    }

    func refreshPayload() {
        let payload: NowPlayingPayload?
        switch providerKind {
        case .system:
            payload = systemProvider.snapshot()
        case .mock:
            payload = mockProvider.snapshot()
        case .music:
            payload = musicProvider.snapshot()
        }

        if let payload {
            currentPayload = payload
            lastSendStatus = "Payload updated at \(DateFormatter.shortTime.string(from: payload.updatedAt))"
        } else {
            currentPayload = NowPlayingPayload.placeholder(source: providerKind.rawValue)
            switch providerKind {
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

    private func startPolling() {
        pollTask?.cancel()
        pollTask = Task { [weak self] in
            while !Task.isCancelled {
                guard let self else { return }
                self.refreshPayload()
                if self.autoSendEnabled {
                    self.sendNow()
                }
                try? await Task.sleep(for: .seconds(1))
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
