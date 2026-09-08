import Foundation
import Network
import Security

/// Independent implementation of the documented A1/P1 LAN JPEG framing.
struct BambuCameraFrames {
    static let maximumFrameSize = 2 * 1024 * 1024
    private var buffer = Data()

    static func authentication(accessCode: String) -> Data {
        var packet = Data([0x40, 0, 0, 0, 0, 0x30, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0])
        for value in ["bblp", accessCode] {
            let bytes = Array(value.utf8.prefix(32))
            packet.append(contentsOf: bytes)
            packet.append(contentsOf: repeatElement(UInt8(0), count: 32 - bytes.count))
        }
        return packet
    }

    mutating func append(_ data: Data) throws -> [Data] {
        buffer.append(data)
        var frames: [Data] = []
        while buffer.count >= 16 {
            let length = (0..<4).reduce(0) { $0 | Int(buffer[$1]) << ($1 * 8) }
            guard (4...Self.maximumFrameSize).contains(length) else { throw URLError(.cannotDecodeContentData) }
            guard buffer.count >= length + 16 else { break }
            let frame = buffer.subdata(in: 16..<(16 + length))
            guard frame.starts(with: [0xff, 0xd8]), frame.suffix(2) == Data([0xff, 0xd9]) else {
                throw URLError(.cannotDecodeContentData)
            }
            frames.append(frame)
            buffer = Data(buffer.dropFirst(length + 16))
        }
        guard buffer.count <= Self.maximumFrameSize + 16 else { throw URLError(.dataLengthExceedsMaximum) }
        return frames
    }
}

protocol BambuCameraStreaming: AnyObject {
    var onFrame: (@Sendable (Data) -> Void)? { get set }
    var onStatus: (@Sendable (String) -> Void)? { get set }
    func start(configuration: BambuPrinterConfiguration)
    func stop()
}

final class BambuCameraTransport: BambuCameraStreaming, @unchecked Sendable {
    var onFrame: (@Sendable (Data) -> Void)?
    var onStatus: (@Sendable (String) -> Void)?
    private let queue = DispatchQueue(label: "com.netmilk.ScryBarCompanion.camera")
    private var connection: NWConnection?
    private var parser = BambuCameraFrames()
    private var watchdog: DispatchWorkItem?

    func start(configuration: BambuPrinterConfiguration) {
        queue.async { [weak self] in
            guard let self else { return }
            self.close()
            guard configuration.isReady else { self.onStatus?("Configure the printer first"); return }
            let tls = NWProtocolTLS.Options()
            sec_protocol_options_set_verify_block(tls.securityProtocolOptions, { _, _, complete in complete(true) }, self.queue)
            let tcp = NWProtocolTCP.Options()
            tcp.connectionTimeout = 8
            let connection = NWConnection(host: .init(configuration.host), port: 6000, using: NWParameters(tls: tls, tcp: tcp))
            self.connection = connection
            self.parser = BambuCameraFrames()
            self.onStatus?("Connecting…")
            connection.stateUpdateHandler = { [weak self, weak connection] state in
                guard let self, let connection, self.connection === connection else { return }
                switch state {
                case .ready:
                    connection.send(content: BambuCameraFrames.authentication(accessCode: configuration.accessCode), completion: .contentProcessed { [weak self] error in
                        guard let self, self.connection === connection else { return }
                        if error != nil { self.fail() } else { self.receive(connection) }
                    })
                case .failed: self.fail()
                default: break
                }
            }
            connection.start(queue: self.queue)
            self.armWatchdog()
        }
    }

    func stop() { queue.async { [weak self] in self?.close() } }
    private func close() {
        watchdog?.cancel()
        connection?.stateUpdateHandler = nil
        connection?.cancel()
        connection = nil
        parser = BambuCameraFrames()
    }
    private func fail() { close(); onStatus?("Camera unavailable — check LAN liveview, then retry") }
    private func armWatchdog() {
        watchdog?.cancel()
        let work = DispatchWorkItem { [weak self] in self?.fail() }
        watchdog = work
        queue.asyncAfter(deadline: .now() + 15, execute: work)
    }
    private func receive(_ connection: NWConnection) {
        connection.receive(minimumIncompleteLength: 1, maximumLength: 65536) { [weak self, weak connection] data, _, complete, error in
            guard let self, let connection, self.connection === connection else { return }
            if let data {
                do {
                    for frame in try self.parser.append(data) {
                        self.armWatchdog()
                        self.onFrame?(frame)
                    }
                } catch { self.fail(); return }
            }
            if complete || error != nil { self.fail(); return }
            self.receive(connection)
        }
    }
}
