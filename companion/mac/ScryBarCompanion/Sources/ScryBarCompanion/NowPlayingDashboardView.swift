import AppKit
import SwiftUI

struct NowPlayingDashboardView: View {
    @EnvironmentObject private var model: AppModel

    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 18) {
                dashboardHeader

                ViewThatFits(in: .horizontal) {
                    HStack(alignment: .top, spacing: 18) {
                        connectionCard
                            .frame(maxWidth: 360, alignment: .topLeading)
                        nowPlayingCard
                    }

                    VStack(alignment: .leading, spacing: 18) {
                        connectionCard
                        nowPlayingCard
                    }
                }

                payloadCard
            }
            .padding(20)
            .frame(maxWidth: .infinity, alignment: .leading)
        }
        .background(DashboardPalette.windowBackground.ignoresSafeArea())
        .preferredColorScheme(CompanionTheme.preferredColorScheme)
    }
}

private extension NowPlayingDashboardView {
    var dashboardHeader: some View {
        DashboardCard {
            HStack(alignment: .top, spacing: 16) {
                VStack(alignment: .leading, spacing: 8) {
                    VersionBadge(label: CompanionBuildInfo.displayString)

                    Text("ScryBar Companion")
                        .font(.system(size: 27, weight: .semibold))
                        .foregroundStyle(DashboardPalette.primaryText)

                    Text("Discovery, transport, and now-playing metadata in one focused macOS utility surface.")
                        .font(.system(size: 13, weight: .regular))
                        .foregroundStyle(DashboardPalette.secondaryText)
                        .fixedSize(horizontal: false, vertical: true)
                }

                Spacer(minLength: 12)

                VStack(alignment: .trailing, spacing: 8) {
                    StatusPill(
                        systemImage: "dot.radiowaves.left.and.right",
                        title: model.discoveryStatus,
                        tint: .blue
                    )

                    StatusPill(
                        systemImage: model.currentPayload.isPlaying ? "play.fill" : "pause.fill",
                        title: model.currentPayload.isPlaying ? "Playing" : "Paused",
                        tint: model.currentPayload.isPlaying ? .green : .orange
                    )
                }
            }
        }
    }

    var connectionCard: some View {
        DashboardCard(title: "Connection", subtitle: "Find a ScryBar and keep a target ready.") {
            VStack(alignment: .leading, spacing: 14) {
                HStack(alignment: .firstTextBaseline) {
                    VStack(alignment: .leading, spacing: 4) {
                        Text("Discovery")
                            .font(.system(size: 13, weight: .semibold))
                        Text(model.discoveryStatus)
                            .font(.system(size: 12))
                            .foregroundStyle(DashboardPalette.secondaryText)
                            .fixedSize(horizontal: false, vertical: true)
                    }

                    Spacer()

                    Button {
                        model.rescan()
                    } label: {
                        Label("Rescan", systemImage: "arrow.clockwise")
                    }
                    .buttonStyle(CompanionCompactActionButtonStyle())
                    .controlSize(.small)
                }

                Divider().overlay(DashboardPalette.divider)

                VStack(alignment: .leading, spacing: 10) {
                    Text("Discovered ScryBars")
                        .font(.system(size: 12, weight: .semibold))
                        .foregroundStyle(DashboardPalette.secondaryText)
                        .textCase(.uppercase)

                    if model.discoveredEndpoints.isEmpty {
                        ContentUnavailableView(
                            "No ScryBar Found",
                            systemImage: "dot.radiowaves.left.and.right",
                            description: Text("Wait for Bonjour discovery or enter a manual host below.")
                        )
                        .padding(.vertical, 6)
                    } else {
                        VStack(alignment: .leading, spacing: 8) {
                            ForEach(model.discoveredEndpoints) { endpoint in
                                EndpointRow(
                                    endpoint: endpoint,
                                    isSelected: model.selectedDiscoveredEndpointID == endpoint.id
                                ) {
                                    model.selectedDiscoveredEndpointID = endpoint.id
                                }
                            }
                        }
                    }
                }

                Divider().overlay(DashboardPalette.divider)

                VStack(alignment: .leading, spacing: 10) {
                    Text("Manual Target")
                        .font(.system(size: 12, weight: .semibold))
                        .foregroundStyle(DashboardPalette.secondaryText)
                        .textCase(.uppercase)

                    Grid(alignment: .leading, horizontalSpacing: 12, verticalSpacing: 10) {
                        GridRow {
                            fieldLabel("Host")
                            TextField("Hostname or IP", text: $model.manualHost)
                                .textFieldStyle(.roundedBorder)
                        }

                        GridRow {
                            fieldLabel("Port")
                            TextField("8080", text: $model.manualPort)
                                .textFieldStyle(.roundedBorder)
                        }
                    }

                    HStack(spacing: 10) {
                        Button("Save Manual Target") {
                            model.saveManualTarget()
                        }
                        .buttonStyle(CompanionCompactActionButtonStyle())
                        .controlSize(.small)

                        if let endpoint = model.selectedEndpoint {
                            StatusPill(
                                systemImage: "target",
                                title: endpoint.displayName,
                                tint: .blue
                            )
                        } else {
                            StatusPill(
                                systemImage: "questionmark.circle",
                                title: "No target selected",
                                tint: .gray
                            )
                        }
                    }
                }
            }
        }
    }

    var nowPlayingCard: some View {
        DashboardCard(title: "Now Playing", subtitle: "Pick a provider, inspect metadata, then send.") {
            VStack(alignment: .leading, spacing: 16) {
                Picker("Provider", selection: $model.providerKind) {
                    ForEach(ProviderKind.allCases) { provider in
                        Text(provider.rawValue).tag(provider)
                    }
                }
                .pickerStyle(.segmented)

                HStack(alignment: .top, spacing: 16) {
                    ArtworkTile(
                        summary: model.currentPayload.artworkSummary,
                        artworkURL: model.currentPayload.artworkURL
                    )
                        .frame(width: 128)

                    VStack(alignment: .leading, spacing: 12) {
                        VStack(alignment: .leading, spacing: 6) {
                            Text(model.currentPayload.title)
                                .font(.system(size: 24, weight: .semibold))
                                .foregroundStyle(DashboardPalette.primaryText)
                                .lineLimit(2)

                            Text(model.currentPayload.artist)
                                .font(.system(size: 15, weight: .medium))
                                .foregroundStyle(DashboardPalette.secondaryText)
                                .lineLimit(2)

                            if !model.currentPayload.album.isEmpty {
                                Text(model.currentPayload.album)
                                    .font(.system(size: 13))
                                    .foregroundStyle(DashboardPalette.tertiaryText)
                                    .lineLimit(2)
                            }
                        }

                        MetricGrid {
                            metric("Source", model.currentPayload.source.isEmpty ? "—" : model.currentPayload.source)
                            metric("App", model.currentPayload.appName.isEmpty ? "—" : model.currentPayload.appName)
                            metric("State", model.currentPayload.isPlaying ? "Playing" : "Paused")
                            metric("Sync", model.currentPayload.inSync ? "In sync" : "Out of sync")
                            metric("Time", "\(Int(model.currentPayload.elapsedSec))s / \(Int(model.currentPayload.durationSec))s")
                            metric("Remaining", "\(model.currentPayload.remainingPercent)%")
                        }
                    }

                    Spacer(minLength: 0)
                }

                Divider().overlay(DashboardPalette.divider)

                HStack(alignment: .center, spacing: 12) {
                    Toggle("Auto-send every second", isOn: $model.autoSendEnabled)

                    Spacer()

                    Button {
                        model.refreshPayload()
                    } label: {
                        Label("Refresh", systemImage: "arrow.clockwise")
                    }
                    .buttonStyle(CompanionCompactActionButtonStyle())
                    .controlSize(.small)

                    Button {
                        model.sendNow()
                    } label: {
                        Label("Send Now", systemImage: "paperplane.fill")
                    }
                    .buttonStyle(.borderedProminent)
                    .tint(.blue)
                    .controlSize(.small)

                    if model.providerKind == .mock {
                        Button {
                            model.nextMockTrack()
                        } label: {
                            Label("Next Mock Track", systemImage: "forward.fill")
                        }
                        .buttonStyle(CompanionCompactActionButtonStyle())
                        .controlSize(.small)
                    }
                }

                StatusPill(
                    systemImage: "arrow.triangle.2.circlepath",
                    title: model.lastSendStatus,
                    tint: .indigo
                )
            }
        }
    }

    var payloadCard: some View {
        DashboardCard(title: "Payload Preview", subtitle: "What gets posted to `/api/now-playing`.") {
            VStack(alignment: .leading, spacing: 10) {
                CompanionCodeBlock(text: model.currentPayload.prettyJSON, minHeight: 240)

                HStack {
                    StatusPill(
                        systemImage: "clock",
                        title: "Updated \(model.currentPayload.updatedAt.formatted(date: .omitted, time: .standard))",
                        tint: .gray
                    )

                    Spacer()

                    Text(model.currentPayload.artworkSummary)
                        .font(.system(size: 12, weight: .medium))
                        .foregroundStyle(DashboardPalette.secondaryText)
                }
            }
        }
    }

    func fieldLabel(_ text: String) -> some View {
        Text(text)
            .font(.system(size: 12, weight: .semibold))
            .foregroundStyle(DashboardPalette.secondaryText)
    }

    func metric(_ label: String, _ value: String) -> some View {
        VStack(alignment: .leading, spacing: 2) {
            Text(label.uppercased())
                .font(.system(size: 10, weight: .semibold))
                .foregroundStyle(DashboardPalette.tertiaryText)
                .tracking(0.8)
            Text(value)
                .font(.system(size: 13, weight: .medium))
                .foregroundStyle(DashboardPalette.primaryText)
                .lineLimit(2)
        }
    }
}

private struct VersionBadge: View {
    let label: String

    var body: some View {
        Text(label)
            .font(.system(size: 11, weight: .semibold, design: .monospaced))
            .foregroundStyle(DashboardPalette.tertiaryText)
            .padding(.horizontal, 10)
            .padding(.vertical, 5)
            .background(
                Capsule(style: .continuous)
                    .fill(DashboardPalette.inlineSurface.opacity(0.9))
            )
            .overlay(
                Capsule(style: .continuous)
                    .stroke(DashboardPalette.divider, lineWidth: 1)
            )
    }
}

private enum CompanionBuildInfo {
    static var displayString: String {
        let version = Bundle.main.object(forInfoDictionaryKey: "CFBundleShortVersionString") as? String ?? "0.0.0"
        let build = Bundle.main.object(forInfoDictionaryKey: "CFBundleVersion") as? String ?? "0"
        return "v\(version) (\(build))"
    }
}

private struct DashboardCard<Content: View>: View {
    private let title: String?
    private let subtitle: String?
    @ViewBuilder private let content: Content

    init(
        title: String? = nil,
        subtitle: String? = nil,
        @ViewBuilder content: () -> Content
    ) {
        self.title = title
        self.subtitle = subtitle
        self.content = content()
    }

    var body: some View {
        VStack(alignment: .leading, spacing: 14) {
            if title != nil || subtitle != nil {
                VStack(alignment: .leading, spacing: 4) {
                    if let title {
                        Text(title)
                            .font(.system(size: 17, weight: .semibold))
                            .foregroundStyle(DashboardPalette.primaryText)
                    }

                    if let subtitle {
                        Text(subtitle)
                            .font(.system(size: 12))
                            .foregroundStyle(DashboardPalette.secondaryText)
                            .fixedSize(horizontal: false, vertical: true)
                    }
                }
            }

            content
        }
        .padding(16)
        .frame(maxWidth: .infinity, alignment: .leading)
        .background(DashboardPalette.card)
        .overlay(
            RoundedRectangle(cornerRadius: 18, style: .continuous)
                .stroke(DashboardPalette.border, lineWidth: 1)
        )
        .clipShape(RoundedRectangle(cornerRadius: 18, style: .continuous))
    }
}

private struct StatusPill: View {
    let systemImage: String
    let title: String
    let tint: Color

    var body: some View {
        HStack(spacing: 8) {
            Image(systemName: systemImage)
                .font(.system(size: 11, weight: .semibold))
            Text(title)
                .font(.system(size: 12, weight: .medium))
                .lineLimit(1)
        }
        .foregroundStyle(tint)
        .padding(.vertical, 7)
        .padding(.horizontal, 11)
        .background(tint.opacity(0.12))
        .overlay(
            Capsule(style: .continuous)
                .stroke(tint.opacity(0.28), lineWidth: 1)
        )
        .clipShape(Capsule(style: .continuous))
    }
}

private struct EndpointRow: View {
    let endpoint: ScryBarEndpoint
    let isSelected: Bool
    let action: () -> Void

    var body: some View {
        Button(action: action) {
            HStack(spacing: 10) {
                Image(systemName: isSelected ? "checkmark.circle.fill" : "circle")
                    .foregroundStyle(isSelected ? Color.green : DashboardPalette.tertiaryText)

                VStack(alignment: .leading, spacing: 2) {
                    Text(endpoint.name)
                        .font(.system(size: 13, weight: .medium))
                        .foregroundStyle(DashboardPalette.primaryText)
                    Text(endpoint.displayName)
                        .font(.system(size: 12))
                        .foregroundStyle(DashboardPalette.secondaryText)
                }

                Spacer(minLength: 0)

                Text(endpoint.source.rawValue)
                    .font(.system(size: 11, weight: .semibold))
                    .foregroundStyle(DashboardPalette.tertiaryText)
                    .padding(.vertical, 4)
                    .padding(.horizontal, 8)
                    .background(DashboardPalette.inlineSurface)
                    .clipShape(Capsule(style: .continuous))
            }
            .padding(10)
            .background(isSelected ? DashboardPalette.selectionFill : DashboardPalette.inlineSurface)
            .overlay(
                RoundedRectangle(cornerRadius: 12, style: .continuous)
                    .stroke(isSelected ? DashboardPalette.accent.opacity(0.45) : DashboardPalette.border, lineWidth: 1)
            )
            .clipShape(RoundedRectangle(cornerRadius: 12, style: .continuous))
        }
        .buttonStyle(.plain)
    }
}

private struct ArtworkTile: View {
    let summary: String
    let artworkURL: String?
    @State private var previewImage: NSImage?

    private var hasArtwork: Bool {
        previewImage != nil || summary != "none"
    }

    var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            ZStack {
                RoundedRectangle(cornerRadius: 22, style: .continuous)
                    .fill(DashboardPalette.inlineSurface)
                    .overlay(
                        RoundedRectangle(cornerRadius: 22, style: .continuous)
                            .stroke(DashboardPalette.border, lineWidth: 1)
                    )

                if let previewImage {
                    Image(nsImage: previewImage)
                        .resizable()
                        .scaledToFill()
                        .frame(width: 128, height: 128)
                        .clipped()
                } else {
                    VStack(spacing: 10) {
                        Image(systemName: hasArtwork ? "photo.on.rectangle.angled" : "music.note")
                            .font(.system(size: 28, weight: .semibold))
                            .foregroundStyle(hasArtwork ? DashboardPalette.accent : DashboardPalette.secondaryText)

                        Text(hasArtwork ? "Artwork ready" : "No artwork")
                            .font(.system(size: 13, weight: .semibold))
                            .foregroundStyle(DashboardPalette.primaryText)

                        Text(summary)
                            .font(.system(size: 11))
                            .foregroundStyle(DashboardPalette.secondaryText)
                            .multilineTextAlignment(.center)
                            .lineLimit(3)
                            .padding(.horizontal, 10)
                    }
                    .padding(16)
                }
            }
            .frame(width: 128, height: 128)

            Text("Artwork")
                .font(.system(size: 11, weight: .semibold))
                .foregroundStyle(DashboardPalette.tertiaryText)
                .textCase(.uppercase)

            Text(summary)
                .font(.system(size: 12, weight: .medium))
                .foregroundStyle(DashboardPalette.secondaryText)
                .lineLimit(4)
        }
        .task(id: artworkURL) {
            previewImage = await loadArtworkPreview(from: artworkURL)
        }
    }

    private func loadArtworkPreview(from rawURL: String?) async -> NSImage? {
        guard let rawURL, !rawURL.isEmpty else { return nil }

        let url = URL(string: rawURL) ?? URL(fileURLWithPath: rawURL)
        if url.isFileURL {
            guard FileManager.default.fileExists(atPath: url.path) else { return nil }
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

private struct MetricGrid<Content: View>: View {
    @ViewBuilder let content: Content

    var body: some View {
        Grid(alignment: .leading, horizontalSpacing: 18, verticalSpacing: 12) {
            content
        }
        .padding(12)
        .background(DashboardPalette.inlineSurface)
        .overlay(
            RoundedRectangle(cornerRadius: 14, style: .continuous)
                .stroke(DashboardPalette.border, lineWidth: 1)
        )
        .clipShape(RoundedRectangle(cornerRadius: 14, style: .continuous))
    }
}

private enum DashboardPalette {
    static let windowBackground = CompanionTheme.windowBackground
    static let card = CompanionTheme.surface
    static let inlineSurface = CompanionTheme.windowBackgroundAlt
    static let selectionFill = CompanionTheme.accentSoft.opacity(0.16)
    static let accent = CompanionTheme.accent
    static let border = CompanionTheme.border
    static let primaryText = CompanionTheme.textPrimary
    static let secondaryText = CompanionTheme.textSecondary
    static let tertiaryText = CompanionTheme.textTertiary
    static let divider = CompanionTheme.divider
}
