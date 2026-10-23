import Foundation
import Testing
@testable import ScryBarCompanion

@Test func networkRatesRequireBaselineAndActualElapsedTime() throws {
    var tracker = NetworkRateTracker<String>()
    #expect(tracker.update(["en0": .init(download: 1_000, upload: 500)], at: 10) == nil)
    let measured = tracker.update(["en0": .init(download: 3_000, upload: 1_000)], at: 12)
    let rates = try #require(measured)
    #expect(rates["en0"] == .init(download: 1_000, upload: 250))
    #expect(tracker.update(["en0": .init(download: 3_000, upload: 1_000)], at: 13)?["en0"] == .init(download: 0, upload: 0))
}

@Test func networkCounterResetAndSleepRequireNewBaseline() throws {
    var tracker = NetworkRateTracker<String>()
    _ = tracker.update(["en0": .init(download: 9_000, upload: 5_000)], at: 1)
    #expect(tracker.update(["en0": .init(download: 100, upload: 20)], at: 2) == nil)
    #expect(tracker.update(["en0": .init(download: 200, upload: 40)], at: 3)?["en0"] == .init(download: 100, upload: 20))
    #expect(tracker.update(["en0": .init(download: 5_000, upload: 800)], at: 30) == nil)
    #expect(tracker.update(["en0": .init(download: 5_100, upload: 850)], at: 31)?["en0"] == .init(download: 100, upload: 50))
    tracker.reset()
    #expect(tracker.update(["en0": .init(download: 5_200, upload: 900)], at: 32) == nil)
}

@Test func networkInterfaceChangesDoNotCreateTrafficSpikes() throws {
    var tracker = NetworkRateTracker<String>()
    _ = tracker.update(["en0": .init(download: 100, upload: 100)], at: 1)
    let measured = tracker.update([
        "en0": .init(download: 200, upload: 150),
        "en1": .init(download: 8_000_000, upload: 9_000_000),
    ], at: 2)
    let mixed = try #require(measured)
    #expect(mixed.count == 1 && mixed["en0"] == .init(download: 100, upload: 50))
    #expect(tracker.update([:], at: 3)?.isEmpty == true)
    #expect(tracker.update(["en1": .init(download: 8_000_050, upload: 9_000_020)], at: 4) == nil)
    #expect(tracker.update(["en1": .init(download: 8_000_150, upload: 9_000_070)], at: 5)?["en1"] == .init(download: 100, upload: 50))
}

@Test func networkRatesRejectNonPositiveAndNonFiniteIntervals() {
    var tracker = NetworkRateTracker<String>()
    let counters = ["en0": NetworkByteCounters(download: 10, upload: 20)]
    _ = tracker.update(counters, at: 2)
    #expect(tracker.update(counters, at: 2) == nil)
    #expect(tracker.update(counters, at: 1) == nil)
    #expect(tracker.update(counters, at: .infinity) == nil)
}

@Test func nettopParserHandlesChunksQuotesColumnOrderAndDuplicates() throws {
    var parser = NettopCSVParser()
    let chunk1 = ",bytes_out,bytes_in,\n\"Browser, Demo.42\",40,200,\nBrowser Helper.43,10,50,\nBrowser Helper.43,10,50,\nnot-a-process,99,99,\n"
    #expect(parser.consume(Data(chunk1.utf8), at: 1).isEmpty)
    #expect(parser.consume(Data(",bytes_out,byte".utf8), at: 2).isEmpty)
    let blocks = parser.consume(Data("s_in,\nBrowser Helper.43,20,100,\n".utf8), at: 2)
    let sample = try #require(blocks.first)
    #expect(blocks.count == 1 && sample.counters.count == 2 && sample.sampledAt == 1)
    #expect(sample.counters[.init(pid: 42, name: "Browser, Demo")] == .init(download: 200, upload: 40))
    #expect(sample.counters[.init(pid: 43, name: "Browser Helper")] == .init(download: 50, upload: 10))
    #expect(NettopCSVParser.csvFields("\"App \"\"Demo\"\".12\",1,2,") == ["App \"Demo\".12", "1", "2", ""])
}

@Test func appHelpersAggregateAfterPerProcessDeltasAndNewProcessesBaseline() throws {
    let app = NettopProcessKey(pid: 1, name: "Browser")
    let helper = NettopProcessKey(pid: 2, name: "Browser Helper")
    let newHelper = NettopProcessKey(pid: 3, name: "Browser Helper")
    var tracker = NetworkRateTracker<NettopProcessKey>()
    _ = tracker.update([app: .init(download: 1_000, upload: 500), helper: .init(download: 100, upload: 50)], at: 1)
    let measured = tracker.update([
        app: .init(download: 1_100, upload: 520), helper: .init(download: 300, upload: 80),
        newHelper: .init(download: 900_000, upload: 90_000),
    ], at: 2)
    let rates = try #require(measured)
    let activities = NetworkActivityBuilder.activities(rates) { _ in .init(id: "browser", name: "Browser") }
    #expect(activities == [.init(id: "browser", name: "Browser", downloadBytesPerSec: 300, uploadBytesPerSec: 50)])
    // A helper exits: the surviving process keeps its own baseline and the
    // disappearing helper's lifetime counters cannot reduce the aggregate.
    let nextMeasured = tracker.update([app: .init(download: 1_250, upload: 540)], at: 3)
    let next = try #require(nextMeasured)
    #expect(next[app] == .init(download: 150, upload: 20))
}

@Test func nettopEmptySamplesClearTrafficButMalformedRowsDoNotFakeIdle() throws {
    var parser = NettopCSVParser()
    _ = parser.consume(Data(",bytes_in,bytes_out,\nApp.42,100,20,\n".utf8), at: 1)
    let active = parser.consume(Data(",bytes_in,bytes_out,\n".utf8), at: 2)
    #expect(active.first?.counters.count == 1)
    let idle = parser.consume(Data(",bytes_in,bytes_out,\n".utf8), at: 3)
    #expect(idle.count == 1 && idle.first?.counters.isEmpty == true)
    _ = parser.consume(Data("App.42,unreadable,not-a-number,\n".utf8), at: 3)
    #expect(parser.consume(Data(",bytes_in,bytes_out,\n".utf8), at: 4).isEmpty)
}

@Test func networkAttributionSortsActiveAppsAndDropsInvalidRates() {
    let rates: [NettopProcessKey: NetworkByteRates] = [
        .init(pid: 1, name: "Small"): .init(download: 1, upload: 2),
        .init(pid: 2, name: "Large"): .init(download: 50, upload: 0),
        .init(pid: 3, name: "Idle"): .init(download: 0, upload: 0),
        .init(pid: 4, name: "Invalid"): .init(download: .nan, upload: 10),
    ]
    let apps = NetworkActivityBuilder.activities(rates) { .init(id: $0.name, name: $0.name) }
    #expect(apps.map(\.name) == ["Large", "Small"])
}

@Test func networkAttributionFreshnessCannotPresentStaleTrafficAsLive() {
    let sample = NetworkAttributionSample(apps: [], sampledAt: 10)
    #expect(sample.isFresh(at: 12))
    #expect(!sample.isFresh(at: 14))
    #expect(!sample.isFresh(at: 9))
    #expect(!sample.isFresh(at: .infinity))
}

@Test func stoppedNetworkProviderNeverStartsOnLaterSnapshots() {
    let provider = NetworkStatsProvider()
    provider.stop()
    provider.stop()
    for _ in 0..<3 {
        let sample = provider.snapshot()
        #expect(sample.downloadBytesPerSec == nil && sample.uploadBytesPerSec == nil)
        #expect(!sample.attributionAvailable && sample.apps.isEmpty)
        #expect(sample.attributionMessage == "Network monitoring stopped")
    }
}

/// Opt-in local verification exercises actual macOS counters and nettop. It
/// records only availability and numeric rates, never names or destinations.
@Test func networkLiveSmokeWhenExplicitlyEnabled() async throws {
    guard ProcessInfo.processInfo.environment["SCRYBAR_NETWORK_LIVE_SMOKE"] == "1" else { return }
    let provider = NetworkStatsProvider()
    defer { provider.stop() }
    var observedTotal = false, observedApps = false
    for _ in 0..<7 {
        let sample = provider.snapshot()
        if let download = sample.downloadBytesPerSec, let upload = sample.uploadBytesPerSec {
            #expect(download.isFinite && download >= 0 && upload.isFinite && upload >= 0)
            observedTotal = true
        }
        observedApps = observedApps || sample.attributionAvailable
        try await Task.sleep(for: .seconds(1))
    }
    #expect(observedTotal)
    #expect(observedApps)
    provider.stop()
    let stopped = provider.snapshot()
    #expect(!stopped.attributionAvailable && stopped.apps.isEmpty)
    #expect(stopped.downloadBytesPerSec == nil && stopped.uploadBytesPerSec == nil)
}
