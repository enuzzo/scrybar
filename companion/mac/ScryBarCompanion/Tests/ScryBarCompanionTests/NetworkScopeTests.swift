import Darwin
import Foundation
import Testing
@testable import ScryBarCompanion

@Test func networkScopeClassifiesPeersIncludingTailnetAndIPv6() {
    for host in ["10.2.3.4", "172.16.0.1", "172.31.255.254", "192.168.4.8", "100.64.0.1", "100.127.255.254", "127.0.0.1", "169.254.1.2", "224.0.0.251", "::1", "fe80::123", "fd7a:115c:a1e0::1234", "fd00::1", "ff02::fb", "::ffff:192.168.1.2"] {
        #expect(NetworkDestination.address(host) == .local, "\(host)")
    }
    for host in ["1.1.1.1", "8.8.8.8", "100.63.255.254", "100.128.0.1", "172.15.255.254", "172.32.0.1", "2606:4700:4700::1111", "::ffff:8.8.8.8", "64:ff9b::808:808"] {
        #expect(NetworkDestination.address(host) == .internet, "\(host)")
    }
    for host in ["*", "not-an-address", "192.0.2.1", "198.51.100.8", "203.0.113.9", "2001:db8::1"] {
        #expect(NetworkDestination.address(host) == .unknown)
    }
    #expect(NetworkDestination.endpoint("[::1]:443") == .unknown) // unsupported syntax is not Internet
    #expect(NetworkDestination.endpoint("fd7a:115c:a1e0::12.443") == .local)
    #expect(NetworkDestination.endpoint("fe80::1%en0.5353") == .local)
    #expect(NetworkDestination.endpoint("2606:4700:4700::1111.443") == .internet)
    #expect(NetworkDestination.endpoint("8.8.8.8:443") == .internet)
    #expect(NetworkDestination.endpoint("*.*") == .unknown)
}

@Test func networkConnectionParserUsesSocketOwnershipAndNeverSummaryTotals() throws {
    var parser = NettopConnectionCSVParser()
    let header = ",bytes_in,bytes_out,\n"
    let body = "\"App, Demo.42\",999999,888888,\n" +
        "tcp4 192.168.1.2:123<->1.1.1.1:443,100,20,\n" +
        "tcp4 192.168.1.2:123<->1.1.1.1:443,100,20,\n" +
        "udp6 ::.123<->*.*,10,5,\nOther.43,777,888,\n" +
        "tcp6 fd7a:115c:a1e0::1.123<->fd7a:115c:a1e0::2.80,30,40,\n"
    #expect(parser.consume(Data((header + body).utf8), at: 1).isEmpty)
    let sample = try #require(parser.consume(Data(header.utf8), at: 2).first)
    #expect(sample.counters.count == 3)
    #expect(sample.counters.values.reduce(0) { $0 + $1.download } == 140)
    #expect(sample.counters.keys.filter { $0.process.pid == 42 }.count == 2)
    #expect(sample.counters.keys.first { $0.process.pid == 43 }?.destination == .local)
    #expect(parser.consume(Data(header.utf8), at: 3).first?.counters.isEmpty == true)
    _ = parser.consume(Data("App.42,100,20,\ntcp4 *:1<->1.1.1.1:80,invalid,20,\n".utf8), at: 3)
    #expect(parser.consume(Data(header.utf8), at: 4).first?.hasUsableCounters == false)
}

@Test func networkScopeTotalsAppsAndWireUseSameFilteredConnections() throws {
    let process = NettopProcessKey(pid: 1, name: "Mixed app")
    let vpn = NettopProcessKey(pid: 2, name: "Transport")
    let wan = NettopConnectionKey(process: process, label: "tcp4 192.168.1.2:1<->1.1.1.1:443")
    let lan = NettopConnectionKey(process: process, label: "tcp4 192.168.1.2:2<->192.168.1.3:21")
    let tail = NettopConnectionKey(process: process, label: "tcp4 100.64.0.1:3<->100.64.0.2:22")
    let transport = NettopConnectionKey(process: vpn, label: "tcp4 192.168.1.2:4<->8.8.8.8:443")
    let sample = NetworkConnectionSample(rates: [wan: .init(download: 100, upload: 20),
        lan: .init(download: 200, upload: 30), tail: .init(download: 300, upload: 40),
        transport: .init(download: 99999, upload: 99999)], sampledAt: 1)
    let resolve: (NettopProcessKey) -> NetworkAppIdentity = {
        $0.pid == 2 ? .init(id: "app:/Applications/Tailscale.app", name: "Tailscale") : .init(id: "mixed", name: "Mixed app")
    }
    let internet = sample.snapshot(scope: .internet, resolve: resolve)
    let local = sample.snapshot(scope: .local, resolve: resolve)
    let all = sample.snapshot(scope: .all, resolve: resolve)
    #expect(internet.downloadBytesPerSec == 100 && internet.uploadBytesPerSec == 20)
    #expect(local.downloadBytesPerSec == 500 && local.uploadBytesPerSec == 70)
    #expect(all.downloadBytesPerSec == 600 && all.uploadBytesPerSec == 90)
    for snapshot in [internet, local, all] {
        #expect(snapshot.apps.count == 1)
        #expect(snapshot.apps.first?.downloadBytesPerSec == snapshot.downloadBytesPerSec)
        let payload = MacStatsPayload(cpuTempC: nil, gpuTempC: nil, cpuUsagePct: nil, gpuUsagePct: nil,
            ramUsedGB: 1, ramTotalGB: 8, diskUsedGB: 1, diskTotalGB: 100, updatedAt: .now)
            .withNetwork(snapshot, hostName: nil)
        #expect(payload.networkScope == snapshot.scope.rawValue)
        #expect(payload.networkAppDownBytesPerSec == payload.netDownBytesPerSec)
    }
    // Switching scope reuses the same measured interval, never lifetime counters.
    #expect(sample.snapshot(scope: .internet, resolve: resolve).downloadBytesPerSec == 100)
}

@Test func networkUnknownPeerIsExplicitAndNeverFakeInternetZero() {
    let key = NettopConnectionKey(process: .init(pid: 1, name: "UDP"), label: "udp4 *:123<->*:*")
    let sample = NetworkConnectionSample(rates: [key: .init(download: 100, upload: 20)], sampledAt: 1)
    let resolve: (NettopProcessKey) -> NetworkAppIdentity = { .init(id: "udp", name: $0.name) }
    for scope in [NetworkScope.internet, .local] {
        let result = sample.snapshot(scope: scope, resolve: resolve)
        #expect(result.downloadBytesPerSec == nil && result.uploadBytesPerSec == nil)
        #expect(result.coveragePartial && !result.attributionAvailable && result.apps.isEmpty)
    }
    let all = sample.snapshot(scope: .all, resolve: resolve)
    #expect(all.downloadBytesPerSec == 100 && all.apps.first?.downloadBytesPerSec == 100)
    #expect(!all.coveragePartial)
    let idle = NetworkConnectionSample(rates: [:], sampledAt: 1).snapshot(scope: .internet, resolve: resolve)
    #expect(idle.downloadBytesPerSec == 0 && !idle.coveragePartial)
}

@Test func networkScopePreferenceDefaultsToInternetAndPersists() throws {
    let name = "network-scope-test-\(UUID().uuidString)"
    let defaults = try #require(UserDefaults(suiteName: name))
    defer { defaults.removePersistentDomain(forName: name) }
    #expect(NetworkScope.stored(in: defaults) == .internet)
    defaults.set("local", forKey: "network.scope")
    #expect(NetworkScope.stored(in: defaults) == .local)
    defaults.set("all", forKey: "network.scope")
    #expect(NetworkScope.stored(in: defaults) == .all)
    defaults.set("unsupported", forKey: "network.scope")
    #expect(NetworkScope.stored(in: defaults) == .internet)
}

@Test func networkPublicIPv6OnPhysicalSubnetIsLocal() throws {
    let prefix = try #require(NetworkLocalNetworks.Prefix(address: "2606:4700:1234:5678::1", mask: "ffff:ffff:ffff:ffff::"))
    let networks = NetworkLocalNetworks(prefixes: [prefix])
    #expect(NetworkDestination.endpoint("2606:4700:1234:5678::2.443", onLink: networks) == .local)
    #expect(NetworkDestination.endpoint("2606:4700:1234:5679::2.443", onLink: networks) == .internet)
    #expect(NetworkLocalNetworks.Prefix(address: "::1", mask: "::") == nil)
    #expect(NetworkLocalNetworks.Prefix(address: "::1", mask: "ffff:0:ffff::") == nil)
}

@Test func networkMissingNativeCountersDoNotInvalidateKnownFlowsOrFakeIdle() throws {
    var parser = NettopConnectionCSVParser()
    let header = ",bytes_in,bytes_out,\n"
    _ = parser.consume(Data((header + "App.1,900,200,\ntcp4 *:80<->*:*,1,2,\nudp4 *:5353<->*:*, , ,\n").utf8), at: 1)
    let sample = try #require(parser.consume(Data(header.utf8), at: 2).first)
    #expect(sample.counters.count == 1 && sample.missingCounters)
    let missing = NetworkConnectionSample(rates: [:], sampledAt: 1, missingCounters: true)
        .snapshot(scope: .all) { .init(id: $0.name, name: $0.name) }
    #expect(missing.downloadBytesPerSec == nil && missing.coveragePartial)
}

/// Real local traffic, opt-in only. Both endpoints are in this test process;
/// its socket activity must appear under Local/All and never under Internet.
@Test func networkLiveLoopbackStaysOutOfInternetWhenExplicitlyEnabled() async throws {
    guard ProcessInfo.processInfo.environment["SCRYBAR_NETWORK_LIVE_SMOKE"] == "1" else { return }
    let listener = socket(AF_INET, SOCK_STREAM, 0)
    #expect(listener >= 0)
    defer { close(listener) }
    var address = sockaddr_in()
    address.sin_len = UInt8(MemoryLayout<sockaddr_in>.size)
    address.sin_family = sa_family_t(AF_INET)
    address.sin_addr.s_addr = inet_addr("127.0.0.1")
    let bound = withUnsafePointer(to: &address) {
        $0.withMemoryRebound(to: sockaddr.self, capacity: 1) { bind(listener, $0, socklen_t(MemoryLayout<sockaddr_in>.size)) }
    }
    #expect(bound == 0 && listen(listener, 1) == 0)
    var size = socklen_t(MemoryLayout<sockaddr_in>.size)
    _ = withUnsafeMutablePointer(to: &address) {
        $0.withMemoryRebound(to: sockaddr.self, capacity: 1) { getsockname(listener, $0, &size) }
    }
    let client = socket(AF_INET, SOCK_STREAM, 0)
    defer { shutdown(client, SHUT_RDWR); close(client) }
    var noSigpipe: Int32 = 1
    _ = setsockopt(client, SOL_SOCKET, SO_NOSIGPIPE, &noSigpipe, socklen_t(MemoryLayout<Int32>.size))
    let connected = withUnsafePointer(to: &address) {
        $0.withMemoryRebound(to: sockaddr.self, capacity: 1) { connect(client, $0, size) }
    }
    #expect(connected == 0)
    let receiver = accept(listener, nil, nil)
    #expect(receiver >= 0)
    defer { shutdown(receiver, SHUT_RDWR); close(receiver) }
    let drain = Task.detached {
        var buffer = [UInt8](repeating: 0, count: 65536)
        while recv(receiver, &buffer, buffer.count, 0) > 0 { }
    }
    defer { drain.cancel() }
    let sender = Task.detached {
        let buffer = [UInt8](repeating: 1, count: 32768)
        while !Task.isCancelled {
            _ = buffer.withUnsafeBytes { send(client, $0.baseAddress, $0.count, 0) }
            try? await Task.sleep(for: .milliseconds(30))
        }
    }
    defer { sender.cancel() }
    let provider = NetworkStatsProvider()
    defer { provider.stop() }
    var observed = false
    // proc_pidpath resolves the test executable to the same identity used by
    // the provider. No process names/endpoints need to be printed or retained.
    var executable = [CChar](repeating: 0, count: 4096)
    let count = proc_pidpath(getpid(), &executable, UInt32(executable.count))
    #expect(count > 0)
    let path = String(cString: executable)
    let ownID: String
    if let appEnd = path.range(of: ".app/")?.upperBound {
        ownID = "app:" + String(path[..<path.index(before: appEnd)])
    } else { ownID = "process:" + path }
    for _ in 0..<8 {
        let local = provider.snapshot(scope: .local)
        let internet = provider.snapshot(scope: .internet)
        let all = provider.snapshot(scope: .all)
        if let own = local.apps.first(where: { $0.id == ownID }), own.downloadBytesPerSec > 10000 {
            observed = true
            #expect(!internet.apps.contains(where: { $0.id == ownID }))
            #expect(all.apps.contains(where: { $0.id == ownID && $0.downloadBytesPerSec > 10000 }))
        }
        try await Task.sleep(for: .seconds(1))
    }
    #expect(observed)
}

@Test func networkLiveOutputWithoutUsableSamplesTriggersRecovery() {
    #expect(!NettopStreamHealth.isStalled(now: 14, started: 10, output: 14, sample: 10))
    #expect(NettopStreamHealth.isStalled(now: 16, started: 10, output: 16, sample: 10))
    #expect(!NettopStreamHealth.isStalled(now: 16, started: 10, output: 16, sample: 15))
    // A slow complete frame can still contain an obsolete measurement.
    #expect(NettopStreamHealth.isStalled(now: 30, started: 10, output: 30, sample: 30, measurement: 26))
    #expect(!NettopStreamHealth.isStalled(now: 30, started: 10, output: 30, sample: 30, measurement: 29))
    #expect(NettopStreamHealth.isStalled(now: 22, started: 10, output: 16, sample: 21))
}

@Test func networkOneBadRowDoesNotDiscardOtherAppsAndSocketCapRequestsRestart() throws {
    let header = ",bytes_in,bytes_out,\n"
    var parser = NettopConnectionCSVParser()
    _ = parser.consume(Data((header + "App.1,100,20,\ntcp4 127.0.0.1:1<->1.1.1.1:443,100,20,\nudp4 *:1<->*:*,unavailable,20,\n").utf8), at: 1)
    let sample = try #require(parser.consume(Data(header.utf8), at: 2).first)
    #expect(sample.counters.count == 1 && sample.missingCounters)
    #expect(parser.partialSamples == 1 && !parser.capacityExceeded)
    let rows = (0..<16400).map { "tcp4 127.0.0.1:\($0)<->1.1.1.1:443,10,20,\n" }.joined()
    _ = parser.consume(Data(("App.1,100,20,\n" + rows).utf8), at: 2)
    #expect(parser.capacityExceeded)
    let capped = try #require(parser.consume(Data(header.utf8), at: 3).first)
    #expect(capped.counters.count == 16384 && capped.missingCounters)
}

@Test func networkUnavailableListenersDoNotHideMeasuredIdleSockets() {
    let key = NettopConnectionKey(process: .init(pid: 1, name: "Idle"), label: "tcp4 127.0.0.1:1<->1.1.1.1:443")
    let sample = NetworkConnectionSample(rates: [key: .init(download: 0, upload: 0)], sampledAt: 1, missingCounters: true)
    for scope in NetworkScope.allCases {
        let result = sample.snapshot(scope: scope) { .init(id: $0.name, name: $0.name) }
        #expect(result.downloadBytesPerSec == 0 && result.uploadBytesPerSec == 0)
        #expect(result.coveragePartial && result.attributionAvailable)
    }
}

@Test func networkLongRunWhenExplicitlyEnabled() async throws {
    guard let setting = ProcessInfo.processInfo.environment["SCRYBAR_NETWORK_LONG_SMOKE_SECONDS"],
          let duration = Int(setting), duration >= 60 else { return }
    let provider = NetworkStatsProvider()
    defer { provider.stop() }
    var observed = 0
    for second in 0..<duration {
        let sample = provider.snapshot(scope: .all)
        if sample.downloadBytesPerSec != nil { observed += 1 }
        let health = provider.diagnostics()
        if second > 10 {
            #expect(health.sampleAge < 6 || !health.running)
        }
        try await Task.sleep(for: .seconds(1))
    }
    let health = provider.diagnostics()
    print("Collector long run: seconds=\(duration), measured=\(observed), frames=\(health.validSamples), restarts=\(health.restarts), lastSampleAge=\(health.sampleAge)")
    #expect(observed > duration * 3 / 4)
}
