import AppKit
import Combine
import Foundation

struct ScryBarCameraRequest: Decodable {
    let session: String
    let active: Bool
    let lightRevision: Int
    let light: Bool
}

/// One stream, two viewers. The device's requests are leases, never durable subscriptions.
@MainActor
final class BambuCameraModel: ObservableObject {
    @Published private(set) var image: NSImage?
    @Published private(set) var status = "Camera stopped"
    @Published private(set) var running = false
    @Published private(set) var lightOn: Bool?
    @Published private(set) var lightPending = false
    @Published var macViewing = false
    var configuration: (() -> BambuPrinterConfiguration)?
    var endpoint: (() -> ScryBarEndpoint?)?
    var setLight: ((Bool, String) -> Void)?
    private var transport: (any BambuCameraStreaming)?
    private var task: Task<Void, Never>?
    private var generation = UUID()
    private var deviceSession: String?
    private var deviceLightRevision = 0
    private var deviceLastSeen = Date.distantPast
    private var lastFrameAt = Date.distantPast
    private var frameVersion = 0
    private var sentVersion = -1
    private var frameRGB: String?
    private var desiredLight: Bool?
    private var lastLightAttempt = Date.distantPast
    private var streamSerial = ""
    private var streamHost = ""
    private var shuttingDown = false
    private var deviceEndpoint = ""
    private let defaults: UserDefaults
    private let makeTransport: () -> any BambuCameraStreaming
    private var pendingSerial: String {
        get { defaults.string(forKey: "cameraLightCleanupSerial") ?? "" }
        set { defaults.set(newValue, forKey: "cameraLightCleanupSerial") }
    }

    init(defaults: UserDefaults = .standard, makeTransport: @escaping () -> any BambuCameraStreaming = { BambuCameraTransport() }) {
        self.makeTransport = makeTransport
        self.defaults = defaults
        lightPending = !pendingSerial.isEmpty
    }

    func startPolling() {
        task = Task { [weak self] in
            while !Task.isCancelled {
                guard let self else { return }
                await self.tick()
                try? await Task.sleep(for: .seconds(1))
            }
        }
    }

    func viewOnMac(_ active: Bool) { macViewing = active; reconcile() }
    func retry() { stopStream(); reconcile() }
    func toggleLight() {
        guard running else { return }
        requestLight(!(desiredLight ?? lightOn ?? false))
    }
    func receiveLight(serial: String, on: Bool) {
        guard serial == configuration?().serial else { return }
        lightOn = on
        if desiredLight == on { desiredLight = nil }
        if !on, pendingSerial == serial, desiredLight != true {
            pendingSerial = ""
            lightPending = false
        }
    }
    private func requestLight(_ on: Bool) {
        guard let config = configuration?(), config.isReady else { return }
        guard pendingSerial.isEmpty || pendingSerial == config.serial else { return }
        pendingSerial = config.serial // Persist before sending: recover after a crash/relaunch.
        lightPending = true
        desiredLight = on
        lastLightAttempt = .distantPast
        sendLightIfNeeded()
    }
    private func sendLightIfNeeded() {
        guard Date().timeIntervalSince(lastLightAttempt) >= 3 else { return }
        if let desiredLight {
            let serial = pendingSerial.isEmpty ? (configuration?().serial ?? "") : pendingSerial
            setLight?(desiredLight, serial)
            lastLightAttempt = .now
        } else if !pendingSerial.isEmpty && !running {
            desiredLight = false
            sendLightIfNeeded()
        }
    }
    private func reconcile() {
        let wantsStream = !shuttingDown && (macViewing || deviceSession != nil)
        if !wantsStream {
            if running { stopStream(); requestLight(false) }
            return
        }
        guard let config = configuration?(), config.isReady else { status = "Configure the printer first"; return }
        if running && (config.serial != streamSerial || config.host != streamHost) {
            stopStream()
            if !pendingSerial.isEmpty { desiredLight = false }
        }
        guard !running else { return }
        if !pendingSerial.isEmpty {
            status = "Waiting for light-off confirmation"
            desiredLight = false
            return
        }
        running = true
        streamSerial = config.serial
        streamHost = config.host
        status = "Connecting…"
        let token = UUID()
        generation = token
        let transport = makeTransport()
        self.transport = transport
        transport.onFrame = { [weak self] data in
            Task { @MainActor in
                guard let self, self.generation == token, self.running,
                      let image = NSImage(data: data) else { return }
                self.image = image
                self.lastFrameAt = .now
                self.status = "Live · A1 / P1 camera"
                self.frameRGB = Self.rgb565(image)
                self.frameVersion += 1
            }
        }
        transport.onStatus = { [weak self] status in
            Task { @MainActor in
                guard let self, self.generation == token else { return }
                self.status = status
                if status.contains("unavailable") {
                    self.image = nil
                    self.frameRGB = nil
                    self.requestLight(false)
                }
            }
        }
        transport.start(configuration: config)
    }
    private func stopStream() {
        generation = UUID()
        transport?.stop()
        transport = nil
        running = false
        image = nil
        frameRGB = nil
        status = "Camera stopped"
    }

    private func tick() async {
        if !shuttingDown, let endpoint = endpoint?(), let base = endpoint.baseURL {
            if deviceEndpoint != base.absoluteString {
                deviceEndpoint = base.absoluteString
                deviceSession = nil
                deviceLastSeen = .distantPast
                reconcile()
            }
            do {
                var request = URLRequest(url: base.appending(path: "api/bambu/camera"))
                request.timeoutInterval = 2
                request.cachePolicy = .reloadIgnoringLocalCacheData
                let (data, response) = try await URLSession.shared.data(for: request)
                guard (response as? HTTPURLResponse)?.statusCode == 200 else { throw URLError(.badServerResponse) }
                guard endpoint.baseURL == self.endpoint?()?.baseURL else { return }
                let control = try JSONDecoder().decode(ScryBarCameraRequest.self, from: data)
                receiveDeviceControl(control)
                if control.active {
                    var body: [String: Any] = ["session": control.session, "status": status,
                                              "lightKnown": lightOn != nil, "lightOn": lightOn ?? false,
                                              "lightPending": desiredLight != nil]
                    if frameVersion != sentVersion, Date().timeIntervalSince(lastFrameAt) < 12, let frameRGB {
                        body["rgb565"] = frameRGB
                    }
                    request.httpMethod = "POST"
                    request.setValue("application/json", forHTTPHeaderField: "Content-Type")
                    request.httpBody = try JSONSerialization.data(withJSONObject: body, options: [.withoutEscapingSlashes])
                    let version = frameVersion
                    let (_, reply) = try await URLSession.shared.data(for: request)
                    if (reply as? HTTPURLResponse)?.statusCode == 200 { sentVersion = version }
                }
            } catch { /* The lease below handles unreachable, old and rebooted firmware. */ }
        } else { deviceSession = nil }
        expireDeviceLease(at: .now)
        reconcile()
        sendLightIfNeeded()
    }

    func receiveDeviceControl(_ control: ScryBarCameraRequest, at now: Date = .now) {
        if control.active && !shuttingDown {
            if deviceSession != control.session {
                deviceSession = control.session
                deviceLightRevision = 0
                sentVersion = -1
            }
            deviceLastSeen = now
        } else { deviceSession = nil }
        reconcile()
        if control.active && running && deviceLightRevision != control.lightRevision {
            deviceLightRevision = control.lightRevision
            requestLight(control.light)
        }
    }

    func expireDeviceLease(at now: Date) {
        if now.timeIntervalSince(deviceLastSeen) > 5 {
            deviceSession = nil
            reconcile()
        }
    }

    func prepareToQuit() async -> Bool {
        shuttingDown = true
        macViewing = false
        deviceSession = nil
        if running { stopStream(); requestLight(false) }
        if !pendingSerial.isEmpty { desiredLight = false }
        for _ in 0..<20 {
            sendLightIfNeeded()
            if pendingSerial.isEmpty && desiredLight == nil { return true }
            try? await Task.sleep(for: .milliseconds(250))
        }
        shuttingDown = false
        return false
    }
    func shutdown() { task?.cancel(); stopStream() }

    static func rgb565(_ image: NSImage) -> String? {
        var rect = CGRect(origin: .zero, size: image.size)
        guard let cg = image.cgImage(forProposedRect: &rect, context: nil, hints: nil) else { return nil }
        let width = 240, height = 135
        var pixels = [UInt8](repeating: 0, count: width * height * 4)
        guard let context = CGContext(data: &pixels, width: width, height: height, bitsPerComponent: 8,
                                      bytesPerRow: width * 4, space: CGColorSpaceCreateDeviceRGB(),
                                      bitmapInfo: CGBitmapInfo.byteOrder32Little.rawValue | CGImageAlphaInfo.premultipliedFirst.rawValue) else { return nil }
        let scale = min(CGFloat(width) / CGFloat(cg.width), CGFloat(height) / CGFloat(cg.height))
        let w = CGFloat(cg.width) * scale, h = CGFloat(cg.height) * scale
        context.interpolationQuality = .medium
        context.draw(cg, in: CGRect(x: (CGFloat(width) - w) / 2, y: (CGFloat(height) - h) / 2, width: w, height: h))
        var output = Data(capacity: width * height * 2)
        for i in stride(from: 0, to: pixels.count, by: 4) {
            let value = UInt16(pixels[i + 2] >> 3) << 11 | UInt16(pixels[i + 1] >> 2) << 5 | UInt16(pixels[i] >> 3)
            output.append(UInt8(value >> 8)); output.append(UInt8(value & 255))
        }
        return output.base64EncodedString()
    }
}
