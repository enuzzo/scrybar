import Foundation

struct DisplayDeliveryState {
    var lastSuccess: Date?
    var errors: [String: String] = [:]
    var error: String? { errors.keys.sorted().map { "\($0): \(errors[$0]!)" }.first }
    var capabilities: CompanionCapabilities?
    var feeds: [String: Date] = [:]
    func connected(at now: Date = .now) -> Bool {
        guard let lastSuccess else { return false }
        return now.timeIntervalSince(lastSuccess) < 8
    }
}

/// Independent lanes per destination/feed. Busy lanes drop intermediate snapshots;
/// there is no durable queue that can replay stale data after reconnection.
@MainActor
final class DisplayDelivery {
    private let client: ScryBarClient
    private var tasks: [String: Task<Void, Never>] = [:]
    private var tokens: [String: UUID] = [:]
    private var selected: [String: ScryBarEndpoint] = [:]
    private(set) var states: [String: DisplayDeliveryState] = [:]
    var onChange: (([String: DisplayDeliveryState]) -> Void)?
    init(client: ScryBarClient = ScryBarClient()) { self.client = client }

    func select(_ endpoints: [ScryBarEndpoint]) {
        let next = Dictionary(endpoints.map { ($0.stableKey, $0) }, uniquingKeysWith: { a, _ in a })
        for (key, endpoint) in selected where next[key]?.addressKey != endpoint.addressKey {
            for lane in Array(tasks.keys) where lane.hasPrefix(key + "|") {
                tasks.removeValue(forKey: lane)?.cancel(); tokens[lane] = nil
            }
            states[key] = nil
            Task { await client.invalidate(endpoint) }
        }
        selected = next
        onChange?(states)
    }
    func stop() { select([]) }
    func send(nowPlaying: NowPlayingPayload? = nil, bambu: BambuPrinterPayload? = nil, stats: MacStatsPayload? = nil, now: Date = .now) {
        for endpoint in selected.values {
            if let payload = nowPlaying, now.timeIntervalSince(payload.updatedAt) < 15 {
                submit("now-playing", endpoint: endpoint) { try await self.client.send(payload, to: endpoint) }
            }
            if let payload = bambu, now.timeIntervalSince(payload.updatedAt) < 15 {
                submit("bambu", endpoint: endpoint) { try await self.client.sendBambu(payload, to: endpoint) }
            }
            if let payload = stats, now.timeIntervalSince(payload.updatedAt) < 15 {
                submit("mac-stats", endpoint: endpoint) { try await self.client.sendMacStats(payload, to: endpoint) }
            }
        }
    }
    private func submit(_ feed: String, endpoint: ScryBarEndpoint, action: @escaping @MainActor () async throws -> Void) {
        let key = endpoint.stableKey, lane = key + "|" + feed
        guard tasks[lane] == nil else { return }
        let token = UUID(); tokens[lane] = token
        tasks[lane] = Task { [weak self] in
            guard let self else { return }
            defer { if self.tokens[lane] == token { self.tasks[lane] = nil; self.tokens[lane] = nil } }
            do {
                let caps = try await self.client.negotiate(endpoint)
                guard self.tokens[lane] == token, !Task.isCancelled else { return }
                self.states[key, default: .init()].capabilities = caps
                if caps.feeds.contains(feed) {
                    try await action()
                    guard self.tokens[lane] == token, !Task.isCancelled else { return }
                    self.states[key, default: .init()].feeds[feed] = .now
                    self.states[key, default: .init()].lastSuccess = .now
                    self.states[key, default: .init()].errors[feed] = nil
                }
            } catch {
                guard self.tokens[lane] == token, !Task.isCancelled else { return }
                self.states[key, default: .init()].errors[feed] = error.localizedDescription
                await self.client.invalidate(endpoint)
            }
            self.onChange?(self.states)
        }
    }
}
