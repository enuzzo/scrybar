import AppKit
import Foundation
import Testing
@testable import ScryBarCompanion

@Test func printStateClassifiesSoundTransitions() {
    var payload = BambuPrinterPayload()
    #expect(!payload.isPrinting)
    #expect(!payload.isPausedOrBlocked)
    #expect(!payload.isFinished)

    payload.status = "running"
    #expect(payload.isPrinting)

    payload.status = "pause"
    #expect(payload.isPausedOrBlocked)

    payload.status = "idle"
    payload.hmsCount = 1
    #expect(payload.isPausedOrBlocked)

    payload.hmsCount = 0
    payload.status = "completed"
    #expect(payload.isFinished)
}

@Test func displayPayloadUsesStableJSONFieldNames() throws {
    var payload = BambuPrinterPayload()
    payload.connected = true
    payload.status = "RUNNING"
    payload.jobName = "plate_1"
    payload.progressPercent = 42
    payload.nozzleTargetC = 220
    payload.bedTargetC = 65
    payload.speedPercent = 100
    payload.coolingFanPercent = 80
    payload.wifiSignal = "-48dBm"
    payload.activeFilamentLabel = "Bambu PLA Basic"
    payload.activeFilamentRemainingPercent = 76
    payload.printFilamentTrayIDs = [0, 1]
    payload.printFilamentLabels = ["Bambu PLA Basic", "Bambu PETG HF"]
    payload.printFilamentColors = ["#FFFFFF", "#00AE42"]
    payload.filamentChangesCompleted = 2
    payload.filamentChangesTotal = 10
    payload.amsHumidityPercent = 30

    let encoder = JSONEncoder()
    encoder.dateEncodingStrategy = .iso8601
    let object = try #require(
        JSONSerialization.jsonObject(with: encoder.encode(payload)) as? [String: Any]
    )

    #expect(object["connected"] as? Bool == true)
    #expect(object["status"] as? String == "RUNNING")
    #expect(object["jobName"] as? String == "plate_1")
    #expect(object["progressPercent"] as? Int == 42)
    #expect(object["nozzleTargetC"] as? Double == 220)
    #expect(object["bedTargetC"] as? Double == 65)
    #expect(object["speedPercent"] as? Int == 100)
    #expect(object["coolingFanPercent"] as? Int == 80)
    #expect(object["wifiSignal"] as? String == "-48dBm")
    #expect(object["activeFilamentLabel"] as? String == "Bambu PLA Basic")
    #expect(object["activeFilamentRemainingPercent"] as? Int == 76)
    #expect(object["printFilamentTrayIDs"] as? [Int] == [0, 1])
    #expect(object["printFilamentLabels"] as? [String] == ["Bambu PLA Basic", "Bambu PETG HF"])
    #expect(object["printFilamentColors"] as? [String] == ["#FFFFFF", "#00AE42"])
    #expect(object["filamentChangesCompleted"] as? Int == 2)
    #expect(object["filamentChangesTotal"] as? Int == 10)
    #expect(object["amsHumidityPercent"] as? Int == 30)
    #expect(object["accessCode"] == nil)
}

@Test func filamentChangeCounterCountsOnlySettledTrayTransitions() {
    var counter = BambuFilamentChangeCounter()

    #expect(counter.update(
        jobIdentity: "plate_1.gcode",
        status: "RUNNING",
        activeTray: 0,
        targetTray: 0,
        filamentSensorState: 1,
        amsStatus: 0
    ) == 0)
    #expect(counter.update(
        jobIdentity: "plate_1.gcode",
        status: "RUNNING",
        activeTray: 0,
        targetTray: 1,
        filamentSensorState: 1,
        amsStatus: 0x0100
    ) == 0)
    #expect(counter.update(
        jobIdentity: "plate_1.gcode",
        status: "RUNNING",
        activeTray: 1,
        targetTray: 1,
        filamentSensorState: 1,
        amsStatus: 0
    ) == 1)
    #expect(counter.update(
        jobIdentity: "plate_1.gcode",
        status: "PAUSE",
        activeTray: 1,
        targetTray: 1,
        filamentSensorState: 1,
        amsStatus: 0
    ) == 1)
    #expect(counter.update(
        jobIdentity: "plate_2.gcode",
        status: "RUNNING",
        activeTray: 2,
        targetTray: 2,
        filamentSensorState: 1,
        amsStatus: 0
    ) == 0)
}

@Test func parsesBambuSSDPDiscoveryResponse() throws {
    let response = """
    HTTP/1.1 200 OK\r
    ST: urn:bambulab-com:device:3dprinter:1\r
    USN: uuid:039012345678901::urn:bambulab-com:device:3dprinter:1\r
    Location: 192.168.1.42\r
    DevModel.bambu.com: N2S\r
    DevName.bambu.com: Workshop A1\r
    DevConnect.bambu.com: lan\r
    \r
    """

    let printer = try #require(BambuDiscoveryResponseParser.parse(
        Data(response.utf8),
        sourceHost: "192.168.1.42"
    ))

    #expect(printer.id == "039012345678901")
    #expect(printer.name == "Workshop A1")
    #expect(printer.model == "Bambu Lab A1")
    #expect(printer.host == "192.168.1.42")
    #expect(printer.serial == "039012345678901")
}

@Test func discoveryRejectsBareSSDPEchoWithoutPrinterIdentity() {
    let response = """
    HTTP/1.1 200 OK\r
    ST: urn:bambulab-com:device:3dprinter:1\r
    \r
    """

    #expect(BambuDiscoveryResponseParser.parse(
        Data(response.utf8),
        sourceHost: "192.168.1.33"
    ) == nil)
}

@Test func discoveryMergesPartialAndRichAnnouncementsFromOnePrinter() {
    let partial = BambuDiscoveredPrinter(
        id: "192.168.1.42",
        name: "",
        model: "Bambu Lab printer",
        modelCode: "",
        host: "192.168.1.42",
        serial: ""
    )
    let rich = BambuDiscoveredPrinter(
        id: "039012345678901",
        name: "Workshop A1",
        model: "Bambu Lab A1",
        modelCode: "N2S",
        host: "192.168.1.42",
        serial: "039012345678901"
    )

    let merged = BambuDiscoveryResults.mergingDuplicates([partial, rich])

    #expect(merged.count == 1)
    #expect(merged[0].id == "039012345678901")
    #expect(merged[0].name == "Workshop A1")
    #expect(merged[0].model == "Bambu Lab A1")
}

@Test func selectionPrefersSavedSerialOverSelectedStaleAddress() throws {
    let staleFallback = BambuDiscoveredPrinter(
        id: "192.168.1.33",
        name: "",
        model: "Bambu Lab printer",
        modelCode: "",
        host: "192.168.1.33",
        serial: ""
    )
    let currentIdentity = BambuDiscoveredPrinter(
        id: "039012345678901",
        name: "Workshop A1",
        model: "Bambu Lab A1",
        modelCode: "N2S",
        host: "192.168.1.96",
        serial: "039012345678901"
    )

    let preferred = try #require(BambuPrinterSelectionPolicy.preferredPrinter(
        in: [staleFallback, currentIdentity],
        savedSerial: "039012345678901",
        savedName: "Workshop A1",
        savedHost: "192.168.1.33",
        selectedID: staleFallback.id
    ))

    #expect(preferred.id == currentIdentity.id)
    #expect(preferred.host == "192.168.1.96")
}

@Test func selectionPrefersSavedLANNameOverStaleSelectedAddress() throws {
    let staleFallback = BambuDiscoveredPrinter(
        id: "192.168.1.33",
        name: "",
        model: "Bambu Lab printer",
        modelCode: "",
        host: "192.168.1.33",
        serial: ""
    )
    let currentIdentity = BambuDiscoveredPrinter(
        id: "192.168.1.96",
        name: "3DP-039-146",
        model: "Bambu Lab A1",
        modelCode: "N2S",
        host: "192.168.1.96",
        serial: ""
    )

    let preferred = try #require(BambuPrinterSelectionPolicy.preferredPrinter(
        in: [staleFallback, currentIdentity],
        savedSerial: "039012345678901",
        savedName: "3dp-039-146",
        savedHost: staleFallback.host,
        selectedID: staleFallback.id
    ))

    #expect(preferred.id == currentIdentity.id)
    #expect(preferred.host == "192.168.1.96")
}

@Test func selectionKeepsManualFallbackWithoutStableIdentityMatch() throws {
    let first = BambuDiscoveredPrinter(
        id: "192.168.1.42",
        name: "",
        model: "Bambu Lab printer",
        modelCode: "",
        host: "192.168.1.42",
        serial: ""
    )
    let second = BambuDiscoveredPrinter(
        id: "192.168.1.43",
        name: "",
        model: "Bambu Lab printer",
        modelCode: "",
        host: "192.168.1.43",
        serial: ""
    )

    let preferred = try #require(BambuPrinterSelectionPolicy.preferredPrinter(
        in: [first, second],
        savedSerial: "",
        savedName: "",
        savedHost: second.host,
        selectedID: second.id
    ))

    #expect(preferred.id == second.id)
}

@Test func serviceFingerprintRequiresMQTTAndOneBambuCompanionService() {
    #expect(BambuLANServiceFingerprint.isLikelyBambu(openPorts: [8883, 990]))
    #expect(BambuLANServiceFingerprint.isLikelyBambu(openPorts: [8883, 6000]))
    #expect(!BambuLANServiceFingerprint.isLikelyBambu(openPorts: [8883]))
    #expect(!BambuLANServiceFingerprint.isLikelyBambu(openPorts: [990, 6000]))
}

@Test func popoverHeightUsesContentUntilScreenSpaceRequiresScrolling() {
    #expect(PopoverHeightPolicy.resolvedHeight(contentHeight: 720, maximumHeight: 900) == 720)
    #expect(PopoverHeightPolicy.resolvedHeight(contentHeight: 980, maximumHeight: 840) == 840)
    #expect(PopoverHeightPolicy.resolvedHeight(contentHeight: 120, maximumHeight: 900) == 320)
    #expect(PopoverHeightPolicy.maximumHeight(visibleFrameHeight: 900) == 884)
    #expect(PopoverHeightPolicy.maximumHeight(visibleFrameHeight: 300) == 320)
}

@Test func connectionStatusMapsToVisiblePhase() {
    #expect(BambuConnectionPhase(status: "Not configured") == .notConfigured)
    #expect(BambuConnectionPhase(status: "Connecting to 192.168.1.42:8883…") == .connecting)
    #expect(BambuConnectionPhase(status: "TLS connected; authenticating…") == .connecting)
    #expect(BambuConnectionPhase(status: "Printer status live") == .live)
    #expect(BambuConnectionPhase(status: "Printer rejected the LAN access code") == .failed)

    #expect(BambuConnectionPhase.notConfigured.title == "Setup required")
    #expect(BambuConnectionPhase.connecting.title == "Connecting…")
    #expect(BambuConnectionPhase.live.title == "Connected")
    #expect(BambuConnectionPhase.failed.title == "Connection failed")
}

@Test func networkFailureStartsRateLimitedPrinterRecoveryDiscovery() {
    let now = Date(timeIntervalSince1970: 1_000)

    #expect(BambuRecoveryScanPolicy.shouldStart(
        status: "Connection failed: host is down. Retrying in 5 s…",
        isScanning: false,
        lastStartedAt: now.addingTimeInterval(-20),
        now: now
    ))
    #expect(!BambuRecoveryScanPolicy.shouldStart(
        status: "Connection failed: host is down. Retrying in 5 s…",
        isScanning: true,
        lastStartedAt: nil,
        now: now
    ))
    #expect(!BambuRecoveryScanPolicy.shouldStart(
        status: "Connection failed: host is down. Retrying in 5 s…",
        isScanning: false,
        lastStartedAt: now.addingTimeInterval(-5),
        now: now
    ))
}

@Test func authenticationFailureDoesNotStartPrinterDiscovery() {
    let now = Date(timeIntervalSince1970: 1_000)

    #expect(!BambuRecoveryScanPolicy.shouldStart(
        status: "Printer rejected the LAN access code. Retrying in 5 s…",
        isScanning: false,
        lastStartedAt: nil,
        now: now
    ))
    #expect(!BambuRecoveryScanPolicy.shouldStart(
        status: "MQTT authentication failed (5). Retrying in 5 s…",
        isScanning: false,
        lastStartedAt: nil,
        now: now
    ))
}

@Test @MainActor func secureAccessCodeFieldHandlesCommandV() throws {
    let pasteboard = NSPasteboard(name: .init("ScryBarCompanionTests.CommandV"))
    pasteboard.clearContents()
    #expect(pasteboard.setString("12345678", forType: .string))

    final class TestSecureField: CommandVPasteSecureTextField {
        let testPasteboard: NSPasteboard

        init(pasteboard: NSPasteboard) {
            testPasteboard = pasteboard
            super.init(frame: .zero)
        }

        required init?(coder: NSCoder) {
            fatalError("init(coder:) has not been implemented")
        }

        override var sourcePasteboard: NSPasteboard { testPasteboard }
    }

    let window = NSWindow(
        contentRect: NSRect(x: 0, y: 0, width: 220, height: 60),
        styleMask: [.titled],
        backing: .buffered,
        defer: false
    )
    let field = TestSecureField(pasteboard: pasteboard)
    field.frame = NSRect(x: 10, y: 10, width: 200, height: 32)
    window.contentView?.addSubview(field)
    #expect(window.makeFirstResponder(field))

    let event = try #require(NSEvent.keyEvent(
        with: .keyDown,
        location: .zero,
        modifierFlags: .command,
        timestamp: 0,
        windowNumber: window.windowNumber,
        context: nil,
        characters: "v",
        charactersIgnoringModifiers: "v",
        isARepeat: false,
        keyCode: 9
    ))

    #expect(field.performKeyEquivalent(with: event))
    #expect(field.stringValue == "12345678")
    pasteboard.clearContents()
}

@Test func decodesEmbeddedRGB565ArtworkForCompanionPreview() throws {
    // One red RGB565 pixel, stored in the same big-endian format sent to ScryBar.
    let image = try #require(ArtworkRGB565Decoder.image(
        base64: Data([0xF8, 0x00]).base64EncodedString(),
        width: 1,
        height: 1
    ))
    let representation = try #require(image.representations.first as? NSBitmapImageRep)
    let color = try #require(representation.colorAt(x: 0, y: 0)?.usingColorSpace(.deviceRGB))

    #expect(color.redComponent > 0.95)
    #expect(color.greenComponent < 0.05)
    #expect(color.blueComponent < 0.05)
    #expect(ArtworkRGB565Decoder.image(base64: "invalid", width: 1, height: 1) == nil)
}

@Test func artworkWithoutSourceIdentifierGetsStableWireIdentifier() {
    var payload = NowPlayingPayload.placeholder(source: "TIDAL")
    payload.title = "Onde"
    payload.artist = "Lucio Corsi"
    payload.album = "Cosa Faremo Da Grandi?"
    payload.artworkWidth = 1
    payload.artworkHeight = 1
    payload.artworkRGB565B64 = Data([0xF8, 0x00]).base64EncodedString()

    let firstID = payload.wireArtworkID
    #expect(firstID?.hasPrefix("track-") == true)
    #expect(payload.networkPayload(includeArtworkData: true).artworkID == firstID)
    #expect(payload.networkPayload(includeArtworkData: false).artworkID == firstID)

    var sameTrack = payload
    sameTrack.elapsedSec = 42
    #expect(sameTrack.wireArtworkID == firstID)

    var nextTrack = payload
    nextTrack.title = "Altro brano"
    #expect(nextTrack.wireArtworkID != firstID)
}

@Test func artworkKeepsPlayerIdentifierAndDoesNotInventOneWithoutPixels() {
    var identified = NowPlayingPayload.placeholder(source: "Music")
    identified.artworkID = "player-cover-id"
    identified.artworkRGB565B64 = "pixels"
    #expect(identified.wireArtworkID == "player-cover-id")

    var metadataOnly = NowPlayingPayload.placeholder(source: "TIDAL")
    metadataOnly.title = "No artwork"
    #expect(metadataOnly.wireArtworkID == nil)
}
