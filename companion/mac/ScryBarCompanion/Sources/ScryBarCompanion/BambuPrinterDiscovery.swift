import Darwin
import Foundation

struct BambuDiscoveredPrinter: Identifiable, Equatable, Sendable {
    let id: String
    let name: String
    let model: String
    let modelCode: String
    let host: String
    let serial: String

    var displayName: String {
        name.isEmpty ? "\(model) · \(host)" : "\(name) · \(model)"
    }
}

/// Chooses a printer using its stable LAN identity before transient network
/// state. Bambu printers can keep the same serial while DHCP changes their IP;
/// a provisional service-scan result must therefore never pin the app to an
/// obsolete address once a serial-bearing SSDP record is available.
enum BambuPrinterSelectionPolicy {
    static func preferredPrinter(
        in printers: [BambuDiscoveredPrinter],
        savedSerial: String,
        savedName: String,
        savedHost: String,
        selectedID: String?
    ) -> BambuDiscoveredPrinter? {
        let normalizedSerial = savedSerial.trimmingCharacters(in: .whitespacesAndNewlines)
        if !normalizedSerial.isEmpty,
           let identityMatch = printers.first(where: {
               !$0.serial.isEmpty &&
               $0.serial.caseInsensitiveCompare(normalizedSerial) == .orderedSame
           }) {
            return identityMatch
        }

        // DevName.bambu.com is the stable, user-visible LAN identity (for
        // example "3DP-039-146"). It is not the full serial required by MQTT,
        // but it is a safer way to recover the printer's current address than
        // preferring a stale selection or DHCP address.
        let normalizedName = savedName.trimmingCharacters(in: .whitespacesAndNewlines)
        if !normalizedName.isEmpty,
           let nameMatch = printers.first(where: {
               !$0.name.isEmpty &&
               $0.name.caseInsensitiveCompare(normalizedName) == .orderedSame
           }) {
            return nameMatch
        }

        if let selectedID,
           let currentSelection = printers.first(where: { $0.id == selectedID }) {
            return currentSelection
        }

        let normalizedHost = savedHost.trimmingCharacters(in: .whitespacesAndNewlines)
        if !normalizedHost.isEmpty,
           let addressMatch = printers.first(where: {
               $0.host.caseInsensitiveCompare(normalizedHost) == .orderedSame
           }) {
            return addressMatch
        }

        return printers.count == 1 ? printers[0] : nil
    }
}

enum BambuDiscoveryResponseParser {
    static func parse(_ data: Data, sourceHost: String) -> BambuDiscoveredPrinter? {
        guard let message = String(data: data, encoding: .utf8) else { return nil }
        var headers: [String: String] = [:]
        for line in message.components(separatedBy: .newlines) {
            guard let separator = line.firstIndex(of: ":") else { continue }
            let key = line[..<separator].trimmingCharacters(in: .whitespacesAndNewlines).lowercased()
            let value = line[line.index(after: separator)...].trimmingCharacters(in: .whitespacesAndNewlines)
            if !key.isEmpty, !value.isEmpty { headers[key] = value }
        }

        let serviceType = headers["st"] ?? headers["nt"] ?? ""
        let modelCode = headers["devmodel.bambu.com"] ?? ""
        guard serviceType.localizedCaseInsensitiveContains("bambulab-com:device:3dprinter") || !modelCode.isEmpty else {
            return nil
        }

        let rawSerial = headers["usn"] ?? ""
        let serial = (rawSerial
            .components(separatedBy: "::").first ?? rawSerial)
            .replacingOccurrences(of: "uuid:", with: "", options: .caseInsensitive)
            .trimmingCharacters(in: .whitespacesAndNewlines)
        // Some non-printer devices echo the requested ST verbatim without any
        // Bambu identity headers. Accepting that bare response creates a false
        // printer and prevents the verified LAN-service fallback from running.
        guard !serial.isEmpty || !modelCode.isEmpty else { return nil }
        let host = sourceHost.isEmpty ? hostFromLocation(headers["location"]) : sourceHost
        guard !host.isEmpty else { return nil }

        let model = friendlyModelName(modelCode)
        let name = headers["devname.bambu.com"] ?? ""
        return BambuDiscoveredPrinter(
            id: serial.isEmpty ? host : serial,
            name: name,
            model: model,
            modelCode: modelCode,
            host: host,
            serial: serial
        )
    }

    private static func hostFromLocation(_ location: String?) -> String {
        guard let location, !location.isEmpty else { return "" }
        if let url = URL(string: location), let host = url.host { return host }
        return location.split(separator: ":").first.map(String.init) ?? ""
    }

    private static func friendlyModelName(_ code: String) -> String {
        let names: [String: String] = [
            "N1": "Bambu Lab A1 mini",
            "N2S": "Bambu Lab A1",
            "C11": "Bambu Lab P1P",
            "C12": "Bambu Lab P1S",
            "C13": "Bambu Lab X1E",
            "BL-P001": "Bambu Lab X1",
            "BL-P002": "Bambu Lab X1 Carbon",
            "O1D": "Bambu Lab H2D",
            "O1S": "Bambu Lab H2S",
        ]
        return names[code.uppercased()] ?? (code.isEmpty ? "Bambu Lab printer" : code)
    }
}

enum BambuDiscoveryResults {
    static func mergingDuplicates(_ printers: [BambuDiscoveredPrinter]) -> [BambuDiscoveredPrinter] {
        var merged: [BambuDiscoveredPrinter] = []
        for printer in printers {
            if let index = merged.firstIndex(where: { existing in
                existing.host.caseInsensitiveCompare(printer.host) == .orderedSame ||
                (!existing.serial.isEmpty && !printer.serial.isEmpty &&
                 existing.serial.caseInsensitiveCompare(printer.serial) == .orderedSame)
            }) {
                merged[index] = richer(merged[index], printer)
            } else {
                merged.append(printer)
            }
        }
        return merged.sorted {
            $0.displayName.localizedCaseInsensitiveCompare($1.displayName) == .orderedAscending
        }
    }

    private static func richer(
        _ first: BambuDiscoveredPrinter,
        _ second: BambuDiscoveredPrinter
    ) -> BambuDiscoveredPrinter {
        let serial = preferred(first.serial, second.serial)
        let host = preferred(first.host, second.host)
        let modelCode = preferred(first.modelCode, second.modelCode)
        let genericModel = "Bambu Lab printer"
        let model = first.model == genericModel ? second.model : first.model
        let name = preferred(first.name, second.name)
        return BambuDiscoveredPrinter(
            id: serial.isEmpty ? host : serial,
            name: name,
            model: model.isEmpty ? genericModel : model,
            modelCode: modelCode,
            host: host,
            serial: serial
        )
    }

    private static func preferred(_ first: String, _ second: String) -> String {
        first.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty ? second : first
    }
}

enum BambuLANServiceFingerprint {
    /// Bambu printers expose their local MQTT broker on 8883 and at least one
    /// companion LAN service (FTPS or camera) on the supported product lines.
    /// The candidate is still authenticated with the saved serial/code before
    /// it becomes the active printer.
    static func isLikelyBambu(openPorts: Set<UInt16>) -> Bool {
        openPorts.contains(8883) && (openPorts.contains(990) || openPorts.contains(6000))
    }
}

private final class BambuCandidateCollector: @unchecked Sendable {
    private let lock = NSLock()
    private var hosts: [String] = []

    func append(_ host: String) {
        lock.lock()
        hosts.append(host)
        lock.unlock()
    }

    func snapshot() -> [String] {
        lock.lock()
        defer { lock.unlock() }
        return hosts.sorted { $0.localizedStandardCompare($1) == .orderedAscending }
    }
}

/// Conservative fallback for printers that do not answer SSDP while busy or
/// while cloud mode is active. It only scans the Mac's private /24 networks and
/// only reports hosts matching the Bambu LAN service fingerprint.
private enum BambuLANServiceScanner {
    static func scan(completion: @escaping @Sendable ([BambuDiscoveredPrinter]) -> Void) {
        let prefixes = localPrivate24Prefixes()
        guard !prefixes.isEmpty else {
            completion([])
            return
        }

        DispatchQueue.global(qos: .utility).async {
            let collector = BambuCandidateCollector()
            let group = DispatchGroup()
            let workers = DispatchQueue(
                label: "com.netmilk.ScryBarCompanion.bambu-service-scan",
                qos: .utility,
                attributes: .concurrent
            )
            let limiter = DispatchSemaphore(value: 32)

            for prefix in prefixes {
                for hostByte in UInt32(1)...UInt32(254) {
                    let address = (prefix << 8) | hostByte
                    group.enter()
                    workers.async {
                        limiter.wait()
                        defer {
                            limiter.signal()
                            group.leave()
                        }
                        guard portIsOpen(address: address, port: 8883, timeoutMilliseconds: 150) else {
                            return
                        }
                        var ports: Set<UInt16> = [8883]
                        if portIsOpen(address: address, port: 990, timeoutMilliseconds: 120) {
                            ports.insert(990)
                        }
                        if !ports.contains(990),
                           portIsOpen(address: address, port: 6000, timeoutMilliseconds: 120) {
                            ports.insert(6000)
                        }
                        guard BambuLANServiceFingerprint.isLikelyBambu(openPorts: ports) else { return }
                        collector.append(ipv4String(address))
                    }
                }
            }

            group.wait()
            let printers = collector.snapshot().map { host in
                BambuDiscoveredPrinter(
                    id: host,
                    name: "",
                    model: "Bambu Lab printer",
                    modelCode: "",
                    host: host,
                    serial: ""
                )
            }
            completion(printers)
        }
    }

    private static func localPrivate24Prefixes() -> [UInt32] {
        var interfaces: UnsafeMutablePointer<ifaddrs>?
        guard getifaddrs(&interfaces) == 0, let first = interfaces else { return [] }
        defer { freeifaddrs(interfaces) }

        var prefixes = Set<UInt32>()
        var cursor: UnsafeMutablePointer<ifaddrs>? = first
        while let interface = cursor {
            defer { cursor = interface.pointee.ifa_next }
            guard let address = interface.pointee.ifa_addr,
                  address.pointee.sa_family == UInt8(AF_INET) else { continue }
            let flags = Int32(interface.pointee.ifa_flags)
            guard (flags & IFF_UP) != 0, (flags & IFF_LOOPBACK) == 0 else { continue }
            let socketAddress = UnsafeRawPointer(address).assumingMemoryBound(to: sockaddr_in.self)
            let hostOrder = UInt32(bigEndian: socketAddress.pointee.sin_addr.s_addr)
            guard isPrivateIPv4(hostOrder) else { continue }
            prefixes.insert(hostOrder >> 8)
        }
        return prefixes.sorted()
    }

    private static func isPrivateIPv4(_ address: UInt32) -> Bool {
        let first = (address >> 24) & 0xFF
        let second = (address >> 16) & 0xFF
        return first == 10 ||
            (first == 172 && (16...31).contains(second)) ||
            (first == 192 && second == 168)
    }

    private static func ipv4String(_ address: UInt32) -> String {
        "\((address >> 24) & 0xFF).\((address >> 16) & 0xFF).\((address >> 8) & 0xFF).\(address & 0xFF)"
    }

    private static func portIsOpen(
        address: UInt32,
        port: UInt16,
        timeoutMilliseconds: Int32
    ) -> Bool {
        let descriptor = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)
        guard descriptor >= 0 else { return false }
        defer { Darwin.close(descriptor) }

        let currentFlags = fcntl(descriptor, F_GETFL, 0)
        guard currentFlags >= 0,
              fcntl(descriptor, F_SETFL, currentFlags | O_NONBLOCK) == 0 else { return false }

        var target = sockaddr_in()
        target.sin_len = UInt8(MemoryLayout<sockaddr_in>.size)
        target.sin_family = sa_family_t(AF_INET)
        target.sin_port = in_port_t(port).bigEndian
        target.sin_addr = in_addr(s_addr: address.bigEndian)
        let result = withUnsafePointer(to: &target) { pointer in
            pointer.withMemoryRebound(to: sockaddr.self, capacity: 1) {
                Darwin.connect(descriptor, $0, socklen_t(MemoryLayout<sockaddr_in>.size))
            }
        }
        if result == 0 { return true }
        guard errno == EINPROGRESS else { return false }

        var pollDescriptor = pollfd(fd: descriptor, events: Int16(POLLOUT), revents: 0)
        guard Darwin.poll(&pollDescriptor, 1, timeoutMilliseconds) > 0 else { return false }
        var socketError: Int32 = 0
        var socketErrorLength = socklen_t(MemoryLayout<Int32>.size)
        guard getsockopt(descriptor, SOL_SOCKET, SO_ERROR, &socketError, &socketErrorLength) == 0 else {
            return false
        }
        return socketError == 0
    }
}

/// Active SSDP discovery for Bambu printers. Each scan sends an M-SEARCH from
/// an ephemeral UDP port, so Bambu Studio can keep its own port 2021 listener.
final class BambuPrinterDiscovery: @unchecked Sendable {
    var onPrinters: (@Sendable ([BambuDiscoveredPrinter]) -> Void)?
    var onStatus: (@Sendable (String) -> Void)?
    var onScanningChanged: (@Sendable (Bool) -> Void)?

    private let queue = DispatchQueue(label: "com.netmilk.ScryBarCompanion.bambu-discovery")
    private var readSource: DispatchSourceRead?
    private var socketFD: Int32 = -1
    private var announcementSource: DispatchSourceRead?
    private var announcementSocketFD: Int32 = -1
    private var printersByID: [String: BambuDiscoveredPrinter] = [:]
    private var scanID = UUID()

    func start() {
        queue.async { [weak self] in self?.startOnQueue() }
    }

    func stop() {
        queue.async { [weak self] in
            guard let self else { return }
            self.scanID = UUID()
            self.stopOnQueue()
            self.onScanningChanged?(false)
        }
    }

    private func startOnQueue() {
        stopOnQueue()
        printersByID.removeAll()
        publishPrinters()
        scanID = UUID()
        let currentScanID = scanID

        let descriptor = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)
        guard descriptor >= 0 else {
            onStatus?("Printer search could not start.")
            onScanningChanged?(false)
            return
        }
        socketFD = descriptor

        var enabled: Int32 = 1
        setsockopt(descriptor, SOL_SOCKET, SO_REUSEADDR, &enabled, socklen_t(MemoryLayout<Int32>.size))
        setsockopt(descriptor, SOL_SOCKET, SO_BROADCAST, &enabled, socklen_t(MemoryLayout<Int32>.size))

        var localAddress = sockaddr_in()
        localAddress.sin_len = UInt8(MemoryLayout<sockaddr_in>.size)
        localAddress.sin_family = sa_family_t(AF_INET)
        localAddress.sin_port = 0
        localAddress.sin_addr = in_addr(s_addr: INADDR_ANY)
        let bindResult = withUnsafePointer(to: &localAddress) { pointer in
            pointer.withMemoryRebound(to: sockaddr.self, capacity: 1) {
                bind(descriptor, $0, socklen_t(MemoryLayout<sockaddr_in>.size))
            }
        }
        guard bindResult == 0 else {
            Darwin.close(descriptor)
            socketFD = -1
            onStatus?("Printer search could not use the local network.")
            onScanningChanged?(false)
            return
        }

        let source = DispatchSource.makeReadSource(fileDescriptor: descriptor, queue: queue)
        source.setEventHandler { [weak self] in self?.readAvailableDatagrams() }
        readSource = source
        source.resume()
        startAnnouncementListener()

        onScanningChanged?(true)
        onStatus?("Searching for Bambu printers…")
        sendSearch(on: descriptor)
        queue.asyncAfter(deadline: .now() + 1) { [weak self] in
            guard let self, self.scanID == currentScanID, self.socketFD == descriptor else { return }
            self.sendSearch(on: descriptor)
        }
        queue.asyncAfter(deadline: .now() + 6) { [weak self] in
            guard let self, self.scanID == currentScanID else { return }
            let count = self.printersByID.count
            if count > 0 {
                self.stopOnQueue()
                self.onScanningChanged?(false)
                self.onStatus?("Found \(count) Bambu printer\(count == 1 ? "" : "s").")
                return
            }

            self.stopOnQueue()
            self.onStatus?("Discovery was silent. Checking Bambu services on this LAN…")
            BambuLANServiceScanner.scan { [weak self] candidates in
                guard let self else { return }
                self.queue.async {
                    guard self.scanID == currentScanID else { return }
                    let merged = BambuDiscoveryResults.mergingDuplicates(
                        Array(self.printersByID.values) + candidates
                    )
                    self.printersByID = Dictionary(uniqueKeysWithValues: merged.map { ($0.id, $0) })
                    self.publishPrinters()
                    self.onScanningChanged?(false)
                    let found = self.printersByID.count
                    self.onStatus?(found == 0
                        ? "No Bambu printer answered discovery or the local service check."
                        : "Found \(found) Bambu printer\(found == 1 ? "" : "s") by LAN services.")
                }
            }
        }
    }

    private func stopOnQueue() {
        readSource?.setEventHandler {}
        readSource?.cancel()
        readSource = nil
        if socketFD >= 0 {
            Darwin.close(socketFD)
            socketFD = -1
        }
        announcementSource?.setEventHandler {}
        announcementSource?.cancel()
        announcementSource = nil
        if announcementSocketFD >= 0 {
            Darwin.close(announcementSocketFD)
            announcementSocketFD = -1
        }
    }

    private func startAnnouncementListener() {
        let descriptor = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)
        guard descriptor >= 0 else { return }

        var enabled: Int32 = 1
        setsockopt(descriptor, SOL_SOCKET, SO_REUSEADDR, &enabled, socklen_t(MemoryLayout<Int32>.size))
        setsockopt(descriptor, SOL_SOCKET, SO_REUSEPORT, &enabled, socklen_t(MemoryLayout<Int32>.size))

        var listenAddress = sockaddr_in()
        listenAddress.sin_len = UInt8(MemoryLayout<sockaddr_in>.size)
        listenAddress.sin_family = sa_family_t(AF_INET)
        listenAddress.sin_port = in_port_t(2021).bigEndian
        listenAddress.sin_addr = in_addr(s_addr: INADDR_ANY)
        let bindResult = withUnsafePointer(to: &listenAddress) { pointer in
            pointer.withMemoryRebound(to: sockaddr.self, capacity: 1) {
                bind(descriptor, $0, socklen_t(MemoryLayout<sockaddr_in>.size))
            }
        }
        guard bindResult == 0 else {
            Darwin.close(descriptor)
            return
        }

        var membership = ip_mreq()
        membership.imr_multiaddr.s_addr = inet_addr("239.255.255.250")
        membership.imr_interface.s_addr = INADDR_ANY
        _ = withUnsafePointer(to: &membership) {
            setsockopt(descriptor, IPPROTO_IP, IP_ADD_MEMBERSHIP, $0,
                       socklen_t(MemoryLayout<ip_mreq>.size))
        }

        announcementSocketFD = descriptor
        let source = DispatchSource.makeReadSource(fileDescriptor: descriptor, queue: queue)
        source.setEventHandler { [weak self] in self?.readAvailableDatagrams(from: descriptor) }
        announcementSource = source
        source.resume()
    }

    private func sendSearch(on descriptor: Int32) {
        let request = """
        M-SEARCH * HTTP/1.1\r
        HOST: 239.255.255.250:2021\r
        MAN: "ssdp:discover"\r
        MX: 2\r
        ST: urn:bambulab-com:device:3dprinter:1\r
        \r
        """
        let bytes = Array(request.utf8)
        send(bytes, to: "239.255.255.250", descriptor: descriptor)
        send(bytes, to: "255.255.255.255", descriptor: descriptor)
    }

    private func send(_ bytes: [UInt8], to host: String, descriptor: Int32) {
        var target = sockaddr_in()
        target.sin_len = UInt8(MemoryLayout<sockaddr_in>.size)
        target.sin_family = sa_family_t(AF_INET)
        target.sin_port = in_port_t(2021).bigEndian
        inet_pton(AF_INET, host, &target.sin_addr)
        bytes.withUnsafeBytes { payload in
            withUnsafePointer(to: &target) { pointer in
                pointer.withMemoryRebound(to: sockaddr.self, capacity: 1) {
                    _ = sendto(descriptor, payload.baseAddress, payload.count, 0, $0,
                               socklen_t(MemoryLayout<sockaddr_in>.size))
                }
            }
        }
    }

    private func readAvailableDatagrams() {
        readAvailableDatagrams(from: socketFD)
    }

    private func readAvailableDatagrams(from descriptor: Int32) {
        guard descriptor >= 0 else { return }
        while true {
            var bytes = [UInt8](repeating: 0, count: 4_096)
            var sourceAddress = sockaddr_in()
            var sourceLength = socklen_t(MemoryLayout<sockaddr_in>.size)
            let count = bytes.withUnsafeMutableBytes { payload in
                withUnsafeMutablePointer(to: &sourceAddress) { pointer in
                    pointer.withMemoryRebound(to: sockaddr.self, capacity: 1) {
                        recvfrom(descriptor, payload.baseAddress, payload.count, MSG_DONTWAIT, $0, &sourceLength)
                    }
                }
            }
            guard count > 0 else { return }

            var address = sourceAddress.sin_addr
            var buffer = [CChar](repeating: 0, count: Int(INET_ADDRSTRLEN))
            inet_ntop(AF_INET, &address, &buffer, socklen_t(INET_ADDRSTRLEN))
            let hostBytes = buffer.prefix { $0 != 0 }.map { UInt8(bitPattern: $0) }
            let host = String(decoding: hostBytes, as: UTF8.self)
            let data = Data(bytes.prefix(count))
            guard let printer = BambuDiscoveryResponseParser.parse(data, sourceHost: host) else { continue }
            let merged = BambuDiscoveryResults.mergingDuplicates(Array(printersByID.values) + [printer])
            printersByID = Dictionary(uniqueKeysWithValues: merged.map { ($0.id, $0) })
            publishPrinters()
            onStatus?("Found \(printersByID.count) Bambu printer\(printersByID.count == 1 ? "" : "s").")
        }
    }

    private func publishPrinters() {
        let printers = printersByID.values.sorted {
            $0.displayName.localizedCaseInsensitiveCompare($1.displayName) == .orderedAscending
        }
        onPrinters?(printers)
    }
}
