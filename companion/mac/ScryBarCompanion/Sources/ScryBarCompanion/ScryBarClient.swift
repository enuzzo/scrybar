import Foundation

struct CompanionCapabilities: Codable, Equatable, Sendable {
    let version: Int
    let product: String
    let feeds: [String]
    let maxBodyBytes: Int
    let artwork: Bool
    let camera: Bool
    var artworkSize: Int? = nil
    var artworkMaxBodyBytes: Int? = nil
    static let legacy = Self(version: 1, product: "scrybar", feeds: ["now-playing", "bambu", "mac-stats"],
        maxBodyBytes: 262144, artwork: true, camera: true)
}

struct CompanionHTTPReply: Sendable { let data: Data; let status: Int }
protocol CompanionHTTPTransport: Sendable {
    func send(_ request: URLRequest) async throws -> CompanionHTTPReply
}
struct CompanionURLTransport: CompanionHTTPTransport {
    func send(_ request: URLRequest) async throws -> CompanionHTTPReply {
        let (data, response) = try await URLSession.shared.data(for: request)
        guard let response = response as? HTTPURLResponse else { throw URLError(.badServerResponse) }
        return .init(data: data, status: response.statusCode)
    }
}
enum CompanionProtocolError: Error, LocalizedError {
    case capability, oversized, unsupported
    var errorDescription: String? {
        switch self {
        case .capability: return "Display capability handshake failed"
        case .oversized: return "Payload exceeds display limit"
        case .unsupported: return "Feed not supported by display"
        }
    }
}

actor ScryBarClient {
    private let transport: any CompanionHTTPTransport
    private let uptime: @Sendable () -> TimeInterval
    private var lastSentArtworkIDByEndpoint: [String: String] = [:]
    private var lastArtworkUploadAtByEndpoint: [String: TimeInterval] = [:]
    private var capabilities: [String: (CompanionCapabilities, Date)] = [:]
    private var probes: [String: Task<CompanionCapabilities, Error>] = [:]
    init(transport: any CompanionHTTPTransport = CompanionURLTransport(),
         uptime: @escaping @Sendable () -> TimeInterval = { ProcessInfo.processInfo.systemUptime }) {
        self.transport = transport
        self.uptime = uptime
    }

    func negotiate(_ endpoint: ScryBarEndpoint) async throws -> CompanionCapabilities {
        let key = endpoint.addressKey
        if let cached = capabilities[key], Date().timeIntervalSince(cached.1) < 60 { return cached.0 }
        if let probe = probes[key] { return try await probe.value }
        let transport = self.transport
        let probe = Task<CompanionCapabilities, Error> {
            guard let url = endpoint.baseURL else { throw URLError(.badURL) }
            var request = URLRequest(url: url.appending(path: "api/companion"))
            request.timeoutInterval = 2
            request.cachePolicy = .reloadIgnoringLocalCacheData
            let reply = try await transport.send(request)
            if [404, 405].contains(reply.status), endpoint.product == "scrybar" { return .legacy }
            guard reply.status == 200, reply.data.count <= 4096 else { throw CompanionProtocolError.capability }
            let value = try JSONDecoder().decode(CompanionCapabilities.self, from: reply.data)
            guard value.version == 1, ["meguru", "scrybar"].contains(value.product),
                  endpoint.product != "meguru" || value.product == "meguru",
                  (512...1_048_576).contains(value.maxBodyBytes) else { throw CompanionProtocolError.capability }
            return value
        }
        probes[key] = probe
        do {
            let value = try await probe.value
            probes[key] = nil
            capabilities[key] = (value, .now)
            return value
        } catch { probes[key] = nil; throw error }
    }
    func invalidate(_ endpoint: ScryBarEndpoint) {
        capabilities[endpoint.addressKey] = nil
        lastSentArtworkIDByEndpoint[endpoint.addressKey] = nil
        lastArtworkUploadAtByEndpoint[endpoint.addressKey] = nil
    }
    func send(_ payload: NowPlayingPayload, to endpoint: ScryBarEndpoint) async throws {
        let caps = try await negotiate(endpoint)
        let key = endpoint.addressKey
        let artworkNeedsRefresh = lastArtworkUploadAtByEndpoint[key]
            .map { uptime() - $0 >= 15 } ?? true
        let decodedArtwork = payload.artworkRGB565B64.flatMap { Data(base64Encoded: $0) }
        let validInlineArtwork: Bool = {
            guard let width = payload.artworkWidth, let height = payload.artworkHeight,
                  (1...512).contains(width), (1...512).contains(height),
                  let decodedArtwork else { return false }
            return decodedArtwork.count == width * height * 2
        }()
        let legacyArtwork = payload.artworkWidth == 150 && payload.artworkHeight == 150 &&
            validInlineArtwork ? decodedArtwork : nil
        let supportsHD = caps.artworkSize == 512 && (caps.artworkMaxBodyBytes ?? 0) >= 524_288
        let hdArtwork = supportsHD && payload.artworkRGB565HD?.count == 524_288 ? payload.artworkRGB565HD : nil
        let meguruArtwork = caps.product == "meguru" && caps.artwork ?
            (hdArtwork ?? (legacyArtwork?.count == 45_000 ? legacyArtwork : nil)) : nil
        let deliveryID = payload.wireArtworkID.map { identifier in
            guard let bytes = meguruArtwork else { return identifier }
            return "\(identifier)|rgb565-\(bytes.count == 524_288 ? 512 : 150)"
        }
        let artworkKey = meguruArtwork == nil ? nil : deliveryID.map(Self.artworkKey)
        let includeArtwork = caps.product != "meguru" && caps.artwork && payload.wireArtworkID != nil &&
            validInlineArtwork && (lastSentArtworkIDByEndpoint[key] != payload.wireArtworkID || artworkNeedsRefresh)
        var metadata = payload
        if let meguruArtwork {
            metadata.artworkWidth = meguruArtwork.count == 524_288 ? 512 : 150
            metadata.artworkHeight = metadata.artworkWidth
        }
        if !caps.artwork {
            metadata.artworkURL = nil; metadata.artworkID = nil; metadata.artworkWidth = nil
            metadata.artworkHeight = nil; metadata.artworkRGB565B64 = nil
        }
        let data = try encode(metadata.networkPayload(includeArtworkData: includeArtwork, artworkKey: artworkKey))
        try await post(data, feed: "now-playing", endpoint: endpoint, capabilities: caps, updatedAt: payload.updatedAt)
        if let artworkKey, let meguruArtwork,
           lastSentArtworkIDByEndpoint[key] != deliveryID || artworkNeedsRefresh {
            guard let url = endpoint.baseURL else { throw URLError(.badURL) }
            var request = URLRequest(url: url.appending(path: "api/now-playing/artwork"))
            request.httpMethod = "POST"; request.httpBody = meguruArtwork; request.timeoutInterval = 3
            request.setValue("application/octet-stream", forHTTPHeaderField: "Content-Type")
            request.setValue(artworkKey, forHTTPHeaderField: "X-Artwork-Key")
            let reply = try await transport.send(request)
            try Task.checkCancellation()
            guard (200...299).contains(reply.status) else { throw URLError(.badServerResponse) }
            lastArtworkUploadAtByEndpoint[key] = uptime()
        }
        if caps.product == "meguru" {
            lastSentArtworkIDByEndpoint[key] = artworkKey == nil ? nil : deliveryID
        } else if includeArtwork {
            // Metadata can arrive before the asynchronous cover. Only bytes
            // actually uploaded may suppress its later first delivery.
            lastSentArtworkIDByEndpoint[key] = deliveryID
            // A reboot can clear a display's cover without changing its
            // endpoint or interrupting metadata. Refresh its RAM periodically.
            lastArtworkUploadAtByEndpoint[key] = uptime()
        } else if deliveryID == nil {
            lastSentArtworkIDByEndpoint[key] = nil
        }
    }
    private static func artworkKey(_ identifier: String) -> String {
        var hash: UInt64 = 0xcbf29ce484222325
        for byte in identifier.utf8 { hash ^= UInt64(byte); hash &*= 0x100000001b3 }
        return String(format: "%016llx", hash)
    }
    func sendMacStats(_ payload: MacStatsPayload, to endpoint: ScryBarEndpoint) async throws {
        try await post(encode(payload), feed: "mac-stats", endpoint: endpoint, capabilities: negotiate(endpoint), updatedAt: payload.updatedAt)
    }
    func sendBambu(_ payload: BambuPrinterPayload, to endpoint: ScryBarEndpoint) async throws {
        try await post(encode(payload), feed: "bambu", endpoint: endpoint, capabilities: negotiate(endpoint), updatedAt: payload.updatedAt)
    }
    private func encode<T: Encodable>(_ value: T) throws -> Data {
        let encoder = JSONEncoder(); encoder.dateEncodingStrategy = .iso8601
        return try encoder.encode(value)
    }
    private func post(_ data: Data, feed: String, endpoint: ScryBarEndpoint, capabilities: CompanionCapabilities, updatedAt: Date) async throws {
        guard Date().timeIntervalSince(updatedAt) < 15 else { throw URLError(.dataNotAllowed) }
        guard capabilities.feeds.contains(feed) else { throw CompanionProtocolError.unsupported }
        // Meguru separates metadata (16 KiB) from camera frames (96 KiB).
        let limit = capabilities.product == "meguru" ? min(16384, capabilities.maxBodyBytes) : capabilities.maxBodyBytes
        guard data.count <= limit else { throw CompanionProtocolError.oversized }
        guard let url = endpoint.baseURL else { throw URLError(.badURL) }
        var request = URLRequest(url: url.appending(path: "api/\(feed)"))
        request.httpMethod = "POST"; request.httpBody = data; request.timeoutInterval = 2
        request.setValue("application/json", forHTTPHeaderField: "Content-Type")
        let reply = try await transport.send(request)
        try Task.checkCancellation()
        guard (200...299).contains(reply.status) else { throw URLError(.badServerResponse) }
    }
}
