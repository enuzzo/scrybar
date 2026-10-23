import AppKit
import Combine
import Foundation

struct ScryBarCameraRequest: Decodable, Sendable {
    let session: String
    let active: Bool
    let lightRevision: Int
    let light: Bool
    var issuedAt: String? = nil
    var expiresAt: String? = nil
}

/// One printer stream, independent display leases and an optional Mac viewer. The device's requests are leases, never durable subscriptions.
@MainActor
final class BambuCameraModel: ObservableObject {
    @Published private(set) var image: NSImage?
    @Published private(set) var status = "Camera stopped"
    @Published private(set) var running = false
    @Published private(set) var lightOn: Bool?
    @Published private(set) var lightPending = false
    @Published var macViewing = false
    var configuration: (() -> BambuPrinterConfiguration)?
    var endpoints: (() -> [ScryBarEndpoint])?
    var bodyLimit: ((ScryBarEndpoint) -> Int)?
    var setLight: ((Bool, String) -> Void)?
    var setLightWithDeadline: ((Bool, String, Date?) -> Void)?
    private var transport: (any BambuCameraStreaming)?
    private var task: Task<Void, Never>?
    private var generation = UUID()
    private struct DeviceLease {
        var session: String
        var active = false
        var lightRevision = 0
        var lastSeen = Date.distantPast
        var sentVersion = -1
        var lightError: String?
        var pending = false
        var retiredSessions: [String] = []
    }
    private var devices: [String: DeviceLease] = [:]
    private var deviceTasks: [String: Task<Void, Never>] = [:]
    private var pollTokens: [String: UUID] = [:]
    private var targetAddresses: [String: String] = [:]
    private var lightDeadline: Date?
    private var lightOwner: String?
    private let http: any CompanionHTTPTransport
    private var lastFrameAt = Date.distantPast
    private var frameVersion = 0
    private var frameRGB: String?
    private var desiredLight: Bool?
    private var lastLightAttempt = Date.distantPast
    private var streamSerial = ""
    private var streamHost = ""
    private var shuttingDown = false
    private let defaults: UserDefaults
    private let makeTransport: () -> any BambuCameraStreaming
    private var pendingSerial: String {
        get { defaults.string(forKey: "cameraLightCleanupSerial") ?? "" }
        set { defaults.set(newValue, forKey: "cameraLightCleanupSerial") }
    }

    init(defaults: UserDefaults = .standard, http: any CompanionHTTPTransport = CompanionURLTransport(), makeTransport: @escaping () -> any BambuCameraStreaming = { BambuCameraTransport() }) {
        self.makeTransport = makeTransport
        self.http = http
        self.defaults = defaults
        lightPending = !pendingSerial.isEmpty
    }

    func startPolling() {
        task = Task { [weak self] in
            while !Task.isCancelled {
                guard let self else { return }
                self.tick()
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
        if desiredLight == on {
            desiredLight = nil; lightDeadline = nil; lightOwner = nil
            for key in devices.keys where devices[key]?.pending == true { devices[key]?.pending = false }
        }
        if !on, pendingSerial == serial, desiredLight != true {
            pendingSerial = ""
            lightPending = false
        }
    }
    private func requestLight(_ on: Bool, deadline: Date? = nil, owner: String? = nil) {
        guard let config = configuration?(), config.isReady else { return }
        guard pendingSerial.isEmpty || pendingSerial == config.serial else { return }
        for key in devices.keys where key != owner && devices[key]?.pending == true {
            devices[key]?.pending = false; devices[key]?.lightError = "superseded"
        }
        lightOwner = owner
        pendingSerial = config.serial // Persist before sending: recover after a crash/relaunch.
        lightPending = true
        desiredLight = on
        lightDeadline = deadline ?? (on ? Date().addingTimeInterval(5) : nil)
        lastLightAttempt = .distantPast
        sendLightIfNeeded()
    }
    private func sendLightIfNeeded() {
        if let lightDeadline, Date() >= lightDeadline {
            for key in devices.keys where devices[key]?.pending == true {
                devices[key]?.pending = false; devices[key]?.lightError = "confirmation_timeout"
            }
            self.lightDeadline = nil; self.lightOwner = nil
            // Never retry a late ON command. Persisted OFF cleanup is intentionally durable.
            desiredLight = false
            lastLightAttempt = .distantPast
        }
        guard Date().timeIntervalSince(lastLightAttempt) >= 3 else { return }
        if let desiredLight {
            let serial = pendingSerial.isEmpty ? (configuration?().serial ?? "") : pendingSerial
            if let setLightWithDeadline { setLightWithDeadline(desiredLight, serial, lightDeadline) }
            else { setLight?(desiredLight, serial) }
            lastLightAttempt = .now
        } else if !pendingSerial.isEmpty && !running {
            desiredLight = false
            sendLightIfNeeded()
        }
    }
    private func reconcile() {
        let wantsStream = !shuttingDown && (macViewing || devices.values.contains(where: \.active))
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

    func targetsDidChange() {
        let targets = endpoints?() ?? []
        let addresses = Dictionary(targets.map { ($0.stableKey, $0.addressKey) }, uniquingKeysWith: { a, _ in a })
        for key in Array(targetAddresses.keys) where addresses[key] != targetAddresses[key] {
            deviceTasks.removeValue(forKey: key)?.cancel()
            pollTokens[key] = nil
            // Keep revision/session tombstones across disconnects; drop only the lease.
            devices[key]?.active = false
            cancelLightOwner(key, error: "target_removed")
        }
        targetAddresses = addresses
        reconcile()
    }

    private func tick() {
        targetsDidChange()
        expireDeviceLease(at: .now)
        if running, image != nil, Date().timeIntervalSince(lastFrameAt) >= 12 {
            image = nil; frameRGB = nil; status = "Waiting for fresh camera frame"
        }
        if !shuttingDown {
            for endpoint in endpoints?() ?? [] where deviceTasks[endpoint.stableKey] == nil {
                let key = endpoint.stableKey, token = UUID()
                pollTokens[key] = token
                deviceTasks[key] = Task { [weak self] in
                    guard let self else { return }
                    await self.poll(endpoint, token: token)
                    if self.pollTokens[key] == token { self.deviceTasks[key] = nil; self.pollTokens[key] = nil }
                }
            }
        }
        reconcile()
        sendLightIfNeeded()
    }

    private func poll(_ endpoint: ScryBarEndpoint, token: UUID) async {
        let key = endpoint.stableKey
        guard let base = endpoint.baseURL else { return }
        func stillSelected() -> Bool {
            !Task.isCancelled && !shuttingDown && pollTokens[key] == token &&
                (endpoints?() ?? []).contains { $0.stableKey == key && $0.addressKey == endpoint.addressKey }
        }
        do {
            var request = URLRequest(url: base.appending(path: "api/bambu/camera"))
            request.timeoutInterval = 2; request.cachePolicy = .reloadIgnoringLocalCacheData
            let reply = try await http.send(request)
            guard stillSelected(), reply.status == 200, reply.data.count <= 4096 else { return }
            let control = try JSONDecoder().decode(ScryBarCameraRequest.self, from: reply.data)
            receiveDeviceControl(control, device: key, requiresFreshness: endpoint.product == "meguru")
            guard control.active, devices[key]?.active == true, devices[key]?.session == control.session else { return }
            var body = deviceStatusBody(key)
            if frameVersion != devices[key]?.sentVersion, Date().timeIntervalSince(lastFrameAt) < 12, let frameRGB {
                body["rgb565"] = frameRGB
            }
            let data = try JSONSerialization.data(withJSONObject: body, options: [.withoutEscapingSlashes])
            guard data.count <= (bodyLimit?(endpoint) ?? 98304), stillSelected() else { return }
            request.httpMethod = "POST"; request.httpBody = data
            request.setValue("application/json", forHTTPHeaderField: "Content-Type")
            let version = frameVersion
            let response = try await http.send(request)
            if response.status == 200, stillSelected(), devices[key]?.session == control.session { devices[key]?.sentVersion = version }
        } catch { /* Independent lease expiry handles unreachable/rebooted firmware. */ }
    }

    func deviceStatusBody(_ key: String = "legacy") -> [String: Any] {
        guard let lease = devices[key] else { return [:] }
        return ["session": lease.session, "status": status, "lightKnown": lightOn != nil,
                "lightOn": lightOn ?? false, "lightPending": lease.pending,
                "lightRevision": lease.lightRevision, "lightError": lease.lightError ?? ""]
    }

    func receiveDeviceControl(_ control: ScryBarCameraRequest, device key: String = "legacy",
                              requiresFreshness: Bool = false, at now: Date = .now) {
        guard !control.session.isEmpty, control.session.count <= 128, control.lightRevision >= 0 else { return }
        var lease = devices[key] ?? DeviceLease(session: control.session)
        if lease.session != control.session {
            guard !lease.retiredSessions.contains(control.session) else { return }
            cancelLightOwner(key, error: "session_changed")
            lease.retiredSessions.append(lease.session)
            // Bound memory; sessions must be unguessable and unique per boot/view.
            if lease.retiredSessions.count > 32 { lease.retiredSessions.removeFirst() }
            lease.session = control.session; lease.lightRevision = 0; lease.sentVersion = -1
            lease.lightError = nil; lease.pending = false
        }
        lease.active = control.active && !shuttingDown
        lease.lastSeen = now
        if !lease.active {
            if lightOwner == key { lease.lightError = "view_closed" }
            lease.pending = false; cancelLightOwner(key, error: "view_closed")
        }
        devices[key] = lease
        reconcile()
        guard lease.active, running, control.lightRevision > lease.lightRevision else { return }
        let ledgerKey = key + "|" + control.session
        var consumed = defaults.dictionary(forKey: "cameraConsumedRevisions.v1") as? [String: Int] ?? [:]
        if control.lightRevision <= (consumed[ledgerKey] ?? 0) {
            devices[key]?.lightRevision = consumed[ledgerKey] ?? 0
            devices[key]?.lightError = "already_consumed"
            return
        }
        consumed[ledgerKey] = control.lightRevision
        var consumedAt = defaults.dictionary(forKey: "cameraConsumedAt.v1") as? [String: Double] ?? [:]
        consumedAt[ledgerKey] = now.timeIntervalSince1970
        if consumed.count > 256, let oldest = consumedAt.min(by: { $0.value < $1.value })?.key {
            consumed[oldest] = nil; consumedAt[oldest] = nil
        }
        // Persist before sending: a quick Companion restart must not replay the tap.
        defaults.set(consumed, forKey: "cameraConsumedRevisions.v1")
        defaults.set(consumedAt, forKey: "cameraConsumedAt.v1")
        // Consume revision even on rejection; polling is not a replay/retry mechanism.
        devices[key]?.lightRevision = control.lightRevision
        devices[key]?.lightError = nil
        var deadline = now.addingTimeInterval(5)
        if requiresFreshness {
            func parsed(_ value: String?) -> Date? {
                guard let value else { return nil }
                let format = ISO8601DateFormatter()
                if let date = format.date(from: value) { return date }
                format.formatOptions.insert(.withFractionalSeconds)
                return format.date(from: value)
            }
            guard let issued = parsed(control.issuedAt), let expires = parsed(control.expiresAt),
                  issued <= now.addingTimeInterval(1), now < expires,
                  expires > issued, expires.timeIntervalSince(issued) <= 5,
                  now.timeIntervalSince(issued) <= 5 else {
                devices[key]?.lightError = "expired_or_invalid_command"
                return
            }
            deadline = expires
        }
        for other in devices.keys where devices[other]?.pending == true {
            devices[other]?.pending = false; devices[other]?.lightError = "superseded"
        }
        devices[key]?.pending = true
        requestLight(control.light, deadline: deadline, owner: key)
    }

    private func cancelLightOwner(_ key: String, error: String) {
        guard lightOwner == key else { return }
        lightOwner = nil; lightDeadline = nil
        devices[key]?.pending = false; devices[key]?.lightError = error
        // Stop retrying a command whose viewer/session no longer exists.
        // OFF is safety cleanup, never a delayed replay of the original ON.
        desiredLight = false; lastLightAttempt = .distantPast
        sendLightIfNeeded()
    }

    func expireDeviceLease(at now: Date) {
        for key in devices.keys where now.timeIntervalSince(devices[key]!.lastSeen) > 5 {
            devices[key]?.active = false
            cancelLightOwner(key, error: "lease_expired")
            if devices[key]?.pending == true {
                devices[key]?.pending = false; devices[key]?.lightError = "lease_expired"
            }
        }
        reconcile()
    }

    func prepareToQuit() async -> Bool {
        shuttingDown = true
        macViewing = false
        for key in devices.keys { devices[key]?.active = false }
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
    func shutdown() { task?.cancel(); deviceTasks.values.forEach { $0.cancel() }; deviceTasks.removeAll(); stopStream() }

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
