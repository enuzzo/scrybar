import AppKit
import Foundation

protocol NowPlayingProviding: Sendable {
    func snapshot() -> NowPlayingPayload?
}

private final class BlockingResult<Value>: @unchecked Sendable {
    var value: Value?
}

// Providers use internal mutable caches but are only called sequentially
// from a single polling task, so @unchecked Sendable is safe here.

final class FallbackNowPlayingProvider: NowPlayingProviding, @unchecked Sendable {
    private let providers: [any NowPlayingProviding]

    init(_ providers: [any NowPlayingProviding]) {
        self.providers = providers
    }

    func snapshot() -> NowPlayingPayload? {
        for provider in providers {
            if let payload = provider.snapshot() {
                return payload
            }
            // An explicitly hidden/empty system source must not be replaced
            // with old music found in another player's disk cache.
            if let system = provider as? SystemNowPlayingProvider, system.suppressFallback { return nil }
        }
        return nil
    }
}

final class MockNowPlayingProvider: NowPlayingProviding, @unchecked Sendable {
    private var index = 0

    func next() {
        index = (index + 1) % MockLibrary.tracks.count
    }

    func snapshot() -> NowPlayingPayload? {
        let track = MockLibrary.tracks[index]
        return NowPlayingPayload(
            title: track.title,
            artist: track.artist,
            album: track.album,
            source: track.source,
            appName: track.source,
            durationSec: track.durationSec,
            elapsedSec: track.elapsedSec,
            isPlaying: true,
            inSync: true,
            artworkURL: nil,
            artworkID: nil,
            artworkWidth: nil,
            artworkHeight: nil,
            artworkRGB565B64: nil,
            updatedAt: Date(),
            isDemo: true
        )
    }
}

final class MusicNowPlayingProvider: NowPlayingProviding, @unchecked Sendable {
    func snapshot() -> NowPlayingPayload? {
        let script = """
        on cleanText(theValue)
            if theValue is missing value then return ""
            return (theValue as text)
        end cleanText

        tell application "Music"
            if it is not running then return ""
            set stateText to (player state as text)
            if stateText is "stopped" then return ""
            set trackRef to current track
            set trackName to cleanText(name of trackRef)
            set trackArtist to cleanText(artist of trackRef)
            set trackAlbum to cleanText(album of trackRef)
            set trackDuration to (duration of trackRef) as text
            set trackPosition to (player position) as text
            return trackName & tab & trackArtist & tab & trackAlbum & tab & trackDuration & tab & trackPosition & tab & stateText
        end tell
        """

        guard let appleScript = NSAppleScript(source: script) else { return nil }
        var error: NSDictionary?
        let result = appleScript.executeAndReturnError(&error)
        if let error {
            NSLog("MusicNowPlayingProvider AppleScript error: %@", error)
        }

        guard let output = result.stringValue, !output.isEmpty else { return nil }
        let parts = output.components(separatedBy: "\t")
        guard parts.count >= 6 else { return nil }

        let duration = Double(parts[3]) ?? 0
        let elapsed = Double(parts[4]) ?? 0
        let state = parts[5].lowercased()

        return NowPlayingPayload(
            title: parts[0],
            artist: parts[1],
            album: parts[2],
            source: "Music",
            appName: "Music.app",
            durationSec: duration,
            elapsedSec: elapsed,
            isPlaying: state == "playing",
            inSync: true,
            artworkURL: nil,
            artworkID: nil,
            artworkWidth: nil,
            artworkHeight: nil,
            artworkRGB565B64: nil,
            updatedAt: Date()
        )
    }
}

final class TidalNowPlayingProvider: NowPlayingProviding, @unchecked Sendable {
    private let fileManager = FileManager.default
    private let localStorageURL = FileManager.default.homeDirectoryForCurrentUser
        .appendingPathComponent("Library/Application Support/TIDAL/Local Storage/leveldb")
    private let indexedDBURL = FileManager.default.homeDirectoryForCurrentUser
        .appendingPathComponent("Library/Application Support/TIDAL/IndexedDB/https_desktop.tidal.com_0.indexeddb.leveldb")
    private let cacheDataURL = FileManager.default.homeDirectoryForCurrentUser
        .appendingPathComponent("Library/Application Support/TIDAL/Cache/Cache_Data")

    private var scanDeadline: TimeInterval = 0
    private var catalog: [URL] = []
    private var catalogAt: TimeInterval = -.infinity
    private var metadataCache: [String: TidalTrackMetadata] = [:]
    private var metadataMisses: [String: TimeInterval] = [:]
    private var cachedPayloadKey: String?
    private var cachedPayload: NowPlayingPayload?

    func snapshot() -> NowPlayingPayload? {
        scanDeadline = ProcessInfo.processInfo.systemUptime + 0.75
        let candidates = rankedPlaybackCandidates()
        guard let resolved = bestResolvedCandidate(from: candidates) else {
            NSLog("[ScryBar] TidalProvider: no resolved candidate (metadata lookup failed for all)")
            return nil
        }

        let candidate = resolved.candidate
        let metadata = resolved.metadata
        let cacheKey = "\(candidate.mediaID):\(candidate.timestampMs):\(candidate.confidence):\(candidate.isPlaying ?? true)"
        if cacheKey == cachedPayloadKey, let cachedPayload {
            return cachedPayload
        }

        let artworkURL = metadata.coverID.flatMap { coverURLString(for: $0, size: 640) }
        let payload = NowPlayingPayload(
            title: metadata.title,
            artist: metadata.artist,
            album: metadata.album,
            source: "TIDAL",
            appName: "TIDAL",
            durationSec: metadata.durationSec,
            elapsedSec: candidate.elapsedSec ?? 0,
            isPlaying: candidate.isPlaying ?? true,
            inSync: candidate.confidence >= 2 && isFresh(candidate, maxAgeMs: 300_000),
            artworkURL: artworkURL,
            artworkID: metadata.coverID,
            artworkWidth: nil,
            artworkHeight: nil,
            artworkRGB565B64: nil,
            updatedAt: Date(timeIntervalSince1970: TimeInterval(candidate.timestampMs) / 1000.0)
        )

        cachedPayloadKey = cacheKey
        cachedPayload = payload
        return payload
    }

    private func rankedPlaybackCandidates() -> [TidalPlaybackCandidate] {
        deduplicatedCandidates(
            indexedDBPlaybackCandidates()
                + queuePlaybackCandidates()
                + freshPlayerStates()
        )
        .sorted { lhs, rhs in
            if lhs.confidence != rhs.confidence {
                return lhs.confidence > rhs.confidence
            }
            return lhs.timestampMs > rhs.timestampMs
        }
    }

    private func bestResolvedCandidate(from candidates: [TidalPlaybackCandidate]) -> (candidate: TidalPlaybackCandidate, metadata: TidalTrackMetadata)? {
        for candidate in candidates.prefix(8) {
            guard ProcessInfo.processInfo.systemUptime < scanDeadline else { break }
            if let metadata = cachedTrackMetadata(for: candidate.mediaID) {
                return (candidate, metadata)
            }
        }
        return nil
    }

    private func freshPlayerStates(maxAgeMs: Int64 = 120_000) -> [TidalPlaybackCandidate] {
        let files = candidateFiles(in: localStorageURL, extensions: ["log", "ldb"])
        var candidates: [TidalPlaybackCandidate] = []
        let now = nowMs()

        for file in files {
            for state in playerStates(in: file) {
                guard state.mediaType.caseInsensitiveCompare("track") == .orderedSame else { continue }
                let age = now - state.setTime
                guard age <= maxAgeMs else { continue }
                candidates.append(
                    TidalPlaybackCandidate(
                        mediaID: state.mediaID,
                        timestampMs: state.setTime,
                        elapsedSec: state.mediaTimestamp,
                        isPlaying: state.currentlyPlaying,
                        confidence: state.currentlyPlaying ? 1 : 0,
                        origin: "local-storage"
                    )
                )
            }
        }

        return candidates
    }

    private func candidateFiles(in directory: URL, extensions: Set<String>) -> [URL] {
        recentFiles(in: directory) { extensions.contains($0.pathExtension.lowercased()) }
    }

    private func recentFiles(in directory: URL, matches: (URL) -> Bool) -> [URL] {
        guard let enumerator = fileManager.enumerator(at: directory,
            includingPropertiesForKeys: [.contentModificationDateKey],
            options: [.skipsHiddenFiles, .skipsSubdirectoryDescendants]) else { return [] }
        var files: [(URL, Date)] = []
        var visited = 0
        while let url = enumerator.nextObject() as? URL {
            visited += 1
            guard visited <= 20000, ProcessInfo.processInfo.systemUptime < scanDeadline else { break }
            guard matches(url) else { continue }
            files.append((url, modificationDate(for: url) ?? .distantPast))
        }
        return files.sorted { $0.1 > $1.1 }.map(\.0)
    }

    private func candidateCacheDataFiles(suffixes: Set<String> = ["_0", "_s"]) -> [URL] {
        let now = ProcessInfo.processInfo.systemUptime
        if now - catalogAt < 15 { return catalog }
        catalog = recentFiles(in: cacheDataURL) { url in
            suffixes.contains { url.lastPathComponent.hasSuffix($0) }
        }
        catalogAt = now
        return catalog
    }

    private func playerStates(in fileURL: URL) -> [TidalPlayerState] {
        guard let string = lossyString(from: fileURL) else { return [] }

        let pattern = #"\{"currentlyPlaying":(true|false),"mediaId":"(\d+)","mediaTimestamp":([0-9]+(?:\.[0-9]+)?),"mediaType":"([^"]+)","setTime":(\d+)\}"#
        guard let regex = try? NSRegularExpression(pattern: pattern) else { return [] }

        let nsString = string as NSString
        let range = NSRange(location: 0, length: nsString.length)

        return regex.matches(in: string, range: range).compactMap { match in
            guard match.numberOfRanges == 6 else { return nil }
            let currentlyPlaying = nsString.substring(with: match.range(at: 1)) == "true"
            let mediaID = nsString.substring(with: match.range(at: 2))
            let mediaTimestamp = Double(nsString.substring(with: match.range(at: 3))) ?? 0
            let mediaType = nsString.substring(with: match.range(at: 4))
            let setTime = Int64(nsString.substring(with: match.range(at: 5))) ?? 0

            return TidalPlayerState(
                currentlyPlaying: currentlyPlaying,
                mediaID: mediaID,
                mediaTimestamp: mediaTimestamp,
                mediaType: mediaType,
                setTime: setTime
            )
        }
    }

    private func indexedDBPlaybackCandidates(maxFiles: Int = 6, maxAgeMs: Int64 = 7_200_000) -> [TidalPlaybackCandidate] {
        let now = nowMs()
        let files = Array(candidateFiles(in: indexedDBURL, extensions: ["log", "ldb"]).prefix(maxFiles))
        var candidates: [TidalPlaybackCandidate] = []

        for file in files {
            guard let string = lossyString(from: file) else { continue }
            candidates.append(contentsOf: playbackCandidatesFromIndexedDBBlob(string))
        }

        return candidates.filter { now - $0.timestampMs <= maxAgeMs }
    }

    private func playbackCandidatesFromIndexedDBBlob(_ string: String) -> [TidalPlaybackCandidate] {
        let playbackSessionMatches = captureGroups(
            in: string,
            pattern: #""name":"playback_session".*?"actualProductId":"(\d+)".*?"startAssetPosition":([0-9]+(?:\.[0-9]+)?).*?"ts":(\d+)"#
        )

        let playbackSessionCandidates = playbackSessionMatches.compactMap { groups -> TidalPlaybackCandidate? in
            guard groups.count >= 3 else { return nil }
            return TidalPlaybackCandidate(
                mediaID: groups[0],
                timestampMs: Int64(groups[2]) ?? 0,
                elapsedSec: Double(groups[1]),
                isPlaying: true,
                confidence: 3,
                origin: "indexeddb-playback"
            )
        }

        let sessionStartMatches = captureGroups(
            in: string,
            pattern: #""name":"streaming_session_start".*?"sessionProductId":"(\d+)".*?"timestamp":(\d+)"#
        )

        let sessionStartCandidates = sessionStartMatches.compactMap { groups -> TidalPlaybackCandidate? in
            guard groups.count >= 2 else { return nil }
            return TidalPlaybackCandidate(
                mediaID: groups[0],
                timestampMs: Int64(groups[1]) ?? 0,
                elapsedSec: nil,
                isPlaying: true,
                confidence: 2,
                origin: "indexeddb-session"
            )
        }

        return playbackSessionCandidates + sessionStartCandidates
    }

    private func queuePlaybackCandidates(maxFiles: Int = 80, maxAgeMs: Int64 = 7_200_000) -> [TidalPlaybackCandidate] {
        let files = Array(candidateCacheDataFiles().prefix(maxFiles))
        var queueStateMap: [String: TidalQueueState] = [:]
        var queuePageList: [TidalQueuePage] = []

        for file in files {
            guard let string = lossyString(from: file) else { continue }
            let fileTimestampMs = modificationDate(for: file).map { Int64($0.timeIntervalSince1970 * 1000.0) } ?? nowMs()

            for state in extractQueueStates(from: string, defaultTimestampMs: fileTimestampMs) {
                if let existing = queueStateMap[state.queueID], existing.timestampMs >= state.timestampMs {
                    continue
                }
                queueStateMap[state.queueID] = state
            }

            queuePageList.append(contentsOf: extractQueuePages(from: string, defaultTimestampMs: fileTimestampMs))
        }

        return queuePageList.compactMap { page -> TidalPlaybackCandidate? in
            guard let state = queueStateMap[page.queueID] else { return nil }
            let relativeIndex = state.position - page.offset
            guard relativeIndex >= 0, relativeIndex < page.mediaIDs.count else { return nil }

            let timestampMs = max(state.timestampMs, page.timestampMs)
            guard nowMs() - timestampMs <= maxAgeMs else { return nil }

            return TidalPlaybackCandidate(
                mediaID: page.mediaIDs[relativeIndex],
                timestampMs: timestampMs,
                elapsedSec: nil,
                isPlaying: true,
                confidence: 2,
                origin: "queue-state"
            )
        }

    }

    private func extractQueueStates(from string: String, defaultTimestampMs: Int64) -> [TidalQueueState] {
        captureGroups(
            in: string,
            pattern: #"https://connectqueue\.tidal\.com/v1/queues/([0-9a-fA-F\-]+)\?[^{}]*\{"id":"[^"]+","repeat_mode":"[^"]+","shuffled":(?:true|false),"properties":\{"position":"(\d+)"\}"#
        ).compactMap { groups -> TidalQueueState? in
            guard groups.count >= 2 else { return nil }
            return TidalQueueState(
                queueID: groups[0],
                position: Int(groups[1]) ?? 0,
                timestampMs: defaultTimestampMs
            )
        }
    }

    private func extractQueuePages(from string: String, defaultTimestampMs: Int64) -> [TidalQueuePage] {
        guard let regex = try? NSRegularExpression(
            pattern: #"https://connectqueue\.tidal\.com/v1/queues/([0-9a-fA-F\-]+)/items\?[^{}]*\{"items":\[(.*?)\],"offset":(\d+),"limit":\d+,"total":\d+\}"#,
            options: [.dotMatchesLineSeparators]
        ) else {
            return []
        }

        let nsString = string as NSString
        let range = NSRange(location: 0, length: nsString.length)

        return regex.matches(in: string, range: range).compactMap { match in
            guard match.numberOfRanges == 4 else { return nil }
            let queueID = nsString.substring(with: match.range(at: 1))
            let itemsBlock = nsString.substring(with: match.range(at: 2))
            let offset = Int(nsString.substring(with: match.range(at: 3))) ?? 0
            let mediaIDs = captureGroups(in: itemsBlock, pattern: #""media_id":"(\d+)""#).compactMap(\.first)
            guard !mediaIDs.isEmpty else { return nil }
            return TidalQueuePage(queueID: queueID, offset: offset, mediaIDs: mediaIDs, timestampMs: defaultTimestampMs)
        }
    }

    private func cachedTrackMetadata(for mediaID: String) -> TidalTrackMetadata? {
        if let cached = metadataCache[mediaID] { return cached }
        let now = ProcessInfo.processInfo.systemUptime
        if let missed = metadataMisses[mediaID], now - missed < 15 { return nil }
        let files = candidateCacheDataFiles()
        for file in files.prefix(128) {
            guard ProcessInfo.processInfo.systemUptime < scanDeadline else { return nil }
            guard let string = lossyString(from: file) else { continue }
            guard string.contains("\"id\":\(mediaID),") || string.contains("/track/\(mediaID)") || string.contains("/v1/tracks/\(mediaID)") else {
                continue
            }

            if let metadata = extractTrackMetadata(mediaID: mediaID, from: string) {
                if metadataCache.count >= 64 { metadataCache.removeAll(keepingCapacity: true) }
                metadataCache[mediaID] = metadata
                return metadata
            }
        }
        if metadataMisses.count >= 64 { metadataMisses.removeAll(keepingCapacity: true) }
        metadataMisses[mediaID] = now
        return nil
    }

    private func extractTrackMetadata(mediaID: String, from string: String) -> TidalTrackMetadata? {
        let markers = [
            "\"id\":\(mediaID),\"title\":\"",
            "\"url\":\"http://www.tidal.com/track/\(mediaID)\"",
            "\"url\":\"https://tidal.com/browse/track/\(mediaID)\"",
        ]

        guard let markerRange = markers.lazy.compactMap({ string.range(of: $0) }).first else {
            return nil
        }

        let lowerBound = max(string.distance(from: string.startIndex, to: markerRange.lowerBound) - 512, 0)
        let upperBound = min(string.distance(from: string.startIndex, to: markerRange.upperBound) + 4096, string.count)
        let startIndex = string.index(string.startIndex, offsetBy: lowerBound)
        let endIndex = string.index(string.startIndex, offsetBy: upperBound)
        let snippet = String(string[startIndex..<endIndex])

        let titlePatterns = [
            "\"id\":\(mediaID),\"title\":\"([^\"]+)\"",
            "\"item\":\\{\"id\":\(mediaID),\"title\":\"([^\"]+)\"",
        ]

        guard let rawTitle = firstMatch(in: snippet, patterns: titlePatterns) else {
            return nil
        }

        let rawArtist = firstMatch(in: snippet, patterns: [
            "\"artist\":\\{\"id\":[0-9]+,\"name\":\"([^\"]+)\"",
            "\"artists\":\\[\\{\"id\":[0-9]+,\"name\":\"([^\"]+)\"",
        ]) ?? ""

        let rawAlbum = firstMatch(in: snippet, patterns: [
            "\"album\":\\{\"id\":[0-9]+,\"title\":\"([^\"]+)\"",
        ]) ?? ""
        let durationText = firstMatch(in: snippet, patterns: [
            "\"id\":\(mediaID),\"title\":\"[^\"]+\",\"duration\":([0-9]+)",
            "\"duration\":([0-9]+)",
        ]) ?? "0"
        let coverID = firstMatch(in: snippet, patterns: [
            "\"album\":\\{\"id\":[0-9]+,\"title\":\"[^\"]+\",\"cover\":\"([0-9a-fA-F\\-]+)\"",
        ])

        return TidalTrackMetadata(
            title: decodeJSONStringFragment(rawTitle) ?? rawTitle,
            artist: decodeJSONStringFragment(rawArtist) ?? rawArtist,
            album: decodeJSONStringFragment(rawAlbum) ?? rawAlbum,
            durationSec: Double(durationText) ?? 0,
            coverID: coverID
        )
    }

    private func lossyString(from fileURL: URL) -> String? {
        guard ProcessInfo.processInfo.systemUptime < scanDeadline,
              let handle = try? FileHandle(forReadingFrom: fileURL) else { return nil }
        defer { try? handle.close() }
        let limit: UInt64 = 512 * 1024
        guard let size = try? handle.seekToEnd() else { return nil }
        // LevelDB's recent state is appended; skip huge Chromium media bodies.
        if size > limit && fileURL.pathExtension.isEmpty { return nil }
        try? handle.seek(toOffset: size > limit ? size - limit : 0)
        guard let data = try? handle.read(upToCount: Int(limit)) else { return nil }
        return String(decoding: data, as: UTF8.self)
    }

    private func firstMatch(in string: String, pattern: String) -> String? {
        guard let regex = try? NSRegularExpression(pattern: pattern, options: [.dotMatchesLineSeparators]) else {
            return nil
        }

        let nsString = string as NSString
        let range = NSRange(location: 0, length: nsString.length)
        guard let match = regex.firstMatch(in: string, options: [], range: range), match.numberOfRanges >= 2 else {
            return nil
        }
        return nsString.substring(with: match.range(at: 1))
    }

    private func firstMatch(in string: String, patterns: [String]) -> String? {
        for pattern in patterns {
            if let match = firstMatch(in: string, pattern: pattern) {
                return match
            }
        }
        return nil
    }

    private func decodeJSONStringFragment(_ raw: String) -> String? {
        let wrapped = "\"\(raw)\""
        guard let data = wrapped.data(using: .utf8) else { return nil }
        return try? JSONDecoder().decode(String.self, from: data)
    }

    private func captureGroups(in string: String, pattern: String) -> [[String]] {
        guard let regex = try? NSRegularExpression(pattern: pattern, options: [.dotMatchesLineSeparators]) else {
            return []
        }

        let nsString = string as NSString
        let range = NSRange(location: 0, length: nsString.length)
        return regex.matches(in: string, range: range).map { match in
            (1..<match.numberOfRanges).compactMap { index in
                let groupRange = match.range(at: index)
                guard groupRange.location != NSNotFound else { return nil }
                return nsString.substring(with: groupRange)
            }
        }
    }

    private func modificationDate(for fileURL: URL) -> Date? {
        try? fileURL.resourceValues(forKeys: [.contentModificationDateKey]).contentModificationDate
    }

    private func deduplicatedCandidates(_ candidates: [TidalPlaybackCandidate]) -> [TidalPlaybackCandidate] {
        var bestByKey: [String: TidalPlaybackCandidate] = [:]
        for candidate in candidates where candidate.timestampMs > 0 {
            let key = "\(candidate.mediaID):\(candidate.origin)"
            if let existing = bestByKey[key] {
                if candidate.confidence > existing.confidence || candidate.timestampMs > existing.timestampMs {
                    bestByKey[key] = candidate
                }
            } else {
                bestByKey[key] = candidate
            }
        }
        return Array(bestByKey.values)
    }

    private func nowMs() -> Int64 {
        Int64(Date().timeIntervalSince1970 * 1000.0)
    }

    private func isFresh(_ candidate: TidalPlaybackCandidate, maxAgeMs: Int64) -> Bool {
        nowMs() - candidate.timestampMs <= maxAgeMs
    }

    private func coverURLString(for coverID: String, size: Int) -> String? {
        let parts = coverID.split(separator: "-")
        guard parts.count == 5 else { return nil }
        return "https://resources.tidal.com/images/\(parts[0])/\(parts[1])/\(parts[2])/\(parts[3])/\(parts[4])/\(size)x\(size).jpg"
    }
}

/// Uses JXA (JavaScript for Automation) via osascript to query MediaRemote.
/// This bypasses the macOS 15.4+ entitlement restriction on MRMediaRemoteGetNowPlayingInfo
/// by using the MRNowPlayingRequest Objective-C class through the JXA bridge.
final class SystemNowPlayingProvider: NowPlayingProviding, @unchecked Sendable {

    // Pause debounce: MediaRemote's isPlaying/playbackRate can flicker.
    // Require several consecutive "not playing" polls before treating as paused.
    private var consecutivePausedPolls: Int = 0
    private var stableIsPlaying: Bool = false
    private var lastTrackKey: String = ""
    private static let pauseDebounceCount = 3

    private let artworkCache: NowPlayingArtworkCache
    private let nativeMedia: NativeMediaRemote
    private let readSnapshot: @Sendable () -> Data?
    private(set) var suppressFallback = false

    init(artworkCache: NowPlayingArtworkCache? = nil, readSnapshot: (@Sendable () -> Data?)? = nil,
         nativeMedia: NativeMediaRemote = .shared) {
        let resolver = SystemArtworkResolver(nativeMedia: nativeMedia)
        self.nativeMedia = nativeMedia
        self.artworkCache = artworkCache ?? NowPlayingArtworkCache { resolver.resolve($0) }
        self.readSnapshot = readSnapshot ?? {
            if let snapshot = nativeMedia.snapshot() { return snapshot }
            if nativeMedia.ownsReader { return nil }
            return Self.readSystemSnapshot()
        }
    }

    // JXA script that loads MediaRemote.framework and reads now-playing via
    // MRNowPlayingRequest — an Obj-C class that osascript can access without
    // the entitlement that blocks the C function API on macOS 15.4+.
    // Computes real-time elapsed from: elapsedTime + (now - timestamp) * playbackRate,
    // because kMRMediaRemoteNowPlayingInfoElapsedTime alone is a stale snapshot.
    private static let jxaScript = """
    function run() {
      var MR = $.NSBundle.bundleWithPath("/System/Library/PrivateFrameworks/MediaRemote.framework/");
      MR.load;
      var Req = $.NSClassFromString("MRNowPlayingRequest");
      var client = Req.localNowPlayingPlayerPath.client;
      var ci = {};
      try { ci.bundleIdentifier = client.bundleIdentifier.js; } catch(e) {}
      try { ci.parentApplicationBundleIdentifier = client.parentApplicationBundleIdentifier.js; } catch(e) {}
      try { ci.processIdentifier = client.processIdentifier; } catch(e) {}
      var dict = Req.localNowPlayingItem.nowPlayingInfo;
      var info = {};
      var keys = dict.allKeys;
      for (var i = 0; i < keys.count; i++) {
        var k = keys.objectAtIndex(i).js;
        var v = dict.valueForKey(keys.objectAtIndex(i));
        if (k === "kMRMediaRemoteNowPlayingInfoArtworkData") continue;
        try { info[k] = v.js; } catch(e) { try { info[k] = v.toString(); } catch(e2) {} }
      }
      var rawElapsed = 0;
      try { rawElapsed = dict.valueForKey("kMRMediaRemoteNowPlayingInfoElapsedTime").doubleValue; } catch(e) {}
      var rate = 0;
      try { rate = dict.valueForKey("kMRMediaRemoteNowPlayingInfoPlaybackRate").doubleValue; } catch(e) {}
      var computedElapsed = rawElapsed;
      var ts = dict.valueForKey("kMRMediaRemoteNowPlayingInfoTimestamp");
      if (ts) {
        try { computedElapsed = rawElapsed + $.NSDate.date.timeIntervalSinceDate(ts) * rate; } catch(e) {}
      }
      return JSON.stringify({ isPlaying: Req.localIsPlaying, client: ci, info: info, computedElapsed: computedElapsed });
    }
    """

    private static func readSystemSnapshot() -> Data? {
        let process = Process()
        process.executableURL = URL(fileURLWithPath: "/usr/bin/osascript")
        process.arguments = ["-l", "JavaScript", "-e", Self.jxaScript]

        let pipe = Pipe()
        process.standardOutput = pipe
        process.standardError = FileHandle.nullDevice
        let didExit = DispatchSemaphore(value: 0)
        process.terminationHandler = { _ in didExit.signal() }

        do {
            try process.run()
        } catch {
            NSLog("[ScryBar] SystemProvider: osascript launch failed: %@", error.localizedDescription)
            return nil
        }
        if didExit.wait(timeout: .now() + 3) == .timedOut {
            process.terminate()
            _ = didExit.wait(timeout: .now() + 1)
            NSLog("[ScryBar] SystemProvider: osascript timed out")
            return nil
        }

        let data = pipe.fileHandleForReading.readDataToEndOfFile()
        guard !data.isEmpty else {
            NSLog("[ScryBar] SystemProvider: no output from osascript")
            return nil
        }

        return data
    }

    func snapshot() -> NowPlayingPayload? {
        suppressFallback = false
        guard let data = readSnapshot(),
              let json = try? JSONSerialization.jsonObject(with: data) as? [String: Any] else {
            suppressFallback = nativeMedia.ownsReader
            return nil
        }
        let info = json["info"] as? [String: Any] ?? [:]
        let client = json["client"] as? [String: Any] ?? [:]
        let isPlaying = json["isPlaying"] as? Bool ?? false

        let bundleIdentifier = client["bundleIdentifier"] as? String
        let appName = bundleIdentifier ?? "System"

        // Privacy first: a source outside the policy (video players, browsers, unknown apps) stops here,
        // before artwork lookup, caching or logging. Only the category is remembered, never the title.
        let policy = SourcePolicy.current
        guard policy.allows(bundleID: bundleIdentifier) else {
            suppressFallback = true
            SourcePolicyLog.shared.lastHidden = SourcePolicy.category(of: bundleIdentifier)
            return nil
        }
        SourcePolicyLog.shared.lastHidden = nil
        guard let title = info["kMRMediaRemoteNowPlayingInfoTitle"] as? String, !title.isEmpty else {
            return nil
        }
        let artist = info["kMRMediaRemoteNowPlayingInfoArtist"] as? String ?? ""
        let album = info["kMRMediaRemoteNowPlayingInfoAlbum"] as? String ?? ""
        let durationSec = (info["kMRMediaRemoteNowPlayingInfoDuration"] as? Double) ?? 0
        let playbackRate = (info["kMRMediaRemoteNowPlayingInfoPlaybackRate"] as? Double) ?? 0

        // Real-time elapsed computed in JXA from: elapsedTime + (now - timestamp) * rate
        var elapsedSec = (json["computedElapsed"] as? Double) ?? 0
        if elapsedSec < 0 { elapsedSec = 0 }
        if durationSec > 0 && elapsedSec > durationSec { elapsedSec = durationSec }

        let source: String
        if let bid = bundleIdentifier {
            source = NSWorkspace.shared.runningApplications
                .first(where: { $0.bundleIdentifier == bid })?
                .localizedName ?? bid
        } else {
            source = "System"
        }

        // artworkIdentifier may be a direct URL, a template with {w}/{h}/{f} placeholders,
        // or an opaque hash (TIDAL). Resolve to a concrete URL when possible.
        let artworkID = info["kMRMediaRemoteNowPlayingInfoArtworkIdentifier"] as? String
        let artworkURL: String? = {
            guard var id = artworkID else { return nil }
            // Expand Apple template placeholders: {w}x{h}bb.{f}
            if id.contains("{w}") || id.contains("{h}") || id.contains("{f}") {
                id = id.replacingOccurrences(of: "{w}", with: "600")
                id = id.replacingOccurrences(of: "{h}", with: "600")
                id = id.replacingOccurrences(of: "{f}", with: "jpg")
            }
            guard let url = URL(string: id),
                  let scheme = url.scheme?.lowercased(),
                  scheme == "http" || scheme == "https" else { return nil }
            return id
        }()

        // Transcode artwork to RGB565 for the device display.
        // Cache by artworkID to avoid re-downloading every poll cycle.
        var artworkRequest = NowPlayingArtworkRequest(
            artworkURL: artworkURL, artworkID: artworkID, title: title, artist: artist,
            album: album, tidal: bundleIdentifier == "com.tidal.desktop",
            bundleIdentifier: bundleIdentifier,
            parentBundleIdentifier: client["parentApplicationBundleIdentifier"] as? String,
            processIdentifier: client["processIdentifier"] as? Int,
            contentItemIdentifier: info["kMRMediaRemoteNowPlayingInfoContentItemIdentifier"] as? String,
            uniqueIdentifier: info["kMRMediaRemoteNowPlayingInfoUniqueIdentifier"] as? String
        )
        artworkRequest.nativeArtworkRevision = nativeMedia.artworkRevision(for: artworkRequest)
        let artwork = artworkCache.value(for: artworkRequest)

        // Debounce isPlaying: MediaRemote can flicker, so require consecutive paused polls.
        let actuallyPlaying = isPlaying || playbackRate > 0.001
        let trackKey = "\(title)|\(artist)"

        if trackKey != lastTrackKey {
            consecutivePausedPolls = 0
            stableIsPlaying = actuallyPlaying
            lastTrackKey = trackKey
        } else if actuallyPlaying {
            consecutivePausedPolls = 0
            stableIsPlaying = true
        } else {
            consecutivePausedPolls += 1
            if consecutivePausedPolls >= Self.pauseDebounceCount {
                stableIsPlaying = false
            }
        }

        return NowPlayingPayload(
            title: title,
            artist: artist,
            album: album,
            source: source,
            appName: appName,
            durationSec: durationSec,
            elapsedSec: elapsedSec,
            isPlaying: stableIsPlaying,
            inSync: true,
            artworkURL: artworkURL,
            artworkID: artworkID,
            artworkWidth: artwork?.width,
            artworkHeight: artwork?.height,
            artworkRGB565B64: artwork?.rgb565,
            updatedAt: Date(),
            artworkRGB565HD: artwork?.hd,
            webArtworkURL: artwork?.webURL,
            artworkFingerprint: artwork?.fingerprint
        )
    }
}

private final class SystemArtworkResolver: @unchecked Sendable {
    private let nativeMedia: NativeMediaRemote
    private let enrichment = VerifiedArtworkEnrichment()
    private var nativeFingerprint: String?
    init(nativeMedia: NativeMediaRemote) { self.nativeMedia = nativeMedia }
    // Cached artwork transcoding — only re-download when artworkID changes
    private let tidalArtworkProvider = TidalNowPlayingProvider()
    private var cachedArtworkHD: Data?
    private var cachedArtworkID: String?
    private var cachedArtworkResult: (width: Int, height: Int, rgb565Base64: String)?
    // Public https URL of the image actually used (TIDAL edition or iTunes fallback), for web displays.
    private var cachedArtworkWebURL: String?

    private func resolveArtworkRGB565(
        artworkURL: String?,
        artworkID: String?,
        title: String,
        artist: String,
        album: String,
        tidal: Bool
    ) -> (Int?, Int?, String?) {
        // Return cached result if artwork hasn't changed
        let effectiveID = "\(artworkID ?? artworkURL ?? "")|\(title)|\(artist)|\(album)"
        if effectiveID == cachedArtworkID, let cached = cachedArtworkResult {
            return (cached.width, cached.height, cached.rgb565Base64)
        }
        cachedArtworkWebURL = nil

        // Match the actual TIDAL edition when its system artwork ID is opaque.
        var resolvedURL = artworkURL
        if tidal, let track = tidalArtworkProvider.snapshot(),
           track.title.caseInsensitiveCompare(title) == .orderedSame,
           track.artist.caseInsensitiveCompare(artist) == .orderedSame,
           album.isEmpty || track.album.isEmpty || track.album.caseInsensitiveCompare(album) == .orderedSame {
            resolvedURL = track.artworkURL ?? resolvedURL
        }
        // Prefer a 640 px source for the 512 px Meguru variant.
        if let data = downloadImageSync(from: ArtworkTranscoder.preferredURL(resolvedURL)) {
            if let result = ArtworkTranscoder.encodeRGB565Base64(from: data) {
                cachedArtworkWebURL = ArtworkTranscoder.preferredURL(resolvedURL)
                cachedArtworkID = effectiveID
                cachedArtworkResult = result
                cachedArtworkHD = ArtworkTranscoder.encodeRGB565Base64(from: data, size: 512)
                    .flatMap { Data(base64Encoded: $0.rgb565Base64) }
                NSLog("[ScryBar] SystemProvider: artwork transcoded %dx%d from URL", result.width, result.height)
                return (result.width, result.height, result.rgb565Base64)
            }
        }

        // Fallback: iTunes Search API
        let query = "\(artist) \(title)".trimmingCharacters(in: .whitespaces)
        if !query.isEmpty,
           let encoded = query.addingPercentEncoding(withAllowedCharacters: .urlQueryAllowed),
           let itunesURL = iTunesArtworkURL(query: encoded),
           let data = downloadImageSync(from: itunesURL) {
            if let result = ArtworkTranscoder.encodeRGB565Base64(from: data) {
                cachedArtworkWebURL = itunesURL
                cachedArtworkID = effectiveID
                cachedArtworkResult = result
                cachedArtworkHD = ArtworkTranscoder.encodeRGB565Base64(from: data, size: 512)
                    .flatMap { Data(base64Encoded: $0.rgb565Base64) }
                NSLog("[ScryBar] SystemProvider: artwork transcoded %dx%d from iTunes", result.width, result.height)
                return (result.width, result.height, result.rgb565Base64)
            }
        }

        cachedArtworkID = effectiveID
        cachedArtworkResult = nil
        cachedArtworkHD = nil
        return (nil, nil, nil)
    }

    private func downloadImageSync(from urlString: String?) -> Data? {
        guard let urlString, let url = URL(string: urlString),
              let scheme = url.scheme?.lowercased(),
              scheme == "http" || scheme == "https" else { return nil }
        let sem = DispatchSemaphore(value: 0)
        let result = BlockingResult<Data>()
        URLSession.shared.dataTask(with: url) { data, _, _ in
            result.value = data
            sem.signal()
        }.resume()
        _ = sem.wait(timeout: .now() + 5)
        return result.value
    }

    private func iTunesArtworkURL(query: String) -> String? {
        guard let searchURL = URL(string: "https://itunes.apple.com/search?term=\(query)&media=music&limit=1") else {
            return nil
        }
        let sem = DispatchSemaphore(value: 0)
        let artURL = BlockingResult<String>()
        URLSession.shared.dataTask(with: searchURL) { data, _, _ in
            defer { sem.signal() }
            guard let data,
                  let json = try? JSONSerialization.jsonObject(with: data) as? [String: Any],
                  let results = json["results"] as? [[String: Any]],
                  let first = results.first,
                  let url100 = first["artworkUrl100"] as? String else { return }
            artURL.value = url100.replacingOccurrences(of: "100x100", with: "600x600")
        }.resume()
        _ = sem.wait(timeout: .now() + 5)
        return artURL.value
    }


    func resolve(_ request: NowPlayingArtworkRequest) -> NowPlayingArtwork? {
        if let bytes = nativeMedia.artwork(for: request) {
            let signature = ArtworkTranscoder.fingerprint(for: bytes)
            let identity = "\(request.bundleIdentifier ?? "")|\(request.processIdentifier ?? 0)|\(request.contentItemIdentifier ?? "")|\(request.artworkID ?? "")|\(request.title)|\(request.artist)|\(request.album)"
            if signature != nativeFingerprint || cachedArtworkID != identity {
                guard let encoded = NativeArtworkEncoding.encode(bytes) else { return nil }
                nativeFingerprint = signature; cachedArtworkID = identity
                cachedArtworkResult = (encoded.width, encoded.height, encoded.rgb565)
                cachedArtworkHD = encoded.hd; cachedArtworkWebURL = request.artworkURL
            }
            if let cached = cachedArtworkResult {
                let verified = enrichment.value(for: request, nativeRGB565: cached.rgb565Base64)
                return .init(width: cached.width, height: cached.height, rgb565: cached.rgb565Base64,
                             hd: cachedArtworkHD ?? verified?.hd, webURL: cachedArtworkWebURL ?? verified?.webURL)
            }
        }
        // With a healthy native stream, wait for this exact player's artwork.
        // A catalog hit for a similarly named track is not an album substitute.
        if nativeMedia.running { return nil }
        let (width, height, pixels) = resolveArtworkRGB565(
            artworkURL: request.artworkURL, artworkID: request.artworkID,
            title: request.title, artist: request.artist, album: request.album, tidal: request.tidal
        )
        guard let width, let height, let pixels else { return nil }
        return .init(width: width, height: height, rgb565: pixels,
                     hd: cachedArtworkHD, webURL: cachedArtworkWebURL)
    }
}

private struct EncodedArtwork {
    let identifier: String
    let debugURL: String?
    let width: Int
    let height: Int
    let rgb565Base64: String
}

private struct TidalPlayerState {
    let currentlyPlaying: Bool
    let mediaID: String
    let mediaTimestamp: Double
    let mediaType: String
    let setTime: Int64
}

private struct TidalPlaybackCandidate {
    let mediaID: String
    let timestampMs: Int64
    let elapsedSec: Double?
    let isPlaying: Bool?
    let confidence: Int
    let origin: String
}

private struct TidalQueueState {
    let queueID: String
    let position: Int
    let timestampMs: Int64
}

private struct TidalQueuePage {
    let queueID: String
    let offset: Int
    let mediaIDs: [String]
    let timestampMs: Int64
}

private struct TidalTrackMetadata {
    let title: String
    let artist: String
    let album: String
    let durationSec: Double
    let coverID: String?
}

enum ArtworkTranscoder {
    static let targetSize = 150

    static func preferredURL(_ original: String?) -> String? {
        guard let original, var url = URLComponents(string: original),
              url.scheme == "https", url.host == "resources.tidal.com",
              url.path.hasPrefix("/images/") else { return original }
        // Only upgrade the known TIDAL image-size suffix, preserving album ID.
        url.path = url.path.replacingOccurrences(of: #"/[0-9]+x[0-9]+\.jpg$"#,
                                                 with: "/640x640.jpg", options: .regularExpression)
        return url.string ?? original
    }

    static func encodeRGB565Base64(from data: Data, size targetSize: Int = 150) -> (width: Int, height: Int, rgb565Base64: String)? {
        guard [150, 512].contains(targetSize) else { return nil }
        guard let image = NSImage(data: data) else { return nil }
        var proposedRect = CGRect(origin: .zero, size: image.size)
        guard let cgImage = image.cgImage(forProposedRect: &proposedRect, context: nil, hints: nil) else {
            return nil
        }

        let sourceWidth = cgImage.width
        let sourceHeight = cgImage.height
        let edge = min(sourceWidth, sourceHeight)
        // A 512-pixel payload represents source detail, never an enlarged thumbnail.
        guard targetSize != 512 || edge >= 512 else { return nil }
        let cropRect = CGRect(
            x: (sourceWidth - edge) / 2,
            y: (sourceHeight - edge) / 2,
            width: edge,
            height: edge
        )
        guard let croppedImage = cgImage.cropping(to: cropRect) else { return nil }

        let bytesPerPixel = 4
        let bytesPerRow = targetSize * bytesPerPixel
        var rgbaPixels = [UInt8](repeating: 0, count: targetSize * targetSize * bytesPerPixel)
        let colorSpace = CGColorSpace(name: CGColorSpace.sRGB) ?? CGColorSpaceCreateDeviceRGB()
        // byteOrder32Little|premultipliedFirst = native BGRA on Apple Silicon.
        // byteOrder32Big|premultipliedLast (RGBA) is not hardware-accelerated; CG renders
        // BGRA regardless, so reading as RGBA swaps R↔B and zeros G high bits for warm colors.
        let bitmapInfo = CGBitmapInfo.byteOrder32Little.rawValue | CGImageAlphaInfo.premultipliedFirst.rawValue
        guard let context = CGContext(
            data: &rgbaPixels,
            width: targetSize,
            height: targetSize,
            bitsPerComponent: 8,
            bytesPerRow: bytesPerRow,
            space: colorSpace,
            bitmapInfo: bitmapInfo
        ) else {
            return nil
        }

        context.interpolationQuality = .high
        context.draw(croppedImage, in: CGRect(x: 0, y: 0, width: targetSize, height: targetSize))

        var rgb565 = Data(count: targetSize * targetSize * 2)
        rgb565.withUnsafeMutableBytes { destinationBytes in
            guard let destination = destinationBytes.bindMemory(to: UInt8.self).baseAddress else { return }
            for pixelIndex in 0..<(targetSize * targetSize) {
                let sourceOffset = pixelIndex * bytesPerPixel
                // BGRA layout: byte[0]=B, byte[1]=G, byte[2]=R, byte[3]=A
                let b = rgbaPixels[sourceOffset]
                let g = rgbaPixels[sourceOffset + 1]
                let r = rgbaPixels[sourceOffset + 2]
                let packed = (UInt16(r >> 3) << 11) | (UInt16(g >> 2) << 5) | UInt16(b >> 3)
                let destinationOffset = pixelIndex * 2
                destination[destinationOffset] = UInt8((packed >> 8) & 0xFF)
                destination[destinationOffset + 1] = UInt8(packed & 0xFF)
            }
        }

        return (targetSize, targetSize, rgb565.base64EncodedString())
    }

    static func fingerprint(for data: Data) -> String {
        var hash: UInt64 = 1469598103934665603
        for byte in data {
            hash ^= UInt64(byte)
            hash &*= 1099511628211
        }
        return String(hash, radix: 16)
    }
}

// MediaRemoteBridge removed — macOS 15.4+ blocks MRMediaRemoteGetNowPlayingInfo
// for unsigned apps. SystemNowPlayingProvider now uses JXA via osascript which
// accesses MRNowPlayingRequest (Obj-C class) through the entitled osascript binary.
