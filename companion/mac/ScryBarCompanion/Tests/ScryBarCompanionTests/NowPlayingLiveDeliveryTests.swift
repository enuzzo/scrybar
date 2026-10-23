import Foundation
import Testing
@testable import ScryBarCompanion

private func configuredLiveNativeMedia() -> NativeMediaRemote {
    let environment = ProcessInfo.processInfo.environment
    if let script = environment["SCRYBAR_NATIVE_MEDIA_SCRIPT"], let framework = environment["SCRYBAR_NATIVE_MEDIA_FRAMEWORK"] {
        return .init(configuration: .init(script: URL(fileURLWithPath: script), framework: URL(fileURLWithPath: framework)))
    }
    return .shared
}

private actor LiveArtworkHTTP: CompanionHTTPTransport {
    struct Receipt: Sendable {
        let path: String
        let bytes: Int
        let status: Int
    }
    private var receipts: [Receipt] = []
    func send(_ request: URLRequest) async throws -> CompanionHTTPReply {
        let reply = try await CompanionURLTransport().send(request)
        // Never retain/log endpoints, metadata, image bytes, identifiers or URLs.
        receipts.append(.init(path: request.url?.path ?? "", bytes: request.httpBody?.count ?? 0,
                              status: reply.status))
        return reply
    }
    func summary() -> [Receipt] { receipts }
}

/// Explicit integration probe: current permitted music -> selected live Meguru.
/// It does not change playback, selection or preferences and never opens USB.
/// HTTP acknowledgments establish transport delivery, not visible panel pixels.
@Test func nowPlayingLiveMeguruArtworkDeliveryWhenExplicitlyEnabled() async throws {
    guard ProcessInfo.processInfo.environment["SCRYBAR_NOWPLAYING_LIVE_DELIVERY"] == "meguru" else { return }
    let defaults = try #require(UserDefaults(suiteName: "com.netmilk.ScryBarCompanion"))
    let targetData = try #require(defaults.data(forKey: "displayTargets.v1"))
    let targets = try JSONDecoder().decode([DisplayTarget].self, from: targetData)
    let target = try #require(targets.first { $0.enabled && $0.endpoint.product == "meguru" })
    let native = configuredLiveNativeMedia()
    defer { native.stop() }
    let provider = SystemNowPlayingProvider(nativeMedia: native)
    var ready: NowPlayingPayload?
    for _ in 0..<20 {
        if let payload = provider.snapshot(), payload.isPlaying,
           SourcePolicy.current.allows(bundleID: SourcePolicy.bundleID(of: payload)),
           payload.artworkWidth == 150, payload.artworkHeight == 150,
           Data(base64Encoded: payload.artworkRGB565B64 ?? "")?.count == 45_000 {
            ready = payload
            break
        }
        try await Task.sleep(for: .seconds(1))
    }
    let payload = try #require(ready)
    let transport = LiveArtworkHTTP(), client = ScryBarClient(transport: transport)
    do {
        let caps = try await client.negotiate(target.endpoint)
        #expect(caps.product == "meguru" && caps.artwork && caps.artworkSize == 512)
        try await client.send(payload, to: target.endpoint)
    } catch {
        let code = (error as NSError).code
        Issue.record("Live Meguru artwork transport failed (error code \(code)); private details omitted")
        return
    }
    let receipts = await transport.summary()
    let metadataAccepted = receipts.contains { $0.path == "/api/now-playing" && (200...299).contains($0.status) }
    let expectedBytes = payload.artworkRGB565HD?.count == 524_288 ? 524_288 : 45_000
    let pixelsAccepted = receipts.contains { $0.path == "/api/now-playing/artwork" && $0.bytes == expectedBytes && (200...299).contains($0.status) }
    #expect(metadataAccepted && pixelsAccepted)
    print("Live Meguru metadataAck=\(metadataAccepted), artworkAck=\(pixelsAccepted), binaryBytes=\(expectedBytes); endpoint, music and image data omitted")
}

@Test func nativeNowPlayingLiveSourceWhenExplicitlyEnabled() async throws {
    guard ProcessInfo.processInfo.environment["SCRYBAR_NOWPLAYING_NATIVE_LIVE_SMOKE"] == "1" else { return }
    let native = configuredLiveNativeMedia()
    defer { native.stop() }
    let provider = SystemNowPlayingProvider(nativeMedia: native)
    let samples = Int(ProcessInfo.processInfo.environment["SCRYBAR_NATIVE_SAMPLES"] ?? "12") ?? 12
    let coldStart = ProcessInfo.processInfo.systemUptime
    var firstCoherent = false
    for _ in 0..<50 {
        if provider.snapshot() != nil { firstCoherent = true; break }
        try await Task.sleep(for: .milliseconds(100))
    }
    let readySeconds = ProcessInfo.processInfo.systemUptime - coldStart
    #expect(firstCoherent && readySeconds < 5)
    var nilSnapshots = 0, covers = 0, hd = 0, trackChanges = 0, previous: String?, maxLatency = 0.0
    var trace: [[String: Any]] = []
    for index in 0..<samples {
        let started = ProcessInfo.processInfo.systemUptime
        let payload = provider.snapshot()
        let duration = ProcessInfo.processInfo.systemUptime - started
        maxLatency = max(maxLatency, duration)
        var record: [String: Any] = ["sample": index, "utc": ISO8601DateFormatter().string(from: .now),
            "native_running": native.running, "snapshot_present": payload != nil, "latency_ms": duration * 1000]
        if let payload {
            let identity = "\(payload.title)|\(payload.artist)|\(payload.album)"
            if let previous, previous != identity { trackChanges += 1 }
            previous = identity
            let pixels = Data(base64Encoded: payload.artworkRGB565B64 ?? "")?.count ?? 0
            if pixels == 45_000 { covers += 1 }
            if payload.artworkRGB565HD?.count == 524_288 { hd += 1 }
            record["in_sync"] = payload.inSync; record["playing"] = payload.isPlaying
            record["standard_bytes"] = pixels; record["hd_bytes"] = payload.artworkRGB565HD?.count ?? 0
            record["wire_id_bytes"] = payload.wireArtworkID?.utf8.count ?? 0
            #expect(payload.inSync)
        } else { nilSnapshots += 1 }
        trace.append(record)
        try await Task.sleep(for: .seconds(1))
    }
    if let path = ProcessInfo.processInfo.environment["SCRYBAR_NATIVE_TRACE_PATH"] {
        try JSONSerialization.data(withJSONObject: trace, options: [.prettyPrinted, .sortedKeys]).write(to: URL(fileURLWithPath: path))
    }
    print("Native firstCoherentSeconds=\(readySeconds), music samples=\(samples), nil=\(nilSnapshots), covers150=\(covers), covers512=\(hd), trackChanges=\(trackChanges), maxSnapshotSeconds=\(maxLatency); private details omitted")
    #expect(native.running && covers > 0 && nilSnapshots == 0)
}
