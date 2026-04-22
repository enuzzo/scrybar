import Foundation

// MARK: - Mac Stats

struct MacStatsPayload: Codable {
    var cpuTempC: Float?
    var gpuTempC: Float?
    var ramUsedGB: Float
    var ramTotalGB: Float
    var diskUsedGB: Float
    var diskTotalGB: Float
    var updatedAt: Date
}

// MARK: -

enum ProviderKind: String, CaseIterable, Identifiable {
    case system = "System"
    case mock = "Mock"
    case music = "Music.app"

    var id: String { rawValue }
}

enum EndpointSource: String {
    case discovery = "Discovery"
    case manual = "Manual"
}

struct ScryBarEndpoint: Identifiable, Hashable {
    let id: UUID
    var name: String
    var host: String
    var port: Int
    var source: EndpointSource

    init(id: UUID = UUID(), name: String, host: String, port: Int, source: EndpointSource) {
        self.id = id
        self.name = name
        self.host = host
        self.port = port
        self.source = source
    }

    var displayName: String {
        "\(name) (\(host):\(port))"
    }

    var baseURL: URL? {
        URL(string: "http://\(host):\(port)")
    }
}

struct NowPlayingPayload: Codable {
    var title: String
    var artist: String
    var album: String
    var source: String
    var appName: String
    var durationSec: Double
    var elapsedSec: Double
    var isPlaying: Bool
    var inSync: Bool
    var artworkURL: String?
    var artworkID: String?
    var artworkWidth: Int?
    var artworkHeight: Int?
    var artworkRGB565B64: String?
    var updatedAt: Date

    static func placeholder(source: String) -> Self {
        .init(
            title: "—",
            artist: "—",
            album: "",
            source: source,
            appName: "",
            durationSec: 0,
            elapsedSec: 0,
            isPlaying: false,
            inSync: false,
            artworkURL: nil,
            artworkID: nil,
            artworkWidth: nil,
            artworkHeight: nil,
            artworkRGB565B64: nil,
            updatedAt: .now
        )
    }

    var remainingPercent: Int {
        guard durationSec > 0 else { return 0 }
        let clampedElapsed = min(max(elapsedSec, 0), durationSec)
        let remaining = max(durationSec - clampedElapsed, 0)
        return Int((remaining / durationSec) * 100.0)
    }

    var prettyJSON: String {
        let encoder = JSONEncoder()
        encoder.outputFormatting = [.prettyPrinted, .sortedKeys]
        encoder.dateEncodingStrategy = .iso8601
        guard let data = try? encoder.encode(previewPayload),
              let string = String(data: data, encoding: .utf8) else {
            return "{}"
        }
        return string
    }

    var artworkSummary: String {
        guard let artworkID, !artworkID.isEmpty else { return "none" }
        let dims: String
        if let artworkWidth, let artworkHeight {
            dims = "\(artworkWidth)x\(artworkHeight)"
        } else {
            dims = "unknown size"
        }
        if let artworkRGB565B64, !artworkRGB565B64.isEmpty {
            return "\(artworkID) (\(dims), \(artworkRGB565B64.count) b64 chars)"
        }
        return "\(artworkID) (\(dims), metadata only)"
    }

    func networkPayload(includeArtworkData: Bool) -> NowPlayingWirePayload {
        .init(
            title: title,
            artist: artist,
            album: album,
            source: source,
            appName: appName,
            durationSec: durationSec,
            elapsedSec: elapsedSec,
            isPlaying: isPlaying,
            inSync: inSync,
            artworkURL: artworkURL,
            artworkID: artworkID,
            artworkWidth: artworkWidth,
            artworkHeight: artworkHeight,
            artworkRGB565B64: includeArtworkData ? artworkRGB565B64 : nil,
            updatedAt: updatedAt
        )
    }

    private var previewPayload: NowPlayingWirePayload {
        .init(
            title: title,
            artist: artist,
            album: album,
            source: source,
            appName: appName,
            durationSec: durationSec,
            elapsedSec: elapsedSec,
            isPlaying: isPlaying,
            inSync: inSync,
            artworkURL: artworkURL,
            artworkID: artworkID,
            artworkWidth: artworkWidth,
            artworkHeight: artworkHeight,
            artworkRGB565B64: artworkRGB565B64.map { "<\($0.count) base64 chars>" },
            updatedAt: updatedAt
        )
    }
}

struct NowPlayingWirePayload: Codable {
    var title: String
    var artist: String
    var album: String
    var source: String
    var appName: String
    var durationSec: Double
    var elapsedSec: Double
    var isPlaying: Bool
    var inSync: Bool
    var artworkURL: String?
    var artworkID: String?
    var artworkWidth: Int?
    var artworkHeight: Int?
    var artworkRGB565B64: String?
    var updatedAt: Date
}

struct MockTrack {
    var title: String
    var artist: String
    var album: String
    var source: String
    var durationSec: Double
    var elapsedSec: Double
}

enum MockLibrary {
    static let tracks: [MockTrack] = [
        MockTrack(
            title: "The City Was Electric and the Night Smelled Like Rain (Extended Skyline Rebuild Mix)",
            artist: "Marta Bellavita and the Extremely Overprepared Weather Satellites",
            album: "Paper Maps for Neon Highways and Other Late Decisions",
            source: "TIDAL",
            durationSec: 567,
            elapsedSec: 221
        ),
        MockTrack(
            title: "Archive of Warm Machines, Side B: Notes Left Inside the Last Available Drawer",
            artist: "Her Future Ghost Orchestra Featuring Alessandro From Accounting On Portable Percussion",
            album: "Faint Diagrams For Sleep-Deprived Engineers",
            source: "Podcasts",
            durationSec: 1842,
            elapsedSec: 423
        ),
        MockTrack(
            title: "Dancing Through Seven Overlapping Calendars in a Borrowed Neon Suit",
            artist: "The Committee for Loud, Unreasonable, and Surprisingly Elegant Synthesizers",
            album: "Seasonal Delays and Other Confirmed Miracles",
            source: "Spotify",
            durationSec: 412,
            elapsedSec: 103
        ),
    ]
}
