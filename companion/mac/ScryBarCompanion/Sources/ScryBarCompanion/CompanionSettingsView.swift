import AppKit
import SwiftUI

struct SettingsView: View {
    @Binding var showSettings: Bool
    let viewportHeight: CGFloat
    let onContentHeightChange: (CGFloat) -> Void
    @EnvironmentObject var model: AppModel
    @State private var showBambuHelp = false
    @FocusState private var headerHasFocus: Bool
    @State private var measuredBodyHeight: CGFloat = 0

    private let fixedChromeHeight: CGFloat = 78

    var body: some View {
        VStack(alignment: .leading, spacing: 0) {
            header
            Divider().overlay(CompanionTheme.divider)

            ScrollViewReader { proxy in
                ScrollView {
                    VStack(alignment: .leading, spacing: 14) {
                        Color.clear.frame(height: 0).id("settings-top")
                        scryBarSettingsSection
                        bambuSettingsSection
                    }
                    .padding(.vertical, 14)
                    .background {
                        GeometryReader { geometry in
                            Color.clear.preference(
                                key: PopoverBodyHeightPreferenceKey.self,
                                value: geometry.size.height
                            )
                        }
                    }
                }
                .scrollIndicators(
                    measuredBodyHeight + fixedChromeHeight > viewportHeight + 1
                        ? .visible
                        : .hidden
                )
                .onAppear {
                    DispatchQueue.main.async {
                        headerHasFocus = true
                        proxy.scrollTo("settings-top", anchor: .top)
                    }
                }
                .onPreferenceChange(PopoverBodyHeightPreferenceKey.self) { height in
                    guard height > 0 else { return }
                    measuredBodyHeight = ceil(height)
                    onContentHeightChange(measuredBodyHeight + fixedChromeHeight)
                }
            }

            Divider().overlay(CompanionTheme.divider)
            footer
        }
        .padding(.horizontal, 16)
        .frame(width: 360)
        .frame(maxHeight: .infinity, alignment: .top)
        .background(CompanionTheme.windowBackground)
        .preferredColorScheme(.dark)
        .sheet(isPresented: $showBambuHelp) {
            BambuSetupHelpView()
                .environmentObject(model)
        }
    }

    private var header: some View {
        HStack {
            Button {
                showSettings = false
            } label: {
                HStack(spacing: 5) {
                    Image(systemName: "chevron.left")
                        .font(.system(size: 12, weight: .semibold))
                    Text("Back")
                        .font(.system(size: 13.5, weight: .semibold))
                }
            }
            .buttonStyle(.plain)
            .foregroundStyle(CompanionTheme.accent)
            .keyboardShortcut(.cancelAction)
            .focused($headerHasFocus)

            Spacer()

            Text("Settings")
                .font(.system(size: 17, weight: .semibold))
                .foregroundStyle(CompanionTheme.textPrimary)
        }
        .frame(height: 34)
    }

    private var footer: some View {
        HStack {
            Text(versionString)
                .font(.system(size: 11.5, weight: .medium, design: .monospaced))
                .foregroundStyle(CompanionTheme.textTertiary)

            Spacer()

            Button("Quit Companion") {
                NSApplication.shared.terminate(nil)
            }
            .buttonStyle(CompanionSecondaryButtonStyle())
        }
        .frame(height: 42)
    }

    private var scryBarSettingsSection: some View {
        settingsSection(title: "SCRYBAR DISPLAY", systemImage: "display") {
            VStack(alignment: .leading, spacing: 12) {
                settingRow("Content source") {
                    Picker("Content source", selection: $model.providerKind) {
                        ForEach(ProviderKind.allCases) { kind in
                            Text(kind.rawValue).tag(kind)
                        }
                    }
                    .labelsHidden()
                    .pickerStyle(.menu)
                    .accessibilityLabel("Content source")
                }

                settingRow("ScryBar") {
                    if model.discoveredEndpoints.isEmpty {
                        Text("No display found")
                            .font(CompanionTheme.Typography.caption)
                            .foregroundStyle(CompanionTheme.textTertiary)
                    } else {
                        Picker("ScryBar target", selection: $model.selectedDiscoveredEndpointID) {
                            ForEach(model.discoveredEndpoints) { endpoint in
                                Text(endpoint.name).tag(Optional(endpoint.id))
                            }
                        }
                        .labelsHidden()
                        .pickerStyle(.menu)
                        .accessibilityLabel("ScryBar target")
                    }
                }

                if let endpoint = model.selectedEndpoint {
                    HStack(spacing: 7) {
                        Circle()
                            .fill(model.selectedEndpointIsConnected ? CompanionTheme.success : CompanionTheme.warning)
                            .frame(width: 7, height: 7)
                        Text(model.selectedEndpointIsConnected ? "Display connected" : "Waiting for display")
                            .font(.system(size: 11.5, weight: .semibold))
                            .foregroundStyle(model.selectedEndpointIsConnected ? CompanionTheme.success : CompanionTheme.warning)
                        Spacer()
                        Text("\(endpoint.host):\(endpoint.port)")
                            .font(.system(size: 10.5, weight: .medium, design: .monospaced))
                            .foregroundStyle(CompanionTheme.textTertiary)
                            .lineLimit(1)
                    }
                }

                Divider().overlay(CompanionTheme.divider)

                Toggle(isOn: Binding(
                    get: { model.autoSendEnabled },
                    set: { model.setAutoSend($0) }
                )) {
                    VStack(alignment: .leading, spacing: 2) {
                        Text("Auto-send")
                            .font(CompanionTheme.Typography.label)
                            .foregroundStyle(CompanionTheme.textPrimary)
                        Text("Send live Companion data to the selected display.")
                            .font(.system(size: 11.5))
                            .foregroundStyle(CompanionTheme.textTertiary)
                    }
                }
                .toggleStyle(.switch)

                DisclosureGroup("Manual display connection") {
                    VStack(alignment: .leading, spacing: 9) {
                        labeledField("Host", placeholder: "Hostname or IP", text: $model.manualHost)
                        labeledField("Port", placeholder: "8080", text: $model.manualPort)
                        Button("Save manual display") { model.saveManualTarget() }
                            .buttonStyle(CompanionSecondaryButtonStyle())
                    }
                    .padding(.top, 8)
                }
                .font(CompanionTheme.Typography.caption)
                .foregroundStyle(CompanionTheme.textSecondary)
            }
        }
    }

    private var bambuSettingsSection: some View {
        VStack(alignment: .leading, spacing: 8) {
            Label("BAMBU LAB PRINTER", systemImage: "printer.fill")
                .font(CompanionTheme.Typography.sectionTitle)
                .foregroundStyle(CompanionTheme.bambuAccent)
                .tracking(0.7)

            VStack(alignment: .leading, spacing: 12) {
                bambuConnectionBanner
                discoveryRow

                if !model.discoveredBambuPrinters.isEmpty {
                    printerPicker
                }

                DisclosureGroup("Advanced printer data") {
                    VStack(alignment: .leading, spacing: 9) {
                        labeledField("Printer name", placeholder: "Workshop A1", text: $model.bambuPrinterName)
                        labeledField("Printer IP", placeholder: "192.168.1.123", text: $model.bambuHost)
                        labeledField("Serial", placeholder: "Printer serial", text: $model.bambuSerial)
                    }
                    .padding(.top, 8)
                }
                .font(CompanionTheme.Typography.caption)
                .foregroundStyle(CompanionTheme.textSecondary)

                VStack(alignment: .leading, spacing: 5) {
                    Text("LAN access code")
                        .font(.system(size: 11.5, weight: .medium))
                        .foregroundStyle(CompanionTheme.textSecondary)
                    PasteEnabledSecureField(
                        placeholder: "Enter the code from the printer",
                        text: $model.bambuAccessCode,
                        onSubmit: connectIfReady
                    )
                        .frame(height: 32)
                        .accessibilityLabel("LAN access code")
                }

                Toggle(isOn: Binding(
                    get: { model.bambuSoundsEnabled },
                    set: { model.setBambuSoundsEnabled($0) }
                )) {
                    VStack(alignment: .leading, spacing: 2) {
                        Text("Print event sounds")
                            .font(CompanionTheme.Typography.label)
                            .foregroundStyle(CompanionTheme.textPrimary)
                        Text("Play a sound for start, pause, errors, and completion.")
                            .font(.system(size: 11.5))
                            .foregroundStyle(CompanionTheme.textTertiary)
                    }
                }
                .toggleStyle(.switch)
                .tint(CompanionTheme.bambuAccent)

                Button {
                    model.saveBambuSettings()
                } label: {
                    HStack(spacing: 8) {
                        if model.bambuConnectionPhase == .connecting {
                            ProgressView()
                                .controlSize(.small)
                                .tint(.black)
                        }
                        Text(connectButtonTitle)
                    }
                    .frame(maxWidth: .infinity)
                }
                .buttonStyle(CompanionPrimaryButtonStyle(
                    tint: CompanionTheme.bambuAccent,
                    foreground: .black
                ))
                .keyboardShortcut(.defaultAction)
                .disabled(!bambuSettingsAreReady || model.bambuConnectionPhase == .connecting)
                .accessibilityHint("Stores the code in Keychain and connects to the printer")

                connectionActionStatus

                HStack {
                    Button {
                        showBambuHelp = true
                    } label: {
                        Label("Setup Help", systemImage: "questionmark.circle")
                    }
                    .buttonStyle(.plain)
                    .font(.system(size: 12, weight: .semibold))
                    .foregroundStyle(CompanionTheme.bambuAccent)
                    Spacer()
                    Text("Code stored in Keychain")
                        .font(.system(size: 10.5))
                        .foregroundStyle(CompanionTheme.textTertiary)
                }
            }
            .padding(14)
            .frame(maxWidth: .infinity, alignment: .leading)
            .background(CompanionTheme.bambuSurface, in: RoundedRectangle(cornerRadius: CompanionPalette.Layout.cardCornerRadius, style: .continuous))
            .overlay {
                RoundedRectangle(cornerRadius: CompanionPalette.Layout.cardCornerRadius, style: .continuous)
                    .strokeBorder(CompanionTheme.bambuBorder, lineWidth: 1)
            }
        }
    }

    private var discoveryRow: some View {
        HStack(spacing: 8) {
            if model.isBambuScanning {
                ProgressView()
                    .controlSize(.small)
                    .tint(CompanionTheme.bambuAccent)
            }
            VStack(alignment: .leading, spacing: 2) {
                Text(model.isBambuScanning ? "Searching local network…" : discoverySummary)
                    .font(.system(size: 12.5, weight: .semibold))
                    .foregroundStyle(CompanionTheme.textPrimary)
                Text(discoveryDetail)
                    .font(.system(size: 11.5))
                    .foregroundStyle(CompanionTheme.textTertiary)
                    .lineLimit(2)
            }
            Spacer(minLength: 8)
            Button {
                model.scanForBambuPrinters()
            } label: {
                Image(systemName: "arrow.clockwise")
                    .font(.system(size: 13, weight: .semibold))
                    .frame(width: 28, height: 28)
            }
            .buttonStyle(.plain)
            .foregroundStyle(CompanionTheme.bambuAccent)
            .background(CompanionTheme.bambuSurfaceElevated, in: RoundedRectangle(cornerRadius: CompanionPalette.Layout.controlCornerRadius, style: .continuous))
            .overlay {
                RoundedRectangle(cornerRadius: CompanionPalette.Layout.controlCornerRadius, style: .continuous)
                    .strokeBorder(CompanionTheme.bambuBorder, lineWidth: 1)
            }
            .help("Search again")
            .accessibilityLabel("Search for Bambu printers again")
        }
    }

    private var printerPicker: some View {
        VStack(alignment: .leading, spacing: 5) {
            Text("Printer")
                .font(.system(size: 11.5, weight: .medium))
                .foregroundStyle(CompanionTheme.textSecondary)
            Picker("Printer", selection: Binding(
                get: { model.selectedBambuPrinterID },
                set: { model.selectBambuPrinter(id: $0) }
            )) {
                if model.discoveredBambuPrinters.count > 1 {
                    Text("Select a printer").tag(Optional<String>.none)
                }
                ForEach(model.discoveredBambuPrinters) { printer in
                    Text(printer.displayName).tag(Optional(printer.id))
                }
            }
            .labelsHidden()
            .pickerStyle(.menu)
            .controlSize(.large)
            .accessibilityLabel("Bambu printer")

            if let printer = selectedPrinter {
                Text("\(printer.model) · \(printer.host)")
                    .font(.system(size: 11, weight: .medium, design: .monospaced))
                    .foregroundStyle(CompanionTheme.textTertiary)
                    .lineLimit(1)
            }
        }
    }

    private var bambuConnectionBanner: some View {
        HStack(alignment: .top, spacing: 9) {
            Circle()
                .fill(bambuStatusColor)
                .frame(width: 8, height: 8)
                .padding(.top, 4)
            VStack(alignment: .leading, spacing: 2) {
                Text(bambuStatusTitle)
                    .font(.system(size: 12.5, weight: .semibold))
                    .foregroundStyle(bambuStatusColor)
                if model.bambuConnectionStatus != bambuStatusTitle {
                    Text(model.bambuConnectionStatus)
                        .font(.system(size: 11.5))
                        .foregroundStyle(CompanionTheme.textSecondary)
                        .fixedSize(horizontal: false, vertical: true)
                }
            }
            Spacer(minLength: 0)
        }
        .padding(10)
        .frame(maxWidth: .infinity, alignment: .leading)
        .background(bambuStatusColor.opacity(0.08), in: RoundedRectangle(cornerRadius: 9, style: .continuous))
        .overlay {
            RoundedRectangle(cornerRadius: 9, style: .continuous)
                .strokeBorder(bambuStatusColor.opacity(0.24), lineWidth: 1)
        }
    }

    private var connectionActionStatus: some View {
        HStack(spacing: 8) {
            if model.bambuConnectionPhase == .connecting {
                ProgressView()
                    .controlSize(.small)
                    .tint(bambuStatusColor)
            } else {
                Image(systemName: bambuStatusSymbol)
                    .font(.system(size: 11, weight: .semibold))
                    .foregroundStyle(bambuStatusColor)
            }

            Text(model.bambuConnectionStatus)
                .font(.system(size: 11.5, weight: .medium))
                .foregroundStyle(CompanionTheme.textSecondary)
                .lineLimit(2)

            Spacer(minLength: 0)
        }
        .padding(.horizontal, 10)
        .padding(.vertical, 8)
        .frame(maxWidth: .infinity, minHeight: 34, alignment: .leading)
        .background(CompanionTheme.bambuSurfaceElevated, in: RoundedRectangle(cornerRadius: 9, style: .continuous))
        .overlay {
            RoundedRectangle(cornerRadius: 9, style: .continuous)
                .strokeBorder(bambuStatusColor.opacity(0.22), lineWidth: 1)
        }
        .accessibilityElement(children: .combine)
        .accessibilityLabel("Printer connection status")
        .accessibilityValue("\(model.bambuConnectionPhase.title). \(model.bambuConnectionStatus)")
    }

    private func settingsSection<Content: View>(
        title: String,
        systemImage: String,
        accent: Color = CompanionTheme.accent,
        @ViewBuilder content: @escaping () -> Content
    ) -> some View {
        VStack(alignment: .leading, spacing: 8) {
            Label(title, systemImage: systemImage)
                .font(CompanionTheme.Typography.sectionTitle)
                .foregroundStyle(accent)
                .tracking(0.7)
            CompanionCard(padding: 14) { content() }
        }
    }

    private func settingRow<Content: View>(_ label: String, @ViewBuilder content: () -> Content) -> some View {
        HStack(alignment: .center, spacing: 12) {
            Text(label)
                .font(CompanionTheme.Typography.label)
                .foregroundStyle(CompanionTheme.textSecondary)
            Spacer(minLength: 8)
            content()
        }
    }

    private func labeledField(_ label: String, placeholder: String, text: Binding<String>) -> some View {
        VStack(alignment: .leading, spacing: 4) {
            Text(label)
                .font(.system(size: 11.5, weight: .medium))
                .foregroundStyle(CompanionTheme.textSecondary)
            TextField(placeholder, text: text)
                .textFieldStyle(.roundedBorder)
                .font(.system(size: 13))
                .accessibilityLabel(label)
        }
    }

    private var selectedPrinter: BambuDiscoveredPrinter? {
        guard let id = model.selectedBambuPrinterID else { return nil }
        return model.discoveredBambuPrinters.first { $0.id == id }
    }

    private var discoverySummary: String {
        let count = model.discoveredBambuPrinters.count
        if count == 0 {
            if model.bambuConnectionPhase == .live {
                return "Printer connected directly"
            }
            return model.bambuHost.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty
                ? "No printer found"
                : "Saved printer not discoverable"
        }
        return "\(count) printer\(count == 1 ? "" : "s") found"
    }

    private var discoveryDetail: String {
        if model.bambuConnectionPhase == .live, model.discoveredBambuPrinters.isEmpty {
            return "Live telemetry active. Search again if the printer address changes."
        }
        return model.bambuDiscoveryStatus
    }

    private var bambuSettingsAreReady: Bool {
        !model.bambuHost.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty &&
        !model.bambuSerial.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty &&
        !model.bambuAccessCode.isEmpty
    }

    private func connectIfReady() {
        guard bambuSettingsAreReady, model.bambuConnectionPhase != .connecting else { return }
        model.saveBambuSettings()
    }

    private var connectButtonTitle: String {
        switch model.bambuConnectionPhase {
        case .connecting: return "Connecting…"
        case .live: return "Reconnect"
        case .failed: return "Retry Connection"
        case .notConfigured: return "Save & Connect"
        }
    }

    private var bambuStatusTitle: String {
        model.bambuConnectionPhase.title
    }

    private var bambuStatusSymbol: String {
        switch model.bambuConnectionPhase {
        case .notConfigured: return "exclamationmark.circle.fill"
        case .connecting: return "antenna.radiowaves.left.and.right"
        case .live: return "checkmark.circle.fill"
        case .failed: return "xmark.circle.fill"
        }
    }

    private var bambuStatusColor: Color {
        switch model.bambuConnectionPhase {
        case .notConfigured: return CompanionTheme.bambuTextMuted
        case .connecting: return CompanionTheme.bambuAccent
        case .live: return CompanionTheme.bambuAccent
        case .failed: return CompanionTheme.danger
        }
    }

    private var versionString: String {
        let version = Bundle.main.object(forInfoDictionaryKey: "CFBundleShortVersionString") as? String ?? "0.0.0"
        let build = Bundle.main.object(forInfoDictionaryKey: "CFBundleVersion") as? String ?? "0"
        return "v\(version) (\(build))"
    }
}

private struct PasteEnabledSecureField: NSViewRepresentable {
    let placeholder: String
    @Binding var text: String
    let onSubmit: () -> Void

    func makeCoordinator() -> Coordinator {
        Coordinator(text: $text, onSubmit: onSubmit)
    }

    func makeNSView(context: Context) -> CommandVPasteSecureTextField {
        let field = CommandVPasteSecureTextField()
        field.delegate = context.coordinator
        field.target = context.coordinator
        field.action = #selector(Coordinator.submit)
        field.placeholderString = placeholder
        field.font = .systemFont(ofSize: 13.5)
        field.isBezeled = true
        field.isBordered = true
        field.isEditable = true
        field.isSelectable = true
        field.bezelStyle = .roundedBezel
        field.focusRingType = .default
        field.stringValue = text
        return field
    }

    func updateNSView(_ field: CommandVPasteSecureTextField, context: Context) {
        field.placeholderString = placeholder
        if field.stringValue != text {
            field.stringValue = text
        }
    }

    final class Coordinator: NSObject, NSTextFieldDelegate {
        @Binding private var text: String
        private let onSubmit: () -> Void

        init(text: Binding<String>, onSubmit: @escaping () -> Void) {
            _text = text
            self.onSubmit = onSubmit
        }

        func controlTextDidChange(_ notification: Notification) {
            guard let field = notification.object as? NSTextField else { return }
            text = field.stringValue
        }

        @objc func submit() {
            onSubmit()
        }
    }
}

class CommandVPasteSecureTextField: NSSecureTextField {
    var sourcePasteboard: NSPasteboard { .general }

    override func performKeyEquivalent(with event: NSEvent) -> Bool {
        let modifiers = event.modifierFlags.intersection(.deviceIndependentFlagsMask)
        if modifiers == .command,
           event.charactersIgnoringModifiers?.lowercased() == "v",
           let editor = currentEditor() as? NSTextView,
           let pastedText = sourcePasteboard.string(forType: .string) {
            editor.insertText(pastedText, replacementRange: editor.selectedRange())
            return true
        }
        return super.performKeyEquivalent(with: event)
    }
}
