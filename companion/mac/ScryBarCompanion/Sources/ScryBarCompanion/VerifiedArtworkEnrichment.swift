import Foundation
import ImageIO

/// Higher resolution is optional: the native cover is delivered immediately.
/// A catalog candidate must match the complete edition metadata AND the native
/// pixels after the same 150 px crop. The JPEG tolerance is explicit (8/255 RMS
/// per color component); a matching title alone never replaces the native cover.
enum ArtworkPixelMatch {
    static func matches(nativeRGB565: Data, candidateRGB565: Data) -> Bool {
        guard nativeRGB565.count == 45_000, candidateRGB565.count == 45_000 else { return false }
        var squared = 0.0
        for index in stride(from: 0, to: 45_000, by: 2) {
            let a = UInt16(nativeRGB565[index]) << 8 | UInt16(nativeRGB565[index + 1])
            let b = UInt16(candidateRGB565[index]) << 8 | UInt16(candidateRGB565[index + 1])
            for (shift, mask, scale) in [(11, UInt16(31), 255.0 / 31), (5, UInt16(63), 255.0 / 63), (0, UInt16(31), 255.0 / 31)] {
                let delta = Double(Int((a >> shift) & mask) - Int((b >> shift) & mask)) * scale
                squared += delta * delta
            }
        }
        return squared / 67_500 <= 64
    }
}

private final class EnrichmentData: @unchecked Sendable {
    private let lock = NSLock()
    private var stored: Data?
    func set(_ value: Data?) { lock.lock(); stored = value; lock.unlock() }
    func get() -> Data? { lock.lock(); defer { lock.unlock() }; return stored }
}

final class VerifiedArtworkEnrichment: @unchecked Sendable {
    struct Result: Sendable { let hd: Data; let webURL: String }
    private struct Key: Equatable { let request: NowPlayingArtworkRequest; let pixels: String }
    private let lock = NSLock()
    private let queue = DispatchQueue(label: "com.netmilk.ScryBarCompanion.verified-cover", qos: .utility)
    private let tidal = TidalNowPlayingProvider()
    private var busy = false
    private var completed: Key?
    private var completedAt: TimeInterval = 0
    private var result: Result?

    func value(for request: NowPlayingArtworkRequest, nativeRGB565: String) -> Result? {
        guard let pixels = Data(base64Encoded: nativeRGB565) else { return nil }
        var identity = request; identity.nativeArtworkRevision = 0
        let key = Key(request: identity, pixels: ArtworkTranscoder.fingerprint(for: pixels))
        lock.lock(); defer { lock.unlock() }
        if completed == key, result != nil || ProcessInfo.processInfo.systemUptime - completedAt < 30 { return result }
        guard !busy else { return nil }
        busy = true
        queue.async { [self] in
            let value = resolve(request, nativePixels: pixels)
            lock.lock(); completed = key; result = value
            completedAt = ProcessInfo.processInfo.systemUptime; busy = false; lock.unlock()
        }
        return nil
    }

    private func resolve(_ request: NowPlayingArtworkRequest, nativePixels: Data) -> Result? {
        guard let bundle = request.bundleIdentifier, SourcePolicy.current.allows(bundleID: bundle) else { return nil }
        var candidates: [String] = []
        if let direct = request.artworkURL { candidates.append(ArtworkTranscoder.preferredURL(direct) ?? direct) }
        if request.tidal, let track = tidal.snapshot(),
           Self.same(track.title, request.title), Self.same(track.artist, request.artist), Self.same(track.album, request.album),
           let url = track.artworkURL { candidates.append(ArtworkTranscoder.preferredURL(url) ?? url) }
        for candidate in candidates {
            if let result = verified(candidate, nativePixels: nativePixels) { return result }
        }
        // No ambiguous album/artist: missing edition fields simply retain native.
        guard !request.title.isEmpty, !request.artist.isEmpty, !request.album.isEmpty else { return nil }
        var search = URLComponents(string: "https://itunes.apple.com/search")!
        search.queryItems = [.init(name: "term", value: "\(request.artist) \(request.title) \(request.album)"),
                             .init(name: "media", value: "music"), .init(name: "entity", value: "song"), .init(name: "limit", value: "20")]
        guard let url = search.url, let data = download(url),
              let json = try? JSONSerialization.jsonObject(with: data) as? [String: Any],
              let tracks = json["results"] as? [[String: Any]] else { return nil }
        let exact = tracks.filter {
            Self.same($0["trackName"] as? String ?? "", request.title) &&
            Self.same($0["artistName"] as? String ?? "", request.artist) &&
            Self.same($0["collectionName"] as? String ?? "", request.album)
        }
        for track in exact.prefix(3) {
            guard SourcePolicy.current.allows(bundleID: bundle) else { return nil }
            if let raw = track["artworkUrl100"] as? String,
               let result = verified(raw.replacingOccurrences(of: "100x100", with: "600x600"), nativePixels: nativePixels) { return result }
        }
        return nil
    }

    private static func same(_ a: String, _ b: String) -> Bool {
        a.precomposedStringWithCanonicalMapping.trimmingCharacters(in: .whitespacesAndNewlines)
            .caseInsensitiveCompare(b.precomposedStringWithCanonicalMapping.trimmingCharacters(in: .whitespacesAndNewlines)) == .orderedSame
    }

    private func verified(_ raw: String, nativePixels: Data) -> Result? {
        guard let url = URL(string: raw), let data = download(url),
              let image = CGImageSourceCreateWithData(data as CFData, nil),
              let properties = CGImageSourceCopyPropertiesAtIndex(image, 0, nil) as? [CFString: Any],
              let width = properties[kCGImagePropertyPixelWidth] as? Int,
              let height = properties[kCGImagePropertyPixelHeight] as? Int, min(width, height) >= 512,
              let comparison = ArtworkTranscoder.encodeRGB565Base64(from: data),
              let pixels = Data(base64Encoded: comparison.rgb565Base64),
              ArtworkPixelMatch.matches(nativeRGB565: nativePixels, candidateRGB565: pixels),
              let encoded = ArtworkTranscoder.encodeRGB565Base64(from: data, size: 512),
              let hd = Data(base64Encoded: encoded.rgb565Base64) else { return nil }
        return .init(hd: hd, webURL: raw)
    }

    private func download(_ url: URL) -> Data? {
        guard let scheme = url.scheme?.lowercased(), ["http", "https"].contains(scheme) else { return nil }
        let signal = DispatchSemaphore(value: 0), box = EnrichmentData()
        var request = URLRequest(url: url); request.timeoutInterval = 4
        let task = URLSession.shared.dataTask(with: request) { data, response, _ in
            if let response = response as? HTTPURLResponse, (200...299).contains(response.statusCode),
               let data, data.count <= 8_000_000 { box.set(data) }
            signal.signal()
        }
        task.resume()
        guard signal.wait(timeout: .now() + 5) == .success else { task.cancel(); return nil }
        return box.get()
    }
}
