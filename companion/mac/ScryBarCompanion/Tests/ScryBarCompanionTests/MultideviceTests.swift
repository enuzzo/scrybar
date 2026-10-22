import Foundation
import AppKit
import Testing
@testable import ScryBarCompanion

private actor DisplayMockHTTP: CompanionHTTPTransport {
    var requests: [URLRequest] = []
    var failed = Set<String>()
    var delayed = Set<String>()
    var caps: [String: CompanionCapabilities] = [:]
    func configure(_ host: String, cap: CompanionCapabilities? = nil, failing: Bool = false, slow: Bool = false) {
        caps[host] = cap
        if failing { failed.insert(host) } else { failed.remove(host) }
        if slow { delayed.insert(host) } else { delayed.remove(host) }
    }
    func send(_ request: URLRequest) async throws -> CompanionHTTPReply {
        requests.append(request)
        let host = request.url!.host!
        if delayed.contains(host) { try await Task.sleep(for: .milliseconds(400)) }
        if failed.contains(host) { throw URLError(.timedOut) }
        if request.url!.path == "/api/companion" {
            if let cap = caps[host] { return .init(data: try JSONEncoder().encode(cap), status: 200) }
            return .init(data: Data(), status: 404)
        }
        return .init(data: Data(), status: 200)
    }
    func posts(_ host: String) -> [URLRequest] { requests.filter { $0.url?.host == host && $0.httpMethod == "POST" } }
}
private func display(_ host: String, product: String = "scrybar") -> ScryBarEndpoint {
    .init(name: host, host: host, port: 8080, source: .discovery, product: product, serviceID: "\(product)|\(host)")
}
private let meguruCaps = CompanionCapabilities(version: 1, product: "meguru", feeds: ["now-playing", "bambu"], maxBodyBytes: 98304, artwork: false, camera: true)

@Test @MainActor func networkStatsReachBothCapableDisplaysAndSkipOlderMeguru() async throws {
    let http = DisplayMockHTTP()
    let circle = display("stats-circle", product: "meguru")
    let strip = display("stats-strip")
    let older = display("old-circle", product: "meguru")
    await http.configure("stats-circle", cap: .init(version: 1, product: "meguru", feeds: ["mac-stats"],
        maxBodyBytes: 98304, artwork: true, camera: true))
    await http.configure("old-circle", cap: meguruCaps)
    let delivery = DisplayDelivery(client: ScryBarClient(transport: http))
    delivery.select([circle, strip, older])
    var stats = MacStatsPayload(cpuTempC: 40, gpuTempC: 38, cpuUsagePct: 20, gpuUsagePct: 10,
        ramUsedGB: 12, ramTotalGB: 32, diskUsedGB: 100, diskTotalGB: 1000, updatedAt: .now)
    stats.netDownBytesPerSec = 1_250_000
    stats.netUpBytesPerSec = 45_000
    stats.networkApp = "Demo app"
    stats.networkAttributionAvailable = true
    delivery.send(stats: stats)
    try await Task.sleep(for: .milliseconds(100))
    for host in ["stats-circle", "stats-strip"] {
        let posts = await http.posts(host)
        #expect(posts.count == 1 && posts.first?.url?.path == "/api/mac-stats")
        let body = try #require(posts.first?.httpBody)
        let json = try JSONSerialization.jsonObject(with: body) as! [String: Any]
        #expect(json["netDownBytesPerSec"] as? Double == 1_250_000)
        #expect(json["networkApp"] as? String == "Demo app")
        #expect(json["updatedAt"] is String && body.count < 16384)
    }
    #expect(await http.posts("old-circle").isEmpty)
    stats.updatedAt = Date.now.addingTimeInterval(-16)
    delivery.send(stats: stats)
    try await Task.sleep(for: .milliseconds(30))
    #expect(await http.posts("stats-circle").count == 1)
    delivery.stop()
}

@Test func hdTranscodingPreservesFineColorDetailAndAlbumIdentity() throws {
    let space = try #require(CGColorSpace(name: CGColorSpace.sRGB))
    let context = try #require(CGContext(data: nil, width: 512, height: 512, bitsPerComponent: 8,
        bytesPerRow: 512 * 4, space: space,
        bitmapInfo: CGBitmapInfo.byteOrder32Little.rawValue | CGImageAlphaInfo.premultipliedFirst.rawValue))
    for x in stride(from: 0, to: 512, by: 4) {
        context.setFillColor((x / 4) % 2 == 0 ? NSColor.red.cgColor : NSColor.blue.cgColor)
        context.fill(CGRect(x: x, y: 0, width: 4, height: 512))
    }
    let image = try #require(context.makeImage())
    let png = try #require(NSBitmapImageRep(cgImage: image).representation(using: .png, properties: [:]))
    let hd = try #require(ArtworkTranscoder.encodeRGB565Base64(from: png, size: 512))
    let pixels = try #require(Data(base64Encoded: hd.rgb565Base64))
    #expect(hd.width == 512 && hd.height == 512 && pixels.count == 524288)
    // Adjacent four-pixel stripes preserve distinct red and blue on the panel source.
    #expect(pixels[96 * 2] == 0xF8 && pixels[96 * 2 + 1] == 0)
    #expect(pixels[100 * 2] == 0 && pixels[100 * 2 + 1] == 0x1F)
    let legacy = try #require(ArtworkTranscoder.encodeRGB565Base64(from: png))
    #expect(legacy.width == 150 && Data(base64Encoded: legacy.rgb565Base64)?.count == 45000)
    #expect(ArtworkTranscoder.encodeRGB565Base64(from: png, size: 4096) == nil)
    let cover = "https://resources.tidal.com/images/a/b/c/d/e/160x160.jpg"
    #expect(ArtworkTranscoder.preferredURL(cover) == "https://resources.tidal.com/images/a/b/c/d/e/640x640.jpg")
    #expect(ArtworkTranscoder.preferredURL("https://example.test/160x160.jpg") == "https://example.test/160x160.jpg")
}

@Test func highResolutionArtworkNegotiatesOnlyItsBinaryLimit() async throws {
    let http = DisplayMockHTTP(), client = ScryBarClient(transport: http)
    var caps = CompanionCapabilities(version: 1, product: "meguru", feeds: ["now-playing"],
                                     maxBodyBytes: 98304, artwork: true, camera: false)
    caps.artworkSize = 512; caps.artworkMaxBodyBytes = 524288
    await http.configure("hd-circle", cap: caps)
    var payload = NowPlayingPayload.placeholder(source: "fixture")
    payload.artworkID = "exact-album-edition"; payload.artworkWidth = 150; payload.artworkHeight = 150
    payload.artworkRGB565B64 = Data(repeating: 0x33, count: 45000).base64EncodedString()
    payload.artworkRGB565HD = Data(repeating: 0xA5, count: 524288)
    try await client.send(payload, to: display("hd-circle", product: "meguru"))
    let posts = await http.posts("hd-circle")
    #expect(posts.count == 2 && posts[1].httpBody == payload.artworkRGB565HD)
    let metadata = try JSONSerialization.jsonObject(with: posts[0].httpBody!) as! [String: Any]
    #expect(metadata["artworkWidth"] as? Int == 512)
    #expect(metadata["artworkRGB565HD"] == nil && metadata["artworkRGB565B64"] == nil)
    #expect(posts[0].httpBody!.count < 16384)
    try await client.send(payload, to: display("hd-strip"))
    let strip = try #require(await http.posts("hd-strip").first?.httpBody)
    let stripMetadata = try JSONSerialization.jsonObject(with: strip) as! [String: Any]
    #expect(stripMetadata["artworkWidth"] as? Int == 150)
    #expect(stripMetadata["artworkRGB565B64"] as? String == payload.artworkRGB565B64)
    #expect(stripMetadata["artworkRGB565HD"] == nil)
    caps.artworkMaxBodyBytes = 98304
    await http.configure("limited-circle", cap: caps)
    try await client.send(payload, to: display("limited-circle", product: "meguru"))
    #expect(await http.posts("limited-circle").last?.httpBody?.count == 45000)
    payload.artworkRGB565HD = Data(repeating: 0, count: 524287)
    await http.configure("bad-hd-circle", cap: .init(version: 1, product: "meguru", feeds: ["now-playing"],
        maxBodyBytes: 98304, artwork: true, camera: false, artworkSize: 512, artworkMaxBodyBytes: 524288))
    try await client.send(payload, to: display("bad-hd-circle", product: "meguru"))
    #expect(await http.posts("bad-hd-circle").last?.httpBody?.count == 45000)
}

@Test func meguruArtworkUsesItsOwnBinaryLaneWithoutChangingScryBar() async throws {
    let http = DisplayMockHTTP(), client = ScryBarClient(transport: http)
    let circle = display("art-circle", product: "meguru"), strip = display("art-strip")
    await http.configure("art-circle", cap: .init(version: 1, product: "meguru", feeds: ["now-playing", "bambu"], maxBodyBytes: 98304, artwork: true, camera: true))
    var payload = NowPlayingPayload.placeholder(source: "test")
    payload.artworkID = "album-123"; payload.artworkWidth = 150; payload.artworkHeight = 150
    payload.artworkRGB565B64 = Data(repeating: 0xA5, count: 45_000).base64EncodedString()
    try await client.send(payload, to: circle)
    let first = await http.posts("art-circle")
    #expect(first.count == 2)
    #expect(first[0].url?.path == "/api/now-playing")
    #expect(first[1].url?.path == "/api/now-playing/artwork")
    #expect(first[1].httpBody?.count == 45_000)
    #expect(first[1].value(forHTTPHeaderField: "X-Artwork-Key")?.count == 16)
    let metadata = try JSONSerialization.jsonObject(with: first[0].httpBody!) as! [String: Any]
    #expect(metadata["artworkKey"] as? String == first[1].value(forHTTPHeaderField: "X-Artwork-Key"))
    #expect(metadata["artworkRGB565B64"] == nil)
    try await client.send(payload, to: circle)
    #expect(await http.posts("art-circle").count == 3)
    try await client.send(payload, to: strip)
    let stripBody = try #require(await http.posts("art-strip").first?.httpBody)
    let stripMetadata = try JSONSerialization.jsonObject(with: stripBody) as! [String: Any]
    #expect(stripMetadata["artworkRGB565B64"] != nil)
}

@Test func targetsMigratePersistSelectionAndFollowDHCP() throws {
    let suite = "display-tests-\(UUID())", defaults = try #require(UserDefaults(suiteName: suite))
    defer { defaults.removePersistentDomain(forName: suite) }
    defaults.set("old-display", forKey: "manualHost")
    var store = DisplayTargetStore(defaults: defaults)
    #expect(store.targets.count == 1 && store.targets[0].enabled)
    var scry = display("old-display"), meguru = display("circle", product: "meguru")
    store.merge([scry, meguru])
    #expect(store.targets.count == 2)
    #expect(store.targets.allSatisfy { $0.enabled })
    store.setEnabled(false, id: meguru.stableKey)
    scry.host = "new-address"; meguru.host = "new-circle"
    store.merge([scry, meguru])
    store.merge([])
    let restored = DisplayTargetStore(defaults: defaults)
    #expect(restored.targets.count == 2)
    #expect(restored.targets.first { $0.endpoint.product == "scrybar" }?.endpoint.host == "new-address")
    #expect(restored.targets.first { $0.endpoint.product == "meguru" }?.enabled == false)
}

@Test func capabilityContractAndArtworkArePerEndpoint() async throws {
    let http = DisplayMockHTTP(), client = ScryBarClient(transport: http)
    let circle = display("circle", product: "meguru"), strip = display("strip"), second = display("second")
    await http.configure("circle", cap: meguruCaps)
    var payload = NowPlayingPayload.placeholder(source: "test")
    payload.artworkID = "cover"; payload.artworkWidth = 1; payload.artworkHeight = 1; payload.artworkRGB565B64 = "AAA="
    try await client.send(payload, to: strip)
    try await client.send(payload, to: circle)
    try await client.send(payload, to: strip)
    try await client.send(payload, to: second)
    let stripBodies = try await http.posts("strip").map { try JSONSerialization.jsonObject(with: $0.httpBody!) as! [String: Any] }
    let circleBody = try #require(try await http.posts("circle").first.map { try JSONSerialization.jsonObject(with: $0.httpBody!) as! [String: Any] })
    #expect(stripBodies[0]["artworkRGB565B64"] != nil)
    #expect(stripBodies[1]["artworkRGB565B64"] == nil)
    #expect(circleBody.keys.allSatisfy { !$0.hasPrefix("artwork") })
    let secondBody = try #require(try await http.posts("second").first.map { try JSONSerialization.jsonObject(with: $0.httpBody!) as! [String: Any] })
    #expect(secondBody["artworkRGB565B64"] != nil)
    await client.invalidate(strip)
    try await client.send(payload, to: strip)
    #expect(await http.posts("strip").count == 3)
    await #expect(throws: (any Error).self) { try await client.negotiate(display("unknown", product: "meguru")) }
    #expect(await http.posts("unknown").isEmpty)
}

@Test @MainActor func slowDisplayDoesNotBlockFastDisplayAndStaleSnapshotsAreDropped() async throws {
    let http = DisplayMockHTTP(), delivery = DisplayDelivery(client: ScryBarClient(transport: http))
    let slow = display("slow"), fast = display("fast")
    await http.configure("slow", failing: true, slow: true)
    delivery.select([slow, fast])
    var payload = NowPlayingPayload.placeholder(source: "test")
    delivery.send(nowPlaying: payload)
    try await Task.sleep(for: .milliseconds(80))
    #expect(await http.posts("fast").count == 1)
    #expect(delivery.states[fast.stableKey]?.connected() == true)
    #expect(delivery.states[slow.stableKey]?.connected() != true)
    // Flood busy lanes: intermediate snapshots do not form a backlog.
    for _ in 0..<20 { delivery.send(nowPlaying: payload) }
    delivery.select([fast])
    try await Task.sleep(for: .milliseconds(450))
    #expect(delivery.states[slow.stableKey] == nil)
    let before = await http.posts("fast").count
    payload.updatedAt = Date().addingTimeInterval(-16)
    delivery.send(nowPlaying: payload)
    try await Task.sleep(for: .milliseconds(30))
    #expect(await http.posts("fast").count == before)
    delivery.stop()
}

@Test func oversizedMetadataNeverReachesDisplay() async throws {
    let http = DisplayMockHTTP(), client = ScryBarClient(transport: http)
    await http.configure("circle", cap: meguruCaps)
    var payload = NowPlayingPayload.placeholder(source: "test")
    payload.title = String(repeating: "x", count: 17000)
    await #expect(throws: (any Error).self) { try await client.send(payload, to: display("circle", product: "meguru")) }
    #expect(await http.posts("circle").isEmpty)
}

@Test func mockMetadataIsExplicitlyDemoAndLegacyPayloadsRemainDecodable() throws {
    let payload = try #require(MockNowPlayingProvider().snapshot())
    let encoder = JSONEncoder(); encoder.dateEncodingStrategy = .iso8601
    let decoder = JSONDecoder(); decoder.dateDecodingStrategy = .iso8601
    let bytes = try encoder.encode(payload.networkPayload(includeArtworkData: false))
    let wire = try JSONSerialization.jsonObject(with: bytes) as! [String: Any]
    #expect(wire["isDemo"] as? Bool == true)
    var oldWire = wire; oldWire.removeValue(forKey: "isDemo")
    let old = try decoder.decode(NowPlayingWirePayload.self, from: JSONSerialization.data(withJSONObject: oldWire))
    #expect(old.isDemo == nil)
    #expect(NowPlayingPayload.placeholder(source: "System").isDemo == nil)
}

@Test func metadataFirstDoesNotSuppressLateArtworkOnEitherDisplay() async throws {
    let http = DisplayMockHTTP(), client = ScryBarClient(transport: http)
    let strip = display("late-strip"), circle = display("late-circle", product: "meguru")
    await http.configure("late-circle", cap: .init(version: 1, product: "meguru", feeds: ["now-playing"], maxBodyBytes: 98304, artwork: true, camera: false))
    var payload = NowPlayingPayload.placeholder(source: "fixture")
    payload.title = "Current track"; payload.artworkID = "current-edition"
    try await client.send(payload, to: strip)
    try await client.send(payload, to: circle)
    payload.artworkWidth = 150; payload.artworkHeight = 150
    payload.artworkRGB565B64 = Data(repeating: 0x23, count: 45000).base64EncodedString()
    try await client.send(payload, to: strip)
    try await client.send(payload, to: circle)
    let posts = await http.posts("late-strip")
    let first = try JSONSerialization.jsonObject(with: posts[0].httpBody!) as! [String: Any]
    let second = try JSONSerialization.jsonObject(with: posts[1].httpBody!) as! [String: Any]
    #expect(first["title"] as? String == "Current track" && first["artworkRGB565B64"] == nil)
    #expect(second["artworkRGB565B64"] as? String == payload.artworkRGB565B64)
    #expect(await http.posts("late-circle").last?.httpBody?.count == 45000)
}
