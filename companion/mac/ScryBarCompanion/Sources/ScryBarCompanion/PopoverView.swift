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
    private var hasDevice: Bool { !model.discoveredEndpoints.isEmpty || model.selectedEndpoint != nil }
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

                // Gear icon top-right
                Button {
                    showSettings = true
                } label: {
                    Image(systemName: "gearshape")
                        .font(.system(size: 12))
                        .foregroundStyle(CompanionTheme.textDisabled)
                }
                .buttonStyle(.plain)
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
                        .fill(hasDevice ? CompanionTheme.success : CompanionTheme.textDisabled)
                        .frame(width: 5, height: 5)
                    Text(hasDevice ? "ScryBar" : "No device")
                        .font(.system(size: 10, weight: .medium))
                        .foregroundStyle(CompanionTheme.textTertiary)
                }

                Spacer()
            }
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