import SwiftUI

struct ConnectionSidebarView: View {
    @EnvironmentObject var model: AppModel
    @FocusState private var manualHostFocused: Bool
    @FocusState private var manualPortFocused: Bool

    var body: some View {
        ScrollView {
            LazyVStack(alignment: .leading, spacing: 16) {
                header
                activeTargetCard
                discoveryCard
                manualTargetCard
                footerCard
            }
            .padding(18)
            .frame(maxWidth: 380, alignment: .leading)
        }
        .background(sidebarBackground.ignoresSafeArea())
        .preferredColorScheme(.dark)
    }

    private var header: some View {
        VStack(alignment: .leading, spacing: 10) {
            HStack(alignment: .firstTextBaseline, spacing: 12) {
                VStack(alignment: .leading, spacing: 4) {
                    Text("Connection")
                        .font(.system(.title2, design: .rounded).weight(.semibold))
                        .foregroundStyle(.primary)
                    Text("Find the bar, lock a target, and keep the transport clean.")
                        .font(.callout)
                        .foregroundStyle(.secondary)
                        .fixedSize(horizontal: false, vertical: true)
                }

                Spacer(minLength: 8)

                StatusPill(text: connectionTone.label, tone: connectionTone)
            }

            Divider()
        }
    }

    private var activeTargetCard: some View {
        SidebarCard(title: "Active Target", subtitle: "Where the companion is currently pointed.") {
            if let endpoint = model.selectedEndpoint {
                VStack(alignment: .leading, spacing: 10) {
                    HStack(alignment: .top, spacing: 12) {
                        Image(systemName: endpoint.source == .discovery ? "dot.radiowaves.left.and.right" : "link")
                            .font(.body.weight(.semibold))
                            .foregroundStyle(.secondary)
                            .frame(width: 22)

                        VStack(alignment: .leading, spacing: 3) {
                            Text(endpoint.name)
                                .font(.headline)
                            Text(endpoint.displayName)
                                .font(.subheadline)
                                .foregroundStyle(.secondary)
                        }

                        Spacer(minLength: 8)

                        SourceChip(text: endpoint.source.rawValue)
                    }

                    HStack(spacing: 8) {
                        Label(endpoint.host, systemImage: "server.rack")
                            .lineLimit(1)
                            .truncationMode(.middle)
                        Text("•")
                            .foregroundStyle(.tertiary)
                        Text("\(endpoint.port)")
                            .monospacedDigit()
                        if endpoint.source == .manual {
                            Text("manual override")
                                .foregroundStyle(.secondary)
                        }
                    }
                    .font(.footnote)
                    .foregroundStyle(.secondary)
                }
            } else {
                EmptyStateRow(
                    symbol: "scope",
                    title: "No active target yet",
                    message: "Pick a discovered ScryBar or enter a manual host/IP below."
                )
            }
        }
    }

    private var discoveryCard: some View {
        SidebarCard(title: "Discovered ScryBars", subtitle: discoverySummary) {
            if model.discoveredEndpoints.isEmpty {
                EmptyStateRow(
                    symbol: "dot.radiowaves.left.and.right",
                    title: "No devices found",
                    message: "The browser is scanning `_scrybar._tcp`. Manual host/IP entry remains available."
                )
            } else {
                VStack(alignment: .leading, spacing: 8) {
                    ForEach(model.discoveredEndpoints) { endpoint in
                        DiscoveryRow(
                            endpoint: endpoint,
                            isSelected: model.selectedDiscoveredEndpointID == endpoint.id
                        ) {
                            model.selectedDiscoveredEndpointID = endpoint.id
                        }
                    }
                }
            }

            HStack {
                Button {
                    model.rescan()
                } label: {
                    Label("Rescan", systemImage: "arrow.clockwise")
                }
                .buttonStyle(.bordered)

                Spacer(minLength: 8)

                Text(model.discoveryStatus)
                    .font(.footnote)
                    .foregroundStyle(.secondary)
                    .multilineTextAlignment(.trailing)
                    .frame(maxWidth: 200, alignment: .trailing)
            }
        }
    }

    private var manualTargetCard: some View {
        SidebarCard(title: "Manual Target", subtitle: "Use this when discovery is slow or unavailable.") {
            VStack(alignment: .leading, spacing: 10) {
                LabeledField(
                    title: "Hostname or IP",
                    placeholder: "scrybar-db1c.local",
                    text: manualHostBinding,
                    isFocused: $manualHostFocused
                )

                LabeledField(
                    title: "Port",
                    placeholder: "8080",
                    text: manualPortBinding,
                    isFocused: $manualPortFocused,
                    monospaced: true
                )

                HStack(spacing: 8) {
                    Button {
                        model.saveManualTarget()
                    } label: {
                        Label("Save Target", systemImage: "tray.and.arrow.down")
                    }
                    .buttonStyle(.borderedProminent)
                    .disabled(model.manualHost.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty)

                    Button {
                        model.selectedDiscoveredEndpointID = nil
                    } label: {
                        Label("Clear Discovery", systemImage: "xmark.circle")
                    }
                    .buttonStyle(.bordered)

                    Spacer(minLength: 8)
                }

                if let endpoint = model.selectedEndpoint {
                    Text("Targeting \(endpoint.displayName)")
                        .font(.footnote)
                        .foregroundStyle(.secondary)
                } else {
                    Text("Type a host to enable manual targeting.")
                        .font(.footnote)
                        .foregroundStyle(.secondary)
                }
            }
        }
    }

    private var footerCard: some View {
        SidebarCard(title: "Transport", subtitle: "Read the current companion state at a glance.") {
            VStack(alignment: .leading, spacing: 10) {
                SummaryRow(label: "Last action", value: model.lastSendStatus)
                SummaryRow(label: "Auto-send", value: model.autoSendEnabled ? "Enabled" : "Disabled")
                SummaryRow(label: "Provider", value: model.providerKind.rawValue)
            }
        }
    }

    private var discoverySummary: String {
        if model.discoveredEndpoints.isEmpty {
            return "Nothing discovered yet."
        }
        return "\(model.discoveredEndpoints.count) discovered device\(model.discoveredEndpoints.count == 1 ? "" : "s")."
    }

    private var connectionTone: StatusTone {
        let text = model.discoveryStatus.lowercased()
        if text.contains("failed") || text.contains("error") {
            return .critical(label: "Attention")
        }
        if text.contains("found") || model.selectedEndpoint != nil {
            return .success(label: "Ready")
        }
        if text.contains("no scrybar") {
            return .warning(label: "Searching")
        }
        return .neutral(label: "Live")
    }

    private var manualHostBinding: Binding<String> {
        Binding(
            get: { model.manualHost },
            set: { model.manualHost = $0 }
        )
    }

    private var manualPortBinding: Binding<String> {
        Binding(
            get: { model.manualPort },
            set: { model.manualPort = $0 }
        )
    }

    private var sidebarBackground: some View {
        Color(nsColor: .windowBackgroundColor)
    }
}

private struct SidebarCard<Content: View>: View {
    let title: String
    let subtitle: String
    @ViewBuilder var content: Content

    var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            VStack(alignment: .leading, spacing: 4) {
                Text(title)
                    .font(.headline)
                    .foregroundStyle(.primary)
                Text(subtitle)
                    .font(.caption)
                    .foregroundStyle(.secondary)
                    .fixedSize(horizontal: false, vertical: true)
            }

            content
        }
        .padding(16)
        .frame(maxWidth: .infinity, alignment: .leading)
        .background(
            RoundedRectangle(cornerRadius: 18, style: .continuous)
                .fill(Color(nsColor: .controlBackgroundColor))
        )
        .overlay(
            RoundedRectangle(cornerRadius: 18, style: .continuous)
                .strokeBorder(Color(nsColor: .separatorColor).opacity(0.65), lineWidth: 1)
        )
    }
}

private struct EmptyStateRow: View {
    let symbol: String
    let title: String
    let message: String

    var body: some View {
        HStack(alignment: .top, spacing: 12) {
            Image(systemName: symbol)
                .font(.body.weight(.semibold))
                .foregroundStyle(.secondary)
                .frame(width: 22)

            VStack(alignment: .leading, spacing: 4) {
                Text(title)
                    .font(.subheadline.weight(.semibold))
                Text(message)
                    .font(.caption)
                    .foregroundStyle(.secondary)
                    .fixedSize(horizontal: false, vertical: true)
            }

            Spacer(minLength: 0)
        }
        .padding(12)
        .background(
            RoundedRectangle(cornerRadius: 14, style: .continuous)
                .fill(Color(nsColor: .textBackgroundColor).opacity(0.5))
        )
        .overlay(
            RoundedRectangle(cornerRadius: 14, style: .continuous)
                .strokeBorder(Color(nsColor: .separatorColor).opacity(0.35), lineWidth: 1)
        )
    }
}

private struct DiscoveryRow: View {
    let endpoint: ScryBarEndpoint
    let isSelected: Bool
    let action: () -> Void

    var body: some View {
        Button(action: action) {
            HStack(alignment: .top, spacing: 12) {
                Image(systemName: isSelected ? "checkmark.circle.fill" : "circle")
                    .font(.body.weight(.semibold))
                    .foregroundStyle(isSelected ? Color.accentColor : .secondary)
                    .frame(width: 22)

                VStack(alignment: .leading, spacing: 3) {
                    Text(endpoint.name)
                        .font(.subheadline.weight(.semibold))
                        .foregroundStyle(.primary)
                    Text("\(endpoint.host):\(endpoint.port)")
                        .font(.caption)
                        .foregroundStyle(.secondary)
                        .lineLimit(1)
                        .truncationMode(.middle)
                }

                Spacer(minLength: 8)

                SourceChip(text: endpoint.source.rawValue)
            }
            .padding(12)
            .frame(maxWidth: .infinity, alignment: .leading)
        }
        .buttonStyle(.plain)
        .background(
            RoundedRectangle(cornerRadius: 14, style: .continuous)
                .fill(isSelected ? Color.accentColor.opacity(0.18) : Color(nsColor: .textBackgroundColor).opacity(0.45))
        )
        .overlay(
            RoundedRectangle(cornerRadius: 14, style: .continuous)
                .strokeBorder(isSelected ? Color.accentColor.opacity(0.55) : Color(nsColor: .separatorColor).opacity(0.28), lineWidth: 1)
        )
        .contentShape(RoundedRectangle(cornerRadius: 14, style: .continuous))
    }
}

private struct LabeledField: View {
    let title: String
    let placeholder: String
    let text: Binding<String>
    let isFocused: FocusState<Bool>.Binding
    var monospaced: Bool = false

    var body: some View {
        VStack(alignment: .leading, spacing: 6) {
            Text(title)
                .font(.caption.weight(.semibold))
                .foregroundStyle(.secondary)

            TextField(placeholder, text: text)
                .textFieldStyle(.plain)
                .focused(isFocused)
                .font(monospaced ? .system(.body, design: .monospaced) : .body)
                .padding(.horizontal, 12)
                .padding(.vertical, 10)
                .background(
                    RoundedRectangle(cornerRadius: 12, style: .continuous)
                        .fill(Color(nsColor: .textBackgroundColor).opacity(0.72))
                )
                .overlay(
                    RoundedRectangle(cornerRadius: 12, style: .continuous)
                        .strokeBorder(isFocused.wrappedValue ? Color.accentColor.opacity(0.85) : Color(nsColor: .separatorColor).opacity(0.35), lineWidth: 1)
                )
        }
    }
}

private struct SummaryRow: View {
    let label: String
    let value: String

    var body: some View {
        HStack(alignment: .firstTextBaseline, spacing: 12) {
            Text(label)
                .font(.caption.weight(.semibold))
                .foregroundStyle(.secondary)
                .frame(width: 84, alignment: .leading)

            Text(value)
                .font(.caption)
                .foregroundStyle(.primary)
                .lineLimit(2)
                .multilineTextAlignment(.leading)

            Spacer(minLength: 0)
        }
    }
}

private struct SourceChip: View {
    let text: String

    var body: some View {
        Text(text)
            .font(.caption2.weight(.semibold))
            .foregroundStyle(.secondary)
            .padding(.horizontal, 8)
            .padding(.vertical, 4)
            .background(
                Capsule(style: .continuous)
                    .fill(Color(nsColor: .textBackgroundColor).opacity(0.8))
            )
            .overlay(
                Capsule(style: .continuous)
                    .strokeBorder(Color(nsColor: .separatorColor).opacity(0.35), lineWidth: 1)
            )
    }
}

private struct StatusPill: View {
    let text: String
    let tone: StatusTone

    var body: some View {
        HStack(spacing: 6) {
            Circle()
                .fill(tone.fill)
                .frame(width: 7, height: 7)
            Text(text)
                .font(.caption.weight(.semibold))
        }
        .foregroundStyle(tone.foreground)
        .padding(.horizontal, 10)
        .padding(.vertical, 7)
        .background(
            Capsule(style: .continuous)
                .fill(tone.background)
        )
        .overlay(
            Capsule(style: .continuous)
                .strokeBorder(tone.border, lineWidth: 1)
        )
    }
}

private struct StatusTone {
    let label: String
    let foreground: Color
    let background: Color
    let border: Color
    let fill: Color

    static func success(label: String) -> Self {
        .init(
            label: label,
            foreground: Color(nsColor: .labelColor),
            background: Color.green.opacity(0.16),
            border: Color.green.opacity(0.4),
            fill: Color.green
        )
    }

    static func warning(label: String) -> Self {
        .init(
            label: label,
            foreground: Color(nsColor: .labelColor),
            background: Color.orange.opacity(0.16),
            border: Color.orange.opacity(0.4),
            fill: Color.orange
        )
    }

    static func critical(label: String) -> Self {
        .init(
            label: label,
            foreground: Color(nsColor: .labelColor),
            background: Color.red.opacity(0.16),
            border: Color.red.opacity(0.4),
            fill: Color.red
        )
    }

    static func neutral(label: String) -> Self {
        .init(
            label: label,
            foreground: Color(nsColor: .labelColor),
            background: Color(nsColor: .tertiaryLabelColor).opacity(0.16),
            border: Color(nsColor: .separatorColor).opacity(0.45),
            fill: Color(nsColor: .secondaryLabelColor)
        )
    }
}
