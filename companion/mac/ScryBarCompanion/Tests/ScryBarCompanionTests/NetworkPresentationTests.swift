import Foundation
import AppKit
import SwiftUI
import Testing
@testable import ScryBarCompanion

/// Opt-in native rendering harness. The output is labelled synthetic and must
/// be inspected; a PNG existing is not a visual acceptance assertion.
@Test @MainActor func networkNativeLayoutFixturesWhenRequested() throws {
    guard let path = ProcessInfo.processInfo.environment["SCRYBAR_NETWORK_RENDER_DIR"] else { return }
    try FileManager.default.createDirectory(atPath: path, withIntermediateDirectories: true)
    let fixture = NetworkStatsSnapshot(downloadBytesPerSec: 12_500_000, uploadBytesPerSec: 882_000,
        apps: [.init(id: "dropbox", name: "Dropbox", downloadBytesPerSec: 11_900_000, uploadBytesPerSec: 102_000),
               .init(id: "browser", name: "A deliberately very long browser application name", downloadBytesPerSec: 280_000, uploadBytesPerSec: 650_000),
               .init(id: "music", name: "Music", downloadBytesPerSec: 98_000, uploadBytesPerSec: 1_200),
               .init(id: "system", name: "System", downloadBytesPerSec: 24_000, uploadBytesPerSec: 0),
               .init(id: "other", name: "Another app", downloadBytesPerSec: 0, uploadBytesPerSec: 800)],
        attributionAvailable: true, attributionMessage: nil)
    for (name, data, stale) in [("mac-live", Optional(fixture), false), ("mac-waiting", nil, false), ("mac-stale", Optional(fixture), true)] {
        let renderer = ImageRenderer(content: VStack(alignment: .leading, spacing: 10) {
            Text("NATIVE LAYOUT FIXTURE · NOT LIVE DATA")
                .font(.system(size: 9, weight: .semibold))
                .foregroundStyle(CompanionTheme.warning)
            NetworkStatsSection(snapshot: data, isStale: stale)
        }.padding(12).frame(width: 360).background(CompanionTheme.windowBackground).preferredColorScheme(.dark))
        renderer.scale = 2
        let cgImage = try #require(renderer.cgImage)
        let png = try #require(NSBitmapImageRep(cgImage: cgImage).representation(using: .png, properties: [:]))
        try png.write(to: URL(fileURLWithPath: path).appendingPathComponent(name + ".png"))
    }
}

@Test func networkDisplayUnitsAndUnknownValuesAreUnambiguous() {
    #expect(NetworkRateFormatter.string(nil) == "—")
    #expect(NetworkRateFormatter.string(.nan) == "—")
    #expect(NetworkRateFormatter.string(-1) == "—")
    #expect(NetworkRateFormatter.string(0) == "0 B/s")
    #expect(NetworkRateFormatter.string(1_250_000) == "1.2 MB/s")
    #expect(NetworkRateFormatter.string(12_500_000) == "12.5 MB/s")
    #expect(NetworkRateFormatter.string(999_999) == "1.0 MB/s")
    #expect(NetworkRateFormatter.wireName("\nDemo\tApp\r") == "DemoApp")
    let name = NetworkRateFormatter.wireName(String(repeating: "🍕", count: 50))!
    #expect(name.utf8.count == 80 && !name.contains("�"))
}

@Test func networkWirePayloadKeepsLegacyFieldsAndOmitsUnavailableLeaders() throws {
    let legacy = MacStatsPayload(cpuTempC: nil, gpuTempC: nil, cpuUsagePct: 20, gpuUsagePct: 10,
        ramUsedGB: 12, ramTotalGB: 32, diskUsedGB: 100, diskTotalGB: 1000, updatedAt: .now)
    let encoder = JSONEncoder()
    encoder.dateEncodingStrategy = .iso8601
    let decoder = JSONDecoder()
    decoder.dateDecodingStrategy = .iso8601
    #expect(try decoder.decode(MacStatsPayload.self, from: encoder.encode(legacy)).netDownBytesPerSec == nil)
    let unavailable = NetworkStatsSnapshot(downloadBytesPerSec: 100, uploadBytesPerSec: 20,
        apps: [.init(id: "stale", name: "Stale app", downloadBytesPerSec: 900, uploadBytesPerSec: 100)],
        attributionAvailable: false, attributionMessage: nil)
    let cleared = legacy.withNetwork(unavailable, hostName: "Test Mac")
    let json = try JSONSerialization.jsonObject(with: encoder.encode(cleared)) as! [String: Any]
    #expect(json["networkApp"] == nil && json["networkAppDownBytesPerSec"] == nil)
    #expect(json["networkAttributionAvailable"] as? Bool == false)
    #expect(json["netDownBytesPerSec"] as? Double == 100)
    #expect(json["cpuUsagePct"] as? Double == 20)
    #expect(json["hostName"] as? String == "Test Mac")
    #expect(json["apps"] == nil) // Full rankings stay on the Mac.

    let fresh = NetworkStatsSnapshot(downloadBytesPerSec: .nan, uploadBytesPerSec: 0,
        apps: [.init(id: "idle", name: "Idle", downloadBytesPerSec: 0, uploadBytesPerSec: 0),
               .init(id: "browser", name: "Demo Browser", downloadBytesPerSec: 200, uploadBytesPerSec: 50)],
        attributionAvailable: true, attributionMessage: nil)
    let live = legacy.withNetwork(fresh, hostName: nil)
    #expect(legacy.withNetwork(fresh, hostName: "Demo’s Mac").hostName == "Demo's Mac")
    #expect(live.netDownBytesPerSec == nil && live.netUpBytesPerSec == 0)
    #expect(live.networkApp == "Demo Browser" && live.networkAppUpBytesPerSec == 50)
    let idle = NetworkStatsSnapshot(downloadBytesPerSec: 0, uploadBytesPerSec: 0, apps: [],
        attributionAvailable: true, attributionMessage: nil)
    #expect(live.withNetwork(idle, hostName: nil).networkApp == nil)
}
