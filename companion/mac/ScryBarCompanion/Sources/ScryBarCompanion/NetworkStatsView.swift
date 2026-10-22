import SwiftUI

struct NetworkStatsSection: View {
    let snapshot: NetworkStatsSnapshot?
    var isStale = false

    private var apps: [NetworkAppActivity] {
        guard !isStale, snapshot?.attributionAvailable == true else { return [] }
        return Array((snapshot?.apps ?? []).prefix(5))
    }

    var body: some View {
        VStack(alignment: .leading, spacing: 9) {
            HStack {
                Text("NETWORK")
                    .tracking(0.8)
                    .foregroundStyle(CompanionTheme.textTertiary)
                Spacer()
                Text(isStale ? "Stale" : snapshot?.downloadBytesPerSec == nil ? "Measuring…" : "Live · 1 s")
                    .foregroundStyle(isStale ? CompanionTheme.warning : CompanionTheme.textTertiary)
            }
            .font(.system(size: 11, weight: .semibold))

            HStack(spacing: 16) {
                rate("DOWNLOAD", symbol: "arrow.down", value: snapshot?.downloadBytesPerSec, tint: CompanionTheme.accent)
                rate("UPLOAD", symbol: "arrow.up", value: snapshot?.uploadBytesPerSec, tint: CompanionTheme.success)
            }

            HStack {
                Text("APPS USING THE NETWORK")
                    .font(.system(size: 9, weight: .semibold))
                    .tracking(0.4)
                Spacer(minLength: 0)
                Text("↓ DOWN").frame(width: 66, alignment: .trailing)
                Text("↑ UP").frame(width: 66, alignment: .trailing)
            }
            .font(.system(size: 9, weight: .medium))
            .foregroundStyle(CompanionTheme.textTertiary)
            .padding(.top, 2)

            if apps.isEmpty {
                Text(emptyMessage)
                    .font(.system(size: 11))
                    .foregroundStyle(CompanionTheme.textSecondary)
                    .fixedSize(horizontal: false, vertical: true)
            } else {
                VStack(spacing: 7) {
                    ForEach(apps) { app in
                        HStack(spacing: 8) {
                            Text(app.name)
                                .font(.system(size: 11.5, weight: .medium))
                                .lineLimit(1)
                                .truncationMode(.middle)
                                .frame(maxWidth: .infinity, alignment: .leading)
                                .help(app.name)
                            Text(NetworkRateFormatter.string(app.downloadBytesPerSec))
                                .frame(width: 66, alignment: .trailing)
                            Text(NetworkRateFormatter.string(app.uploadBytesPerSec))
                                .frame(width: 66, alignment: .trailing)
                        }
                        .font(.system(size: 10, weight: .medium, design: .monospaced))
                        .foregroundStyle(CompanionTheme.textSecondary)
                        .accessibilityElement(children: .ignore)
                        .accessibilityLabel(app.name)
                        .accessibilityValue("Download \(NetworkRateFormatter.string(app.downloadBytesPerSec)), upload \(NetworkRateFormatter.string(app.uploadBytesPerSec))")
                    }
                }
            }

            Text("Current traffic · app accounting can differ from totals")
                .font(.system(size: 9))
                .foregroundStyle(CompanionTheme.textDisabled)
        }
    }

    private var emptyMessage: String {
        if isStale { return "Waiting for a fresh network sample…" }
        guard let snapshot else { return "Measuring network activity…" }
        if !snapshot.attributionAvailable { return snapshot.attributionMessage ?? "App activity is unavailable. Total rates remain independent." }
        return "No app traffic in the last sample."
    }

    private func rate(_ label: String, symbol: String, value: Double?, tint: Color) -> some View {
        VStack(alignment: .leading, spacing: 3) {
            Label(label, systemImage: symbol)
                .font(.system(size: 9.5, weight: .semibold))
                .foregroundStyle(tint)
            Text(NetworkRateFormatter.string(isStale ? nil : value))
                .font(.system(size: 21, weight: .semibold, design: .rounded))
                .monospacedDigit()
                .foregroundStyle(CompanionTheme.textPrimary)
                .lineLimit(1)
                .minimumScaleFactor(0.75)
        }
        .frame(maxWidth: .infinity, alignment: .leading)
    }
}
