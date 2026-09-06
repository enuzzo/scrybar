import AppKit
import SwiftUI

@MainActor
final class PopoverLayoutModel: ObservableObject {
    @Published var maximumHeight: CGFloat = 640
}

enum PopoverHeightPolicy {
    static let minimumHeight: CGFloat = 320
    static let screenMargin: CGFloat = 16

    static func maximumHeight(visibleFrameHeight: CGFloat) -> CGFloat {
        max(visibleFrameHeight - screenMargin, minimumHeight)
    }

    static func resolvedHeight(contentHeight: CGFloat, maximumHeight: CGFloat) -> CGFloat {
        min(max(contentHeight, minimumHeight), max(maximumHeight, minimumHeight))
    }
}

struct PopoverBodyHeightPreferenceKey: PreferenceKey {
    static let defaultValue: CGFloat = 0

    static func reduce(value: inout CGFloat, nextValue: () -> CGFloat) {
        value = max(value, nextValue())
    }
}

struct PopoverContentView: View {
    @EnvironmentObject var model: AppModel
    @ObservedObject var layout: PopoverLayoutModel
    @State private var showSettings = false
    @State private var mainContentHeight: CGFloat = 540
    @State private var settingsContentHeight: CGFloat = 610
    let onPreferredSizeChange: (CGSize) -> Void

    private var preferredSize: CGSize {
        let contentHeight = showSettings ? settingsContentHeight : mainContentHeight
        return CGSize(
            width: 360,
            height: PopoverHeightPolicy.resolvedHeight(
                contentHeight: contentHeight,
                maximumHeight: layout.maximumHeight
            )
        )
    }

    var body: some View {
        Group {
            if showSettings {
                SettingsView(
                    showSettings: $showSettings,
                    viewportHeight: preferredSize.height,
                    onContentHeightChange: { settingsContentHeight = $0 }
                )
                    .environmentObject(model)
            } else {
                NowPlayingPopoverView(
                    showSettings: $showSettings,
                    viewportHeight: preferredSize.height,
                    onContentHeightChange: { mainContentHeight = $0 }
                )
                    .environmentObject(model)
            }
        }
        .frame(width: preferredSize.width, height: preferredSize.height, alignment: .top)
        .background(CompanionTheme.windowBackground)
        .onAppear {
            if model.showSettingsOnOpen {
                model.showSettingsOnOpen = false
                showSettings = true
            }
            onPreferredSizeChange(preferredSize)
        }
        .onChange(of: preferredSize) { _, size in
            onPreferredSizeChange(size)
        }
    }
}

struct NowPlayingPopoverView: View {
    @EnvironmentObject var model: AppModel
    @Binding var showSettings: Bool
    let viewportHeight: CGFloat
    let onContentHeightChange: (CGFloat) -> Void
    @State private var now = Date()
    @State private var measuredContentHeight: CGFloat = 0

    private var payload: NowPlayingPayload { model.currentPayload }
    private var hasDevice: Bool { model.selectedEndpoint != nil }
    private var deviceConnected: Bool { model.selectedEndpointIsConnected }
    private var deviceStatus: String {
        if !hasDevice { return "No device" }
        return deviceConnected ? "Connected" : "Waiting"
    }
    private var hasProgress: Bool { payload.durationSec > 0 && interpolatedElapsed > 0 }

    /// Interpolate elapsed time: snapshot value + wall-clock delta while playing
    private var interpolatedElapsed: Double {
        guard payload.isPlaying else { return payload.elapsedSec }
        let delta = now.timeIntervalSince(payload.updatedAt)
        return min(payload.elapsedSec + max(delta, 0), payload.durationSec)
    }

    var body: some View {
        ScrollView(.vertical) {
            VStack(alignment: .leading, spacing: 8) {
            // Artwork + track info
            HStack(alignment: .top, spacing: 10) {
                ArtworkView(
                    artworkURL: payload.artworkURL,
                    artworkID: payload.artworkID,
                    artworkWidth: payload.artworkWidth,
                    artworkHeight: payload.artworkHeight,
                    artworkRGB565B64: payload.artworkRGB565B64,
                    title: payload.title,
                    artist: payload.artist
                )
                    .frame(width: 56, height: 56)

                VStack(alignment: .leading, spacing: 2) {
                    Text(payload.title)
                        .font(.system(size: 14, weight: .semibold))
                        .foregroundStyle(CompanionTheme.textPrimary)
                        .lineLimit(1)

                    Text(payload.artist)
                        .font(.system(size: 12, weight: .medium))
                        .foregroundStyle(CompanionTheme.textSecondary)
                        .lineLimit(1)

                    if !payload.album.isEmpty {
                        Text(payload.album)
                            .font(.system(size: 11))
                            .foregroundStyle(CompanionTheme.textTertiary)
                            .lineLimit(1)
                    }
                }

                Spacer(minLength: 0)

                // Gear + quit icons top-right
                HStack(spacing: 8) {
                    Button {
                        showSettings = true
                    } label: {
                        Image(systemName: "gearshape")
                            .font(.system(size: 13, weight: .medium))
                            .foregroundStyle(CompanionTheme.textTertiary)
                            .frame(width: 28, height: 28)
                    }
                    .buttonStyle(.plain)
                    .help("Settings")

                    Button {
                        NSApplication.shared.terminate(nil)
                    } label: {
                        Image(systemName: "power")
                            .font(.system(size: 13, weight: .medium))
                            .foregroundStyle(CompanionTheme.textTertiary)
                            .frame(width: 28, height: 28)
                    }
                    .buttonStyle(.plain)
                    .help("Quit ScryBar Companion")
                }
            }

            // Progress bar (only when we have real elapsed data)
            if hasProgress {
                HStack(spacing: 6) {
                    GeometryReader { geometry in
                        ZStack(alignment: .leading) {
                            RoundedRectangle(cornerRadius: 1.5)
                                .fill(CompanionTheme.border)
                                .frame(height: 2)
                            RoundedRectangle(cornerRadius: 1.5)
                                .fill(CompanionTheme.accent)
                                .frame(width: progressWidth(totalWidth: geometry.size.width), height: 2)
                        }
                    }
                    .frame(height: 2)

                    Text(formatTime(interpolatedElapsed))
                        .font(.system(size: 11, weight: .medium, design: .monospaced))
                        .foregroundStyle(CompanionTheme.textTertiary)
                }
            }

            // Status row
            HStack(spacing: 8) {
                HStack(spacing: 3) {
                    Image(systemName: payload.isPlaying ? "play.fill" : "pause.fill")
                        .font(.system(size: 8))
                    Text(payload.isPlaying ? "Playing" : "Paused")
                        .font(.system(size: 10, weight: .medium))
                }
                .foregroundStyle(payload.isPlaying ? CompanionTheme.success : CompanionTheme.textTertiary)

                HStack(spacing: 3) {
                    Circle()
                        .fill(deviceConnected ? CompanionTheme.success : (hasDevice ? CompanionTheme.warning : CompanionTheme.textDisabled))
                        .frame(width: 5, height: 5)
                    Text(deviceStatus)
                        .font(.system(size: 10, weight: .medium))
                        .foregroundStyle(CompanionTheme.textTertiary)
                }

                Spacer()
            }

            // ── Mac Stats section ─────────────────────────────────────────
            Divider()
                .background(CompanionTheme.dividerStrong)
                .padding(.vertical, 2)

            MacStatsPopoverSection()
                .environmentObject(model)

            Divider()
                .background(CompanionTheme.dividerStrong)
                .padding(.vertical, 2)

            BambuPopoverSection()
                .environmentObject(model)
            }
            .padding(12)
            .background {
                GeometryReader { geometry in
                    Color.clear.preference(
                        key: PopoverBodyHeightPreferenceKey.self,
                        value: geometry.size.height
                    )
                }
            }
        }
        .scrollIndicators(measuredContentHeight > viewportHeight + 1 ? .visible : .hidden)
        .frame(width: 360)
        .frame(maxHeight: .infinity, alignment: .top)
        .background(CompanionTheme.windowBackground)
        .preferredColorScheme(.dark)
        .onPreferenceChange(PopoverBodyHeightPreferenceKey.self) { height in
            guard height > 0 else { return }
            let roundedHeight = ceil(height)
            measuredContentHeight = roundedHeight
            onContentHeightChange(roundedHeight)
        }
        .onReceive(Timer.publish(every: 1, on: .main, in: .common).autoconnect()) { tick in
            now = tick
        }
    }

    private func progressWidth(totalWidth: CGFloat) -> CGFloat {
        guard payload.durationSec > 0 else { return 0 }
        let ratio = min(interpolatedElapsed / payload.durationSec, 1.0)
        return totalWidth * CGFloat(ratio)
    }

    private func formatTime(_ seconds: Double) -> String {
        let total = Int(max(seconds, 0))
        let m = total / 60
        let s = total % 60
        return String(format: "%d:%02d", m, s)
    }
}

struct BambuPopoverSection: View {
    @EnvironmentObject var model: AppModel

    private var printer: BambuPrinterPayload { model.currentBambu }
    private var connectionColor: Color {
        printer.connected ? CompanionTheme.bambuAccent : statusColor
    }
    private var statusColor: Color {
        if !printer.connected {
            switch model.bambuConnectionPhase {
            case .notConfigured: return CompanionTheme.warning
            case .connecting: return CompanionTheme.accent
            case .live: return CompanionTheme.bambuAccent
            case .failed: return CompanionTheme.danger
            }
        }
        if printer.errorCode != 0 || printer.hmsCount > 0 || ["FAILED", "ERROR"].contains(printer.status.uppercased()) {
            return CompanionTheme.bambuError
        }
        if ["PAUSE", "PAUSED"].contains(printer.status.uppercased()) { return CompanionTheme.bambuPaused }
        if printer.isPrinting { return CompanionTheme.bambuAccent }
        if printer.isFinished { return CompanionTheme.bambuDone }
        return CompanionTheme.bambuAccent
    }

    var body: some View {
        VStack(alignment: .leading, spacing: 10) {
            HStack {
                Label("BAMBU LAB", systemImage: "printer.fill")
                    .font(.system(size: 12, weight: .semibold))
                    .foregroundStyle(printer.connected ? CompanionTheme.bambuAccent : CompanionTheme.textTertiary)
                    .tracking(0.8)
                Spacer()
                Circle().fill(statusColor).frame(width: 7, height: 7)
                Text(printer.connected ? "Connected" : model.bambuConnectionPhase.title)
                    .font(.system(size: 11.5, weight: .semibold))
                    .foregroundStyle(connectionColor)
            }

            if printer.connected {
                HStack(alignment: .firstTextBaseline) {
                    VStack(alignment: .leading, spacing: 2) {
                        Text(printer.jobName.isEmpty ? printer.stage : printer.jobName)
                            .font(.system(size: 13.5, weight: .semibold))
                            .foregroundStyle(CompanionTheme.textPrimary)
                            .lineLimit(1)
                        Text(printer.stage)
                            .font(.system(size: 11.5, weight: .medium))
                            .foregroundStyle(CompanionTheme.textSecondary)
                            .lineLimit(1)
                    }
                    Spacer()
                    Text("\(printer.progressPercent)%")
                        .font(.system(size: 24, weight: .semibold, design: .rounded))
                        .foregroundStyle(CompanionTheme.bambuAccent)
                }

                GeometryReader { geometry in
                    ZStack(alignment: .leading) {
                        RoundedRectangle(cornerRadius: 3).fill(CompanionTheme.bambuTrack).frame(height: 6)
                        RoundedRectangle(cornerRadius: 3).fill(CompanionTheme.bambuAccent)
                            .frame(width: geometry.size.width * CGFloat(printer.progressPercent) / 100, height: 6)
                    }
                }
                .frame(height: 6)
                .accessibilityElement(children: .ignore)
                .accessibilityLabel("Print progress")
                .accessibilityValue("\(printer.progressPercent) percent")

                HStack(spacing: 8) {
                    BambuTemperatureMetric(
                        label: "NOZZLE",
                        current: printer.nozzleTempC,
                        target: printer.nozzleTargetC
                    )
                    BambuTemperatureMetric(
                        label: "BED",
                        current: printer.bedTempC,
                        target: printer.bedTargetC
                    )
                    BambuTemperatureMetric(
                        label: "CHAMBER",
                        current: printer.chamberTempC,
                        target: nil
                    )
                }

                HStack(spacing: 12) {
                    Label(remainingText, systemImage: "clock")
                    Spacer()
                    if showsFilamentChanges {
                        Label(filamentChangesText, systemImage: "arrow.triangle.2.circlepath")
                        Spacer()
                    }
                    Label(layerText, systemImage: "square.3.layers.3d")
                }
                .font(.system(size: 11.5, weight: .medium, design: .rounded))
                .foregroundStyle(CompanionTheme.bambuTextSecondary)

                if !printer.gcodeFile.isEmpty || !printer.printType.isEmpty {
                    Text([printer.printType, printer.gcodeFile].filter { !$0.isEmpty }.joined(separator: " · "))
                        .font(.system(size: 10.5, weight: .medium, design: .monospaced))
                        .foregroundStyle(CompanionTheme.bambuTextMuted)
                        .lineLimit(1)
                }

                if hasMachineTelemetry {
                    Divider().background(CompanionTheme.bambuAccent.opacity(0.22))

                    HStack(spacing: 8) {
                        BambuCompactMetric(label: "SPEED", value: speedText)
                        BambuCompactMetric(label: "FANS", value: fanText)
                        BambuCompactMetric(label: "WI-FI", value: wifiText)
                    }
                }

                ForEach(printer.amsUnits) { unit in
                    BambuAMSUnitRow(unit: unit)
                }

                if let spool = printer.externalSpool, spool.present {
                    BambuTrayRow(tray: spool, prefix: "EXT")
                }

                if printer.errorCode != 0 || printer.hmsCount > 0 {
                    Label(alertText, systemImage: "exclamationmark.triangle.fill")
                        .font(.system(size: 11.5, weight: .semibold))
                        .foregroundStyle(CompanionTheme.bambuError)
                }
            } else {
                Text(model.bambuConnectionStatus)
                    .font(.system(size: 12))
                    .foregroundStyle(CompanionTheme.textTertiary)
                    .fixedSize(horizontal: false, vertical: true)
            }
        }
        .padding(10)
        .background(CompanionTheme.bambuSurface, in: RoundedRectangle(cornerRadius: 12, style: .continuous))
        .overlay {
            RoundedRectangle(cornerRadius: 12, style: .continuous)
                .strokeBorder(printer.connected ? CompanionTheme.bambuAccent.opacity(0.40) : CompanionTheme.bambuBorder, lineWidth: 1)
        }
    }

    private var remainingText: String {
        if printer.isFinished { return "Completed" }
        return "\(printer.remainingMinutes / 60)h \(printer.remainingMinutes % 60)m left"
    }

    private var layerText: String {
        printer.totalLayers > 0
            ? "Layer \(printer.currentLayer) / \(printer.totalLayers)"
            : "Layer —"
    }

    private var showsFilamentChanges: Bool {
        printer.filamentChangesTotal >= 0 || printer.filamentChangesCompleted > 0 ||
            printer.printFilamentColors.count > 1
    }

    private var filamentChangesText: String {
        if printer.filamentChangesTotal >= 0 {
            return "Changes \(printer.filamentChangesCompleted)/\(printer.filamentChangesTotal)"
        }
        return "Changes \(printer.filamentChangesCompleted)"
    }

    private var alertText: String {
        if printer.errorCode != 0 { return "Printer error \(printer.errorCode)" }
        return "\(printer.hmsCount) printer alert\(printer.hmsCount == 1 ? "" : "s")"
    }

    private var hasMachineTelemetry: Bool {
        printer.speedPercent > 0 || printer.speedLevel > 0 ||
            printer.coolingFanPercent >= 0 || printer.heatbreakFanPercent >= 0 ||
            printer.auxiliaryFanPercent >= 0 || printer.chamberFanPercent >= 0 ||
            !printer.wifiSignal.isEmpty
    }

    private var speedText: String {
        if printer.speedPercent > 0 { return "\(printer.speedPercent)%" }
        switch printer.speedLevel {
        case 1: return "Silent"
        case 2: return "Standard"
        case 3: return "Sport"
        case 4: return "Ludicrous"
        default: return "—"
        }
    }

    private var fanText: String {
        let values = [printer.coolingFanPercent, printer.auxiliaryFanPercent, printer.chamberFanPercent]
            .filter { $0 >= 0 }
        return values.isEmpty ? "—" : values.map { "\($0)%" }.joined(separator: "/")
    }

    private var wifiText: String {
        printer.wifiSignal.isEmpty ? "—" : printer.wifiSignal
    }
}

private struct BambuCompactMetric: View {
    let label: String
    let value: String

    var body: some View {
        VStack(alignment: .leading, spacing: 2) {
            Text(label)
                .font(.system(size: 9.5, weight: .bold))
                .foregroundStyle(CompanionTheme.bambuAccent)
                .tracking(0.6)
            Text(value)
                .font(.system(size: 11.5, weight: .semibold, design: .rounded))
                .foregroundStyle(CompanionTheme.bambuTextPrimary)
                .lineLimit(1)
        }
        .padding(.horizontal, 7)
        .padding(.vertical, 6)
        .frame(maxWidth: .infinity, alignment: .leading)
        .background(CompanionTheme.bambuSurfaceElevated, in: RoundedRectangle(cornerRadius: 7, style: .continuous))
    }
}

private struct BambuAMSUnitRow: View {
    let unit: BambuAMSUnitTelemetry

    private var climate: String {
        var parts: [String] = []
        if unit.humidityPercent >= 0 { parts.append("\(unit.humidityPercent)% RH") }
        if unit.temperatureC > 0 { parts.append(String(format: "%.0f°C", unit.temperatureC)) }
        return parts.joined(separator: " · ")
    }

    var body: some View {
        VStack(alignment: .leading, spacing: 5) {
            HStack {
                Text("AMS \(unit.id + 1)")
                    .font(.system(size: 10.5, weight: .bold))
                    .foregroundStyle(CompanionTheme.bambuAccent)
                Spacer()
                if !climate.isEmpty {
                    Text(climate)
                        .font(.system(size: 10.5, weight: .medium, design: .rounded))
                        .foregroundStyle(CompanionTheme.bambuTextSecondary)
                }
            }
            ForEach(unit.trays.filter(\.present)) { tray in
                BambuTrayRow(tray: tray, prefix: "A\(unit.id + 1)-\(tray.trayIndex + 1)")
            }
        }
    }
}

private struct BambuTrayRow: View {
    let tray: BambuTrayTelemetry
    let prefix: String

    var body: some View {
        HStack(spacing: 7) {
            Circle()
                .fill(Color(bambuHex: tray.colorHex) ?? CompanionTheme.textDisabled)
                .frame(width: 8, height: 8)
                .overlay(Circle().stroke(Color.white.opacity(0.4), lineWidth: 0.5))
            Text(prefix)
                .font(.system(size: 10, weight: .bold, design: .monospaced))
                .foregroundStyle(tray.active ? CompanionTheme.bambuAccent : CompanionTheme.bambuTextMuted)
            Text(tray.displayName)
                .font(.system(size: 11, weight: tray.active ? .semibold : .medium))
                .foregroundStyle(CompanionTheme.bambuTextPrimary)
                .lineLimit(1)
            Spacer(minLength: 4)
            if tray.remainingPercent >= 0 {
                Text("\(tray.remainingPercent)%")
                    .font(.system(size: 10.5, weight: .semibold, design: .rounded))
                    .foregroundStyle(CompanionTheme.bambuTextSecondary)
            }
        }
    }
}

private extension Color {
    init?(bambuHex value: String) {
        let hex = value.trimmingCharacters(in: CharacterSet.alphanumerics.inverted)
        guard hex.count >= 6, let rgb = UInt64(hex.prefix(6), radix: 16) else { return nil }
        self.init(
            red: Double((rgb >> 16) & 0xFF) / 255,
            green: Double((rgb >> 8) & 0xFF) / 255,
            blue: Double(rgb & 0xFF) / 255
        )
    }
}

private struct BambuTemperatureMetric: View {
    let label: String
    let current: Float
    let target: Float?

    var body: some View {
        VStack(alignment: .leading, spacing: 3) {
            Text(label)
                .font(.system(size: 10.5, weight: .semibold))
                .foregroundStyle(CompanionTheme.bambuAccent)
                .tracking(0.5)
            Text(value)
                .font(.system(size: 13, weight: .semibold, design: .rounded))
                .foregroundStyle(CompanionTheme.bambuTextPrimary)
                .lineLimit(1)
        }
        .padding(.horizontal, 8)
        .padding(.vertical, 7)
        .frame(maxWidth: .infinity, alignment: .leading)
        .background(CompanionTheme.bambuSurfaceElevated, in: RoundedRectangle(cornerRadius: 8, style: .continuous))
        .accessibilityElement(children: .ignore)
        .accessibilityLabel(label.capitalized)
        .accessibilityValue(value.replacingOccurrences(of: "/", with: "target"))
    }

    private var value: String {
        if let target {
            return String(format: "%.0f° / %.0f°", current, target)
        }
        return String(format: "%.0f°", current)
    }
}

struct ArtworkView: View {
    let artworkURL: String?
    let artworkID: String?
    let artworkWidth: Int?
    let artworkHeight: Int?
    let artworkRGB565B64: String?
    let title: String
    let artist: String
    @State private var image: NSImage?

    /// Cache key: reload artwork only when the track changes
    private var trackKey: String { "\(title)|\(artist)|\(artworkID ?? artworkURL ?? "")" }

    var body: some View {
        ZStack {
            RoundedRectangle(cornerRadius: 8, style: .continuous)
                .fill(CompanionTheme.surfaceElevated)

            if let image {
                Image(nsImage: image)
                    .resizable()
                    .scaledToFill()
                    .clipShape(RoundedRectangle(cornerRadius: 8, style: .continuous))
            } else {
                Image(systemName: "music.note")
                    .font(.system(size: 22, weight: .medium))
                    .foregroundStyle(CompanionTheme.textTertiary)
            }
        }
        .overlay(
            RoundedRectangle(cornerRadius: 8, style: .continuous)
                .stroke(CompanionTheme.border, lineWidth: 1)
        )
        .task(id: trackKey) {
            image = await loadArtwork()
        }
    }

    private func loadArtwork() async -> NSImage? {
        // 1. Reuse the exact pixels already prepared for ScryBar. This avoids
        // a second network request and also works for opaque MediaRemote IDs.
        if let artworkRGB565B64,
           let artworkWidth,
           let artworkHeight,
           let embedded = ArtworkRGB565Decoder.image(
               base64: artworkRGB565B64,
               width: artworkWidth,
               height: artworkHeight
           ) {
            return embedded
        }

        // 2. Try direct URL if available
        if let img = await fetchImage(from: artworkURL) {
            return img
        }

        // 3. Fallback: iTunes Search API
        guard !title.isEmpty else { return nil }
        let query = "\(artist) \(title)".trimmingCharacters(in: .whitespaces)
        guard let encoded = query.addingPercentEncoding(withAllowedCharacters: .urlQueryAllowed),
              let searchURL = URL(string: "https://itunes.apple.com/search?term=\(encoded)&media=music&limit=1") else {
            return nil
        }

        do {
            let (data, _) = try await URLSession.shared.data(from: searchURL)
            guard let json = try JSONSerialization.jsonObject(with: data) as? [String: Any],
                  let results = json["results"] as? [[String: Any]],
                  let first = results.first,
                  let artURL = first["artworkUrl100"] as? String else { return nil }
            // Upgrade to 600x600
            let hiRes = artURL.replacingOccurrences(of: "100x100", with: "600x600")
            return await fetchImage(from: hiRes)
        } catch {
            return nil
        }
    }

    private func fetchImage(from rawURL: String?) async -> NSImage? {
        guard let rawURL, !rawURL.isEmpty, let url = URL(string: rawURL) else { return nil }

        if url.isFileURL {
            return NSImage(contentsOf: url)
        }


        guard let scheme = url.scheme?.lowercased(), scheme == "http" || scheme == "https" else {
            return nil
        }

        do {
            let (data, _) = try await URLSession.shared.data(from: url)
            return NSImage(data: data)
        } catch {
            return nil
        }
    }
}

enum ArtworkRGB565Decoder {
    static func image(base64: String, width: Int, height: Int) -> NSImage? {
        guard width > 0, height > 0,
              let source = Data(base64Encoded: base64),
              source.count == width * height * 2,
              let representation = NSBitmapImageRep(
                  bitmapDataPlanes: nil,
                  pixelsWide: width,
                  pixelsHigh: height,
                  bitsPerSample: 8,
                  samplesPerPixel: 4,
                  hasAlpha: true,
                  isPlanar: false,
                  colorSpaceName: .deviceRGB,
                  bytesPerRow: width * 4,
                  bitsPerPixel: 32
              ),
              let destination = representation.bitmapData else { return nil }

        source.withUnsafeBytes { bytes in
            guard let sourceBytes = bytes.bindMemory(to: UInt8.self).baseAddress else { return }
            for pixel in 0..<(width * height) {
                let sourceOffset = pixel * 2
                let packed = UInt16(sourceBytes[sourceOffset]) << 8 |
                    UInt16(sourceBytes[sourceOffset + 1])
                let destinationOffset = pixel * 4
                destination[destinationOffset] = UInt8(((packed >> 11) & 0x1F) * 255 / 31)
                destination[destinationOffset + 1] = UInt8(((packed >> 5) & 0x3F) * 255 / 63)
                destination[destinationOffset + 2] = UInt8((packed & 0x1F) * 255 / 31)
                destination[destinationOffset + 3] = 255
            }
        }

        let image = NSImage(size: NSSize(width: width, height: height))
        image.addRepresentation(representation)
        return image
    }
}

// MARK: - Mac Stats popover section

struct MacStatsPopoverSection: View {
    @EnvironmentObject var model: AppModel

    private var stats: MacStatsPayload? { model.latestMacStats }
    private var isStale: Bool {
        guard let s = stats else { return true }
        return Date().timeIntervalSince(s.updatedAt) > 15
    }

    var body: some View {
        VStack(alignment: .leading, spacing: 8) {

            // Header row
            HStack {
                Text("MAC STATS")
                    .font(.system(size: 12, weight: .semibold))
                    .foregroundStyle(CompanionTheme.textTertiary)
                    .tracking(0.8)
                Spacer()
                if stats != nil {
                    HStack(spacing: 4) {
                        Circle()
                            .fill(isStale ? CompanionTheme.warning : CompanionTheme.success)
                            .frame(width: 7, height: 7)
                        Text(isStale ? "Stale" : "Live")
                            .font(.system(size: 11.5, weight: .semibold))
                            .foregroundStyle(isStale ? CompanionTheme.warning : CompanionTheme.success)
                    }
                }
            }

            if let s = stats {
                // ── Usage + Temp: 2×2 compact grid ──────────────────────
                HStack(spacing: 8) {
                    MacStatCell(
                        label: "CPU",
                        primary: s.cpuUsagePct.map { String(format: "%.0f%%", $0) } ?? "—",
                        secondary: s.cpuTempC.map   { tempString($0) },
                        primaryTone: s.cpuUsagePct.map { usageTone($0) } ?? .neutral,
                        secondaryTone: s.cpuTempC.map  { tempTone($0)  } ?? .neutral
                    )
                    Rectangle()
                        .fill(CompanionTheme.divider)
                        .frame(width: 1, height: 58)
                    MacStatCell(
                        label: "GPU",
                        primary: s.gpuUsagePct.map { String(format: "%.0f%%", $0) } ?? "—",
                        secondary: s.gpuTempC.map   { tempString($0) },
                        primaryTone: s.gpuUsagePct.map { usageTone($0) } ?? .neutral,
                        secondaryTone: s.gpuTempC.map  { tempTone($0)  } ?? .neutral
                    )
                }
                .frame(maxWidth: .infinity)
                .fixedSize(horizontal: false, vertical: true)

                // ── RAM bar ──────────────────────────────────────────────
                MacStatBar(
                    label: "RAM",
                    used: s.ramUsedGB, total: s.ramTotalGB,
                    unit: "GB", tone: .accent
                )

                // ── Disk bar ─────────────────────────────────────────────
                MacStatBar(
                    label: "DISK",
                    used: s.diskUsedGB, total: s.diskTotalGB,
                    unit: "GB",
                    tone: s.diskTotalGB > 0 && (s.diskUsedGB / s.diskTotalGB) > 0.9 ? .danger
                        : s.diskTotalGB > 0 && (s.diskUsedGB / s.diskTotalGB) > 0.75 ? .warning
                        : .accent
                )

                // macmon hint if temps unavailable
                if s.cpuTempC == nil && !model.macmonAvailable {
                    Text("Install macmon for temperatures: brew install macmon")
                        .font(.system(size: 9))
                        .foregroundStyle(CompanionTheme.textDisabled)
                        .frame(maxWidth: .infinity, alignment: .leading)
                }

            } else {
                Text("Waiting for first sample…")
                    .font(.system(size: 11))
                    .foregroundStyle(CompanionTheme.textDisabled)
                    .frame(maxWidth: .infinity, alignment: .center)
                    .padding(.vertical, 6)
            }
        }
    }

    // MARK: Helpers

    private func tempString(_ c: Float) -> String { String(format: "%.0f°C", c) }

    private func usageTone(_ pct: Float) -> CompanionPalette.Tone {
        pct >= 80 ? .danger : pct >= 60 ? .warning : .neutral
    }

    private func tempTone(_ c: Float) -> CompanionPalette.Tone {
        c >= 85 ? .danger : c >= 70 ? .warning : .neutral
    }
}

/// Compact cell showing label + big primary value + optional small secondary value.
private struct MacStatCell: View {
    let label: String
    let primary: String
    let secondary: String?
    var primaryTone: CompanionPalette.Tone = .neutral
    var secondaryTone: CompanionPalette.Tone = .neutral

    var body: some View {
        VStack(alignment: .leading, spacing: 2) {
                Text(label)
                    .font(.system(size: 11, weight: .semibold))
                .foregroundStyle(CompanionTheme.textTertiary)
                .tracking(0.5)
            Text(primary)
                .font(.system(size: 22, weight: .semibold, design: .rounded))
                .foregroundStyle(CompanionTheme.toneColor(primaryTone) == CompanionTheme.textSecondary
                                 ? CompanionTheme.textPrimary
                                 : CompanionTheme.toneColor(primaryTone))
            if let secondary {
                Text(secondary)
                    .font(.system(size: 12, weight: .medium, design: .monospaced))
                    .foregroundStyle(secondaryTone == .neutral
                                     ? CompanionTheme.textTertiary
                                     : CompanionTheme.toneColor(secondaryTone))
            }
        }
        .frame(maxWidth: .infinity, alignment: .leading)
    }
}

/// Compact labeled progress bar.
private struct MacStatBar: View {
    let label: String
    let used: Float
    let total: Float
    let unit: String
    var tone: CompanionPalette.Tone = .accent

    private var ratio: Double {
        guard total > 0 else { return 0 }
        return min(Double(used / total), 1.0)
    }

    private var barColor: Color {
        switch tone {
        case .danger:  return CompanionTheme.danger
        case .warning: return CompanionTheme.warning
        default:       return CompanionTheme.accent
        }
    }

    var body: some View {
        VStack(alignment: .leading, spacing: 3) {
            HStack(alignment: .firstTextBaseline) {
                Text(label)
                    .font(.system(size: 11, weight: .semibold))
                    .foregroundStyle(CompanionTheme.textTertiary)
                    .tracking(0.5)
                Spacer()
                Text(barLabel)
                    .font(.system(size: 11, weight: .medium, design: .monospaced))
                    .foregroundStyle(CompanionTheme.textSecondary)
            }
            GeometryReader { geo in
                ZStack(alignment: .leading) {
                    RoundedRectangle(cornerRadius: 2, style: .continuous)
                        .fill(CompanionTheme.surfaceElevated)
                        .frame(height: 4)
                    RoundedRectangle(cornerRadius: 2, style: .continuous)
                        .fill(barColor)
                        .frame(width: geo.size.width * CGFloat(ratio), height: 4)
                }
            }
            .frame(height: 4)
        }
        .accessibilityElement(children: .ignore)
        .accessibilityLabel(label)
        .accessibilityValue(barLabel)
    }

    private var barLabel: String {
        let u = used >= 1000 ? String(format: "%.0f", used) : String(format: "%.1f", used)
        let t = total >= 1000 ? String(format: "%.0f", total) : String(format: "%.0f", total)
        return "\(u) / \(t) \(unit)"
    }
}
