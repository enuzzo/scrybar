import AppKit
import Dispatch
import Foundation
import Darwin

protocol NowPlayingProviding {
    func snapshot() -> NowPlayingPayload?
}

final class FallbackNowPlayingProvider: NowPlayingProviding {
    private let providers: [any NowPlayingProviding]

    init(_ providers: [any NowPlayingProviding]) {
        self.providers = providers
    }

    func snapshot() -> NowPlayingPayload? {
        for provider in providers {
            if let payload = provider.snapshot() {
                return payload
            }
        }
        return nil
    }
}

final class MockNowPlayingProvider: NowPlayingProviding {
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
            updatedAt: Date()
        )
    }
}

final class MusicNowPlayingProvider: NowPlayingProviding {
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

final class TidalNowPlayingProvider: NowPlayingProviding {
    private let fileManager = FileManager.default
    private let localStorageURL = FileManager.default.homeDirectoryForCurrentUser
        .appendingPathComponent("Library/Application Support/TIDAL/Local Storage/leveldb")
    private let indexedDBURL = FileManager.default.homeDirectoryForCurrentUser
        .appendingPathComponent("Library/Application Support/TIDAL/IndexedDB/https_desktop.tidal.com_0.indexeddb.leveldb")
    private let cacheDataURL = FileManager.default.homeDirectoryForCurrentUser
        .appendingPathComponent("Library/Application Support/TIDAL/Cache/Cache_Data")

    private var cachedPayloadKey: String?
    private var cachedPayload: NowPlayingPayload?

    func snapshot() -> NowPlayingPayload? {
        guard let resolved = bestResolvedCandidate(from: rankedPlaybackCandidates()) else {
            return nil
        }

        let candidate = resolved.candidate
        let metadata = resolved.metadata
        let cacheKey = "\(candidate.mediaID):\(candidate.timestampMs):\(candidate.confidence):\(candidate.isPlaying ?? true)"
        if cacheKey == cachedPayloadKey, let cachedPayload {
            return cachedPayload
        }

        let artworkURL = metadata.coverID.flatMap { coverURLString(for: $0, size: 320) }
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
        for candidate in candidates {
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
        guard let enumerator = fileManager.enumerator(
            at: directory,
            includingPropertiesForKeys: [.contentModificationDateKey, .isRegularFileKey],
            options: [.skipsHiddenFiles]
        ) else {
            return []
        }

        return enumerator.compactMap { item in
            guard let url = item as? URL else { return nil }
            guard extensions.contains(url.pathExtension.lowercased()) else { return nil }
            return url
        }
        .sorted { lhs, rhs in
            let lhsDate = (try? lhs.resourceValues(forKeys: [.contentModificationDateKey]).contentModificationDate) ?? .distantPast
            let rhsDate = (try? rhs.resourceValues(forKeys: [.contentModificationDateKey]).contentModificationDate) ?? .distantPast
            return lhsDate > rhsDate
        }
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
        let files = Array(candidateFiles(in: cacheDataURL, extensions: ["0", "s"]).prefix(maxFiles))
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
        let files = candidateFiles(in: cacheDataURL, extensions: ["0", "s"])
        for file in files {
            guard let string = lossyString(from: file) else { continue }
            guard string.contains("\"id\":\(mediaID),") || string.contains("/track/\(mediaID)") || string.contains("/v1/tracks/\(mediaID)") else {
                continue
            }

            if let metadata = extractTrackMetadata(mediaID: mediaID, from: string) {
                return metadata
            }
        }
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
        guard let data = try? Data(contentsOf: fileURL, options: [.mappedIfSafe]) else {
            return nil
        }
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

final class SystemNowPlayingProvider: NowPlayingProviding {
    private let bridge = MediaRemoteBridge()
    private var cachedArtworkCacheKey: String?
    private var cachedArtwork: EncodedArtwork?

    func snapshot() -> NowPlayingPayload? {
        guard let snapshot = bridge.snapshot() else { return nil }

        let info = snapshot.info
        guard let title = mediaRemoteString(info[MediaRemoteBridge.titleKey]), !title.isEmpty else {
            return nil
        }

        let artist = mediaRemoteString(info[MediaRemoteBridge.artistKey]) ?? ""
        let album = mediaRemoteString(info[MediaRemoteBridge.albumKey]) ?? ""
        let durationSec = mediaRemoteDouble(info[MediaRemoteBridge.durationKey]) ?? 0
        let elapsedSec = mediaRemoteDouble(info[MediaRemoteBridge.elapsedKey]) ?? 0
        let playbackRate = mediaRemoteDouble(info[MediaRemoteBridge.playbackRateKey]) ?? 0
        let clientDisplayName = mediaRemoteString(snapshot.client["displayName"])
        let bundleIdentifier = mediaRemoteString(snapshot.client["bundleIdentifier"])
        let appName = clientDisplayName ?? bundleIdentifier ?? "System"
        let source = clientDisplayName ?? runningAppName(from: snapshot.client) ?? bundleIdentifier ?? "System"
        let encodedArtwork = materializeArtwork(from: info)

        return NowPlayingPayload(
            title: title,
            artist: artist,
            album: album,
            source: source,
            appName: appName,
            durationSec: durationSec,
            elapsedSec: elapsedSec,
            isPlaying: playbackRate > 0.001 || snapshot.playbackState == 1,
            inSync: true,
            artworkURL: encodedArtwork?.debugURL,
            artworkID: encodedArtwork?.identifier,
            artworkWidth: encodedArtwork?.width,
            artworkHeight: encodedArtwork?.height,
            artworkRGB565B64: encodedArtwork?.rgb565Base64,
            updatedAt: Date()
        )
    }

    private func runningAppName(from client: [String: Any]) -> String? {
        guard let pidNumber = mediaRemoteNumber(client["processIdentifier"]) else { return nil }
        return NSRunningApplication(processIdentifier: pid_t(truncating: pidNumber))?.localizedName
    }

    private func materializeArtwork(from info: [String: Any]) -> EncodedArtwork? {
        guard let artworkData = info[MediaRemoteBridge.artworkDataKey] as? Data,
              !artworkData.isEmpty else {
            cachedArtworkCacheKey = nil
            cachedArtwork = nil
            return nil
        }

        let identifier = sanitizedArtworkIdentifier(
            mediaRemoteString(info[MediaRemoteBridge.artworkIdentifierKey]) ?? ArtworkTranscoder.fingerprint(for: artworkData)
        )
        let mimeType = mediaRemoteString(info[MediaRemoteBridge.artworkMIMETypeKey]) ?? "image/jpeg"
        let cacheKey = "\(identifier):\(artworkData.count)"
        if cacheKey == cachedArtworkCacheKey, let cachedArtwork {
            return cachedArtwork
        }

        let debugURL = materializeArtworkFile(identifier: identifier, mimeType: mimeType, artworkData: artworkData)
        guard let encoded = ArtworkTranscoder.encodeRGB565Base64(from: artworkData) else {
            return nil
        }

        let artwork = EncodedArtwork(
            identifier: identifier,
            debugURL: debugURL,
            width: encoded.width,
            height: encoded.height,
            rgb565Base64: encoded.rgb565Base64
        )
        cachedArtworkCacheKey = cacheKey
        cachedArtwork = artwork
        return artwork
    }

    private func materializeArtworkFile(identifier: String, mimeType: String, artworkData: Data) -> String? {
        let fileExtension: String
        switch mimeType.lowercased() {
        case "image/png":
            fileExtension = "png"
        case "image/webp":
            fileExtension = "webp"
        default:
            fileExtension = "jpg"
        }

        let url = FileManager.default.temporaryDirectory
            .appendingPathComponent("scrybar-artwork-\(identifier)")
            .appendingPathExtension(fileExtension)

        if !FileManager.default.fileExists(atPath: url.path) {
            do {
                try artworkData.write(to: url, options: [.atomic])
            } catch {
                NSLog("SystemNowPlayingProvider artwork write failed: %@", error.localizedDescription)
                return nil
            }
        }
        return url.absoluteString
    }

    private func sanitizedArtworkIdentifier(_ raw: String) -> String {
        raw
            .replacingOccurrences(of: "/", with: "-")
            .replacingOccurrences(of: ":", with: "-")
            .replacingOccurrences(of: " ", with: "-")
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

private enum ArtworkTranscoder {
    static let targetSize = 150

    static func encodeRGB565Base64(from data: Data) -> (width: Int, height: Int, rgb565Base64: String)? {
        guard let image = NSImage(data: data) else { return nil }
        var proposedRect = CGRect(origin: .zero, size: image.size)
        guard let cgImage = image.cgImage(forProposedRect: &proposedRect, context: nil, hints: nil) else {
            return nil
        }

        let sourceWidth = cgImage.width
        let sourceHeight = cgImage.height
        let edge = min(sourceWidth, sourceHeight)
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
        let bitmapInfo = CGBitmapInfo.byteOrder32Big.rawValue | CGImageAlphaInfo.premultipliedLast.rawValue
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
                let r = rgbaPixels[sourceOffset]
                let g = rgbaPixels[sourceOffset + 1]
                let b = rgbaPixels[sourceOffset + 2]
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

private final class MediaRemoteBridge {
    static let titleKey = "kMRMediaRemoteNowPlayingInfoTitle"
    static let artistKey = "kMRMediaRemoteNowPlayingInfoArtist"
    static let albumKey = "kMRMediaRemoteNowPlayingInfoAlbum"
    static let durationKey = "kMRMediaRemoteNowPlayingInfoDuration"
    static let elapsedKey = "kMRMediaRemoteNowPlayingInfoElapsedTime"
    static let playbackRateKey = "kMRMediaRemoteNowPlayingInfoPlaybackRate"
    static let artworkDataKey = "kMRMediaRemoteNowPlayingInfoArtworkData"
    static let artworkMIMETypeKey = "kMRMediaRemoteNowPlayingInfoArtworkMIMEType"
    static let artworkIdentifierKey = "kMRMediaRemoteNowPlayingInfoArtworkIdentifier"

    private typealias GetNowPlayingInfoFn = @convention(c) (DispatchQueue, @escaping @convention(block) (CFDictionary?) -> Void) -> Void
    private typealias GetPlaybackStateFn = @convention(c) (DispatchQueue, @escaping @convention(block) (Int) -> Void) -> Void
    private typealias GetClientFn = @convention(c) (DispatchQueue, @escaping @convention(block) (AnyObject?) -> Void) -> Void

    struct Snapshot {
        var info: [String: Any]
        var client: [String: Any]
        var playbackState: Int
    }

    private let queue = DispatchQueue(label: "ScryBar.MediaRemote")
    private let frameworkHandle: UnsafeMutableRawPointer?
    private let getNowPlayingInfoFn: GetNowPlayingInfoFn?
    private let getPlaybackStateFn: GetPlaybackStateFn?
    private let getClientFn: GetClientFn?

    init() {
        let frameworkPath = "/System/Library/PrivateFrameworks/MediaRemote.framework/MediaRemote"
        frameworkHandle = dlopen(frameworkPath, RTLD_NOW)

        if frameworkHandle == nil {
            if let err = dlerror() {
                NSLog("MediaRemoteBridge dlopen failed: %@", String(cString: err))
            }
            getNowPlayingInfoFn = nil
            getPlaybackStateFn = nil
            getClientFn = nil
            return
        }

        getNowPlayingInfoFn = MediaRemoteBridge.resolve("MRMediaRemoteGetNowPlayingInfo", from: frameworkHandle)
        getPlaybackStateFn = MediaRemoteBridge.resolve("MRMediaRemoteGetNowPlayingApplicationPlaybackState", from: frameworkHandle)
        getClientFn = MediaRemoteBridge.resolve("MRMediaRemoteGetNowPlayingClient", from: frameworkHandle)
    }

    deinit {
        if let frameworkHandle {
            dlclose(frameworkHandle)
        }
    }

    func snapshot(timeout: TimeInterval = 0.35) -> Snapshot? {
        guard let getNowPlayingInfoFn else { return nil }

        let waitDeadline = DispatchTime.now() + timeout
        var info: [String: Any] = [:]
        var playbackState = 0
        var client: [String: Any] = [:]

        let infoSemaphore = DispatchSemaphore(value: 0)
        getNowPlayingInfoFn(queue) { dictionary in
            if let dictionary = dictionary as? [String: Any] {
                info = dictionary
            }
            infoSemaphore.signal()
        }
        guard infoSemaphore.wait(timeout: waitDeadline) == .success else { return nil }

        if let getPlaybackStateFn {
            let stateSemaphore = DispatchSemaphore(value: 0)
            getPlaybackStateFn(queue) { state in
                playbackState = state
                stateSemaphore.signal()
            }
            _ = stateSemaphore.wait(timeout: waitDeadline)
        }

        if let getClientFn {
            let clientSemaphore = DispatchSemaphore(value: 0)
            getClientFn(queue) { object in
                if let dictionary = object as? [String: Any] {
                    client = dictionary
                } else if let dictionary = object as? NSDictionary as? [String: Any] {
                    client = dictionary
                }
                clientSemaphore.signal()
            }
            _ = clientSemaphore.wait(timeout: waitDeadline)
        }

        if info.isEmpty { return nil }
        return Snapshot(info: info, client: client, playbackState: playbackState)
    }

    private static func resolve<T>(_ symbol: String, from handle: UnsafeMutableRawPointer?) -> T? {
        guard let handle, let raw = dlsym(handle, symbol) else { return nil }
        return unsafeBitCast(raw, to: T.self)
    }
}

private func mediaRemoteString(_ raw: Any?) -> String? {
    switch raw {
    case let string as String:
        return string
    case let number as NSNumber:
        return number.stringValue
    case let date as Date:
        return ISO8601DateFormatter().string(from: date)
    default:
        return nil
    }
}

private func mediaRemoteDouble(_ raw: Any?) -> Double? {
    switch raw {
    case let number as NSNumber:
        return number.doubleValue
    case let string as String:
        return Double(string)
    default:
        return nil
    }
}

private func mediaRemoteNumber(_ raw: Any?) -> NSNumber? {
    switch raw {
    case let number as NSNumber:
        return number
    case let string as String:
        guard let intValue = Int(string) else { return nil }
        return NSNumber(value: intValue)
    default:
        return nil
    }
}
