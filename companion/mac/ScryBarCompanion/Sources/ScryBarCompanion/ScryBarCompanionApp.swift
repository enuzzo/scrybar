import SwiftUI

@main
struct ScryBarCompanionApp: App {
    @StateObject private var model = AppModel()

    var body: some Scene {
        WindowGroup("ScryBar Companion") {
            ContentView()
                .environmentObject(model)
                .preferredColorScheme(.dark)
                .frame(minWidth: 1120, minHeight: 780)
        }
        .defaultSize(width: 1240, height: 860)
    }
}
