import Foundation
import Security

/// Optional third destination (2026-10-19): the studio TV ("yakamoz"), reached over HTTPS with a push key.
/// Off until a URL and key are set. It receives only what already passed `SourcePolicy`, and the receiving
/// server keeps its own music-only allowlist as a second defence. When nothing allowed is playing it sends
/// an empty update once, so the TV clears the previous track instead of showing it.
/// The URL lives in UserDefaults, the key in the Keychain; neither is ever written to the repository.
@MainActor
final class YakamozPush {
    static let enabledKey = "yakamoz.enabled"
    static let urlKey = "yakamoz.url"

    private var lastSignature = ""
    private var lastSentAt = Date.distantPast
    private var inFlight = false
    private(set) var status = "Off" { didSet { if status != oldValue { onStatus?(status) } } }
    var onStatus: ((String) -> Void)?

    var enabled: Bool { UserDefaults.standard.bool(forKey: Self.enabledKey) }
    var endpoint: URL? {
        guard let s = UserDefaults.standard.string(forKey: Self.urlKey), let u = URL(string: s),
              u.scheme?.lowercased() == "https" else { return nil }
        return u
    }

    func send(_ payload: NowPlayingPayload) {
        guard enabled else { status = "Off"; return }
        guard let url = endpoint else { status = "Set an https:// URL"; return }
        let key = YakamozKeychain.load()
        guard !key.isEmpty else { status = "Set the push key"; return }

        let nothing = payload.title == "—" || payload.title.isEmpty
        let body: [String: Any]
        if nothing {
            body = ["bundleId": "none"]
        } else {
            body = [
                "title": payload.title, "artist": payload.artist, "album": payload.album,
                "bundleId": SourcePolicy.bundleID(of: payload) ?? "none",
                "isPlaying": payload.isPlaying, "durationSec": payload.durationSec, "elapsedSec": payload.elapsedSec,
                "artworkURL": payload.webArtworkURL ?? payload.artworkURL ?? "",
            ]
        }
        // Send on change, and as a heartbeat every 10 s while something plays (the TV hides it after 30 s of silence).
        let signature = nothing ? "none" : "\(payload.title)|\(payload.artist)|\(payload.isPlaying)|\(payload.webArtworkURL ?? payload.artworkURL ?? "")"
        let due = signature != lastSignature || (!nothing && Date().timeIntervalSince(lastSentAt) >= 10)
        guard due, !inFlight, let data = try? JSONSerialization.data(withJSONObject: body) else { return }

        var request = URLRequest(url: url, timeoutInterval: 5)
        request.httpMethod = "POST"
        request.setValue("application/json", forHTTPHeaderField: "Content-Type")
        request.setValue(key, forHTTPHeaderField: "X-Yakamoz-Key")
        request.setValue("ScryBarCompanion", forHTTPHeaderField: "User-Agent")
        request.httpBody = data
        inFlight = true
        Task {
            let result = try? await URLSession.shared.data(for: request)
            let code = (result?.1 as? HTTPURLResponse)?.statusCode ?? 0
            self.inFlight = false
            if code == 200 {
                self.lastSignature = signature; self.lastSentAt = Date()
                self.status = nothing ? "Connected · nothing playing" : "Connected"
            } else {
                self.status = code == 403 ? "Rejected: wrong push key" : code == 0 ? "Unreachable" : "HTTP \(code)"
            }
        }
    }
}

enum YakamozKeychain {
    private static let service = "com.netmilk.ScryBarCompanion.yakamoz"
    private static let account = "push-key"

    static func load() -> String {
        let query: [String: Any] = [
            kSecClass as String: kSecClassGenericPassword, kSecAttrService as String: service,
            kSecAttrAccount as String: account, kSecReturnData as String: true, kSecMatchLimit as String: kSecMatchLimitOne,
        ]
        var result: CFTypeRef?
        guard SecItemCopyMatching(query as CFDictionary, &result) == errSecSuccess,
              let data = result as? Data, let value = String(data: data, encoding: .utf8) else { return "" }
        return value
    }

    static func save(_ value: String) {
        let identity: [String: Any] = [
            kSecClass as String: kSecClassGenericPassword, kSecAttrService as String: service, kSecAttrAccount as String: account,
        ]
        SecItemDelete(identity as CFDictionary)
        let trimmed = value.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !trimmed.isEmpty else { return }
        var item = identity
        item[kSecValueData as String] = Data(trimmed.utf8)
        item[kSecAttrAccessible as String] = kSecAttrAccessibleAfterFirstUnlock
        SecItemAdd(item as CFDictionary, nil)
    }
}
