import Foundation

enum NetworkRateFormatter {
    /// Decimal byte units, shared by the popover and menu bar. Unknown is never 0.
    static func string(_ rate: Double?) -> String {
        guard let rate, rate.isFinite, rate >= 0 else { return "—" }
        let units = ["B/s", "kB/s", "MB/s", "GB/s", "TB/s"]
        var value = rate
        var unit = 0
        while value >= 999.5, unit < units.count - 1 {
            value /= 1000
            unit += 1
        }
        let digits = unit > 0 && value < 100 ? 1 : 0
        return String(format: "%.*f %@", locale: Locale(identifier: "en_US_POSIX"), digits, value, units[unit])
    }

    static func wireName(_ name: String?) -> String? {
        guard let name else { return nil }
        let clean = name.unicodeScalars.filter { !CharacterSet.controlCharacters.contains($0) }
        var result = ""
        for character in String(String.UnicodeScalarView(clean)).trimmingCharacters(in: .whitespacesAndNewlines) {
            guard result.utf8.count + String(character).utf8.count <= 80 else { break }
            result.append(character)
        }
        return result.isEmpty ? nil : result
    }

    static func validRate(_ value: Double?) -> Double? {
        guard let value, value.isFinite, value >= 0 else { return nil }
        return value
    }
}

extension MacStatsPayload {
    func withNetwork(_ network: NetworkStatsSnapshot, hostName: String?) -> Self {
        var result = self
        result.netDownBytesPerSec = NetworkRateFormatter.validRate(network.downloadBytesPerSec)
        result.netUpBytesPerSec = NetworkRateFormatter.validRate(network.uploadBytesPerSec)
        result.networkAttributionAvailable = network.attributionAvailable
        result.hostName = NetworkRateFormatter.wireName(hostName)
        result.networkApp = nil
        result.networkAppDownBytesPerSec = nil
        result.networkAppUpBytesPerSec = nil
        if network.attributionAvailable,
           let leader = network.apps.max(by: {
               $0.downloadBytesPerSec + $0.uploadBytesPerSec < $1.downloadBytesPerSec + $1.uploadBytesPerSec
           }),
           leader.downloadBytesPerSec + leader.uploadBytesPerSec > 0,
           let name = NetworkRateFormatter.wireName(leader.name) {
            result.networkApp = name
            result.networkAppDownBytesPerSec = NetworkRateFormatter.validRate(leader.downloadBytesPerSec)
            result.networkAppUpBytesPerSec = NetworkRateFormatter.validRate(leader.uploadBytesPerSec)
        }
        return result
    }
}
