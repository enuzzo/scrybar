import Foundation

struct NowPlayingArtworkRequest: Hashable, Sendable {
    let artworkURL: String?
    let artworkID: String?
    let title: String
    let artist: String
    let album: String
    let tidal: Bool
}

struct NowPlayingArtwork: Sendable {
    let width: Int
    let height: Int
    let rgb565: String
    let hd: Data?
    let webURL: String?
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
        if completed == request, result != nil || ProcessInfo.processInfo.systemUptime - completedAt < 30 {
            return result
        }
        guard !busy else { return nil }
        busy = true
        queue.async { [self] in
            let resolved = load(request)
            lock.lock()
            completed = request
            result = resolved
            completedAt = ProcessInfo.processInfo.systemUptime
            busy = false
            lock.unlock()
        }
        return nil
    }
}
