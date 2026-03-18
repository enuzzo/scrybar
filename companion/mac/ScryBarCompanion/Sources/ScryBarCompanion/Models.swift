import Foundation

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
        guard let data = try? encoder.encode(self),
              let string = String(data: data, encoding: .utf8) else {
            return "{}"
        }
        return string
    }
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
