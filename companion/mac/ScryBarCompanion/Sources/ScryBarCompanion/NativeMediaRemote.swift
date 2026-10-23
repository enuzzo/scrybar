import Foundation
import ImageIO
import Darwin

enum NativeArtworkEncoding {
    static func encode(_ bytes: Data) -> NowPlayingArtwork? {
        guard let source = CGImageSourceCreateWithData(bytes as CFData, nil),
              let properties = CGImageSourceCopyPropertiesAtIndex(source, 0, nil) as? [CFString: Any],
              let width = properties[kCGImagePropertyPixelWidth] as? Int,
              let height = properties[kCGImagePropertyPixelHeight] as? Int,
              let standard = ArtworkTranscoder.encodeRGB565Base64(from: bytes) else { return nil }
        let hd = min(width, height) >= 512 ? ArtworkTranscoder.encodeRGB565Base64(from: bytes, size: 512)
            .flatMap { Data(base64Encoded: $0.rgb565Base64) } : nil
        return .init(width: standard.width, height: standard.height, rgb565: standard.rgb565Base64, hd: hd, webURL: nil)
    }
}

struct NativeMediaIdentity: Equatable, Sendable {
    let processID: Int
    let bundleID: String
    let parentBundleID: String?
    let title: String
    let artist: String
    let album: String
    let contentItemID: String?
    let uniqueID: String?
    let artworkID: String?

    func matches(_ request: NowPlayingArtworkRequest) -> Bool {
        guard request.bundleIdentifier == bundleID,
              request.title == title, request.artist == artist, request.album == album else { return false }
        if let pid = request.processIdentifier, pid != processID { return false }
        if let parent = request.parentBundleIdentifier, parent != parentBundleID { return false }
        if let item = request.contentItemIdentifier, item != contentItemID { return false }
        if let unique = request.uniqueIdentifier, unique != uniqueID { return false }
        if let artworkID = request.artworkID, let expected = self.artworkID, artworkID != expected { return false }
        return true
    }
}

/// Pure stream state. The allowlist runs before base64 decoding or artwork
/// retention; a blocked/empty source immediately removes the previous cover.
struct NativeMediaRemoteState {
    private(set) var identity: NativeMediaIdentity?
    private(set) var artwork: Data?
    private(set) var payload: [String: Any]?
    private(set) var artworkRevision: UInt64 = 0

    mutating func consume(_ frame: Data, policy: SourcePolicy) {
        guard let envelope = try? JSONSerialization.jsonObject(with: frame) as? [String: Any],
              envelope["type"] as? String == "data",
              envelope["diff"] as? Bool == false,
              let next = envelope["payload"] as? [String: Any] else { return }
        // Startup can emit an empty frame before the initial callbacks finish.
        // Keep the JXA fallback available until the stream has observed state.
        if next.isEmpty, payload == nil { return }
        let bundle = next["bundleIdentifier"] as? String
        guard let bundle, !bundle.isEmpty, policy.allows(bundleID: bundle) else {
            if identity != nil || artwork != nil { artworkRevision &+= 1 }
            identity = nil; artwork = nil
            // Only source identity is retained for the metadata privacy gate.
            payload = bundle.map { ["bundleIdentifier": $0] } ?? [:]
            return
        }
        // A partial update from an allowed player is not an explicit absence.
        // The native stream's TIDAL debounce supplies its complete next frame.
        guard let pid = next["processIdentifier"] as? Int, pid > 0,
              let title = next["title"] as? String, !title.isEmpty else { return }
        let unique = (next["uniqueIdentifier"] as? String) ?? (next["uniqueIdentifier"] as? NSNumber)?.stringValue
        let current = NativeMediaIdentity(processID: pid, bundleID: bundle,
            parentBundleID: next["parentApplicationBundleIdentifier"] as? String,
            title: title, artist: next["artist"] as? String ?? "", album: next["album"] as? String ?? "",
            contentItemID: next["contentItemIdentifier"] as? String, uniqueID: unique,
            artworkID: next["artworkIdentifier"] as? String)
        let same = current == identity
        var pixels: Data?
        if let encoded = next["artworkData"] as? String, encoded.utf8.count <= 2_800_000 {
            pixels = Data(base64Encoded: encoded)
            if pixels?.isEmpty == true { pixels = nil }
        }
        let replacement = pixels ?? (same ? artwork : nil)
        if !same || replacement != artwork { artworkRevision &+= 1 }
        artwork = replacement
        identity = current
        var metadata = next
        metadata.removeValue(forKey: "artworkData")
        payload = metadata
    }

    func artwork(for request: NowPlayingArtworkRequest, policy: SourcePolicy) -> Data? {
        guard let identity, policy.allows(bundleID: identity.bundleID), identity.matches(request) else { return nil }
        return artwork
    }

    func snapshot(now: Date, policy: SourcePolicy) -> Data? {
        guard let payload else { return nil }
        let bundle = payload["bundleIdentifier"] as? String
        var client: [String: Any] = [:]
        if let bundle { client["bundleIdentifier"] = bundle }
        if let identity {
            client["processIdentifier"] = identity.processID
            if let parent = identity.parentBundleID { client["parentApplicationBundleIdentifier"] = parent }
        }
        var info: [String: Any] = [:]
        var elapsed = 0.0
        var playing = false
        if let identity, policy.allows(bundleID: identity.bundleID) {
            info["kMRMediaRemoteNowPlayingInfoTitle"] = identity.title
            info["kMRMediaRemoteNowPlayingInfoArtist"] = identity.artist
            info["kMRMediaRemoteNowPlayingInfoAlbum"] = identity.album
            if let item = identity.contentItemID { info["kMRMediaRemoteNowPlayingInfoContentItemIdentifier"] = item }
            if let unique = identity.uniqueID { info["kMRMediaRemoteNowPlayingInfoUniqueIdentifier"] = unique }
            if let identifier = payload["artworkIdentifier"] as? String {
                info["kMRMediaRemoteNowPlayingInfoArtworkIdentifier"] = identifier
            }
            let duration = (payload["durationMicros"] as? Double ?? 0) / 1_000_000
            let rate = payload["playbackRate"] as? Double ?? 0
            elapsed = (payload["elapsedTimeMicros"] as? Double ?? 0) / 1_000_000
            if let stamp = payload["timestampEpochMicros"] as? Double {
                elapsed += max(0, now.timeIntervalSince1970 - stamp / 1_000_000) * rate
            }
            if duration > 0 { elapsed = min(duration, elapsed) }
            elapsed = max(0, elapsed)
            info["kMRMediaRemoteNowPlayingInfoDuration"] = duration
            info["kMRMediaRemoteNowPlayingInfoPlaybackRate"] = rate
            playing = payload["playing"] as? Bool ?? false
        }
        return try? JSONSerialization.data(withJSONObject: ["client": client, "info": info,
            "isPlaying": playing, "computedElapsed": elapsed])
    }
}

struct NativeMediaRemoteLines {
    private var pending = Data()
    mutating func append(_ chunk: Data) -> [Data]? {
        pending.append(chunk)
        guard pending.count <= 3_000_000 else { pending.removeAll(); return nil }
        var frames: [Data] = []
        while let newline = pending.firstIndex(of: 10) {
            frames.append(Data(pending[..<newline]))
            pending.removeSubrange(...newline)
        }
        return frames
    }
}

/// One read-only MediaRemote stream. Its stdout is drained while the process is
/// alive, before any wait, so image payloads cannot fill a pipe and deadlock it.
/// No raw stderr, metadata or artwork is logged or persisted.
final class NativeMediaRemote: @unchecked Sendable {
    struct Configuration: Sendable {
        let script: URL
        let framework: URL

        static func bundled() -> Self? {
            guard let resources = Bundle.main.resourceURL else { return nil }
            let root = resources.appendingPathComponent("MediaRemoteAdapter", isDirectory: true)
            let value = Self(script: root.appendingPathComponent("mediaremote-adapter.pl"),
                             framework: root.appendingPathComponent("MediaRemoteAdapter.framework", isDirectory: true))
            guard FileManager.default.fileExists(atPath: value.script.path),
                  FileManager.default.fileExists(atPath: value.framework.path) else { return nil }
            return value
        }
    }

    static let shared = NativeMediaRemote(configuration: .bundled())
    private let lock = NSLock()
    private let queue = DispatchQueue(label: "com.netmilk.ScryBarCompanion.native-media", qos: .utility)
    private let configuration: Configuration?
    private var process: Process?
    private var state = NativeMediaRemoteState()
    private var stopping = false
    private var starting = false
    private var lastStart: TimeInterval = -.infinity

    init(configuration: Configuration?) { self.configuration = configuration }

    func start() {
        lock.lock()
        guard let configuration, !stopping, !starting, process == nil,
              ProcessInfo.processInfo.systemUptime - lastStart >= 10 else { lock.unlock(); return }
        starting = true; lastStart = ProcessInfo.processInfo.systemUptime
        lock.unlock()
        queue.async { [self] in run(configuration) }
    }

    private func run(_ configuration: Configuration) {
        let child = Process(), pipe = Pipe()
        child.executableURL = URL(fileURLWithPath: "/usr/bin/perl")
        child.arguments = [configuration.script.path, configuration.framework.path, "stream", "--no-diff", "--micros",
                           "--debounce=100", "--experimental-peculiar-debounce:com.tidal.desktop=1000"]
        child.standardOutput = pipe; child.standardError = FileHandle.nullDevice
        lock.lock()
        if stopping { starting = false; lock.unlock(); return }
        process = child; starting = false
        lock.unlock()
        defer {
            Self.terminateOwned(child)
            lock.lock(); process = nil; state = NativeMediaRemoteState(); lock.unlock()
        }
        do {
            try child.run()
            var lines = NativeMediaRemoteLines()
            while true {
                // read(upToCount:) may wait to fill its requested count on a
                // long-lived pipe. availableData returns the current chunk,
                // including the final short tail of a complete JSON line.
                let chunk = pipe.fileHandleForReading.availableData
                if chunk.isEmpty { break }
                guard let frames = lines.append(chunk) else { break }
                for frame in frames {
                    lock.lock(); state.consume(frame, policy: .current); lock.unlock()
                }
            }
        } catch {
            // A later poll may retry after the bounded 10-second backoff.
        }
    }

    /// Includes launch and the first coherent frame. While owned, the reader
    /// remains the sole source; starting another synchronous probe can stall
    /// the first metadata poll or substitute stale fallback-player metadata.
    var ownsReader: Bool {
        lock.lock(); defer { lock.unlock() }
        return starting || process != nil
    }

    var running: Bool {
        lock.lock(); defer { lock.unlock() }
        return process?.isRunning == true
    }

    func snapshot() -> Data? {
        start()
        lock.lock(); defer { lock.unlock() }
        guard process?.isRunning == true else { return nil }
        return state.snapshot(now: .now, policy: .current)
    }

    func artwork(for request: NowPlayingArtworkRequest) -> Data? {
        start()
        lock.lock(); defer { lock.unlock() }
        return state.artwork(for: request, policy: .current)
    }

    func artworkRevision(for request: NowPlayingArtworkRequest) -> UInt64 {
        lock.lock(); defer { lock.unlock() }
        guard state.artwork(for: request, policy: .current) != nil else { return 0 }
        return state.artworkRevision
    }

    private static func terminateOwned(_ child: Process) {
        guard child.isRunning else { return }
        child.terminate()
        // EOF/oversized output also reaches this path. Keep this exact child
        // alive in the closure after run() releases its reader ownership.
        DispatchQueue.global(qos: .utility).asyncAfter(deadline: .now() + 2) {
            if child.isRunning { kill(child.processIdentifier, SIGKILL) }
        }
    }

    func stop() {
        lock.lock(); stopping = true; let child = process; lock.unlock()
        if let child { Self.terminateOwned(child) }
    }

    /// Application termination cannot rely on a future Dispatch callback: the
    /// parent may exit before it fires. Bound the final wait and kill this one
    /// owned reader if it did not acknowledge normal termination.
    func stopAndWait() {
        stop()
        lock.lock(); let child = process; lock.unlock()
        let deadline = ProcessInfo.processInfo.systemUptime + 2
        while child?.isRunning == true, ProcessInfo.processInfo.systemUptime < deadline {
            Thread.sleep(forTimeInterval: 0.01)
        }
        if let child, child.isRunning { kill(child.processIdentifier, SIGKILL) }
    }
}

