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
