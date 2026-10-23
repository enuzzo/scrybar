import Foundation
import Testing
@testable import ScryBarCompanion

/// Privacy rule: only music apps by default; video players, browsers and unknown apps never reach a display.
@Suite struct SourcePolicyTests {
    let defaults = SourcePolicy(musicOnly: true, allowBrowsers: false, allowVideoPlayers: false)

    @Test func musicAppsPassByDefault() {
        for id in ["com.apple.Music", "com.spotify.client", "com.tidal.desktop"] {
            #expect(defaults.allows(bundleID: id), "\(id) should pass")
        }
    }

    @Test func videoPlayersBrowsersAndUnknownAreBlockedByDefault() {
        for id in ["org.videolan.vlc", "com.colliderli.iina", "com.apple.QuickTimePlayerX", "com.apple.TV",
                   "com.google.Chrome", "com.apple.Safari", "com.google.Chrome.app.abcdef", "com.apple.Safari.WebApp.1234",
                   "com.example.YouTubeMusic", "com.unknown.player", "", "System"] {
            #expect(!defaults.allows(bundleID: id), "\(id) must be blocked")
        }
        #expect(!defaults.allows(bundleID: nil))
    }

    @Test func extrasNeedMusicOnlyOff() {
        let stillLocked = SourcePolicy(musicOnly: true, allowBrowsers: true, allowVideoPlayers: true)
        #expect(!stillLocked.allows(bundleID: "com.google.Chrome"))
        #expect(!stillLocked.allows(bundleID: "org.videolan.vlc"))
        let open = SourcePolicy(musicOnly: false, allowBrowsers: true, allowVideoPlayers: false)
        #expect(open.allows(bundleID: "com.google.Chrome"))
        #expect(!open.allows(bundleID: "com.colliderli.iina"))
    }

    @Test func categories() {
        #expect(SourcePolicy.category(of: "com.colliderli.iina") == .videoPlayer)
        #expect(SourcePolicy.category(of: "com.google.Chrome.app.xyz") == .browser)
        #expect(SourcePolicy.category(of: "com.spotify.client") == .music)
        #expect(SourcePolicy.category(of: "org.example.thing") == .other)
    }

    @Test func providerPayloadsMapToBundleIDs() {
        var p = NowPlayingPayload.placeholder(source: "Music")
        p.appName = "Music.app"
        #expect(SourcePolicy.bundleID(of: p) == "com.apple.Music")
        p.appName = "TIDAL"
        #expect(SourcePolicy.bundleID(of: p) == "com.tidal.desktop")
        p.appName = "org.videolan.vlc"
        #expect(!defaults.allows(bundleID: SourcePolicy.bundleID(of: p)))
    }

    @Test func musicOnlyIsOnWhenNeverSet() {
        let key = SourcePolicy.musicOnlyKey
        let saved = UserDefaults.standard.object(forKey: key)
        UserDefaults.standard.removeObject(forKey: key)
        #expect(SourcePolicy.current.musicOnly)
        if let saved { UserDefaults.standard.set(saved, forKey: key) }
    }
}
