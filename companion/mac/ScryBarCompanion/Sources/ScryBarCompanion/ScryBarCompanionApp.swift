import SwiftUI

@main
struct ScryBarCompanionApp: App {
    @StateObject private var model = AppModel()

    var body: some Scene {
        WindowGroup("ScryBar Companion") {
            ContentView()
                .environmentObject(model)
                .frame(minWidth: 940, minHeight: 700)
        }
        .defaultSize(width: 980, height: 760)
    }
}
