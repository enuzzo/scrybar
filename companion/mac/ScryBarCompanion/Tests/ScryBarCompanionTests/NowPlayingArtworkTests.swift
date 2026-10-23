import Foundation
import Testing
@testable import ScryBarCompanion

private func systemFixture(title: String = "Current song", bundle: String = "com.tidal.desktop") throws -> Data {
    try JSONSerialization.data(withJSONObject: ["client": ["bundleIdentifier": bundle], "isPlaying": true,
        "computedElapsed": 12.0, "info": ["kMRMediaRemoteNowPlayingInfoTitle": title,
        "kMRMediaRemoteNowPlayingInfoArtist": "Test artist", "kMRMediaRemoteNowPlayingInfoAlbum": "Test edition",
        "kMRMediaRemoteNowPlayingInfoDuration": 240.0, "kMRMediaRemoteNowPlayingInfoPlaybackRate": 1.0]])
}

@Test func nowPlayingMetadataDoesNotWaitForStalledArtwork() throws {
    let started = DispatchSemaphore(value: 0), release = DispatchSemaphore(value: 0)
    let cache = NowPlayingArtworkCache { _ in
        started.signal()
        _ = release.wait(timeout: .now() + 4)
        return .init(width: 150, height: 150, rgb565: "cover", hd: nil, webURL: nil)
    }
    defer { release.signal() }
    let data = try systemFixture()
    let provider = SystemNowPlayingProvider(artworkCache: cache, readSnapshot: { data })
    let time = ProcessInfo.processInfo.systemUptime
    let first = try #require(provider.snapshot())
    #expect(ProcessInfo.processInfo.systemUptime - time < 1)
    #expect(first.title == "Current song" && first.isPlaying && first.elapsedSec == 12)
    #expect(first.artworkRGB565B64 == nil)
    #expect(started.wait(timeout: .now() + 1) == .success)
    let next = try #require(provider.snapshot())
    #expect(next.title == first.title && next.updatedAt >= first.updatedAt)
}

@Test func nowPlayingLateCoverNeverBelongsToAnotherTrackOrEdition() async throws {
    let release = DispatchSemaphore(value: 0)
    let cache = NowPlayingArtworkCache { request in
        if request.album == "First edition" {
            _ = release.wait(timeout: .now() + 4)
        }
        return .init(width: 150, height: 150, rgb565: request.album, hd: nil, webURL: nil)
    }
    defer { release.signal() }
    let first = NowPlayingArtworkRequest(artworkURL: nil, artworkID: "same", title: "Song", artist: "Artist", album: "First edition", tidal: true)
    let second = NowPlayingArtworkRequest(artworkURL: nil, artworkID: "same", title: "Song", artist: "Artist", album: "Second edition", tidal: true)
    #expect(cache.value(for: first) == nil)
    for _ in 0..<20 { #expect(cache.value(for: second) == nil) }
    release.signal()
    var result: NowPlayingArtwork?
    for _ in 0..<100 {
        result = cache.value(for: second)
        if result != nil { break }
        try await Task.sleep(for: .milliseconds(10))
    }
    #expect(result?.rgb565 == "Second edition")
}

@Test func nowPlayingPrivacyGatePrecedesArtworkWorker() throws {
    let called = DispatchSemaphore(value: 0)
    let cache = NowPlayingArtworkCache { _ in called.signal(); return nil }
    let data = try systemFixture(title: "Blocked fixture", bundle: "com.google.Chrome")
    let provider = SystemNowPlayingProvider(artworkCache: cache, readSnapshot: { data })
    #expect(provider.snapshot() == nil)
    #expect(called.wait(timeout: .now() + 0.1) == .timedOut)
}

@Test func systemProviderPublishesLateArtworkForUnchangedTrack() async throws {
    let release = DispatchSemaphore(value: 0)
    let pixels = Data(repeating: 0x23, count: 45_000).base64EncodedString()
    let cache = NowPlayingArtworkCache { _ in
        _ = release.wait(timeout: .now() + 4)
        return .init(width: 150, height: 150, rgb565: pixels,
                     hd: Data(repeating: 0x23, count: 524_288), webURL: nil)
    }
    defer { release.signal() }
    let snapshot = try systemFixture()
    let provider = SystemNowPlayingProvider(artworkCache: cache, readSnapshot: { snapshot })
    let first = try #require(provider.snapshot())
    #expect(first.artworkRGB565B64 == nil)
    release.signal()
    var ready: NowPlayingPayload?
    for _ in 0..<100 {
        let next = provider.snapshot()
        if next?.artworkRGB565B64 != nil { ready = next; break }
        try await Task.sleep(for: .milliseconds(10))
    }
    let result = try #require(ready)
    #expect(result.title == first.title && result.album == first.album && result.artworkID == first.artworkID)
    #expect(result.artworkWidth == 150 && result.artworkHeight == 150)
    #expect(Data(base64Encoded: result.artworkRGB565B64 ?? "")?.count == 45_000)
    #expect(result.artworkRGB565HD?.count == 524_288)
}

@Test func artworkViewReloadsForLatePixelsAndUpdatedURLWithSameTrackID() {
    func key(_ pixels: String? = nil, url: String? = nil) -> NowPlayingArtworkViewKey {
        .init(title: "Same track", artist: "Same artist", artworkURL: url, artworkID: "same-edition",
              artworkWidth: pixels == nil ? nil : 150, artworkHeight: pixels == nil ? nil : 150,
              artworkRGB565B64: pixels)
    }
    #expect(key() == key())
    #expect(key() != key("first-pixels"))
    #expect(key("first-pixels") == key("first-pixels"))
    #expect(key("first-pixels") != key("corrected-pixels"))
    #expect(key(url: "https://example.invalid/old") != key(url: "https://example.invalid/new"))
}

@Test func nowPlayingLiveSmokeWhenExplicitlyEnabled() async throws {
    guard ProcessInfo.processInfo.environment["SCRYBAR_NOWPLAYING_LIVE_SMOKE"] == "1" else { return }
    let provider = SystemNowPlayingProvider()
    var count = 0
    var covers = 0
    var hdCovers = 0
    var longest = 0.0
    let requireArtwork = ProcessInfo.processInfo.environment["SCRYBAR_NOWPLAYING_LIVE_ARTWORK"] == "1"
    for _ in 0..<(requireArtwork ? 20 : 6) {
        let started = ProcessInfo.processInfo.systemUptime
        let payload = try #require(provider.snapshot())
        longest = max(longest, ProcessInfo.processInfo.systemUptime - started)
        #expect(!payload.title.isEmpty && SourcePolicy.musicBundles.contains(payload.appName))
        #expect(Date.now.timeIntervalSince(payload.updatedAt) < 2)
        count += 1
        if payload.artworkWidth == 150, payload.artworkHeight == 150,
           let pixels = payload.artworkRGB565B64, Data(base64Encoded: pixels)?.count == 45_000 {
            covers += 1
        }
        if payload.artworkRGB565HD?.count == 524_288 { hdCovers += 1 }
        if requireArtwork && covers > 0 && hdCovers > 0 { break }
        try await Task.sleep(for: .seconds(1))
    }
    print("Live music snapshots=\(count), covers150=\(covers), covers512=\(hdCovers), maximum latency seconds=\(longest); titles, identifiers and image data omitted")
    #expect(longest < 2)
    if requireArtwork { #expect(covers > 0 && hdCovers > 0) }
}
