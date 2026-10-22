import Foundation

/// Which apps may publish "now playing" to the displays (ScryBar, Meguru, the studio TV).
///
/// Privacy rule (owner, 2026-10-19): only music streaming apps by default. What someone watches or has watched in a
/// video player (VLC, IINA, …) or in a browser tab must never reach a display, a log, or a cover-art lookup.
/// The check runs before any artwork search, so a blocked title is never sent to iTunes Search either.
/// Unknown apps are always blocked unless "music only" is turned off on purpose.
enum SourceCategory: String, Sendable {
    case music
    case browser      // Safari, Chrome, Firefox, … and YouTube / YouTube Music web apps: any site, so off by default
    case videoPlayer  // VLC, IINA, QuickTime, TV, Infuse, mpv, …
    case other
}

struct SourcePolicy: Sendable {
    var musicOnly: Bool
    var allowBrowsers: Bool
    var allowVideoPlayers: Bool

    static let musicBundles: Set<String> = [
        "com.apple.Music",
        "com.spotify.client",
        "com.tidal.desktop",
    ]
    static let browserPrefixes: [String] = [
        "com.apple.Safari", "com.google.Chrome", "org.mozilla.firefox", "com.microsoft.edgemac",
        "company.thebrowser.Browser", "com.brave.Browser", "com.operasoftware.Opera", "com.vivaldi.Vivaldi",
        "app.zen-browser", "org.chromium.Chromium", "com.apple.Safari.WebApp", "com.google.Chrome.app",
    ]
    static let videoPlayerBundles: Set<String> = [
        "org.videolan.vlc", "com.colliderli.iina", "com.apple.QuickTimePlayerX", "com.apple.TV",
        "io.mpv", "com.firecore.Infuse", "com.firecore.infuse", "tv.plex.desktop", "com.plexapp.plexmediaplayer",
        "com.netflix.Netflix", "com.disney.disneyplus", "com.amazon.aiv.AIVApp", "org.xbmc.kodi", "com.movist.MovistPro",
    ]

    static func category(of bundleID: String?) -> SourceCategory {
        guard let id = bundleID, !id.isEmpty else { return .other }
        if musicBundles.contains(id) { return .music }
        if videoPlayerBundles.contains(id) { return .videoPlayer }
        if browserPrefixes.contains(where: { id.hasPrefix($0) }) || id.localizedCaseInsensitiveContains("youtube") {
            return .browser
        }
        return .other
    }

    func allows(bundleID: String?) -> Bool {
        switch Self.category(of: bundleID) {
        case .music: return true
        case .browser: return !musicOnly && allowBrowsers
        case .videoPlayer: return !musicOnly && allowVideoPlayers
        case .other: return !musicOnly
        }
    }

    /// The bundle id behind a payload: the System provider stores it in `appName`; the Music and TIDAL providers
    /// use fixed names; demo tracks are always fine.
    static func bundleID(of payload: NowPlayingPayload) -> String? {
        if payload.isDemo == true { return "com.apple.Music" }
        switch payload.appName {
        case "Music.app": return "com.apple.Music"
        case "TIDAL": return "com.tidal.desktop"
        default: return payload.appName.isEmpty ? nil : payload.appName
        }
    }

    // MARK: settings (UserDefaults); "music only" defaults to ON in every build

    static let musicOnlyKey = "sourcePolicy.musicOnly"
    static let allowBrowsersKey = "sourcePolicy.allowBrowsers"
    static let allowVideoPlayersKey = "sourcePolicy.allowVideoPlayers"

    static var current: SourcePolicy {
        let d = UserDefaults.standard
        return SourcePolicy(
            musicOnly: d.object(forKey: musicOnlyKey) == nil ? true : d.bool(forKey: musicOnlyKey),
            allowBrowsers: d.bool(forKey: allowBrowsersKey),
            allowVideoPlayers: d.bool(forKey: allowVideoPlayersKey)
        )
    }
}

/// Remembers only *that* something was hidden (category, never the title), for the status line.
final class SourcePolicyLog: @unchecked Sendable {
    static let shared = SourcePolicyLog()
    private let lock = NSLock()
    private var _lastHidden: SourceCategory?
    var lastHidden: SourceCategory? {
        get { lock.lock(); defer { lock.unlock() }; return _lastHidden }
        set { lock.lock(); _lastHidden = newValue; lock.unlock() }
    }
}
