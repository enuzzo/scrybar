import Foundation

struct NowPlayingArtworkRequest: Hashable, Sendable {
    let artworkURL: String?
    let artworkID: String?
    let title: String
    let artist: String
    let album: String
    let tidal: Bool
    let bundleIdentifier: String?
    let parentBundleIdentifier: String?
    let processIdentifier: Int?
    let contentItemIdentifier: String?
    let uniqueIdentifier: String?
    var nativeArtworkRevision: UInt64 = 0

    init(artworkURL: String?, artworkID: String?, title: String, artist: String, album: String, tidal: Bool,
         bundleIdentifier: String? = nil, parentBundleIdentifier: String? = nil,
         processIdentifier: Int? = nil, contentItemIdentifier: String? = nil, uniqueIdentifier: String? = nil) {
        self.artworkURL = artworkURL; self.artworkID = artworkID
        self.title = title; self.artist = artist; self.album = album; self.tidal = tidal
        self.bundleIdentifier = bundleIdentifier; self.parentBundleIdentifier = parentBundleIdentifier
        self.processIdentifier = processIdentifier; self.contentItemIdentifier = contentItemIdentifier
        self.uniqueIdentifier = uniqueIdentifier
    }
}

struct NowPlayingArtwork: Sendable {
    let width: Int
    let height: Int
    let rgb565: String
    let hd: Data?
    let webURL: String?
    var fingerprint: String? = nil
}

/// The view's load task must also observe pixels arriving after metadata.
/// Playback time is deliberately absent so ordinary polls do not reload art.
struct NowPlayingArtworkViewKey: Equatable {
    let title: String
    let artist: String
    let artworkURL: String?
    let artworkID: String?
    let artworkWidth: Int?
    let artworkHeight: Int?
    let artworkRGB565B64: String?
}

/// Metadata polling never waits for disk scans, image downloads or transcoding.
/// One serial worker owns the resolver; matching the complete request prevents
/// a late cover from a previous track/edition appearing on the current track.
final class NowPlayingArtworkCache: @unchecked Sendable {
    private let lock = NSLock()
    private let queue = DispatchQueue(label: "com.netmilk.ScryBarCompanion.artwork", qos: .utility)
    private let load: @Sendable (NowPlayingArtworkRequest) -> NowPlayingArtwork?
    private var busy = false
    private var completed: NowPlayingArtworkRequest?
    private var result: NowPlayingArtwork?
    private var completedAt: TimeInterval = 0

    init(load: @escaping @Sendable (NowPlayingArtworkRequest) -> NowPlayingArtwork?) {
        self.load = load
    }

    func value(for request: NowPlayingArtworkRequest) -> NowPlayingArtwork? {
        lock.lock()
        defer { lock.unlock() }
        var previous = completed
        previous?.nativeArtworkRevision = request.nativeArtworkRevision
        let current = previous == request ? result : nil
        if completed == request, ProcessInfo.processInfo.systemUptime - completedAt < (result == nil ? 1 : 5) {
            return result
        }
        guard !busy else { return current }
        busy = true
        queue.async { [self] in
            var resolved = load(request)
            if let bytes = resolved.flatMap({ Data(base64Encoded: $0.rgb565) }) {
                resolved?.fingerprint = ArtworkTranscoder.fingerprint(for: bytes)
            }
            lock.lock()
            completed = request
            result = resolved
            completedAt = ProcessInfo.processInfo.systemUptime
            busy = false
            lock.unlock()
        }
        return current
    }
}
