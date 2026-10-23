import Foundation
import Darwin
import AppKit
import Testing
@testable import ScryBarCompanion

private let musicPolicy = SourcePolicy(musicOnly: true, allowBrowsers: false, allowVideoPlayers: false)

private func nativeFrame(art: Data? = nil, bundle: String = "com.tidal.desktop", pid: Int = 101,
                         title: String = "Track", album: String = "Edition", item: String = "item",
                         unique: String = "unique", artworkID: String = "art") throws -> Data {
    var payload: [String: Any] = ["processIdentifier": pid, "bundleIdentifier": bundle,
        "parentApplicationBundleIdentifier": bundle, "title": title, "artist": "Artist", "album": album,
        "contentItemIdentifier": item, "uniqueIdentifier": unique, "artworkIdentifier": artworkID,
        "playing": true, "playbackRate": 1.0, "durationMicros": 240_000_000,
        "elapsedTimeMicros": 10_000_000, "timestampEpochMicros": 1_000_000_000]
    if let art { payload["artworkData"] = art.base64EncodedString() }
    return try JSONSerialization.data(withJSONObject: ["type": "data", "diff": false, "payload": payload])
}

private func nativeRequest(title: String = "Track", album: String = "Edition", bundle: String = "com.tidal.desktop",
                           pid: Int = 101, item: String = "item", unique: String = "unique", artworkID: String = "art") -> NowPlayingArtworkRequest {
    .init(artworkURL: nil, artworkID: artworkID, title: title, artist: "Artist", album: album, tidal: true,
          bundleIdentifier: bundle, parentBundleIdentifier: bundle, processIdentifier: pid,
          contentItemIdentifier: item, uniqueIdentifier: unique)
}

@Test func nativeArtworkRetainsOnlyTheSameCompleteIdentityAndClearsBlockedSources() throws {
    let bytes = Data(repeating: 0x23, count: 70_000)
    var state = NativeMediaRemoteState()
    state.consume(try nativeFrame(art: bytes), policy: musicPolicy)
    #expect(state.artwork(for: nativeRequest(), policy: musicPolicy)?.count == bytes.count)
    let revision = state.artworkRevision
    state.consume(try nativeFrame(), policy: musicPolicy)
    #expect(state.artworkRevision == revision)
    #expect(state.artwork(for: nativeRequest(), policy: musicPolicy)?.count == bytes.count)
    for mismatch in [nativeRequest(title: "Other"), nativeRequest(album: "Other edition"),
                     nativeRequest(bundle: "com.apple.Music"), nativeRequest(pid: 102),
                     nativeRequest(item: "other-item"), nativeRequest(unique: "other-unique"),
                     nativeRequest(artworkID: "other-art")] {
        #expect(state.artwork(for: mismatch, policy: musicPolicy) == nil)
    }
    state.consume(try nativeFrame(title: "Next", item: "next-item"), policy: musicPolicy)
    #expect(state.artwork == nil)
    #expect(state.artwork(for: nativeRequest(), policy: musicPolicy) == nil)
    state.consume(try nativeFrame(art: bytes, bundle: "com.google.Chrome"), policy: musicPolicy)
    #expect(state.identity == nil && state.artwork == nil)
    #expect(state.payload?.keys.sorted() == ["bundleIdentifier"])
    let json = try #require(state.snapshot(now: Date(timeIntervalSince1970: 1001), policy: musicPolicy))
    let snapshot = try JSONSerialization.jsonObject(with: json) as! [String: Any]
    #expect((snapshot["info"] as? [String: Any])?.isEmpty == true)
}

@Test func nativeArtworkIdentityChangesDropPreviousPixelsAndMetadataKeepsRealTime() throws {
    let bytes = Data(repeating: 0x23, count: 20)
    let changes = [try nativeFrame(pid: 202), try nativeFrame(bundle: "com.apple.Music"),
                   try nativeFrame(album: "Other"), try nativeFrame(item: "next"),
                   try nativeFrame(unique: "next"), try nativeFrame(artworkID: "next")]
    for changed in changes {
        var state = NativeMediaRemoteState()
        state.consume(try nativeFrame(art: bytes), policy: musicPolicy)
        state.consume(changed, policy: musicPolicy)
        #expect(state.artwork == nil)
    }
    var state = NativeMediaRemoteState()
    state.consume(try nativeFrame(art: bytes), policy: musicPolicy)
    let data = try #require(state.snapshot(now: Date(timeIntervalSince1970: 1002.5), policy: musicPolicy))
    let json = try JSONSerialization.jsonObject(with: data) as! [String: Any]
    #expect(json["computedElapsed"] as? Double == 12.5)
    #expect((json["info"] as? [String: Any])?["kMRMediaRemoteNowPlayingInfoDuration"] as? Double == 240)
    state.consume(Data("invalid json".utf8), policy: musicPolicy)
    #expect(state.artwork?.count == bytes.count)
    state.consume(Data("{\"type\":\"data\",\"diff\":false,\"payload\":{}}".utf8), policy: musicPolicy)
    #expect(state.identity == nil && state.artwork == nil)
}

@Test func nativeArtworkRevisionBypassesNegativeCacheAndKeepsSameTrackPixelsDuringUpgrade() async throws {
    let cache = NowPlayingArtworkCache { request in
        guard request.nativeArtworkRevision > 0 else { return nil }
        return .init(width: 150, height: 150,
                     rgb565: Data(repeating: UInt8(request.nativeArtworkRevision), count: 2).base64EncodedString(),
                     hd: nil, webURL: nil)
    }
    var request = nativeRequest()
    #expect(cache.value(for: request) == nil)
    try await Task.sleep(for: .milliseconds(20))
    request.nativeArtworkRevision = 1
    var first: NowPlayingArtwork?
    for _ in 0..<100 {
        first = cache.value(for: request)
        if first != nil { break }
        try await Task.sleep(for: .milliseconds(5))
    }
    #expect(first != nil)
    request.nativeArtworkRevision = 2
    #expect(cache.value(for: request) != nil)
    var upgraded: NowPlayingArtwork?
    for _ in 0..<100 {
        upgraded = cache.value(for: request)
        if upgraded?.rgb565 != first?.rgb565 { break }
        try await Task.sleep(for: .milliseconds(5))
    }
    #expect(upgraded?.rgb565 != first?.rgb565)
    #expect(cache.value(for: nativeRequest(title: "Other track")) == nil)
}

@Test func nativeStreamFramingDrainsLargeSplitLinesAndRejectsOversize() throws {
    let first = try nativeFrame(art: Data(repeating: 0x23, count: 200_000))
    let second = try nativeFrame(title: "Next")
    let stream = first + Data([10]) + second + Data([10])
    for width in [4096, 65_536, stream.count] {
        var lines = NativeMediaRemoteLines(), result: [Data] = []
        for offset in stride(from: 0, to: stream.count, by: width) {
            let appended = lines.append(stream.subdata(in: offset..<min(offset + width, stream.count)))
            result += try #require(appended)
        }
        #expect(result.count == 2 && result.first?.count == first.count && result.last == second)
    }
    var singleBytes = NativeMediaRemoteLines(), smallFrames: [Data] = []
    for byte in Data("{}\n{\"x\":1}\n".utf8) {
        let appended = singleBytes.append(Data([byte]))
        smallFrames += try #require(appended)
    }
    #expect(smallFrames.count == 2)
    var oversized = NativeMediaRemoteLines()
    let rejected = oversized.append(Data(repeating: 1, count: 3_000_001))
    let recovered = oversized.append(Data("{}\n".utf8))
    #expect(rejected == nil)
    #expect(recovered?.count == 1)
}

@Test func nativeStreamDrainsBeforeChildExitAndStopsAStubbornChild() async throws {
    let directory = FileManager.default.temporaryDirectory.appendingPathComponent("scrybar-native-fixture-\(UUID())")
    try FileManager.default.createDirectory(at: directory, withIntermediateDirectories: true)
    defer { try? FileManager.default.removeItem(at: directory) }
    let frame = try nativeFrame(art: Data(repeating: 0x23, count: 200_000))
    let json = try #require(String(data: frame, encoding: .utf8))
    let script = directory.appendingPathComponent("fixture.pl")
    try ("$|=1; $SIG{TERM}='IGNORE'; print <<'FRAME';\n" + json + "\nFRAME\nsleep 30;\n").write(to: script, atomically: true, encoding: .utf8)
    let native = NativeMediaRemote(configuration: .init(script: script, framework: directory))
    defer { native.stop() }
    let provider = SystemNowPlayingProvider(nativeMedia: native)
    let started = ProcessInfo.processInfo.systemUptime
    _ = provider.snapshot()
    #expect(ProcessInfo.processInfo.systemUptime - started < 0.1)
    #expect(native.ownsReader)
    var ready = false
    for _ in 0..<200 {
        ready = native.snapshot() != nil
        if ready { break }
        try await Task.sleep(for: .milliseconds(10))
    }
    #expect(ready && native.running)
    native.stop()
    for _ in 0..<300 {
        if !native.running { break }
        try await Task.sleep(for: .milliseconds(10))
    }
    #expect(!native.running)
}

@Test func longArtworkWireIdentifiersAreBoundedWithoutChangingResolverIdentity() {
    var payload = NowPlayingPayload.placeholder(source: "fixture")
    for count in [254, 255, 256, 300] {
        let original = String(repeating: "a", count: count)
        payload.artworkID = original
        let wire = payload.wireArtworkID
        #expect((wire?.utf8.count ?? 1000) <= 255)
        #expect(payload.artworkID == original && wire == payload.wireArtworkID)
        #expect(count > 255 ? wire != original : wire == original)
    }
    payload.artworkID = String(repeating: "é", count: 128)
    #expect(payload.wireArtworkID?.hasPrefix("art-") == true)
    payload.artworkFingerprint = "pixels-one"
    let original = payload.wireArtworkID
    payload.artworkFingerprint = "pixels-two"
    #expect(payload.wireArtworkID != original)
}

@Test func nativeLowResolutionIsNeverMisrepresentedAsHDAndEnrichmentRequiresPixelMatch() throws {
    func image(_ size: Int, _ swapped: Bool = false) throws -> Data {
        let context = try #require(CGContext(data: nil, width: size, height: size, bitsPerComponent: 8,
            bytesPerRow: size * 4, space: CGColorSpaceCreateDeviceRGB(),
            bitmapInfo: CGBitmapInfo.byteOrder32Little.rawValue | CGImageAlphaInfo.premultipliedFirst.rawValue))
        context.setFillColor((swapped ? NSColor.blue : NSColor.red).cgColor)
        context.fill(CGRect(x: 0, y: 0, width: size / 2, height: size))
        context.setFillColor((swapped ? NSColor.red : NSColor.blue).cgColor)
        context.fill(CGRect(x: size / 2, y: 0, width: size / 2, height: size))
        let cgImage = try #require(context.makeImage())
        let encoded = NSBitmapImageRep(cgImage: cgImage).representation(using: .png, properties: [:])
        return try #require(encoded)
    }
    let low = try image(150), high = try image(600), wrong = try image(600, true)
    let native = try #require(NativeArtworkEncoding.encode(low))
    #expect(native.hd == nil)
    #expect(ArtworkTranscoder.encodeRGB565Base64(from: low, size: 512) == nil)
    #expect(ArtworkTranscoder.encodeRGB565Base64(from: low, size: 150) != nil)
    let hd = try #require(NativeArtworkEncoding.encode(high))
    #expect(hd.hd?.count == 524_288)
    let nativePixels = try #require(Data(base64Encoded: native.rgb565))
    let correctPixels = try #require(Data(base64Encoded: hd.rgb565))
    let encodedWrong = try #require(NativeArtworkEncoding.encode(wrong))
    let wrongPixels = try #require(Data(base64Encoded: encodedWrong.rgb565))
    #expect(ArtworkPixelMatch.matches(nativeRGB565: nativePixels, candidateRGB565: correctPixels))
    #expect(!ArtworkPixelMatch.matches(nativeRGB565: nativePixels, candidateRGB565: wrongPixels))
    #expect(!ArtworkPixelMatch.matches(nativeRGB565: Data(), candidateRGB565: correctPixels))
}

private struct StaleMusicFallback: NowPlayingProviding {
    func snapshot() -> NowPlayingPayload? { .placeholder(source: "stale-music-cache") }
}

@Test func explicitlyBlockedSystemSourceCannotFallBackToOldTidalMusic() throws {
    let blocked = try JSONSerialization.data(withJSONObject: ["client": ["bundleIdentifier": "com.google.Chrome"], "info": [:]])
    let provider = FallbackNowPlayingProvider([SystemNowPlayingProvider(readSnapshot: { blocked }), StaleMusicFallback()])
    #expect(provider.snapshot() == nil)
}

@Test func nativeStreamEOFStillStopsItsStubbornOwnedChild() async throws {
    let directory = FileManager.default.temporaryDirectory.appendingPathComponent("scrybar-native-eof-\(UUID())")
    try FileManager.default.createDirectory(at: directory, withIntermediateDirectories: true)
    defer { try? FileManager.default.removeItem(at: directory) }
    let pidFile = directory.appendingPathComponent("child.pid")
    let script = directory.appendingPathComponent("fixture.pl")
    try ("$SIG{TERM}='IGNORE'; open(my $p, '>', '\(pidFile.path)') or die; print $p $$; close $p; close STDOUT; sleep 30;\n")
        .write(to: script, atomically: true, encoding: .utf8)
    let native = NativeMediaRemote(configuration: .init(script: script, framework: directory))
    var pid: Int32 = 0
    defer { native.stop(); if pid > 0 { kill(pid, SIGKILL) } }
    native.start()
    for _ in 0..<100 {
        if let text = try? String(contentsOf: pidFile, encoding: .utf8),
           let value = Int32(text) { pid = value; break }
        try await Task.sleep(for: .milliseconds(10))
    }
    #expect(pid > 0)
    guard pid > 0 else { return }
    for _ in 0..<300 {
        if kill(pid, 0) == -1 { break }
        try await Task.sleep(for: .milliseconds(10))
    }
    #expect(kill(pid, 0) == -1)
}
