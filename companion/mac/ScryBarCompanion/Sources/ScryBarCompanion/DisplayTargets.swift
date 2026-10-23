import Foundation

struct DisplayTarget: Identifiable, Codable, Equatable {
    var endpoint: ScryBarEndpoint
    var enabled: Bool
    var id: String { endpoint.stableKey }
}

/// Persists identity and the last address, so discovery loss does not replace a target.
struct DisplayTargetStore {
    private let defaults: UserDefaults
    private(set) var targets: [DisplayTarget]
    init(defaults: UserDefaults = .standard) {
        self.defaults = defaults
        targets = defaults.data(forKey: "displayTargets.v1").flatMap {
            try? JSONDecoder().decode([DisplayTarget].self, from: $0)
        } ?? []
        if defaults.object(forKey: "displayTargets.v1") == nil,
           let host = defaults.string(forKey: "manualHost"), !host.trimmingCharacters(in: .whitespaces).isEmpty {
            let port = Int(defaults.string(forKey: "manualPort") ?? "8080") ?? 8080
            targets.append(.init(endpoint: .init(name: "Manual Target", host: host, port: port, source: .manual), enabled: true))
        }
    }
    mutating func merge(_ discovered: [ScryBarEndpoint]) {
        for endpoint in discovered {
            if let index = targets.firstIndex(where: { $0.id == endpoint.stableKey }) {
                var updated = endpoint
                updated = ScryBarEndpoint(id: targets[index].endpoint.id, name: updated.name, host: updated.host,
                    port: updated.port, source: updated.source, product: updated.product, serviceID: updated.serviceID, aliases: updated.aliases)
                targets[index].endpoint = updated
            } else if let index = targets.firstIndex(where: { target in
                target.endpoint.source == .manual && target.endpoint.port == endpoint.port &&
                    (target.endpoint.addressKey == endpoint.addressKey || endpoint.aliases.contains { $0.caseInsensitiveCompare(target.endpoint.host) == .orderedSame })
            }) {
                // The existing manual display becomes the same discovered identity, not a second destination.
                targets[index].endpoint = endpoint
            } else {
                let firstOfProduct = !targets.contains { $0.endpoint.product == endpoint.product }
                targets.append(.init(endpoint: endpoint, enabled: firstOfProduct))
            }
        }
        persist()
    }
    mutating func setEnabled(_ enabled: Bool, id: String) {
        guard let index = targets.firstIndex(where: { $0.id == id }) else { return }
        targets[index].enabled = enabled
        persist()
    }
    mutating func addManual(host: String, port: Int) {
        guard !host.isEmpty, (1...65535).contains(port) else { return }
        if let index = targets.firstIndex(where: { $0.endpoint.addressKey == "\(host.lowercased()):\(port)" }) {
            targets[index].enabled = true
        } else {
            targets.append(.init(endpoint: .init(name: "Manual Target", host: host, port: port, source: .manual), enabled: true))
        }
        persist()
    }
    private func persist() {
        if let data = try? JSONEncoder().encode(targets) { defaults.set(data, forKey: "displayTargets.v1") }
    }
}
