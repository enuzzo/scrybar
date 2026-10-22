import AppKit
import Foundation
import Testing
@testable import ScryBarCompanion

private func wireFrame(_ jpeg: Data) -> Data {
    var data = Data((0..<4).map { UInt8((jpeg.count >> ($0 * 8)) & 255) })
    data.append(contentsOf: [0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0])
    data.append(jpeg)
    return data
}

@Test func cameraAuthenticationHasFixedLittleEndianHeaderAndPaddedCredentials() {
    let data = BambuCameraFrames.authentication(accessCode: "12345678")
    #expect(data.count == 80)
    #expect(Array(data.prefix(8)) == [0x40, 0, 0, 0, 0, 0x30, 0, 0])
    #expect(String(data: data[16..<20], encoding: .ascii) == "bblp")
    #expect(data[20..<48].allSatisfy { $0 == 0 })
    #expect(String(data: data[48..<56], encoding: .ascii) == "12345678")
    #expect(data[56..<80].allSatisfy { $0 == 0 })
}

@Test func cameraReassemblesEveryPossibleSplitAndCoalescedFrames() throws {
    let jpeg = Data([0xff, 0xd8, 2, 3, 4, 0xff, 0xd9])
    let wire = wireFrame(jpeg)
    for split in 0...wire.count {
        var parser = BambuCameraFrames()
        let first = try parser.append(Data(wire.prefix(split)))
        let second = try parser.append(Data(wire.dropFirst(split)))
        #expect(first + second == [jpeg])
    }
    var parser = BambuCameraFrames()
    #expect(try parser.append(wire + wire + wire) == [jpeg, jpeg, jpeg])
}

@Test func cameraRejectsOversizedFramesAndCorruptJPEG() {
    var parser = BambuCameraFrames()
    #expect(throws: (any Error).self) { try parser.append(Data(repeating: 255, count: 16)) }
    parser = BambuCameraFrames()
    #expect(throws: (any Error).self) { try parser.append(wireFrame(Data([1, 2, 3, 4]))) }
}

@Test @MainActor func cameraCleanupSurvivesRelaunchAndRequiresMatchingPrinterReport() throws {
    let suite = "camera-tests-\(UUID())"
    let defaults = try #require(UserDefaults(suiteName: suite))
    defer { defaults.removePersistentDomain(forName: suite) }
    defaults.set("PRINTER-A", forKey: "cameraLightCleanupSerial")
    let model = BambuCameraModel(defaults: defaults)
    model.configuration = { BambuPrinterConfiguration(host: "127.0.0.1", serial: "PRINTER-A", accessCode: "12345678") }
    #expect(model.lightPending)
    model.receiveLight(serial: "PRINTER-B", on: false)
    #expect(model.lightPending)
    model.receiveLight(serial: "PRINTER-A", on: true)
    #expect(model.lightPending)
    model.receiveLight(serial: "PRINTER-A", on: false)
    #expect(!model.lightPending)
    #expect(defaults.string(forKey: "cameraLightCleanupSerial") == "")
}

@Test @MainActor func cameraRGB565PreservesFullImageAndUsesDisplayByteOrder() throws {
    var rgba = [UInt8]()
    for _ in 0..<9 { for x in 0..<16 { rgba += x < 8 ? [255, 0, 0, 255] : [0, 0, 255, 255] } }
    let provider = try #require(CGDataProvider(data: Data(rgba) as CFData))
    let cg = try #require(CGImage(width: 16, height: 9, bitsPerComponent: 8, bitsPerPixel: 32,
        bytesPerRow: 64, space: CGColorSpaceCreateDeviceRGB(),
        bitmapInfo: CGBitmapInfo(rawValue: CGImageAlphaInfo.last.rawValue), provider: provider,
        decode: nil, shouldInterpolate: false, intent: .defaultIntent))
    let image = NSImage(cgImage: cg, size: NSSize(width: 16, height: 9))
    let encoded = try #require(BambuCameraModel.rgb565(image))
    let bytes = try #require(Data(base64Encoded: encoded))
    #expect(bytes.count == 240 * 135 * 2)
    let left = (67 * 240 + 20) * 2, right = (67 * 240 + 220) * 2
    #expect(Array(bytes[left..<left + 2]) == [0xf8, 0])
    #expect(Array(bytes[right..<right + 2]) == [0, 0x1f])
}

private final class CameraTestStream: BambuCameraStreaming {
    var onFrame: (@Sendable (Data) -> Void)?
    var onStatus: (@Sendable (String) -> Void)?
    var starts = 0
    var stops = 0
    func start(configuration: BambuPrinterConfiguration) { starts += 1 }
    func stop() { stops += 1 }
}

@Test @MainActor func cameraSharesOneStreamAndCleansLightWhenLastViewerLeaves() throws {
    let suite = "camera-tests-\(UUID())"
    let defaults = try #require(UserDefaults(suiteName: suite))
    defer { defaults.removePersistentDomain(forName: suite) }
    let stream = CameraTestStream()
    let model = BambuCameraModel(defaults: defaults, makeTransport: { stream })
    model.configuration = { BambuPrinterConfiguration(host: "127.0.0.1", serial: "PRINTER-A", accessCode: "12345678") }
    var commands: [Bool] = []
    model.setLight = { on, _ in commands.append(on) }
    model.viewOnMac(true)
    model.receiveDeviceControl(.init(session: "device-1", active: true, lightRevision: 1, light: true))
    #expect(stream.starts == 1)
    #expect(commands == [true]) // Includes a light tap before the first poll.
    model.receiveLight(serial: "PRINTER-A", on: true)
    model.viewOnMac(false)
    #expect(stream.stops == 0)
    model.receiveDeviceControl(.init(session: "device-1", active: false, lightRevision: 1, light: true))
    #expect(stream.stops == 1)
    #expect(commands == [true, false])
    #expect(model.lightPending)
    model.receiveLight(serial: "PRINTER-A", on: false)
    #expect(!model.lightPending)
}

@Test @MainActor func cameraLostDeviceLeaseStopsStreamAndRequestsLightOff() throws {
    let suite = "camera-tests-\(UUID())"
    let defaults = try #require(UserDefaults(suiteName: suite))
    defer { defaults.removePersistentDomain(forName: suite) }
    let stream = CameraTestStream()
    let model = BambuCameraModel(defaults: defaults, makeTransport: { stream })
    model.configuration = { BambuPrinterConfiguration(host: "127.0.0.1", serial: "PRINTER-A", accessCode: "12345678") }
    var commands: [Bool] = []
    model.setLight = { on, _ in commands.append(on) }
    let now = Date()
    model.receiveDeviceControl(.init(session: "device-1", active: true, lightRevision: 0, light: false), at: now)
    model.expireDeviceLease(at: now.addingTimeInterval(4))
    #expect(model.running)
    model.expireDeviceLease(at: now.addingTimeInterval(6))
    #expect(!model.running)
    #expect(stream.stops == 1)
    #expect(commands == [false])
}

@Test @MainActor func cameraTwoDisplaysHaveIndependentLeasesAndRevisionAcks() throws {
    let suite = "camera-tests-\(UUID())", defaults = try #require(UserDefaults(suiteName: suite))
    defer { defaults.removePersistentDomain(forName: suite) }
    let stream = CameraTestStream(), model = BambuCameraModel(defaults: defaults, makeTransport: { stream })
    model.configuration = { .init(host: "127.0.0.1", serial: "PRINTER-A", accessCode: "12345678") }
    var commands: [Bool] = []
    model.setLight = { on, _ in commands.append(on) }
    let now = Date(), format = ISO8601DateFormatter()
    let first = ScryBarCameraRequest(session: "circle-boot-1-view-1", active: true, lightRevision: 1, light: true,
        issuedAt: format.string(from: now), expiresAt: format.string(from: now.addingTimeInterval(4)))
    model.receiveDeviceControl(first, device: "circle", requiresFreshness: true, at: now)
    model.receiveDeviceControl(first, device: "circle", requiresFreshness: true, at: now)
    model.receiveDeviceControl(.init(session: "strip-view", active: true, lightRevision: 0, light: false), device: "strip", at: now)
    #expect(stream.starts == 1)
    #expect(commands == [true])
    #expect(model.deviceStatusBody("circle")["lightPending"] as? Bool == true)
    #expect(model.deviceStatusBody("circle")["lightRevision"] as? Int == 1)
    model.receiveLight(serial: "PRINTER-A", on: true)
    #expect(model.deviceStatusBody("circle")["lightPending"] as? Bool == false)
    model.receiveDeviceControl(.init(session: first.session, active: false, lightRevision: 1, light: true), device: "circle", at: now)
    #expect(model.running)
    #expect(commands == [true])
    model.expireDeviceLease(at: now.addingTimeInterval(6))
    #expect(!model.running)
    #expect(commands == [true, false])
    model.receiveLight(serial: "PRINTER-A", on: false)
    // Reopening the old request cannot replay an already consumed revision.
    model.receiveDeviceControl(first, device: "circle", requiresFreshness: true, at: now)
    #expect(commands == [true, false])
    model.shutdown()
}

@Test @MainActor func cameraRejectsExpiredLightAndPersistsConsumedRevisionAcrossRelaunch() throws {
    let suite = "camera-tests-\(UUID())", defaults = try #require(UserDefaults(suiteName: suite))
    defer { defaults.removePersistentDomain(forName: suite) }
    let now = Date(), format = ISO8601DateFormatter()
    let model = BambuCameraModel(defaults: defaults, makeTransport: { CameraTestStream() })
    model.configuration = { .init(host: "localhost", serial: "A", accessCode: "12345678") }
    var commands = 0; model.setLight = { _, _ in commands += 1 }
    var command = ScryBarCameraRequest(session: "unique-boot-view", active: true, lightRevision: 4, light: true,
        issuedAt: format.string(from: now.addingTimeInterval(-8)), expiresAt: format.string(from: now.addingTimeInterval(-3)))
    model.receiveDeviceControl(command, device: "circle", requiresFreshness: true, at: now)
    #expect(commands == 0)
    #expect(model.deviceStatusBody("circle")["lightError"] as? String == "expired_or_invalid_command")
    model.shutdown()
    // Rewriting the timestamp cannot revive the same command revision after restart.
    command.issuedAt = format.string(from: now); command.expiresAt = format.string(from: now.addingTimeInterval(4))
    let restored = BambuCameraModel(defaults: defaults, makeTransport: { CameraTestStream() })
    restored.configuration = model.configuration; restored.setLight = { _, _ in commands += 1 }
    restored.receiveDeviceControl(command, device: "circle", requiresFreshness: true, at: now)
    #expect(commands == 0)
    #expect(restored.deviceStatusBody("circle")["lightRevision"] as? Int == 4)
    restored.shutdown()
}

@Test @MainActor func closingCommandOwnerCancelsRetriesWithoutStoppingOtherViewer() throws {
    let suite = "camera-tests-\(UUID())", defaults = try #require(UserDefaults(suiteName: suite))
    defer { defaults.removePersistentDomain(forName: suite) }
    let stream = CameraTestStream(), model = BambuCameraModel(defaults: defaults, makeTransport: { stream })
    model.configuration = { .init(host: "localhost", serial: "A", accessCode: "12345678") }
    var commands: [Bool] = []; model.setLight = { on, _ in commands.append(on) }
    model.viewOnMac(true)
    model.receiveDeviceControl(.init(session: "view", active: true, lightRevision: 1, light: true), device: "circle")
    #expect(commands == [true])
    model.receiveDeviceControl(.init(session: "view", active: false, lightRevision: 1, light: true), device: "circle")
    #expect(commands == [true, false])
    #expect(model.running)
    #expect(stream.stops == 0)
    #expect(model.deviceStatusBody("circle")["lightError"] as? String == "view_closed")
    model.receiveDeviceControl(.init(session: "view", active: true, lightRevision: 1, light: true), device: "circle")
    #expect(commands == [true, false])
    model.shutdown()
}

private actor CameraMockHTTP: CompanionHTTPTransport {
    var replies: [String: ScryBarCameraRequest] = [:]
    var postedSessions: [String: [String]] = [:]
    func control(_ host: String, _ value: ScryBarCameraRequest) { replies[host] = value }
    func send(_ request: URLRequest) async throws -> CompanionHTTPReply {
        let host = request.url!.host!
        if host == "slow" { try await Task.sleep(for: .milliseconds(400)); throw URLError(.timedOut) }
        if request.httpMethod == "POST" {
            let body = try JSONSerialization.jsonObject(with: request.httpBody!) as! [String: Any]
            postedSessions[host, default: []].append(body["session"] as! String)
            return .init(data: Data(), status: 200)
        }
        guard let control = replies[host] else { return .init(data: Data(), status: 404) }
        let body: [String: Any] = ["session": control.session, "active": control.active,
            "lightRevision": control.lightRevision, "light": control.light]
        return .init(data: try JSONSerialization.data(withJSONObject: body), status: 200)
    }
    func posts(_ host: String) -> [String] { postedSessions[host] ?? [] }
}

@Test @MainActor func cameraHTTPPollingHasNoHeadOfLineBlocking() async throws {
    let suite = "camera-tests-\(UUID())", defaults = try #require(UserDefaults(suiteName: suite))
    defer { defaults.removePersistentDomain(forName: suite) }
    let http = CameraMockHTTP(), stream = CameraTestStream()
    await http.control("fast", .init(session: "fast-session", active: true, lightRevision: 0, light: false))
    let model = BambuCameraModel(defaults: defaults, http: http, makeTransport: { stream })
    model.configuration = { .init(host: "localhost", serial: "A", accessCode: "12345678") }
    model.endpoints = { ["slow", "fast"].map { .init(name: $0, host: $0, port: 8080, source: .manual) } }
    model.startPolling()
    try await Task.sleep(for: .milliseconds(100))
    #expect(await http.posts("fast") == ["fast-session"])
    #expect(stream.starts == 1)
    model.endpoints = { [] }; model.targetsDidChange()
    try await Task.sleep(for: .milliseconds(450))
    #expect(!model.running)
    #expect(await http.posts("slow").isEmpty)
    model.shutdown()
}
