import AppKit
import SwiftUI

struct SettingsView: View {
    @Binding var showSettings: Bool
    @EnvironmentObject var model: AppModel

    var body: some View {
        VStack(alignment: .leading, spacing: 0) {
            // Header
            HStack {
                Button {
                    showSettings = false
                } label: {
                    HStack(spacing: 4) {
                        Image(systemName: "chevron.left")
                            .font(.system(size: 11, weight: .semibold))
                        Text("Back")
                            .font(.system(size: 13, weight: .medium))
                    }
                }
                .buttonStyle(.plain)
                .foregroundStyle(CompanionTheme.accent)

                Spacer()

                Text("Settings")
                    .font(.system(size: 15, weight: .semibold))
                    .foregroundStyle(CompanionTheme.textPrimary)
            }
            .padding(.bottom, 12)

            Divider().overlay(CompanionTheme.divider)

            VStack(alignment: .leading, spacing: 14) {
                // Provider
                settingRow("Provider") {
                    Picker("", selection: $model.providerKind) {
                        ForEach(ProviderKind.allCases) { kind in
                            Text(kind.rawValue).tag(kind)
                        }
                    }
                    .labelsHidden()
                    .pickerStyle(.menu)
                }

                // Target
                settingRow("Target") {
                    if model.discoveredEndpoints.isEmpty {
                        Text("No device found")
                            .font(.system(size: 12))
                            .foregroundStyle(CompanionTheme.textTertiary)
                    } else {
                        Picker("", selection: $model.selectedDiscoveredEndpointID) {
                            ForEach(model.discoveredEndpoints) { endpoint in
                                Text(endpoint.name)
                                    .tag(Optional(endpoint.id))
                            }
                        }
                        .labelsHidden()
                        .pickerStyle(.menu)
                    }
                }

                if let endpoint = model.selectedEndpoint {
                    Text("\(endpoint.host):\(endpoint.port)")
                        .font(.system(size: 11, design: .monospaced))
                        .foregroundStyle(CompanionTheme.textTertiary)
                        .padding(.top, -6)
                }

                Divider().overlay(CompanionTheme.divider)

                // Manual target
                DisclosureGroup("Manual Target") {
                    VStack(alignment: .leading, spacing: 8) {
                        settingRow("Host") {
                            TextField("Hostname or IP", text: $model.manualHost)
                                .textFieldStyle(.roundedBorder)
                                .font(.system(size: 12))
                        }
                        settingRow("Port") {
                            TextField("8080", text: $model.manualPort)
                                .textFieldStyle(.roundedBorder)
                                .font(.system(size: 12))
                        }
                        Button("Save") {
                            model.saveManualTarget()
                        }
                        .buttonStyle(CompanionSecondaryButtonStyle())
                        .controlSize(.small)
                    }
                    .padding(.top, 4)
                }
                .font(.system(size: 12, weight: .medium))
                .foregroundStyle(CompanionTheme.textSecondary)

                Divider().overlay(CompanionTheme.divider)

                // Auto-send
                settingRow("Auto-send") {
                    Toggle("", isOn: Binding(
                        get: { model.autoSendEnabled },
                        set: { model.setAutoSend($0) }
                    ))
                    .labelsHidden()
                    .toggleStyle(.switch)
                    .controlSize(.small)
                }

                // Footer: version badge + quit
                HStack {
                    Text(versionString)
                        .font(.system(size: 11, weight: .medium, design: .monospaced))
                        .foregroundStyle(CompanionTheme.textDisabled)

                    Spacer()

                    Button("Quit ScryBar Companion") {
                        NSApplication.shared.terminate(nil)
                    }
                    .buttonStyle(CompanionSecondaryButtonStyle())
                    .controlSize(.small)
                }
            }
            .padding(.top, 10)
        }
        .padding(14)
        .frame(width: 300)
        .background(CompanionTheme.windowBackground)
        .preferredColorScheme(.dark)
    }

    private func settingRow<Content: View>(_ label: String, @ViewBuilder content: () -> Content) -> some View {
        HStack(alignment: .center) {
            Text(label)
                .font(.system(size: 13, weight: .medium))
                .foregroundStyle(CompanionTheme.textSecondary)
                .frame(width: 80, alignment: .leading)
            content()
        }
    }

    private var versionString: String {
        let version = Bundle.main.object(forInfoDictionaryKey: "CFBundleShortVersionString") as? String ?? "0.0.0"
        let build = Bundle.main.object(forInfoDictionaryKey: "CFBundleVersion") as? String ?? "0"
        return "v\(version) (\(build))"
    }
}
