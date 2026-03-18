import SwiftUI

struct ContentView: View {
    @EnvironmentObject private var model: AppModel

    var body: some View {
        VStack(alignment: .leading, spacing: 18) {
            Text("ScryBar Companion")
                .font(.system(size: 28, weight: .semibold, design: .rounded))

            HStack(alignment: .top, spacing: 18) {
                connectionPanel
                    .frame(maxWidth: 360)
                nowPlayingPanel
            }

            GroupBox("Payload Preview") {
                ScrollView {
                    Text(model.currentPayload.prettyJSON)
                        .font(.system(size: 12, weight: .regular, design: .monospaced))
                        .frame(maxWidth: .infinity, alignment: .leading)
                        .textSelection(.enabled)
                        .padding(.vertical, 4)
                }
                .frame(minHeight: 240)
            }
        }
        .padding(20)
    }

    private var connectionPanel: some View {
        GroupBox("Connection") {
            VStack(alignment: .leading, spacing: 12) {
                LabeledContent("Discovery") {
                    Text(model.discoveryStatus)
                        .multilineTextAlignment(.trailing)
                        .foregroundStyle(.secondary)
                }

                Button("Rescan") {
                    model.rescan()
                }

                Divider()

                Text("Discovered ScryBars")
                    .font(.headline)

                if model.discoveredEndpoints.isEmpty {
                    Text("Nothing discovered yet on `_scrybar._tcp`.")
                        .foregroundStyle(.secondary)
                } else {
                    VStack(alignment: .leading, spacing: 8) {
                        ForEach(model.discoveredEndpoints) { endpoint in
                            Button {
                                model.selectedDiscoveredEndpointID = endpoint.id
                            } label: {
                                HStack {
                                    Image(systemName: model.selectedDiscoveredEndpointID == endpoint.id ? "checkmark.circle.fill" : "circle")
                                    VStack(alignment: .leading, spacing: 2) {
                                        Text(endpoint.name)
                                        Text("\(endpoint.host):\(endpoint.port)")
                                            .font(.caption)
                                            .foregroundStyle(.secondary)
                                    }
                                }
                            }
                            .buttonStyle(.plain)
                        }
                    }
                }

                Divider()

                Text("Manual Target")
                    .font(.headline)

                TextField("Hostname or IP", text: $model.manualHost)
                    .textFieldStyle(.roundedBorder)

                TextField("Port", text: $model.manualPort)
                    .textFieldStyle(.roundedBorder)

                Button("Save Manual Target") {
                    model.saveManualTarget()
                }

                if let endpoint = model.selectedEndpoint {
                    Text("Active target: \(endpoint.displayName)")
                        .foregroundStyle(.secondary)
                } else {
                    Text("Select a discovered device or enter a manual target.")
                        .foregroundStyle(.secondary)
                }
            }
            .frame(maxWidth: .infinity, alignment: .leading)
        }
    }

    private var nowPlayingPanel: some View {
        GroupBox("Now Playing") {
            VStack(alignment: .leading, spacing: 12) {
                Picker("Provider", selection: $model.providerKind) {
                    ForEach(ProviderKind.allCases) { provider in
                        Text(provider.rawValue).tag(provider)
                    }
                }
                .pickerStyle(.segmented)

                Toggle("Auto-send every second", isOn: $model.autoSendEnabled)

                HStack(spacing: 10) {
                    Button("Refresh") {
                        model.refreshPayload()
                    }
                    Button("Send Now") {
                        model.sendNow()
                    }
                    if model.providerKind == .mock {
                        Button("Next Mock Track") {
                            model.nextMockTrack()
                        }
                    }
                }

                Divider()

                LabeledContent("Title") {
                    Text(model.currentPayload.title)
                        .multilineTextAlignment(.trailing)
                }
                LabeledContent("Artist") {
                    Text(model.currentPayload.artist)
                        .multilineTextAlignment(.trailing)
                }
                LabeledContent("Album") {
                    Text(model.currentPayload.album)
                        .multilineTextAlignment(.trailing)
                }
                LabeledContent("Source") {
                    Text(model.currentPayload.source)
                }
                LabeledContent("App") {
                    Text(model.currentPayload.appName.isEmpty ? "—" : model.currentPayload.appName)
                }
                LabeledContent("State") {
                    Text(model.currentPayload.isPlaying ? "Playing" : "Paused")
                }
                LabeledContent("Time") {
                    Text("\(Int(model.currentPayload.elapsedSec))s / \(Int(model.currentPayload.durationSec))s")
                }
                LabeledContent("Remaining") {
                    Text("\(model.currentPayload.remainingPercent)%")
                }

                Divider()

                Text(model.lastSendStatus)
                    .foregroundStyle(.secondary)
                    .frame(maxWidth: .infinity, alignment: .leading)
            }
            .frame(maxWidth: .infinity, alignment: .leading)
        }
    }
}
