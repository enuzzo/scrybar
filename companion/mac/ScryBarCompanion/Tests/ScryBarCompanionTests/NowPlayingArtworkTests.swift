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

@Test func nowPlayingLiveSmokeWhenExplicitlyEnabled() async throws {
    guard ProcessInfo.processInfo.environment["SCRYBAR_NOWPLAYING_LIVE_SMOKE"] == "1" else { return }
    let provider = SystemNowPlayingProvider()
    var count = 0
    var longest = 0.0
    for _ in 0..<6 {
        let started = ProcessInfo.processInfo.systemUptime
        let payload = try #require(provider.snapshot())
        longest = max(longest, ProcessInfo.processInfo.systemUptime - started)
        #expect(!payload.title.isEmpty && SourcePolicy.musicBundles.contains(payload.appName))
        #expect(Date.now.timeIntervalSince(payload.updatedAt) < 2)
        count += 1
        try await Task.sleep(for: .seconds(1))
    }
    print("Live music snapshots=\(count), maximum latency seconds=\(longest); titles and identifiers omitted")
    #expect(longest < 2)
}
