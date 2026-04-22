import Foundation

actor ScryBarClient {
    private var lastSentArtworkIDByEndpoint: [String: String] = [:]

    func send(_ payload: NowPlayingPayload, to endpoint: ScryBarEndpoint) async throws {
        guard let baseURL = endpoint.baseURL else {
            throw URLError(.badURL)
        }

        let endpointKey = "\(endpoint.host):\(endpoint.port)"
        let includeArtwork = shouldIncludeArtworkData(for: endpointKey, payload: payload)
        let wirePayload = payload.networkPayload(includeArtworkData: includeArtwork)

        var request = URLRequest(url: baseURL.appending(path: "api/now-playing"))
        request.httpMethod = "POST"
        request.setValue("application/json", forHTTPHeaderField: "Content-Type")
        request.timeoutInterval = 5

        let encoder = JSONEncoder()
        encoder.dateEncodingStrategy = .iso8601
        request.httpBody = try encoder.encode(wirePayload)

        let (_, response) = try await URLSession.shared.data(for: request)
        guard let httpResponse = response as? HTTPURLResponse,
              (200...299).contains(httpResponse.statusCode) else {
            throw URLError(.badServerResponse)
        }

        if includeArtwork, let artworkID = payload.artworkID, !artworkID.isEmpty {
            lastSentArtworkIDByEndpoint[endpointKey] = artworkID
        } else if payload.artworkID == nil || payload.artworkID?.isEmpty == true {
            // ArtworkID disappeared (e.g. pause) — clear dedup cache so artwork
            // is re-sent when artworkID returns on resume
            lastSentArtworkIDByEndpoint.removeValue(forKey: endpointKey)
        }
    }

    func sendMacStats(_ payload: MacStatsPayload, to endpoint: ScryBarEndpoint) async throws {
        guard let baseURL = endpoint.baseURL else { throw URLError(.badURL) }

        var request = URLRequest(url: baseURL.appending(path: "api/mac-stats"))
        request.httpMethod = "POST"
        request.setValue("application/json", forHTTPHeaderField: "Content-Type")
        request.timeoutInterval = 5

        let encoder = JSONEncoder()
        encoder.dateEncodingStrategy = .iso8601
        request.httpBody = try encoder.encode(payload)

        let (_, response) = try await URLSession.shared.data(for: request)
        guard let http = response as? HTTPURLResponse,
              (200...299).contains(http.statusCode) else {
            throw URLError(.badServerResponse)
        }
    }

    /// Send artwork only when the artworkID changes (new track)
    private func shouldIncludeArtworkData(for endpointKey: String, payload: NowPlayingPayload) -> Bool {
        guard let artworkID = payload.artworkID,
              !artworkID.isEmpty,
              let artworkRGB565B64 = payload.artworkRGB565B64,
              !artworkRGB565B64.isEmpty else {
            return false
        }
        return lastSentArtworkIDByEndpoint[endpointKey] != artworkID
    }
}
