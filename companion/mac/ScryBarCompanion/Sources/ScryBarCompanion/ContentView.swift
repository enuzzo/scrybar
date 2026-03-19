import SwiftUI

struct ContentView: View {
    @EnvironmentObject private var model: AppModel

    var body: some View {
        NavigationSplitView {
            ConnectionSidebarView()
                .navigationSplitViewColumnWidth(min: 280, ideal: 320, max: 360)
        } detail: {
            NowPlayingDashboardView()
        }
        .navigationTitle("ScryBar Companion")
        .navigationSubtitle(model.currentPayload.source)
    }
}
