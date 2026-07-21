import AppKit
import SwiftUI

struct PopoverContentView: View {
    @EnvironmentObject var model: AppModel
    @State private var showSettings = false

    var body: some View {
        Group {
            if showSettings {
                SettingsView(showSettings: $showSettings)
                    .environmentObject(model)
            } else {
                NowPlayingPopoverView(showSettings: $showSettings)
                    .environmentObject(model)
            }
        }
        .onAppear {
            if model.showSettingsOnOpen {
                model.showSettingsOnOpen = false
                showSettings = true
            }
        }
    }
}

struct NowPlayingPopoverView: View {
    @EnvironmentObject var model: AppModel
    @Binding var showSettings: Bool
    @State private var now = Date()

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
        VStack(alignment: .leading, spacing: 8) {
            // Artwork + track info
            HStack(alignment: .top, spacing: 10) {
                ArtworkView(artworkURL: payload.artworkURL, title: payload.title, artist: payload.artist)
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
                            .font(.system(size: 12))
                            .foregroundStyle(CompanionTheme.textDisabled)
                    }
                    .buttonStyle(.plain)
                    .help("Settings")

                    Button {
                        NSApplication.shared.terminate(nil)
                    } label: {
                        Image(systemName: "power")
                            .font(.system(size: 12))
                            .foregroundStyle(CompanionTheme.textDisabled)
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
                        .font(.system(size: 9, weight: .medium, design: .monospaced))
                        .foregroundStyle(CompanionTheme.textDisabled)
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
        }
        .padding(12)
        .frame(width: 300)
        .background(CompanionTheme.windowBackground)
        .preferredColorScheme(.dark)
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

struct ArtworkView: View {
    let artworkURL: String?
    let title: String
    let artist: String
    @State private var image: NSImage?

    /// Cache key: reload artwork only when the track changes
    private var trackKey: String { "\(title)|\(artist)|\(artworkURL ?? "")" }

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
        // 1. Try direct URL if available
        if let img = await fetchImage(from: artworkURL) {
            return img
        }

        // 2. Fallback: iTunes Search API
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
                    .font(.system(size: 10, weight: .semibold))
                    .foregroundStyle(CompanionTheme.textTertiary)
                    .tracking(0.8)
                Spacer()
                if stats != nil {
                    HStack(spacing: 4) {
                        Circle()
                            .fill(isStale ? CompanionTheme.warning : CompanionTheme.success)
                            .frame(width: 5, height: 5)
                        Text(isStale ? "Stale" : "Live")
                            .font(.system(size: 9, weight: .medium))
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
                        .frame(width: 1)
                    MacStatCell(
                        label: "GPU",
                        primary: s.gpuUsagePct.map { String(format: "%.0f%%", $0) } ?? "—",
                        secondary: s.gpuTempC.map   { tempString($0) },
                        primaryTone: s.gpuUsagePct.map { usageTone($0) } ?? .neutral,
                        secondaryTone: s.gpuTempC.map  { tempTone($0)  } ?? .neutral
                    )
                }
                .frame(maxWidth: .infinity)

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
                .font(.system(size: 9, weight: .medium))
                .foregroundStyle(CompanionTheme.textTertiary)
                .tracking(0.5)
            Text(primary)
                .font(.system(size: 18, weight: .semibold, design: .rounded))
                .foregroundStyle(CompanionTheme.toneColor(primaryTone) == CompanionTheme.textSecondary
                                 ? CompanionTheme.textPrimary
                                 : CompanionTheme.toneColor(primaryTone))
            if let secondary {
                Text(secondary)
                    .font(.system(size: 10, weight: .medium, design: .monospaced))
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
                    .font(.system(size: 9, weight: .semibold))
                    .foregroundStyle(CompanionTheme.textTertiary)
                    .tracking(0.5)
                Spacer()
                Text(barLabel)
                    .font(.system(size: 9, weight: .medium, design: .monospaced))
                    .foregroundStyle(CompanionTheme.textDisabled)
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
    }

    private var barLabel: String {
        let u = used >= 1000 ? String(format: "%.0f", used) : String(format: "%.1f", used)
        let t = total >= 1000 ? String(format: "%.0f", total) : String(format: "%.0f", total)
        return "\(u) / \(t) \(unit)"
    }
}
