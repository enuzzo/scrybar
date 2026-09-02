import SwiftUI

struct BambuSetupHelpView: View {
    @Environment(\.dismiss) private var dismiss
    @EnvironmentObject var model: AppModel

    var body: some View {
        VStack(alignment: .leading, spacing: 0) {
            HStack {
                Label("Connect a Bambu printer", systemImage: "printer.fill")
                    .font(.system(size: 16, weight: .semibold))
                Spacer()
                Button("Close") { dismiss() }
                    .keyboardShortcut(.cancelAction)
            }
            .padding(16)

            Divider()

            ScrollView {
                VStack(alignment: .leading, spacing: 16) {
                    helpSection("Before you start", steps: [
                        "Connect the Mac and the printer to the same local network.",
                        "Switch on the printer.",
                        "Switch off a VPN if it blocks local network traffic.",
                    ])

                    helpSection("Find the printer", steps: [
                        "Select Search Again in ScryBar Companion.",
                        "If the app finds more than one printer, select the printer that you want to monitor.",
                        "The app enters the IP address and the serial number. Check the values before you connect.",
                    ])

                    helpSection("Find the LAN access code", steps: [
                        "On the printer display, open Settings.",
                        "Open the Network or WLAN page.",
                        "Open LAN Only Mode and enable it after reading the cloud-services warning.",
                        "If your firmware shows a separate Developer Mode switch, enable it. Some A1 firmware exposes local services with LAN Only alone.",
                        "Record the LAN access code that the printer shows.",
                        "Enter the code in ScryBar Companion. Select Save & Connect.",
                    ])

                    helpNote(
                        title: "Security",
                        text: "ScryBar Companion stores one access code for each printer in macOS Keychain. The app does not send this code to ScryBar. Printer discovery cannot read the code."
                    )

                    helpNote(
                        title: "If the connection fails",
                        text: "Make sure that the IP address did not change. Check the access code again. Enable LAN Only Mode; if your firmware offers Developer Mode, enable it too. Read the warning first because LAN Only Mode can stop cloud functions."
                    )
                }
                .padding(16)
            }

            Divider()

            HStack {
                Text(model.bambuDiscoveryStatus)
                    .font(.system(size: 11.5))
                    .foregroundStyle(CompanionTheme.textSecondary)
                    .lineLimit(2)
                Spacer()
                Button("Search Again") {
                    model.scanForBambuPrinters()
                    dismiss()
                }
                .keyboardShortcut(.defaultAction)
            }
            .padding(16)
        }
        .frame(width: 480, height: 560)
        .preferredColorScheme(.dark)
    }

    private func helpSection(_ title: String, steps: [String]) -> some View {
        VStack(alignment: .leading, spacing: 8) {
            Text(title)
                .font(.system(size: 13.5, weight: .semibold))
            ForEach(Array(steps.enumerated()), id: \.offset) { index, step in
                HStack(alignment: .top, spacing: 8) {
                    Text("\(index + 1)")
                        .font(.system(size: 10, weight: .bold, design: .rounded))
                        .foregroundStyle(.white)
                        .frame(width: 20, height: 20)
                        .background(CompanionTheme.bambuAccent, in: Circle())
                    Text(step)
                        .font(.system(size: 12.5))
                        .fixedSize(horizontal: false, vertical: true)
                }
            }
        }
    }

    private func helpNote(title: String, text: String) -> some View {
        VStack(alignment: .leading, spacing: 5) {
            Text(title)
                .font(.system(size: 12.5, weight: .semibold))
            Text(text)
                .font(.system(size: 12))
                .foregroundStyle(CompanionTheme.textSecondary)
                .fixedSize(horizontal: false, vertical: true)
        }
        .padding(10)
        .background(.quaternary.opacity(0.4), in: RoundedRectangle(cornerRadius: 8))
    }
}
