import Foundation
import IOKit

// MARK: - MacStatsProvider

/// Samples macOS system statistics and returns a MacStatsPayload.
/// - RAM: via host_statistics64 (vm_statistics64_data_t)
/// - Disk: via FileManager.attributesOfFileSystem
/// - CPU/GPU temp: via IOKit SMC (Intel + Apple Silicon best-effort; returns nil if key absent)
final class MacStatsProvider: @unchecked Sendable {

    private let smcReader = SMCReader.open()  // nil on unsupported hardware

    func snapshot() -> MacStatsPayload {
        let ram  = sampleRAM()
        let disk = sampleDisk()
        let cpu  = smcReader.flatMap { $0.readTemp(keys: ["TC0P", "TC0D", "TC0E", "Tp09", "Tp0P"]) }
        let gpu  = smcReader.flatMap { $0.readTemp(keys: ["TGDD", "TG0D", "TGOP", "Tg05", "Tg0f"]) }

        return MacStatsPayload(
            cpuTempC:   cpu,
            gpuTempC:   gpu,
            ramUsedGB:  ram.usedGB,
            ramTotalGB: ram.totalGB,
            diskUsedGB: disk.usedGB,
            diskTotalGB: disk.totalGB,
            updatedAt: Date()
        )
    }

    // MARK: RAM

    private func sampleRAM() -> (usedGB: Float, totalGB: Float) {
        let totalBytes = Float(ProcessInfo.processInfo.physicalMemory)
        let pageSize   = Float(getpagesize())

        var stats = vm_statistics64_data_t()
        var count = mach_msg_type_number_t(
            MemoryLayout<vm_statistics64_data_t>.size / MemoryLayout<integer_t>.size
        )

        let kr = withUnsafeMutablePointer(to: &stats) { ptr in
            ptr.withMemoryRebound(to: integer_t.self, capacity: Int(count)) {
                host_statistics64(mach_host_self(), HOST_VM_INFO64, $0, &count)
            }
        }
        guard kr == KERN_SUCCESS else { return (0, totalBytes / 1_073_741_824) }

        // free + speculative pages are considered "available"
        let freePgs      = Float(stats.free_count + stats.speculative_count)
        let inactivePgs  = Float(stats.inactive_count)
        let usedBytes    = totalBytes - (freePgs + inactivePgs) * pageSize
        return (
            usedGB:  max(usedBytes, 0) / 1_073_741_824,
            totalGB: totalBytes / 1_073_741_824
        )
    }

    // MARK: Disk

    private func sampleDisk() -> (usedGB: Float, totalGB: Float) {
        guard let attrs = try? FileManager.default.attributesOfFileSystem(forPath: "/"),
              let total = attrs[.systemSize] as? Int64,
              let free  = attrs[.systemFreeSize] as? Int64 else {
            return (0, 0)
        }
        return (
            usedGB:  Float(total - free) / 1_073_741_824,
            totalGB: Float(total) / 1_073_741_824
        )
    }
}

// MARK: - SMCReader

/// Minimal IOKit SMC reader for temperature sensors.
///
/// The SMC IPC struct is 80 bytes with a known fixed layout (verified against
/// SMCKit / open-apple-smc-driver sources):
///
///   offset  0: key          (4 bytes, big-endian FourCC)
///   offset  4: vers         (6 bytes)
///   offset 10: padding      (2 bytes)
///   offset 12: pLimitData   (16 bytes)
///   offset 28: keyInfo.dataSize       (4 bytes, little-endian)
///   offset 32: keyInfo.dataType       (4 bytes, big-endian FourCC)
///   offset 36: keyInfo.dataAttributes (1 byte)
///   offset 37: 3 bytes padding
///   offset 40: result       (1 byte)
///   offset 41: status       (1 byte)
///   offset 42: data8        (1 byte — command selector)
///   offset 43: 1 byte padding
///   offset 44: data32       (4 bytes)
///   offset 48: bytes[32]    (value payload)
///   total: 80 bytes
///
/// Selector 2 (kSMCHandleYPCEvent) with data8 = 9 reads key info,
/// data8 = 5 reads key value.

final class SMCReader: @unchecked Sendable {

    private var conn: io_connect_t = 0

    private init() {}

    static func open() -> SMCReader? {
        let reader  = SMCReader()
        let service = IOServiceGetMatchingService(kIOMainPortDefault,
                                                  IOServiceMatching("AppleSMC"))
        guard service != IO_OBJECT_NULL else { return nil }
        defer { IOObjectRelease(service) }
        guard IOServiceOpen(service, mach_task_self_, 0, &reader.conn) == KERN_SUCCESS else {
            return nil
        }
        return reader
    }

    deinit { IOServiceClose(conn) }

    // MARK: Public

    /// Returns temperature in Celsius for the first key that resolves successfully.
    func readTemp(keys: [String]) -> Float? {
        for key in keys {
            if let t = readTemperature(key) { return t }
        }
        return nil
    }

    // MARK: Private

    private func readTemperature(_ key: String) -> Float? {
        guard key.count == 4 else { return nil }

        // 1. Get key info (size + type)
        guard let info = getKeyInfo(key) else { return nil }

        // We only handle sp78 (signed 8.8 fixed-point, 2 bytes) for temp sensors
        let sp78type = fourCC("sp78")
        guard info.dataType == sp78type, info.dataSize >= 2 else { return nil }

        // 2. Read raw bytes
        guard let bytes = readKey(key, dataType: info.dataType, dataSize: info.dataSize),
              bytes.count >= 2 else { return nil }

        // sp78: big-endian signed fixed-point 8.8
        let raw = Int16(bitPattern: UInt16(bytes[0]) << 8 | UInt16(bytes[1]))
        let celsius = Float(raw) / 256.0

        // Sanity range: 0..150°C
        guard celsius > 0 && celsius < 150 else { return nil }
        return celsius
    }

    private func getKeyInfo(_ key: String) -> (dataSize: UInt32, dataType: UInt32)? {
        var input = smcBuffer()
        storeKeyBE(key, in: &input, at: 0)
        input[42] = 9  // kSMCGetKeyInfo

        guard let output = callSMC(input: input) else { return nil }

        let dataSize = loadUInt32LE(output, at: 28)
        let dataType = loadUInt32BE(output, at: 32)
        guard dataSize > 0 && dataSize <= 32 else { return nil }
        return (dataSize, dataType)
    }

    private func readKey(_ key: String, dataType: UInt32, dataSize: UInt32) -> [UInt8]? {
        var input = smcBuffer()
        storeKeyBE(key, in: &input, at: 0)
        storeUInt32LE(dataSize, in: &input, at: 28)
        storeUInt32BE(dataType, in: &input, at: 32)
        input[42] = 5  // kSMCReadKey

        guard let output = callSMC(input: input) else { return nil }
        return Array(output[48..<(48 + Int(dataSize))])
    }

    private func callSMC(input: Data) -> Data? {
        var inputCopy = input
        var output    = smcBuffer()
        var outputSize = 80

        let kr = inputCopy.withUnsafeMutableBytes { inPtr in
            output.withUnsafeMutableBytes { outPtr in
                IOConnectCallStructMethod(
                    conn, 2,
                    inPtr.baseAddress,  80,
                    outPtr.baseAddress, &outputSize
                )
            }
        }
        guard kr == KERN_SUCCESS else { return nil }
        return output
    }

    // MARK: Helpers

    private func smcBuffer() -> Data { Data(count: 80) }

    /// 4-char FourCC as a big-endian UInt32 (e.g. "sp78" → 0x73703738)
    private func fourCC(_ s: String) -> UInt32 {
        let b = Array(s.utf8)
        return UInt32(b[0]) << 24 | UInt32(b[1]) << 16 | UInt32(b[2]) << 8 | UInt32(b[3])
    }

    /// Store a FourCC string as 4 ASCII bytes (big-endian)
    private func storeKeyBE(_ key: String, in data: inout Data, at offset: Int) {
        let chars = Array(key.utf8)
        for i in 0..<4 { data[offset + i] = chars[i] }
    }

    /// Store a UInt32 as big-endian bytes (for FourCC dataType)
    private func storeUInt32BE(_ value: UInt32, in data: inout Data, at offset: Int) {
        data[offset]     = UInt8((value >> 24) & 0xFF)
        data[offset + 1] = UInt8((value >> 16) & 0xFF)
        data[offset + 2] = UInt8((value >>  8) & 0xFF)
        data[offset + 3] = UInt8( value        & 0xFF)
    }

    /// Store a UInt32 as little-endian bytes (for integer fields like dataSize)
    private func storeUInt32LE(_ value: UInt32, in data: inout Data, at offset: Int) {
        data[offset]     = UInt8( value        & 0xFF)
        data[offset + 1] = UInt8((value >>  8) & 0xFF)
        data[offset + 2] = UInt8((value >> 16) & 0xFF)
        data[offset + 3] = UInt8((value >> 24) & 0xFF)
    }

    /// Load a UInt32 from big-endian bytes (for FourCC dataType)
    private func loadUInt32BE(_ data: Data, at offset: Int) -> UInt32 {
        UInt32(data[offset]) << 24 | UInt32(data[offset + 1]) << 16 |
        UInt32(data[offset + 2]) << 8 | UInt32(data[offset + 3])
    }

    /// Load a UInt32 from little-endian bytes (for integer fields like dataSize)
    private func loadUInt32LE(_ data: Data, at offset: Int) -> UInt32 {
        UInt32(data[offset]) | UInt32(data[offset + 1]) << 8 |
        UInt32(data[offset + 2]) << 16 | UInt32(data[offset + 3]) << 24
    }
}
