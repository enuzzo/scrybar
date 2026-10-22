import AppKit
import Darwin
import Foundation

struct NetworkAppActivity: Identifiable, Sendable, Equatable {
    let id: String
    let name: String
    let downloadBytesPerSec: Double
    let uploadBytesPerSec: Double
}

struct NetworkStatsSnapshot: Sendable {
    let downloadBytesPerSec: Double?
    let uploadBytesPerSec: Double?
    let apps: [NetworkAppActivity]
    let attributionAvailable: Bool
    let attributionMessage: String?
    var scope: NetworkScope = .internet
    var coveragePartial = false
}

struct NetworkByteCounters: Sendable, Equatable {
    let download: UInt64
    let upload: UInt64
}

struct NetworkByteRates: Sendable, Equatable {
    let download: Double
    let upload: Double
}

/// Counter deltas use actual monotonic elapsed time. A reset, missing baseline,
/// or a gap (including sleep) never becomes a lifetime total or a fake zero.
struct NetworkRateTracker<Key: Hashable> {
    private var previous: [Key: NetworkByteCounters] = [:]
    private var previousAt: TimeInterval?
    let maximumInterval: TimeInterval

    init(maximumInterval: TimeInterval = 5) {
        self.maximumInterval = maximumInterval
    }

    mutating func reset() {
        previous = [:]
        previousAt = nil
    }

    mutating func update(_ current: [Key: NetworkByteCounters], at now: TimeInterval) -> [Key: NetworkByteRates]? {
        let old = previous
        let oldAt = previousAt
        previous = current
        previousAt = now
        guard let oldAt else { return nil }
        let elapsed = now - oldAt
        guard elapsed.isFinite, elapsed > 0, elapsed <= maximumInterval else { return nil }
        var rates: [Key: NetworkByteRates] = [:]
        for (key, value) in current {
            guard let prior = old[key], value.download >= prior.download, value.upload >= prior.upload else { continue }
            rates[key] = .init(download: Double(value.download - prior.download) / elapsed,
                               upload: Double(value.upload - prior.upload) / elapsed)
        }
        // Empty interfaces/processes are an observed idle/disconnected sample.
        // Existing entries with no comparable baseline remain unavailable.
        return rates.isEmpty && !current.isEmpty ? nil : rates
    }
}

struct NettopProcessKey: Hashable, Sendable {
    let pid: Int32
    let name: String
}

struct NettopCounterSample: Sendable {
    let counters: [NettopProcessKey: NetworkByteCounters]
    let sampledAt: TimeInterval
}

/// Reads only process names/PIDs and byte counters; connection rows cannot be
/// accepted. Each repeated CSV header closes the preceding complete sample.
struct NettopCSVParser {
    private var incoming = Data()
    private var columns: (download: Int, upload: Int)?
    private var current: [NettopProcessKey: NetworkByteCounters] = [:]
    private var sampledAt: TimeInterval?
    private var sawDataRow = false

    mutating func consume(_ chunk: Data, at now: TimeInterval) -> [NettopCounterSample] {
        incoming.append(chunk)
        guard incoming.count <= 512 * 1024 else {
            self = .init()
            return []
        }
        var samples: [NettopCounterSample] = []
        while let end = incoming.firstIndex(of: 0x0A) {
            let line = String(decoding: incoming.prefix(upTo: end), as: UTF8.self)
            incoming.removeSubrange(...end)
            let fields = Self.csvFields(line.trimmingCharacters(in: .newlines))
            if let download = fields.firstIndex(of: "bytes_in"),
               let upload = fields.firstIndex(of: "bytes_out"), fields.first == "" {
                if let sampledAt, !current.isEmpty || !sawDataRow {
                    samples.append(.init(counters: current, sampledAt: sampledAt))
                }
                columns = (download, upload)
                current = [:]
                sampledAt = now
                sawDataRow = false
                continue
            }
            if !line.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty { sawDataRow = true }
            guard let columns, sampledAt != nil,
                  fields.count > max(columns.download, columns.upload),
                  let label = fields.first, let dot = label.lastIndex(of: "."),
                  let pid = Int32(label[label.index(after: dot)...]), pid >= 0,
                  let download = UInt64(fields[columns.download]),
                  let upload = UInt64(fields[columns.upload]) else { continue }
            let name = String(label[..<dot]).trimmingCharacters(in: .whitespaces)
            guard !name.isEmpty, current.count < 4096 else { continue }
            let key = NettopProcessKey(pid: pid, name: name)
            // A duplicated process row is a repeated observation, not another
            // process. Keep its largest counters instead of doubling traffic.
            let old = current[key]
            current[key] = .init(download: max(download, old?.download ?? 0),
                                 upload: max(upload, old?.upload ?? 0))
        }
        return samples
    }

    static func csvFields(_ line: String) -> [String] {
        var result: [String] = [], field = ""
        var quoted = false
        var cursor = line.startIndex
        while cursor < line.endIndex {
            let char = line[cursor]
            let next = line.index(after: cursor)
            if char == "\"" {
                if quoted, next < line.endIndex, line[next] == "\"" {
                    field.append("\"")
                    cursor = line.index(after: next)
                    continue
                }
                quoted.toggle()
            } else if char == ",", !quoted {
                result.append(field.trimmingCharacters(in: .whitespaces))
                field = ""
            } else {
                field.append(char)
            }
            cursor = next
        }
        result.append(field.trimmingCharacters(in: .whitespaces))
        return result
    }
}

struct NetworkAppIdentity: Sendable {
    let id: String
    let name: String
}

enum NetworkActivityBuilder {
    static func activities(
        _ rates: [NettopProcessKey: NetworkByteRates],
        resolve: (NettopProcessKey) -> NetworkAppIdentity
    ) -> [NetworkAppActivity] {
        var apps: [String: NetworkAppActivity] = [:]
        for (process, rate) in rates {
            guard rate.download.isFinite, rate.upload.isFinite,
                  rate.download >= 0, rate.upload >= 0,
                  rate.download > 0 || rate.upload > 0 else { continue }
            let identity = resolve(process)
            let old = apps[identity.id]
            apps[identity.id] = .init(id: identity.id, name: identity.name,
                downloadBytesPerSec: (old?.downloadBytesPerSec ?? 0) + rate.download,
                uploadBytesPerSec: (old?.uploadBytesPerSec ?? 0) + rate.upload)
        }
        return apps.values.sorted {
            let left = $0.downloadBytesPerSec + $0.uploadBytesPerSec
            let right = $1.downloadBytesPerSec + $1.uploadBytesPerSec
            return left == right ? $0.id < $1.id : left > right
        }
    }
}

struct NetworkAttributionSample {
    let apps: [NetworkAppActivity]
    let sampledAt: TimeInterval

    func isFresh(at now: TimeInterval, maximumAge: TimeInterval = 3.5) -> Bool {
        let age = now - sampledAt
        return age.isFinite && age >= 0 && age <= maximumAge
    }
}

/// Socket byte deltas power both scope totals and rankings. Destinations stay
/// transient in RAM; DNS resolution is disabled. No physical-counter fallback
/// can leak internal traffic into the Internet scope.
final class NetworkStatsProvider: @unchecked Sendable {
    private static let secondsPerTick: Double = {
        var timebase = mach_timebase_info_data_t()
        mach_timebase_info(&timebase)
        return Double(timebase.numer) / Double(timebase.denom) / 1_000_000_000
    }()

    /// Continuous Mach time includes sleep, unlike ProcessInfo.systemUptime.
    /// A wake therefore invalidates pre-sleep rates and app freshness.
    private static var sampleTime: TimeInterval {
        Double(mach_continuous_time()) * secondsPerTick
    }

    private let lock = NSLock()
    private var appRates = NetworkRateTracker<NettopConnectionKey>()
    private var parser = NettopConnectionCSVParser()
    private var process: Process?
    private var output: FileHandle?
    private var generation: UUID?
    private var startedAt: TimeInterval = 0
    private var lastOutputAt: TimeInterval = 0
    private var retryAt: TimeInterval = 0
    private var stopped = false
    private var latest: NetworkConnectionSample?
    private var attributionMessage = "Measuring app traffic…"
    private var identities: [NettopProcessKey: (identity: NetworkAppIdentity, at: TimeInterval)] = [:]

    deinit { stop() }

    func snapshot(scope: NetworkScope = .internet) -> NetworkStatsSnapshot {
        lock.lock()
        defer { lock.unlock() }
        guard !stopped else {
            return .init(downloadBytesPerSec: nil, uploadBytesPerSec: nil, apps: [],
                         attributionAvailable: false, attributionMessage: "Network monitoring stopped")
        }
        let now = Self.sampleTime
        ensureRunning(at: now)
        if let latest, latest.isFresh(at: now), process?.isRunning == true {
            return latest.snapshot(scope: scope, onLink: .current()) { resolveIdentity($0, at: now) }
        }
        return .init(downloadBytesPerSec: nil, uploadBytesPerSec: nil, apps: [],
                     attributionAvailable: false,
                     attributionMessage: latest == nil ? attributionMessage : "Network sample is stale", scope: scope)
    }

    /// Permanent, idempotent shutdown. A later snapshot cannot restart nettop.
    func stop() {
        lock.lock()
        stopped = true
        let oldProcess = process
        let oldOutput = output
        process = nil
        output = nil
        generation = nil
        latest = nil
        parser = .init()
        identities = [:]
        oldOutput?.readabilityHandler = nil
        lock.unlock()
        try? oldOutput?.close()
        if oldProcess?.isRunning == true { oldProcess?.terminate() }
    }

    // The caller holds lock. A pseudo-terminal is necessary because nettop's
    // endless CSV mode buffers stdout when given a plain Pipe (several seconds
    // of missing data). -L keeps CSV logging mode even though stdout is a TTY.
    private func ensureRunning(at now: TimeInterval) {
        if let process, process.isRunning {
            if now - max(startedAt, lastOutputAt) <= 5 { return }
            output?.readabilityHandler = nil
            try? output?.close()
            output = nil
            generation = nil
            process.terminate()
            self.process = nil
            latest = nil
            attributionMessage = "App traffic collection restarted"
        }
        guard now >= retryAt else { return }
        retryAt = now + 10
        parser = .init()
        appRates.reset()
        latest = nil
        var master: Int32 = -1, slave: Int32 = -1
        guard openpty(&master, &slave, nil, nil, nil) == 0 else {
            attributionMessage = "App traffic unavailable"
            return
        }
        var settings = termios()
        if tcgetattr(slave, &settings) == 0 {
            cfmakeraw(&settings)
            _ = tcsetattr(slave, TCSANOW, &settings)
        }
        _ = fcntl(master, F_SETFL, O_NONBLOCK)
        let reader = FileHandle(fileDescriptor: master, closeOnDealloc: true)
        let writer = FileHandle(fileDescriptor: slave, closeOnDealloc: true)
        let proc = Process()
        proc.executableURL = URL(fileURLWithPath: "/usr/bin/nettop")
        proc.arguments = ["-c", "-n", "-x", "-L", "0", "-s", "1", "-J", "bytes_in,bytes_out"]
        proc.standardOutput = writer
        proc.standardError = FileHandle.nullDevice
        // Keep an idle terminal input open. /dev/null is always readable at
        // EOF and makes nettop's interactive-input loop consume a CPU core,
        // even in logging mode. No input is written to this terminal.
        proc.standardInput = writer
        let token = UUID()
        let descriptor = master
        reader.readabilityHandler = { [weak self] _ in
            self?.consumeAvailableData(from: descriptor, generation: token)
        }
        proc.terminationHandler = { [weak self] _ in self?.didExit(generation: token) }
        do {
            try proc.run()
            try? writer.close()
            process = proc
            output = reader
            generation = token
            startedAt = now
            lastOutputAt = now
            attributionMessage = "Measuring app traffic…"
        } catch {
            reader.readabilityHandler = nil
            try? reader.close()
            try? writer.close()
            attributionMessage = "App traffic unavailable"
        }
    }

    private func consumeAvailableData(from descriptor: Int32, generation token: UUID) {
        lock.lock()
        defer { lock.unlock() }
        guard !stopped, generation == token else { return }
        // A PTY reports EIO when its child exits; Darwin.read handles that
        // expected condition without FileHandle's throwing read behavior.
        var bytes = [UInt8](repeating: 0, count: 64 * 1024)
        let length = Darwin.read(descriptor, &bytes, bytes.count)
        guard length > 0 else { return }
        let chunk = Data(bytes.prefix(length))
        let now = Self.sampleTime
        lastOutputAt = now
        for sample in parser.consume(chunk, at: now) {
            guard let rates = appRates.update(sample.counters, at: sample.sampledAt) else { latest = nil; continue }
            latest = .init(rates: rates, sampledAt: sample.sampledAt, missingCounters: sample.missingCounters)
        }
        if identities.count > 1024 {
            identities = identities.filter { now - $0.value.at < 30 }
        }
    }

    private func didExit(generation token: UUID) {
        lock.lock()
        defer { lock.unlock() }
        guard generation == token else { return }
        output?.readabilityHandler = nil
        try? output?.close()
        output = nil
        process = nil
        generation = nil
        latest = nil
        attributionMessage = "App traffic unavailable"
    }

    private func resolveIdentity(_ key: NettopProcessKey, at now: TimeInterval) -> NetworkAppIdentity {
        if let cached = identities[key], now - cached.at < 30 { return cached.identity }
        // PROC_PIDPATHINFO_MAXSIZE is a C expression macro unavailable to
        // Swift; its public header definition is 4 * MAXPATHLEN.
        var pathBuffer = [CChar](repeating: 0, count: 4 * Int(MAXPATHLEN))
        let count = proc_pidpath(key.pid, &pathBuffer, UInt32(pathBuffer.count))
        let executable = count > 0
            ? String(decoding: pathBuffer.prefix { $0 != 0 }.map { UInt8(bitPattern: $0) }, as: UTF8.self) : nil
        let runningApp = NSRunningApplication(processIdentifier: key.pid)
        let path = executable ?? runningApp?.bundleURL?.path
        let identity: NetworkAppIdentity
        if let path, let appEnd = path.range(of: ".app/", options: .caseInsensitive)?.upperBound {
            let appPath = String(path[..<path.index(before: appEnd)])
            let bundle = Bundle(path: appPath)
            let name = bundle?.object(forInfoDictionaryKey: "CFBundleDisplayName") as? String
                ?? bundle?.object(forInfoDictionaryKey: "CFBundleName") as? String
                ?? URL(fileURLWithPath: appPath).deletingPathExtension().lastPathComponent
            identity = .init(id: "app:\(appPath)", name: name)
        } else if let appURL = runningApp?.bundleURL {
            identity = .init(id: "app:\(appURL.path)", name: runningApp?.localizedName ?? appURL.deletingPathExtension().lastPathComponent)
        } else {
            let name = executable.map { URL(fileURLWithPath: $0).lastPathComponent } ?? key.name
            identity = .init(id: "process:\(executable ?? key.name)", name: name)
        }
        identities[key] = (identity, now)
        return identity
    }

}
