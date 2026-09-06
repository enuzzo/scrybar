import Foundation
import Network
import Security

struct BambuTrayTelemetry: Codable, Equatable, Sendable, Identifiable {
    var id = ""
    var unitIndex = -1
    var trayIndex = -1
    var present = false
    var active = false
    var materialType = ""
    var materialName = ""
    var colorHex = ""
    var remainingPercent = -1

    var displayName: String {
        if !materialName.isEmpty { return materialName }
        if !materialType.isEmpty { return materialType }
        return unitIndex >= 0 && trayIndex >= 0 ? "AMS \(unitIndex + 1) · Slot \(trayIndex + 1)" : "External spool"
    }
}

struct BambuAMSUnitTelemetry: Codable, Equatable, Sendable, Identifiable {
    var id = 0
    var humidityPercent = -1
    var temperatureC: Float = 0
    var trays: [BambuTrayTelemetry] = []
}

struct BambuFilamentChangeCounter: Sendable {
    private(set) var completed = 0
    private var jobIdentity = ""
    private var lastSettledTray = -1
    private var jobWasActive = false

    mutating func update(
        jobIdentity: String,
        status: String,
        activeTray: Int,
        targetTray: Int,
        filamentSensorState: Int,
        amsStatus: Int
    ) -> Int {
        let normalizedIdentity = jobIdentity.trimmingCharacters(in: .whitespacesAndNewlines)
        let normalizedStatus = status.uppercased()
        let jobIsActive = ["RUNNING", "PREPARE", "SLICING", "INIT", "PAUSE", "PAUSED"]
            .contains(normalizedStatus)
        let identityChanged = !normalizedIdentity.isEmpty && normalizedIdentity != self.jobIdentity

        if jobIsActive && (!jobWasActive || identityChanged) {
            completed = 0
            lastSettledTray = -1
            if !normalizedIdentity.isEmpty { self.jobIdentity = normalizedIdentity }
        } else if identityChanged {
            self.jobIdentity = normalizedIdentity
        }
        jobWasActive = jobIsActive

        guard jobIsActive, activeTray >= 0 else { return completed }
        let amsMainStatus = amsStatus < 0 ? -1 : ((amsStatus >> 8) & 0xFF)
        let changingFilament = amsStatus == 1 || amsMainStatus == 1
        let targetIsSeated = targetTray < 0 || activeTray == targetTray
        let filamentIsLoaded = filamentSensorState != 0
        guard !changingFilament, targetIsSeated, filamentIsLoaded else { return completed }

        if lastSettledTray >= 0, lastSettledTray != activeTray {
            completed += 1
        }
        lastSettledTray = activeTray
        return completed
    }
}

struct BambuPrinterPayload: Codable, Equatable, Sendable {
    var connected = false
    var status = "OFFLINE"
    var stage = "Waiting for printer"
    var jobName = ""
    var progressPercent = 0
    var remainingMinutes = 0
    var currentLayer = 0
    var totalLayers = 0
    var nozzleTempC: Float = 0
    var nozzleTargetC: Float = 0
    var bedTempC: Float = 0
    var bedTargetC: Float = 0
    var chamberTempC: Float = 0
    var errorCode = 0
    var hmsCount = 0
    var hmsCodes: [String] = []
    var gcodeFile = ""
    var printType = ""
    var wifiSignal = ""
    var speedLevel = 0
    var speedPercent = 0
    var coolingFanPercent = -1
    var heatbreakFanPercent = -1
    var auxiliaryFanPercent = -1
    var chamberFanPercent = -1
    var filamentSensorState = -1
    var activeTray = -1
    var targetTray = -1
    var amsStatus = -1
    var amsUnits: [BambuAMSUnitTelemetry] = []
    var externalSpool: BambuTrayTelemetry?
    var activeFilamentLabel = ""
    var activeFilamentColorHex = ""
    var activeFilamentRemainingPercent = -1
    var printFilamentTrayIDs: [Int] = []
    var printFilamentLabels: [String] = []
    var printFilamentColors: [String] = []
    var filamentChangesCompleted = 0
    var filamentChangesTotal = -1
    var amsHumidityPercent = -1
    var amsTemperatureC: Float = 0
    var estimatedFilamentWeightG: Float = 0
    var estimatedFilamentLengthMm: Float = 0
    var updatedAt = Date()

    var activeFilament: BambuTrayTelemetry? {
        amsUnits.lazy.flatMap(\.trays).first(where: \.active)
            ?? (externalSpool?.active == true ? externalSpool : nil)
    }

    var primaryAMSUnit: BambuAMSUnitTelemetry? {
        amsUnits.first
    }

    var isPrinting: Bool {
        ["RUNNING", "PREPARE", "SLICING"].contains(status.uppercased())
    }

    var isPausedOrBlocked: Bool {
        ["PAUSE", "PAUSED", "FAILED", "ERROR"].contains(status.uppercased()) || errorCode != 0 || hmsCount > 0
    }

    var isFinished: Bool { ["FINISH", "FINISHED", "COMPLETED"].contains(status.uppercased()) }
}

struct BambuPrinterConfiguration: Equatable, Sendable {
    var host: String
    var serial: String
    var accessCode: String

    var isReady: Bool {
        !host.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty &&
        !serial.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty &&
        !accessCode.isEmpty
    }
}

enum BambuKeychain {
    private static let service = "com.netmilk.ScryBarCompanion.bambu"
    private static let legacyAccount = "lan-access-code"

    static func loadAccessCode(serial: String, allowLegacyFallback: Bool = false) -> String {
        let serial = serial.trimmingCharacters(in: .whitespacesAndNewlines).uppercased()
        if !serial.isEmpty, let value = load(account: account(for: serial)) { return value }
        return allowLegacyFallback ? (load(account: legacyAccount) ?? "") : ""
    }

    private static func load(account: String) -> String? {
        let query: [String: Any] = [
            kSecClass as String: kSecClassGenericPassword,
            kSecAttrService as String: service,
            kSecAttrAccount as String: account,
            kSecReturnData as String: true,
            kSecMatchLimit as String: kSecMatchLimitOne,
        ]
        var result: CFTypeRef?
        guard SecItemCopyMatching(query as CFDictionary, &result) == errSecSuccess,
              let data = result as? Data,
              let value = String(data: data, encoding: .utf8) else { return nil }
        return value
    }

    static func saveAccessCode(_ value: String, serial: String) {
        let serial = serial.trimmingCharacters(in: .whitespacesAndNewlines).uppercased()
        guard !serial.isEmpty else { return }
        let account = account(for: serial)
        let identity: [String: Any] = [
            kSecClass as String: kSecClassGenericPassword,
            kSecAttrService as String: service,
            kSecAttrAccount as String: account,
        ]
        SecItemDelete(identity as CFDictionary)
        guard !value.isEmpty else { return }
        var item = identity
        item[kSecValueData as String] = Data(value.utf8)
        item[kSecAttrAccessible as String] = kSecAttrAccessibleAfterFirstUnlock
        SecItemAdd(item as CFDictionary, nil)
    }

    private static func account(for serial: String) -> String {
        "lan-access-code-\(serial)"
    }
}

/// Minimal MQTT 3.1.1 client for Bambu's LAN broker. It intentionally supports
/// only CONNECT, SUBSCRIBE, PUBLISH, PING and the corresponding acknowledgements.
final class BambuPrinterMonitor: @unchecked Sendable {
    var onPayload: (@Sendable (BambuPrinterPayload) -> Void)?
    var onStatus: (@Sendable (String) -> Void)?

    private let queue = DispatchQueue(label: "com.netmilk.ScryBarCompanion.bambu-mqtt")
    private var connection: NWConnection?
    private var configuration: BambuPrinterConfiguration?
    private var receiveBuffer = Data()
    private var snapshot = BambuPrinterPayload()
    private var reconnectWorkItem: DispatchWorkItem?
    private var connectionTimeoutWorkItem: DispatchWorkItem?
    private var pingTimer: DispatchSourceTimer?
    private var filamentChangeCounter = BambuFilamentChangeCounter()
    private var stopped = true

    func start(configuration: BambuPrinterConfiguration) {
        queue.async { [weak self] in
            guard let self else { return }
            self.configuration = configuration
            self.stopped = false
            self.reconnectWorkItem?.cancel()
            self.disconnectCurrentConnection()
            guard configuration.isReady else {
                self.publishStatus("Add printer IP, serial and LAN access code")
                return
            }
            self.connect()
        }
    }

    func stop() {
        queue.async { [weak self] in
            guard let self else { return }
            self.stopped = true
            self.reconnectWorkItem?.cancel()
            self.disconnectCurrentConnection()
            self.snapshot.connected = false
            self.snapshot.status = "OFFLINE"
            self.snapshot.updatedAt = .now
            self.onPayload?(self.snapshot)
        }
    }

    private func connect() {
        guard !stopped, let configuration, configuration.isReady else { return }
        publishStatus("Connecting to \(configuration.host):8883…")

        let tls = NWProtocolTLS.Options()
        // Bambu LAN brokers use a device certificate that is not rooted in the
        // macOS trust store. Authentication is still enforced by the LAN access
        // code; transport remains encrypted on the local network.
        sec_protocol_options_set_verify_block(tls.securityProtocolOptions, { _, _, complete in
            complete(true)
        }, queue)
        let tcp = NWProtocolTCP.Options()
        tcp.connectionTimeout = 10
        let parameters = NWParameters(tls: tls, tcp: tcp)
        parameters.serviceClass = .responsiveData

        let connection = NWConnection(
            host: NWEndpoint.Host(configuration.host),
            port: NWEndpoint.Port(rawValue: 8883)!,
            using: parameters
        )
        self.connection = connection
        connection.stateUpdateHandler = { [weak self, weak connection] state in
            guard let self, let connection, connection === self.connection else { return }
            self.queue.async { self.handleConnectionState(state, connection: connection) }
        }
        connection.start(queue: queue)

        // NWConnection's TCP timeout is not guaranteed to surface promptly
        // when the printer is reachable but its LAN services are disabled.
        // Give the settings UI a deterministic, actionable result.
        let timeout = DispatchWorkItem { [weak self, weak connection] in
            guard let self, let connection, connection === self.connection else { return }
            self.handleDisconnect(
                "Connection failed: printer LAN services are unavailable. Enable LAN Only; if shown, enable Developer Mode"
            )
        }
        connectionTimeoutWorkItem = timeout
        queue.asyncAfter(deadline: .now() + 8, execute: timeout)
    }

    private func handleConnectionState(_ state: NWConnection.State, connection: NWConnection) {
        switch state {
        case .ready:
            connectionTimeoutWorkItem?.cancel()
            connectionTimeoutWorkItem = nil
            publishStatus("TLS connected; authenticating…")
            sendConnect()
            receiveNext()
        case .failed(let error):
            handleDisconnect("Connection failed: \(error.localizedDescription)")
        case .cancelled:
            if !stopped { scheduleReconnect(reason: "Connection closed") }
        default:
            break
        }
    }

    private func sendConnect() {
        guard let configuration else { return }
        let clientID = "scrybar-\(UUID().uuidString.prefix(8).lowercased())"
        var body = Data()
        body.append(mqttString("MQTT"))
        body.append(4)       // MQTT 3.1.1
        body.append(0xC2)    // username + password + clean session
        body.append(contentsOf: [0x00, 0x14]) // 20 s keepalive
        body.append(mqttString(clientID))
        body.append(mqttString("bblp"))
        body.append(mqttString(configuration.accessCode))
        sendPacket(typeAndFlags: 0x10, body: body)
    }

    private func subscribeAndSync() {
        guard let configuration else { return }
        let reportTopic = "device/\(configuration.serial)/report"
        var body = Data([0x00, 0x01])
        body.append(mqttString(reportTopic))
        body.append(0x00)
        sendPacket(typeAndFlags: 0x82, body: body)
        startPingTimer()
    }

    private func requestInitialStatus() {
        publish(json: #"{"info":{"sequence_id":"0","command":"get_version"}}"#)
        publish(json: #"{"pushing":{"sequence_id":"0","command":"start"}}"#)
        publish(json: #"{"pushing":{"sequence_id":"0","command":"pushall"}}"#)
    }

    private func publish(json: String) {
        guard let configuration else { return }
        var body = mqttString("device/\(configuration.serial)/request")
        body.append(Data(json.utf8))
        sendPacket(typeAndFlags: 0x30, body: body)
    }

    private func sendPacket(typeAndFlags: UInt8, body: Data) {
        guard let connection else { return }
        var packet = Data([typeAndFlags])
        packet.append(contentsOf: mqttRemainingLength(body.count))
        packet.append(body)
        connection.send(content: packet, completion: .contentProcessed { [weak self] error in
            guard let self, let error else { return }
            self.queue.async { self.handleDisconnect("MQTT send failed: \(error.localizedDescription)") }
        })
    }

    private func receiveNext() {
        guard let connection else { return }
        connection.receive(minimumIncompleteLength: 1, maximumLength: 65_536) { [weak self] data, _, complete, error in
            guard let self else { return }
            self.queue.async {
                if let data, !data.isEmpty {
                    self.receiveBuffer.append(data)
                    self.consumePackets()
                }
                if let error {
                    self.handleDisconnect("MQTT receive failed: \(error.localizedDescription)")
                } else if complete {
                    self.handleDisconnect("Printer closed the connection")
                } else if self.connection === connection {
                    self.receiveNext()
                }
            }
        }
    }

    private func consumePackets() {
        while receiveBuffer.count >= 2 {
            guard let decoded = decodeRemainingLength(in: receiveBuffer, start: 1) else { return }
            let headerLength = 1 + decoded.bytes
            let packetLength = headerLength + decoded.value
            guard receiveBuffer.count >= packetLength else { return }
            let packet = receiveBuffer.subdata(in: headerLength..<packetLength)
            let first = receiveBuffer[receiveBuffer.startIndex]
            receiveBuffer.removeSubrange(0..<packetLength)
            handlePacket(type: first >> 4, flags: first & 0x0F, body: packet)
        }
    }

    private func handlePacket(type: UInt8, flags: UInt8, body: Data) {
        switch type {
        case 2: // CONNACK
            guard body.count >= 2, body[1] == 0 else {
                let code = body.count >= 2 ? body[1] : 255
                handleDisconnect(code == 5 ? "Printer rejected the LAN access code" : "MQTT authentication failed (\(code))")
                return
            }
            snapshot.connected = true
            snapshot.updatedAt = .now
            onPayload?(snapshot)
            publishStatus("Connected; subscribing to printer status…")
            subscribeAndSync()
        case 9: // SUBACK
            publishStatus("Printer status live")
            requestInitialStatus()
        case 3: // PUBLISH
            handlePublish(flags: flags, body: body)
        case 13: // PINGRESP
            break
        default:
            break
        }
    }

    private func handlePublish(flags: UInt8, body: Data) {
        guard body.count >= 2 else { return }
        let topicLength = Int(body[0]) << 8 | Int(body[1])
        var cursor = 2 + topicLength
        guard cursor <= body.count else { return }
        let qos = (flags >> 1) & 0x03
        if qos > 0 { cursor += 2 }
        guard cursor <= body.count else { return }
        let payload = body.subdata(in: cursor..<body.count)
        guard let root = try? JSONSerialization.jsonObject(with: payload) as? [String: Any],
              let print = root["print"] as? [String: Any] else { return }
        merge(print: print)
    }

    private func merge(print: [String: Any]) {
        func int(_ key: String) -> Int? {
            if let value = print[key] as? NSNumber { return value.intValue }
            if let value = print[key] as? String {
                if let decimal = Int(value) { return decimal }
                return Int(value.replacingOccurrences(of: "0x", with: ""), radix: 16)
            }
            return nil
        }
        func float(_ key: String) -> Float? {
            if let value = print[key] as? NSNumber { return value.floatValue }
            if let value = print[key] as? String { return Float(value) }
            return nil
        }
        func string(_ key: String) -> String? { print[key] as? String }
        func intArray(_ key: String) -> [Int]? {
            let raw = print[key]
            let values: [Any]
            if let array = raw as? [Any] {
                values = array
            } else if let text = raw as? String,
                      let data = text.data(using: .utf8),
                      let array = try? JSONSerialization.jsonObject(with: data) as? [Any] {
                values = array
            } else {
                return nil
            }
            return values.compactMap { value in
                if let number = value as? NSNumber { return number.intValue }
                if let text = value as? String { return Int(text) }
                return nil
            }
        }

        func fanPercent(_ key: String) -> Int? {
            guard let raw = print[key] else { return nil }
            let value: Int?
            if let number = raw as? NSNumber {
                value = number.intValue
            } else if let text = raw as? String {
                value = Int(text) ?? Int(text, radix: 16)
            } else {
                value = nil
            }
            guard let value else { return nil }
            return value <= 15
                ? min(max(Int((Double(value) / 15.0 * 100.0).rounded()), 0), 100)
                : min(max(value, 0), 100)
        }

        let previousJobIdentity = Self.jobIdentity(for: snapshot)
        if let value = string("gcode_state"), !value.isEmpty { snapshot.status = value.uppercased() }
        if let value = string("subtask_name"), !value.isEmpty { snapshot.jobName = value }
        if let value = string("gcode_file"), !value.isEmpty {
            snapshot.gcodeFile = value
            if snapshot.jobName.isEmpty { snapshot.jobName = value }
        }
        if let value = string("print_type"), !value.isEmpty { snapshot.printType = value }
        let currentJobIdentity = Self.jobIdentity(for: snapshot)
        if !currentJobIdentity.isEmpty, currentJobIdentity != previousJobIdentity {
            snapshot.printFilamentTrayIDs = []
            snapshot.printFilamentLabels = []
            snapshot.printFilamentColors = []
            snapshot.filamentChangesCompleted = 0
            snapshot.filamentChangesTotal = -1
        }
        if let mapping = intArray("ams_mapping"), !mapping.isEmpty {
            snapshot.printFilamentTrayIDs = mapping
        }
        if let sequence = intArray("filament_change_sequence"), !sequence.isEmpty {
            snapshot.filamentChangesTotal = max(sequence.count - 1, 0)
        }
        if let value = int("mc_percent") ?? int("percent") ?? int("progress") {
            snapshot.progressPercent = min(max(value, 0), 100)
        }
        if let value = int("mc_remaining_time") ?? int("remaining_minutes") ?? int("remain_time") {
            snapshot.remainingMinutes = max(value, 0)
        }
        if let value = int("layer_num") ?? int("current_layer") { snapshot.currentLayer = max(value, 0) }
        if let value = int("total_layer_num") ?? int("total_layers") { snapshot.totalLayers = max(value, 0) }
        if let value = float("nozzle_temper") { snapshot.nozzleTempC = value }
        if let value = float("nozzle_target_temper") { snapshot.nozzleTargetC = value }
        if let value = float("bed_temper") { snapshot.bedTempC = value }
        if let value = float("bed_target_temper") { snapshot.bedTargetC = value }
        if let value = float("chamber_temper") { snapshot.chamberTempC = value }
        if let value = int("print_error") { snapshot.errorCode = value }
        if let value = string("wifi_signal"), !value.isEmpty { snapshot.wifiSignal = value }
        if let value = int("spd_lvl") { snapshot.speedLevel = value }
        if let value = int("spd_mag") { snapshot.speedPercent = value }
        if snapshot.speedPercent <= 0 {
            snapshot.speedPercent = [1: 50, 2: 100, 3: 124, 4: 166][snapshot.speedLevel] ?? 0
        }
        if let value = fanPercent("cooling_fan_speed") { snapshot.coolingFanPercent = value }
        if let value = fanPercent("heatbreak_fan_speed") { snapshot.heatbreakFanPercent = value }
        if let value = fanPercent("big_fan1_speed") { snapshot.auxiliaryFanPercent = value }
        if let value = fanPercent("big_fan2_speed") { snapshot.chamberFanPercent = value }
        if let value = int("hw_switch_state") { snapshot.filamentSensorState = value }
        if let value = int("ams_status") { snapshot.amsStatus = value }
        if let hms = print["hms"] as? [Any] {
            snapshot.hmsCount = hms.count
            snapshot.hmsCodes = Self.hmsCodes(from: hms)
        }

        mergeAMS(print: print)
        refreshAMSSummary()
        rememberObservedPrintFilament()
        refreshPrintFilamentSummary()
        snapshot.filamentChangesCompleted = filamentChangeCounter.update(
            jobIdentity: currentJobIdentity,
            status: snapshot.status,
            activeTray: snapshot.activeTray,
            targetTray: snapshot.targetTray,
            filamentSensorState: snapshot.filamentSensorState,
            amsStatus: snapshot.amsStatus
        )

        if let stage = print["stage"] as? [String: Any],
           let name = stage["name"] as? String, !name.isEmpty {
            snapshot.stage = name
        } else if let stageID = int("stg_cur") {
            snapshot.stage = Self.stageLabel(stageID, status: snapshot.status)
        } else {
            snapshot.stage = Self.stageLabel(nil, status: snapshot.status)
        }
        snapshot.connected = true
        snapshot.updatedAt = .now
        onPayload?(snapshot)
    }

    private func mergeAMS(print: [String: Any]) {
        guard let ams = print["ams"] as? [String: Any] else {
            if let external = Self.parseTray(
                print["vt_tray"] as? [String: Any],
                unit: -1,
                tray: -1,
                active: snapshot.activeTray == 254,
                fallback: snapshot.externalSpool
            ) {
                snapshot.externalSpool = external
            }
            return
        }

        func integer(_ value: Any?) -> Int? {
            if let number = value as? NSNumber { return number.intValue }
            if let string = value as? String { return Int(string) }
            return nil
        }
        if let value = integer(ams["tray_now"]) { snapshot.activeTray = value }
        if let value = integer(ams["tray_tar"]) { snapshot.targetTray = value }

        // Work on local values before assigning them back to `snapshot`.
        // Mutating an optional nested inside the struct while simultaneously
        // reading another snapshot property can trigger Swift's runtime
        // exclusivity trap under real, rapid MQTT AMS updates.
        let activeTray = snapshot.activeTray
        var activeUnits = snapshot.amsUnits
        for unitIndex in activeUnits.indices {
            for trayIndex in activeUnits[unitIndex].trays.indices {
                let tray = activeUnits[unitIndex].trays[trayIndex]
                activeUnits[unitIndex].trays[trayIndex].active =
                    activeTray == tray.unitIndex * 4 + tray.trayIndex
            }
        }
        snapshot.amsUnits = activeUnits
        if var externalSpool = snapshot.externalSpool {
            externalSpool.active = activeTray == 254
            snapshot.externalSpool = externalSpool
        }

        if let units = ams["ams"] as? [[String: Any]] {
            var mergedUnits = snapshot.amsUnits
            for unit in units {
                guard let unitIndex = integer(unit["id"]) else { continue }
                let priorUnit = mergedUnits.first(where: { $0.id == unitIndex })
                let humidityRaw = integer(unit["humidity_raw"])
                let humidityIndex = integer(unit["humidity"])
                let humidity: Int
                if let humidityRaw, (0...100).contains(humidityRaw) {
                    humidity = humidityRaw
                } else if let humidityIndex, (1...5).contains(humidityIndex) {
                    humidity = [10, 30, 48, 63, 85][humidityIndex - 1]
                } else {
                    humidity = priorUnit?.humidityPercent ?? -1
                }
                let temperature = (unit["temp"] as? NSNumber)?.floatValue
                    ?? Float(unit["temp"] as? String ?? "")
                    ?? priorUnit?.temperatureC
                    ?? 0
                var mergedTrays = priorUnit?.trays ?? []
                for tray in unit["tray"] as? [[String: Any]] ?? [] {
                    guard let trayIndex = integer(tray["id"]) else { continue }
                    let priorTray = mergedTrays.first(where: { $0.trayIndex == trayIndex })
                    guard let parsed = Self.parseTray(
                        tray,
                        unit: unitIndex,
                        tray: trayIndex,
                        active: activeTray == unitIndex * 4 + trayIndex,
                        fallback: priorTray
                    ) else { continue }
                    if let index = mergedTrays.firstIndex(where: { $0.trayIndex == trayIndex }) {
                        mergedTrays[index] = parsed
                    } else {
                        mergedTrays.append(parsed)
                    }
                }
                mergedTrays.sort { $0.trayIndex < $1.trayIndex }
                let mergedUnit = BambuAMSUnitTelemetry(
                    id: unitIndex,
                    humidityPercent: humidity,
                    temperatureC: temperature,
                    trays: mergedTrays
                )
                if let index = mergedUnits.firstIndex(where: { $0.id == unitIndex }) {
                    mergedUnits[index] = mergedUnit
                } else {
                    mergedUnits.append(mergedUnit)
                }
            }
            snapshot.amsUnits = mergedUnits.sorted { $0.id < $1.id }
        }

        let externalObject = (ams["vt_tray"] as? [String: Any]) ?? (print["vt_tray"] as? [String: Any])
        if let external = Self.parseTray(
            externalObject,
            unit: -1,
            tray: -1,
            active: activeTray == 254,
            fallback: snapshot.externalSpool
        ) {
            snapshot.externalSpool = external
        }
    }

    private func refreshAMSSummary() {
        snapshot.activeFilamentLabel = ""
        snapshot.activeFilamentColorHex = ""
        snapshot.activeFilamentRemainingPercent = -1
        if let active = snapshot.activeFilament {
            snapshot.activeFilamentLabel = active.displayName
            snapshot.activeFilamentColorHex = active.colorHex
            snapshot.activeFilamentRemainingPercent = active.remainingPercent
        }
        snapshot.amsHumidityPercent = -1
        snapshot.amsTemperatureC = 0
        if let unit = snapshot.primaryAMSUnit {
            snapshot.amsHumidityPercent = unit.humidityPercent
            snapshot.amsTemperatureC = unit.temperatureC
        }
    }

    private func rememberObservedPrintFilament() {
        guard snapshot.isPrinting || ["PAUSE", "PAUSED"].contains(snapshot.status.uppercased()) else { return }
        let observed = [snapshot.activeTray, snapshot.targetTray].filter { $0 >= 0 }
        for trayID in observed where !snapshot.printFilamentTrayIDs.contains(trayID) {
            snapshot.printFilamentTrayIDs.append(trayID)
        }
    }

    private func refreshPrintFilamentSummary() {
        var labels: [String] = []
        var colors: [String] = []
        var seen = Set<String>()

        func append(_ tray: BambuTrayTelemetry?) {
            guard let tray, tray.present else { return }
            let identity = tray.id.isEmpty ? "\(tray.unitIndex)-\(tray.trayIndex)" : tray.id
            guard seen.insert(identity).inserted else { return }
            labels.append(tray.displayName)
            colors.append(tray.colorHex.isEmpty ? "#FFFFFF" : tray.colorHex)
        }

        for trayID in snapshot.printFilamentTrayIDs {
            if trayID < 0 || trayID == 254 || trayID == 255 {
                append(snapshot.externalSpool)
                continue
            }
            append(snapshot.amsUnits.lazy.flatMap(\.trays).first {
                $0.unitIndex * 4 + $0.trayIndex == trayID
            })
        }
        if labels.isEmpty { append(snapshot.activeFilament) }

        snapshot.printFilamentLabels = Array(labels.prefix(8))
        snapshot.printFilamentColors = Array(colors.prefix(8))
    }

    private static func jobIdentity(for payload: BambuPrinterPayload) -> String {
        if !payload.gcodeFile.isEmpty { return payload.gcodeFile }
        return payload.jobName
    }

    private static func parseTray(
        _ object: [String: Any]?,
        unit: Int,
        tray: Int,
        active: Bool,
        fallback: BambuTrayTelemetry?
    ) -> BambuTrayTelemetry? {
        guard let object, object.count > 1 else { return nil }
        if object.count <= 2,
           object["tray_type"] == nil,
           object["tray_sub_brands"] == nil,
           object["tray_color"] == nil {
            return BambuTrayTelemetry(
                id: unit >= 0 ? "\(unit)-\(tray)" : "external",
                unitIndex: unit,
                trayIndex: tray,
                present: false,
                active: false
            )
        }
        func string(_ key: String) -> String {
            if let value = object[key] as? String { return value }
            if let value = object[key] as? NSNumber { return value.stringValue }
            return ""
        }
        let remaining = (object["remain"] as? NSNumber)?.intValue
            ?? Int(string("remain"))
            ?? fallback?.remainingPercent
            ?? -1
        let parsedType = string("tray_type")
        let parsedName = string("tray_sub_brands")
        let parsedColor = string("tray_color")
        let materialType = parsedType.isEmpty ? (fallback?.materialType ?? "") : parsedType
        let materialName = parsedName.isEmpty ? (fallback?.materialName ?? "") : parsedName
        let color = parsedColor.isEmpty ? (fallback?.colorHex ?? "") : parsedColor
        return BambuTrayTelemetry(
            id: unit >= 0 ? "\(unit)-\(tray)" : "external",
            unitIndex: unit,
            trayIndex: tray,
            present: !materialType.isEmpty || !materialName.isEmpty || !color.isEmpty,
            active: active,
            materialType: materialType,
            materialName: materialName,
            colorHex: color,
            remainingPercent: remaining
        )
    }

    private static func hmsCodes(from entries: [Any]) -> [String] {
        entries.compactMap { entry in
            guard let object = entry as? [String: Any] else { return nil }
            func uint(_ key: String) -> UInt64? {
                if let value = object[key] as? NSNumber { return value.uint64Value }
                if let text = object[key] as? String {
                    return UInt64(text) ?? UInt64(text.replacingOccurrences(of: "0x", with: ""), radix: 16)
                }
                return nil
            }
            if let attr = uint("attr"), let code = uint("code") {
                return String(format: "%08llX%08llX", attr, code)
            }
            if let code = uint("code") { return String(format: "%016llX", code) }
            return nil
        }
    }

    private static func stageLabel(_ id: Int?, status: String) -> String {
        let stages: [Int: String] = [
            0: "Printing", 1: "Bed leveling", 2: "Heatbed preheating", 3: "Sweeping XY mech mode",
            4: "Changing filament", 5: "M400 pause", 6: "Paused: filament runout", 7: "Heating hotend",
            8: "Calibrating extrusion", 9: "Scanning bed surface", 10: "Inspecting first layer",
            11: "Identifying build plate", 12: "Calibrating micro lidar", 13: "Homing toolhead",
            14: "Cleaning nozzle", 15: "Checking extruder temperature", 16: "Paused by user",
            17: "Paused: front cover removed", 18: "Calibrating micro lidar", 19: "Calibrating extrusion flow",
            20: "Paused: nozzle temperature", 21: "Paused: heatbed temperature", 22: "Filament unloading",
            23: "Paused: skipped step", 24: "Filament loading", 25: "Motor noise calibration",
            26: "Paused: AMS lost", 27: "Paused: heatbreak fan", 28: "Paused: chamber temperature",
            29: "Cooling chamber", 30: "Paused: user G-code", 31: "Motor noise showoff",
            32: "Paused: nozzle filament covered", 33: "Cutter error", 34: "First-layer error",
            35: "Nozzle clog detected", 36: "Air printing detected",
        ]
        if let id, let label = stages[id] { return label }
        switch status.uppercased() {
        case "RUNNING": return "Printing"
        case "PAUSE", "PAUSED": return "Paused"
        case "FINISH", "FINISHED": return "Print complete"
        case "FAILED", "ERROR": return "Printer needs attention"
        case "PREPARE": return "Preparing print"
        default: return "Printer idle"
        }
    }

    private func startPingTimer() {
        pingTimer?.cancel()
        let timer = DispatchSource.makeTimerSource(queue: queue)
        timer.schedule(deadline: .now() + 15, repeating: 15)
        timer.setEventHandler { [weak self] in self?.sendPacket(typeAndFlags: 0xC0, body: Data()) }
        timer.resume()
        pingTimer = timer
    }

    private func handleDisconnect(_ reason: String) {
        guard !stopped else { return }
        snapshot.connected = false
        snapshot.updatedAt = .now
        onPayload?(snapshot)
        scheduleReconnect(reason: reason)
    }

    private func scheduleReconnect(reason: String) {
        disconnectCurrentConnection()
        publishStatus("\(reason). Retrying in 5 s…")
        let item = DispatchWorkItem { [weak self] in self?.connect() }
        reconnectWorkItem = item
        queue.asyncAfter(deadline: .now() + 5, execute: item)
    }

    private func disconnectCurrentConnection() {
        connectionTimeoutWorkItem?.cancel()
        connectionTimeoutWorkItem = nil
        pingTimer?.cancel()
        pingTimer = nil
        connection?.stateUpdateHandler = nil
        connection?.cancel()
        connection = nil
        receiveBuffer.removeAll(keepingCapacity: true)
    }

    private func publishStatus(_ value: String) { onStatus?(value) }

    private func mqttString<S: StringProtocol>(_ value: S) -> Data {
        let utf8 = Data(value.utf8)
        var result = Data([UInt8((utf8.count >> 8) & 0xFF), UInt8(utf8.count & 0xFF)])
        result.append(utf8)
        return result
    }

    private func mqttRemainingLength(_ value: Int) -> [UInt8] {
        var remaining = value
        var bytes: [UInt8] = []
        repeat {
            var byte = UInt8(remaining % 128)
            remaining /= 128
            if remaining > 0 { byte |= 0x80 }
            bytes.append(byte)
        } while remaining > 0
        return bytes
    }

    private func decodeRemainingLength(in data: Data, start: Int) -> (value: Int, bytes: Int)? {
        var multiplier = 1
        var value = 0
        var cursor = start
        while cursor < data.count && cursor < start + 4 {
            let byte = Int(data[cursor])
            value += (byte & 127) * multiplier
            cursor += 1
            if byte & 128 == 0 { return (value, cursor - start) }
            multiplier *= 128
        }
        return nil
    }
}
