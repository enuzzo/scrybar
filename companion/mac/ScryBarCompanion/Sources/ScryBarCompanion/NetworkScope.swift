import Darwin
import Foundation

/// Destination scope, independent of which interface carried the packet.
enum NetworkScope: String, CaseIterable, Identifiable, Sendable {
    case internet, local, all
    var id: String { rawValue }
    var title: String {
        switch self {
        case .internet: "Internet"
        case .local: "Rete interna"
        case .all: "Tutto"
        }
    }
    static func stored(in defaults: UserDefaults = .standard) -> Self {
        defaults.string(forKey: "network.scope").flatMap(Self.init(rawValue:)) ?? .internet
    }
}

enum NetworkDestination: Sendable {
    case internet, local, unknown

    /// nettop -n represents IPv4 ports with ':' and IPv6 ports with '.'.
    /// Unconnected UDP/listening sockets have a wildcard peer, never Internet.
    static func endpoint(_ endpoint: String, onLink: NetworkLocalNetworks = .init()) -> Self {
        let host: String
        if endpoint.filter({ $0 == ":" }).count > 1 {
            guard let port = endpoint.lastIndex(of: ".") else { return .unknown }
            host = String(endpoint[..<port]).components(separatedBy: "%")[0]
        } else {
            guard let port = endpoint.lastIndex(of: ":") else { return .unknown }
            host = String(endpoint[..<port])
        }
        return onLink.contains(host) ? .local : address(host)
    }

    static func address(_ host: String) -> Self {
        var v4 = in_addr()
        if inet_pton(AF_INET, host, &v4) == 1 {
            let ip = UInt32(bigEndian: v4.s_addr)
            // RFC1918, shared/CGNAT (including Tailscale), loopback,
            // link-local, multicast and reserved local destinations.
            if ip >> 24 == 0 || ip >> 24 == 10 || ip >> 24 == 127 ||
                ip >> 22 == 0x191 || ip >> 20 == 0xAC1 ||
                ip >> 16 == 0xA9FE || ip >> 16 == 0xC0A8 || ip >> 28 >= 14 { return .local }
            // Documentation, benchmarking and protocol-reserved ranges aren't
            // evidence of Internet traffic. Keep them unclassified.
            if ip >> 24 == 192 && (ip >> 8 == 0xC00000 || ip >> 8 == 0xC00002) ||
                ip >> 17 == 0x6309 || ip >> 8 == 0xC63364 || ip >> 8 == 0xCB0071 { return .unknown }
            return .internet
        }
        var v6 = in6_addr()
        guard inet_pton(AF_INET6, host, &v6) == 1 else { return .unknown }
        let bytes = withUnsafeBytes(of: &v6) { Array($0) }
        if bytes.prefix(10).allSatisfy({ $0 == 0 }), bytes[10] == 255, bytes[11] == 255 {
            return address(bytes.suffix(4).map(String.init).joined(separator: "."))
        }
        if bytes.prefix(12) == [0, 0x64, 0xff, 0x9b, 0, 0, 0, 0, 0, 0, 0, 0] {
            return address(bytes.suffix(4).map(String.init).joined(separator: "."))
        }
        // fc00::/7 includes Tailscale fd7a:115c:a1e0::/48.
        if bytes[0] & 0xfe == 0xfc || bytes[0] == 0xff ||
            bytes[0] == 0xfe && bytes[1] & 0xc0 == 0x80 ||
            bytes.prefix(15).allSatisfy({ $0 == 0 }) && bytes[15] <= 1 { return .local }
        if bytes.prefix(4) == [0x20, 0x01, 0x0d, 0xb8] { return .unknown }
        return bytes[0] & 0xe0 == 0x20 ? .internet : .unknown
    }
}

struct NettopConnectionKey: Hashable, Sendable {
    let process: NettopProcessKey
    // Endpoints are transient baseline keys in RAM only, never logged or sent.
    let label: String
    var destination: NetworkDestination { destination(onLink: .init()) }
    func destination(onLink: NetworkLocalNetworks) -> NetworkDestination {
        guard let separator = label.range(of: "<->") else { return .unknown }
        return .endpoint(String(label[separator.upperBound...]), onLink: onLink)
    }
}

struct NettopConnectionCounters: Sendable {
    let counters: [NettopConnectionKey: NetworkByteCounters]
    let sampledAt: TimeInterval
    let missingCounters: Bool
    var hasUsableCounters: Bool { !counters.isEmpty || !missingCounters }
}

/// Process summaries establish ownership only. Only socket rows enter totals.
struct NettopConnectionCSVParser {
    private var incoming = Data()
    private var columns: (download: Int, upload: Int)?
    private var owner: NettopProcessKey?
    private var current: [NettopConnectionKey: NetworkByteCounters] = [:]
    private var sampledAt: TimeInterval?
    private var missingCounters = false
    private(set) var capacityExceeded = false
    private(set) var partialSamples = 0

    mutating func consume(_ chunk: Data, at now: TimeInterval) -> [NettopConnectionCounters] {
        incoming.append(chunk)
        guard incoming.count <= 1024 * 1024 else {
            self = .init(); capacityExceeded = true; return []
        }
        var samples: [NettopConnectionCounters] = []
        while let end = incoming.firstIndex(of: 10) {
            let line = String(decoding: incoming.prefix(upTo: end), as: UTF8.self)
            incoming.removeSubrange(...end)
            let fields = NettopCSVParser.csvFields(line.trimmingCharacters(in: .newlines))
            if fields.first == "", let down = fields.firstIndex(of: "bytes_in"), let up = fields.firstIndex(of: "bytes_out") {
                if let sampledAt {
                    samples.append(.init(counters: current, sampledAt: sampledAt, missingCounters: missingCounters))
                    if missingCounters { partialSamples += 1 }
                }
                columns = (down, up); sampledAt = now; current = [:]; owner = nil; missingCounters = false
                continue
            }
            guard let columns, sampledAt != nil, let label = fields.first, !label.isEmpty else { continue }
            if label.hasPrefix("tcp") || label.hasPrefix("udp"), label.contains("<->") {
                guard let owner, fields.count > max(columns.download, columns.upload) else {
                    missingCounters = true; continue
                }
                let key = NettopConnectionKey(process: owner, label: label)
                guard current[key] != nil || current.count < 16384 else {
                    capacityExceeded = true; missingCounters = true; continue
                }
                // Native nettop emits empty fields for sockets without byte
                // accounting (including listeners). They are not measured 0.
                if fields[columns.download].isEmpty && fields[columns.upload].isEmpty {
                    missingCounters = true
                    continue
                }
                guard let down = UInt64(fields[columns.download]), let up = UInt64(fields[columns.upload]) else {
                    missingCounters = true; continue
                }
                let old = current[key]
                current[key] = .init(download: max(down, old?.download ?? 0), upload: max(up, old?.upload ?? 0))
            } else if let dot = label.lastIndex(of: "."), let pid = Int32(label[label.index(after: dot)...]), pid >= 0 {
                owner = .init(pid: pid, name: String(label[..<dot]))
            } else {
                owner = nil
                missingCounters = true
            }
        }
        return samples
    }
}

struct NetworkConnectionSample {
    let rates: [NettopConnectionKey: NetworkByteRates]
    let sampledAt: TimeInterval
    var missingCounters = false
    func isFresh(at now: TimeInterval) -> Bool {
        NetworkAttributionSample(apps: [], sampledAt: sampledAt).isFresh(at: now)
    }

    func snapshot(scope: NetworkScope, onLink: NetworkLocalNetworks = .init(), resolve: (NettopProcessKey) -> NetworkAppIdentity) -> NetworkStatsSnapshot {
        var processes: [NettopProcessKey: NetworkByteRates] = [:]
        var unknownActive = false
        for (key, rate) in rates {
            let identity = resolve(key.process)
            // Count original app sockets once, not the encrypted VPN transport
            // a second time as a public peer/DERP connection.
            if identity.isTailscaleTransport { continue }
            let destination = key.destination(onLink: onLink)
            if destination == .unknown, rate.download > 0 || rate.upload > 0 { unknownActive = true }
            guard scope == .all || scope == .internet && destination == .internet || scope == .local && destination == .local else { continue }
            let old = processes[key.process]
            processes[key.process] = .init(download: (old?.download ?? 0) + rate.download,
                                           upload: (old?.upload ?? 0) + rate.upload)
        }
        let down = processes.values.reduce(0) { $0 + $1.download }
        let up = processes.values.reduce(0) { $0 + $1.upload }
        let unknownOnly = scope != .all && unknownActive && down == 0 && up == 0 || missingCounters && rates.isEmpty
        let apps = NetworkActivityBuilder.activities(processes, resolve: resolve)
        return .init(downloadBytesPerSec: unknownOnly ? nil : down, uploadBytesPerSec: unknownOnly ? nil : up,
                     apps: unknownOnly ? [] : Array(apps.prefix(10)), attributionAvailable: !unknownOnly,
                     attributionMessage: unknownOnly ? (rates.isEmpty ? "Socket byte counters are unavailable." : "Active traffic has no classifiable peer.") : nil,
                     scope: scope, coveragePartial: missingCounters || scope != .all && unknownActive)
    }
}

extension NetworkAppIdentity {
    var isTailscaleTransport: Bool {
        let path = id.lowercased()
        // Exact executable/bundle identities, never generic 'VPN' or 'utun'.
        return path.contains("/tailscale.app/") || path.hasSuffix("/tailscale.app") ||
            path.hasSuffix("/tailscaled") || path.contains("/io.tailscale.ipn.") ||
            path.contains("/io.tailscale.ipn/")
    }
}

/// Public IPv6 addresses can still be on the local Ethernet/Wi-Fi subnet.
/// Snapshot only active physical prefixes, excluding default and tunnel routes.
struct NetworkLocalNetworks: Sendable {
    struct Prefix: Sendable {
        let address: [UInt8]
        let mask: [UInt8]
        init?(address: String, mask: String) {
            guard let address = NetworkLocalNetworks.bytes(address),
                  let mask = NetworkLocalNetworks.bytes(mask), address.count == mask.count else { return nil }
            var zeroSeen = false, oneSeen = false
            for byte in mask {
                for bit in (0..<8).reversed() {
                    if byte & (1 << bit) != 0 {
                        guard !zeroSeen else { return nil }
                        oneSeen = true
                    } else { zeroSeen = true }
                }
            }
            guard oneSeen else { return nil }
            self.address = address; self.mask = mask
        }
    }
    var prefixes: [Prefix] = []
    func contains(_ host: String) -> Bool {
        guard let bytes = Self.bytes(host) else { return false }
        return prefixes.contains { prefix in
            bytes.count == prefix.address.count && zip(zip(bytes, prefix.address), prefix.mask).allSatisfy {
                ($0.0.0 & $0.1) == ($0.0.1 & $0.1)
            }
        }
    }
    private static func bytes(_ host: String) -> [UInt8]? {
        var v4 = in_addr(), v6 = in6_addr()
        if inet_pton(AF_INET, host, &v4) == 1 { return withUnsafeBytes(of: &v4) { Array($0) } }
        if inet_pton(AF_INET6, host, &v6) == 1 { return withUnsafeBytes(of: &v6) { Array($0) } }
        return nil
    }
    static func current() -> Self {
        var first: UnsafeMutablePointer<ifaddrs>?
        guard getifaddrs(&first) == 0 else { return .init() }
        defer { freeifaddrs(first) }
        var result = Self()
        var cursor = first
        while let entry = cursor {
            defer { cursor = entry.pointee.ifa_next }
            let item = entry.pointee
            let name = String(cString: item.ifa_name)
            guard name.hasPrefix("en"), !name.dropFirst(2).isEmpty, name.dropFirst(2).allSatisfy(\.isNumber),
                  Int32(item.ifa_flags) & (IFF_UP | IFF_RUNNING) == (IFF_UP | IFF_RUNNING),
                  let address = item.ifa_addr, let mask = item.ifa_netmask,
                  [AF_INET, AF_INET6].contains(Int32(address.pointee.sa_family)) else { continue }
            func numeric(_ value: UnsafeMutablePointer<sockaddr>) -> String? {
                var buffer = [CChar](repeating: 0, count: Int(NI_MAXHOST))
                guard getnameinfo(value, socklen_t(value.pointee.sa_len), &buffer,
                                  socklen_t(buffer.count), nil, 0, NI_NUMERICHOST) == 0 else { return nil }
                return String(cString: buffer).components(separatedBy: "%")[0]
            }
            if let host = numeric(address), let netmask = numeric(mask),
               let prefix = Prefix(address: host, mask: netmask) { result.prefixes.append(prefix) }
        }
        return result
    }
}

/// Byte activity is not proof of usable samples: a live CSV stream can keep
/// producing invalid/incomplete frames. Both clocks have a bounded grace period.
enum NettopStreamHealth {
    static func isStalled(now: TimeInterval, started: TimeInterval,
                          output: TimeInterval, sample: TimeInterval, measurement: TimeInterval? = nil) -> Bool {
        if let measurement, now - measurement > 3.5 { return true }
        return now - max(started, output) > 5 || now - max(started, sample) > 5
    }
}

struct NetworkCollectorDiagnostics: Sendable {
    let running: Bool
    let outputAge: TimeInterval
    let sampleAge: TimeInterval
    let validSamples: Int
    let restarts: Int
    let partialSamples: Int
    let capacityExceeded: Bool
}
