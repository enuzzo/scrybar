import SwiftUI

@main
struct ScryBarCompanionApp: App {
    @NSApplicationDelegateAdaptor(AppDelegate.self) var appDelegate

    var body: some Scene {
        // AppDelegate owns the real NSStatusItem and popover. SwiftUI still
        // needs a scene, but an empty Settings scene opens a blank window on
        // launch. Keep this placeholder menu item uninserted instead.
        MenuBarExtra("ScryBar Companion", isInserted: .constant(false)) {
            EmptyView()
        }
    }
}
