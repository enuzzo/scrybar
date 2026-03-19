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

    private var payload: NowPlayingPayload { model.currentPayload }
    private var hasDevice: Bool { !model.discoveredEndpoints.isEmpty || model.selectedEndpoint != nil }

    var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            // Artwork + track info
            HStack(alignment: .top, spacing: 12) {
                ArtworkView(artworkURL: payload.artworkURL)
                    .frame(width: 64, height: 64)

                VStack(alignment: .leading, spacing: 3) {
                    Text(payload.title)
                        .font(.system(size: 15, weight: .semibold))
                        .foregroundStyle(CompanionTheme.textPrimary)
                        .lineLimit(1)

                    Text(payload.artist)
                        .font(.system(size: 13, weight: .medium))
                        .foregroundStyle(CompanionTheme.textSecondary)
                        .lineLimit(1)

                    if !payload.album.isEmpty {
                        Text(payload.album)
                            .font(.system(size: 12))
                            .foregroundStyle(CompanionTheme.textTertiary)
                            .lineLimit(1)
                    }

                    Spacer(minLength: 0)

                    // Status row
                    HStack(spacing: 12) {
                        HStack(spacing: 4) {
                            Image(systemName: payload.isPlaying ? "play.fill" : "pause.fill")
                                .font(.system(size: 10))
                            Text(payload.isPlaying ? "Playing" : "Paused")
                                .font(.system(size: 11, weight: .medium))
                        }
                        .foregroundStyle(payload.isPlaying ? CompanionTheme.success : CompanionTheme.textTertiary)

                        HStack(spacing: 4) {
                            Circle()
                                .fill(hasDevice ? CompanionTheme.success : CompanionTheme.textDisabled)
                                .frame(width: 6, height: 6)
                            Text(hasDevice ? "ScryBar" : "No device")
                                .font(.system(size: 11, weight: .medium))
                                .foregroundStyle(CompanionTheme.textTertiary)
                        }
                    }
                }
            }

            // Progress bar
            VStack(spacing: 4) {
                GeometryReader { geometry in
                    ZStack(alignment: .leading) {
                        RoundedRectangle(cornerRadius: 2)
                            .fill(CompanionTheme.border)
                            .frame(height: 3)

                        RoundedRectangle(cornerRadius: 2)
                            .fill(CompanionTheme.accent)
                            .frame(width: progressWidth(totalWidth: geometry.size.width), height: 3)
                    }
                }
                .frame(height: 3)

                HStack {
                    Spacer()
                    Text(formatTime(payload.elapsedSec))
                        .font(.system(size: 10, weight: .medium, design: .monospaced))
                        .foregroundStyle(CompanionTheme.textTertiary)
                }
            }

            // Bottom bar
            HStack {
                Button {
                    model.sendNow()
                } label: {
                    Label("Send Now", systemImage: "paperplane.fill")
                        .font(.system(size: 12, weight: .medium))
                }
                .buttonStyle(CompanionPrimaryButtonStyle())
                .controlSize(.small)
                .disabled(!hasDevice)

                Spacer()

                Button {
                    showSettings = true
                } label: {
                    Image(systemName: "gearshape.fill")
                        .font(.system(size: 14))
                        .foregroundStyle(CompanionTheme.textSecondary)
                }
                .buttonStyle(.plain)
            }
        }
        .padding(16)
        .frame(width: 320)
        .background(CompanionTheme.windowBackground)
        .preferredColorScheme(.dark)
    }

    private func progressWidth(totalWidth: CGFloat) -> CGFloat {
        guard payload.durationSec > 0 else { return 0 }
        let ratio = min(payload.elapsedSec / payload.durationSec, 1.0)
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
    @State private var image: NSImage?

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
        .task(id: artworkURL) {
            image = await loadArtwork()
        }
    }

    private func loadArtwork() async -> NSImage? {
        guard let rawURL = artworkURL, !rawURL.isEmpty else { return nil }
        guard let url = URL(string: rawURL) else { return nil }

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