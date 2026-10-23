import Foundation

final class ScryBarDiscovery: NSObject {
    private let browsers = [NetServiceBrowser(), NetServiceBrowser()]
    private var servicesByName: [String: NetService] = [:]
    private var endpointsByName: [String: ScryBarEndpoint] = [:]

    var onStatus: ((String) -> Void)?
    var onEndpoints: (([ScryBarEndpoint]) -> Void)?

    override init() {
        super.init()
        browsers.forEach { $0.delegate = self }
    }

    func start() {
        stop()
        onStatus?("Scanning for ScryBar and Meguru…")
        browsers[0].searchForServices(ofType: "_scrybar._tcp.", inDomain: "local.")
        browsers[1].searchForServices(ofType: "_meguru._tcp.", inDomain: "local.")
    }

    func stop() {
        browsers.forEach { $0.stop() }
        servicesByName.removeAll()
        endpointsByName.removeAll()
        publish()
    }

    private func key(_ service: NetService) -> String { "\(service.type)|\(service.domain)|\(service.name)" }

    private func publish() {
        let sorted = endpointsByName.values.sorted { lhs, rhs in
            lhs.displayName.localizedCaseInsensitiveCompare(rhs.displayName) == .orderedAscending
        }
        onEndpoints?(sorted)
    }
}

extension ScryBarDiscovery: NetServiceBrowserDelegate {
    func netServiceBrowserWillSearch(_ browser: NetServiceBrowser) {
        onStatus?("Searching…")
    }

    func netServiceBrowser(_ browser: NetServiceBrowser, didNotSearch errorDict: [String : NSNumber]) {
        onStatus?("Discovery failed: \(errorDict)")
    }

    func netServiceBrowserDidStopSearch(_ browser: NetServiceBrowser) {
        if endpointsByName.isEmpty {
            onStatus?("No display discovered yet. Manual host/IP fallback is available.")
        }
    }

    func netServiceBrowser(_ browser: NetServiceBrowser, didFind service: NetService, moreComing: Bool) {
        servicesByName[key(service)] = service
        service.delegate = self
        service.resolve(withTimeout: 5)
        if !moreComing {
            onStatus?("Resolving discovered services…")
        }
    }

    func netServiceBrowser(_ browser: NetServiceBrowser, didRemove service: NetService, moreComing: Bool) {
        servicesByName.removeValue(forKey: key(service))
        endpointsByName.removeValue(forKey: key(service))
        if !moreComing {
            publish()
            if endpointsByName.isEmpty {
                onStatus?("No display discovered yet. Manual host/IP fallback is available.")
            }
        }
    }
}

extension ScryBarDiscovery: NetServiceDelegate {
    func netServiceDidResolveAddress(_ sender: NetService) {
        guard servicesByName[key(sender)] === sender else { return }
        // Extract IPv4 address directly from resolved addresses for reliable connectivity.
        // mDNS hostname resolution (.local) is unreliable for repeated HTTP connections.
        let resolvedHost = extractIPv4(from: sender)
            ?? sender.hostName?.trimmingCharacters(in: CharacterSet(charactersIn: "."))

        guard let resolvedHost, !resolvedHost.isEmpty else { return }

        let product = sender.type == "_meguru._tcp." ? "meguru" : "scrybar"
        if product == "meguru" {
            let txt = sender.txtRecordData().map(NetService.dictionary(fromTXTRecord:)) ?? [:]
            guard txt["product"] == Data("meguru".utf8), txt["protocol"] == Data("1".utf8) else { return }
        }
        let identity = key(sender)
        endpointsByName[identity] = ScryBarEndpoint(
            id: endpointsByName[identity]?.id ?? UUID(),
            name: sender.name,
            host: resolvedHost,
            port: sender.port,
            source: .discovery, product: product, serviceID: identity,
            aliases: [sender.hostName?.trimmingCharacters(in: CharacterSet(charactersIn: "."))].compactMap { $0 }
        )
        publish()
        onStatus?("Found \(endpointsByName.count) display(s).")
    }

    private func extractIPv4(from service: NetService) -> String? {
        guard let addresses = service.addresses else { return nil }
        for addrData in addresses {
            guard addrData.count >= MemoryLayout<sockaddr_in>.size else { continue }
            let family = addrData.withUnsafeBytes { $0.load(as: sockaddr.self).sa_family }
            guard family == UInt8(AF_INET) else { continue }
            let addr = addrData.withUnsafeBytes { $0.load(as: sockaddr_in.self) }
            var buf = [CChar](repeating: 0, count: Int(INET_ADDRSTRLEN))
            var inAddr = addr.sin_addr
            inet_ntop(AF_INET, &inAddr, &buf, socklen_t(INET_ADDRSTRLEN))
            let ipBytes = buf.prefix { $0 != 0 }.map { UInt8(bitPattern: $0) }
            let ip = String(decoding: ipBytes, as: UTF8.self)
            if !ip.isEmpty && ip != "0.0.0.0" { return ip }
        }
        return nil
    }

    func netService(_ sender: NetService, didNotResolve errorDict: [String : NSNumber]) {
        onStatus?("Could not resolve \(sender.name): \(errorDict)")
    }
}
