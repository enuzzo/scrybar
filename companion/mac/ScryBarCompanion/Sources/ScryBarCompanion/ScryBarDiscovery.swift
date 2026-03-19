import Foundation

final class ScryBarDiscovery: NSObject {
    private let browser = NetServiceBrowser()
    private var servicesByName: [String: NetService] = [:]
    private var endpointsByName: [String: ScryBarEndpoint] = [:]

    var onStatus: ((String) -> Void)?
    var onEndpoints: (([ScryBarEndpoint]) -> Void)?

    override init() {
        super.init()
        browser.delegate = self
    }

    func start() {
        stop()
        onStatus?("Scanning for _scrybar._tcp devices on local network…")
        browser.searchForServices(ofType: "_scrybar._tcp.", inDomain: "local.")
    }

    func stop() {
        browser.stop()
        servicesByName.removeAll()
        endpointsByName.removeAll()
        publish()
    }

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
            onStatus?("No ScryBar discovered yet. Manual host/IP fallback is available.")
        }
    }

    func netServiceBrowser(_ browser: NetServiceBrowser, didFind service: NetService, moreComing: Bool) {
        servicesByName[service.name] = service
        service.delegate = self
        service.resolve(withTimeout: 5)
        if !moreComing {
            onStatus?("Resolving discovered services…")
        }
    }

    func netServiceBrowser(_ browser: NetServiceBrowser, didRemove service: NetService, moreComing: Bool) {
        servicesByName.removeValue(forKey: service.name)
        endpointsByName.removeValue(forKey: service.name)
        if !moreComing {
            publish()
            if endpointsByName.isEmpty {
                onStatus?("No ScryBar discovered yet. Manual host/IP fallback is available.")
            }
        }
    }
}

extension ScryBarDiscovery: NetServiceDelegate {
    func netServiceDidResolveAddress(_ sender: NetService) {
        // Extract IPv4 address directly from resolved addresses for reliable connectivity.
        // mDNS hostname resolution (.local) is unreliable for repeated HTTP connections.
        let resolvedHost = extractIPv4(from: sender)
            ?? sender.hostName?.trimmingCharacters(in: CharacterSet(charactersIn: "."))

        guard let resolvedHost, !resolvedHost.isEmpty else { return }

        endpointsByName[sender.name] = ScryBarEndpoint(
            name: sender.name,
            host: resolvedHost,
            port: sender.port,
            source: .discovery
        )
        publish()
        onStatus?("Found \(endpointsByName.count) ScryBar device(s).")
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
            let ip = String(cString: buf)
            if !ip.isEmpty && ip != "0.0.0.0" { return ip }
        }
        return nil
    }

    func netService(_ sender: NetService, didNotResolve errorDict: [String : NSNumber]) {
        onStatus?("Could not resolve \(sender.name): \(errorDict)")
    }
}
