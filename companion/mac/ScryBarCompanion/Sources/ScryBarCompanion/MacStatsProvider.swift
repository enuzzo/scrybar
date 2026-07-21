import Foundation
import IOKit

// MARK: - MacStatsProvider

/// Samples macOS system statistics and returns a MacStatsPayload.
/// - RAM: via host_statistics64 (vm_statistics64_data_t)
/// - Disk: via FileManager.attributesOfFileSystem
/// - CPU usage: via host_processor_info (PROCESSOR_CPU_LOAD_INFO) delta between polls
/// - GPU usage: via IOKit IOAccelerator PerformanceStatistics ("Device Utilization %")
/// - CPU/GPU temp: via `macmon pipe` subprocess (brew install macmon) — works on M1–M5 without sudo.
///   Falls back to SMC reader for Intel Macs. Returns nil if macmon is absent.
final class MacStatsProvider: @unchecked Sendable {

    private let smcReader = SMCReader.open()  // Intel fallback; nil on Apple Silicon

    /// Previous CPU tick snapshot for delta-based usage calculation.
    private var lastCPUTicks: [(user: UInt32, sys: UInt32, idle: UInt32, nice: UInt32)] = []

    /// Resolved path to macmon binary (searched once at init).
    private let macmonPath: String? = {
        for p in ["/opt/homebrew/bin/macmon", "/usr/local/bin/macmon"] {
            if FileManager.default.isExecutableFile(atPath: p) { return p }
        }
        return nil
    }()

    /// Long-lived macmon process. Started lazily on first temperature sample,
    /// kept alive across snapshot() calls so we don't fork+exec 12×/min.
    private var macmonProc: Process?
    private var macmonOutBuffer = Data()
    private let macmonLock = NSLock()
    private var macmonLatest: (cpu: Float?, gpu: Float?, at: Date)?

    deinit {
        stopMacmon()
    }

    /// Explicit shutdown for app termination — deinit never runs for
    /// app-lifetime singletons, so the macmon subprocess must be stopped here.
    func stop() {
        stopMacmon()
    }

    private func stopMacmon() {
        macmonLock.lock(); defer { macmonLock.unlock() }
        if let p = macmonProc, p.isRunning {
            p.terminate()
            p.waitUntilExit()
        }
        macmonProc = nil
        macmonOutBuffer.removeAll(keepingCapacity: false)
        macmonLatest = nil
    }

    func snapshot() -> MacStatsPayload {
        let ram      = sampleRAM()
        let disk     = sampleDisk()
        let cpuUsage = sampleCPUUsage()
        let gpuUsage = sampleGPUUsage()

        // Temperatures: try macmon first (Apple Silicon M1–M5), then SMC (Intel)
        let (cpuTemp, gpuTemp) = sampleTemps()

        return MacStatsPayload(
            cpuTempC:    cpuTemp,
            gpuTempC:    gpuTemp,
            cpuUsagePct: cpuUsage,
            gpuUsagePct: gpuUsage,
            ramUsedGB:   ram.usedGB,
            ramTotalGB:  ram.totalGB,
            diskUsedGB:  disk.usedGB,
            diskTotalGB: disk.totalGB,
            updatedAt:   Date()
        )
    }

    // MARK: Temperatures

    /// Returns (cpuTemp, gpuTemp) in °C.
    /// Uses a long-lived `macmon pipe` subprocess if available; falls back to SMC.
    private func sampleTemps() -> (cpu: Float?, gpu: Float?) {
        if let path = macmonPath {
            return readMacmonLatest(path: path)
        }
        // Intel SMC fallback
        let cpu = smcReader.flatMap { $0.readTemp(keys: ["TC0P","TC0D","TC0E"]) }
        let gpu = smcReader.flatMap { $0.readTemp(keys: ["TGDD","TG0D","TGOP"]) }
        return (cpu, gpu)
    }

    /// Read the latest temperature emitted by the long-lived `macmon pipe` process.
    /// Lazy-starts macmon on the first call. Restarts it if it died unexpectedly.
    /// Each line of stdout is one JSON object emitted at 1 Hz (`-i 1000`); we keep
    /// the most recent one parsed and just hand it back per snapshot tick.
    private func readMacmonLatest(path: String) -> (cpu: Float?, gpu: Float?) {
        ensureMacmonRunning(path: path)

        macmonLock.lock()
        let latest = macmonLatest
        macmonLock.unlock()

        guard let l = latest, Date().timeIntervalSince(l.at) < 30 else {
            return (nil, nil)
        }
        return (l.cpu, l.gpu)
    }

    private func ensureMacmonRunning(path: String) {
        macmonLock.lock(); defer { macmonLock.unlock() }
        if let p = macmonProc, p.isRunning { return }

        let proc = Process()
        proc.executableURL = URL(fileURLWithPath: path)
        proc.arguments = ["pipe", "-s", "1", "-i", "1000"]

        let outPipe = Pipe()
        let errPipe = Pipe()
        proc.standardOutput = outPipe
        proc.standardError = errPipe

        outPipe.fileHandleForReading.readabilityHandler = { [weak self] handle in
            let chunk = handle.availableData
            guard !chunk.isEmpty else { return }
            self?.consumeMacmonChunk(chunk)
        }

        proc.terminationHandler = { [weak self] _ in
            // Drop the buffered handler so the dead process doesn't keep firing.
            outPipe.fileHandleForReading.readabilityHandler = nil
            self?.macmonLock.lock()
            self?.macmonProc = nil
            self?.macmonOutBuffer.removeAll(keepingCapacity: false)
            self?.macmonLock.unlock()
        }

        do {
            try proc.run()
            macmonProc = proc
        } catch {
            macmonProc = nil
        }
    }

    private func consumeMacmonChunk(_ chunk: Data) {
        macmonLock.lock(); defer { macmonLock.unlock() }
        macmonOutBuffer.append(chunk)
        // macmon emits one JSON object per line. Parse complete lines and keep
        // only the latest parsed temps; cap buffer to defend against runaway.
        while let nl = macmonOutBuffer.firstIndex(of: 0x0A) {
            let line = macmonOutBuffer.subdata(in: 0..<nl)
            macmonOutBuffer.removeSubrange(0...nl)
            guard !line.isEmpty,
                  let json = try? JSONSerialization.jsonObject(with: line) as? [String: Any],
                  let temp = json["temp"] as? [String: Any] else { continue }
            let cpu = (temp["cpu_temp_avg"] as? NSNumber).map { Float($0.doubleValue) }
            let gpu = (temp["gpu_temp_avg"] as? NSNumber).map { Float($0.doubleValue) }
            macmonLatest = (cpu, gpu, Date())
        }
        if macmonOutBuffer.count > 16 * 1024 {
            macmonOutBuffer.removeAll(keepingCapacity: false)
        }
    }

    // MARK: CPU Usage

    /// Returns overall CPU usage in % (0–100) using tick deltas from `host_processor_info`.
    /// Returns nil on the very first call (no previous baseline yet).
    private func sampleCPUUsage() -> Float? {
        var cpuInfoArray: processor_info_array_t?
        var cpuInfoCount: mach_msg_type_number_t = 0
        var numCPUs: natural_t = 0

        let kr = host_processor_info(mach_host_self(),
                                     PROCESSOR_CPU_LOAD_INFO,
                                     &numCPUs,
                                     &cpuInfoArray,
                                     &cpuInfoCount)
        guard kr == KERN_SUCCESS, let info = cpuInfoArray, numCPUs > 0 else { return nil }
        defer {
            let size = vm_size_t(Int(cpuInfoCount) * MemoryLayout<integer_t>.size)
            vm_deallocate(mach_task_self_, vm_address_t(bitPattern: info), size)
        }

        let stride = Int(CPU_STATE_MAX)
        var current: [(user: UInt32, sys: UInt32, idle: UInt32, nice: UInt32)] = []
        for i in 0..<Int(numCPUs) {
            let base = i * stride
            let u = UInt32(bitPattern: info[base + Int(CPU_STATE_USER)])
            let s = UInt32(bitPattern: info[base + Int(CPU_STATE_SYSTEM)])
            let d = UInt32(bitPattern: info[base + Int(CPU_STATE_IDLE)])
            let n = UInt32(bitPattern: info[base + Int(CPU_STATE_NICE)])
            current.append((u, s, d, n))
        }

        guard !lastCPUTicks.isEmpty, lastCPUTicks.count == current.count else {
            lastCPUTicks = current
            return nil
        }

        var totalActive: UInt64 = 0
        var totalAll:    UInt64 = 0
        for i in 0..<current.count {
            let dUser = UInt64(current[i].user &- lastCPUTicks[i].user)
            let dSys  = UInt64(current[i].sys  &- lastCPUTicks[i].sys)
            let dIdle = UInt64(current[i].idle &- lastCPUTicks[i].idle)
            let dNice = UInt64(current[i].nice &- lastCPUTicks[i].nice)
            let active = dUser + dSys + dNice
            totalActive += active
            totalAll    += active + dIdle
        }
        lastCPUTicks = current
        guard totalAll > 0 else { return nil }
        return Float(totalActive) / Float(totalAll) * 100.0
    }

    // MARK: GPU Usage

    /// Returns GPU utilisation % via IOAccelerator PerformanceStatistics.
    /// Works on Apple Silicon and Intel Macs without entitlements.
    private func sampleGPUUsage() -> Float? {
        var iter: io_iterator_t = IO_OBJECT_NULL
        let kr = IOServiceGetMatchingServices(kIOMainPortDefault,
                                              IOServiceMatching("IOAccelerator"),
                                              &iter)
        guard kr == KERN_SUCCESS else { return nil }
        defer { IOObjectRelease(iter) }

        var best: Float? = nil
        var service = IOIteratorNext(iter)
        while service != IO_OBJECT_NULL {
            defer { IOObjectRelease(service) }
            var props: Unmanaged<CFMutableDictionary>?
            if IORegistryEntryCreateCFProperties(service, &props, kCFAllocatorDefault, 0) == KERN_SUCCESS,
               let dict = props?.takeRetainedValue() as? [String: Any],
               let perfStats = dict["PerformanceStatistics"] as? [String: Any] {
                // "Device Utilization %" is the primary key; some older drivers use "GPU Activity(%)"
                let val: Float?
                if let v = perfStats["Device Utilization %"] as? NSNumber {
                    val = v.floatValue
                } else if let v = perfStats["GPU Activity(%)"] as? NSNumber {
                    val = v.floatValue
                } else {
                    val = nil
                }
                if let v = val {
                    best = max(best ?? 0, v)
                }
            }
            service = IOIteratorNext(iter)
        }
        return best
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

    deinit {
        if conn != 0 { IOServiceClose(conn) }
    }

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

        let sp78type = fourCC("sp78")
        let fltType  = fourCC("flt ")   // IEEE 754 float — used on Apple Silicon M-series

        let celsius: Float

        if info.dataType == sp78type, info.dataSize >= 2 {
            // sp78: big-endian signed 8.8 fixed-point (2 bytes) — Intel + some Apple Silicon
            guard let bytes = readKey(key, dataType: info.dataType, dataSize: info.dataSize),
                  bytes.count >= 2 else { return nil }
            let raw = Int16(bitPattern: UInt16(bytes[0]) << 8 | UInt16(bytes[1]))
            celsius = Float(raw) / 256.0

        } else if info.dataType == fltType, info.dataSize >= 4 {
            // flt : IEEE 754 float, big-endian (4 bytes) — Apple Silicon M-series
            guard let bytes = readKey(key, dataType: info.dataType, dataSize: info.dataSize),
                  bytes.count >= 4 else { return nil }
            let bits = UInt32(bytes[0]) << 24 | UInt32(bytes[1]) << 16
                     | UInt32(bytes[2]) <<  8 | UInt32(bytes[3])
            celsius = Float(bitPattern: bits)

        } else {
            return nil
        }

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
