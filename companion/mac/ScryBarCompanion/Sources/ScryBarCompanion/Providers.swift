import AppKit
import Dispatch
import Foundation
import Darwin

protocol NowPlayingProviding {
    func snapshot() -> NowPlayingPayload?
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
            updatedAt: Date()
        )
    }
}

final class SystemNowPlayingProvider: NowPlayingProviding {
    private let bridge = MediaRemoteBridge()

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
        let artworkURL = materializeArtworkFile(from: info)

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
            artworkURL: artworkURL,
            updatedAt: Date()
        )
    }

    private func runningAppName(from client: [String: Any]) -> String? {
        guard let pidNumber = mediaRemoteNumber(client["processIdentifier"]) else { return nil }
        return NSRunningApplication(processIdentifier: pid_t(truncating: pidNumber))?.localizedName
    }

    private func materializeArtworkFile(from info: [String: Any]) -> String? {
        guard let artworkData = info[MediaRemoteBridge.artworkDataKey] as? Data,
              !artworkData.isEmpty else {
            return nil
        }

        let identifier = mediaRemoteString(info[MediaRemoteBridge.artworkIdentifierKey])?
            .replacingOccurrences(of: "/", with: "-")
            .replacingOccurrences(of: ":", with: "-") ?? UUID().uuidString
        let mimeType = mediaRemoteString(info[MediaRemoteBridge.artworkMIMETypeKey]) ?? "image/jpeg"
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
