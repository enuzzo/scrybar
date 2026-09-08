import SwiftUI

struct BambuCameraView: View {
    @ObservedObject var camera: BambuCameraModel

    var body: some View {
        VStack(spacing: 12) {
            ZStack {
                Color.black
                if camera.macViewing, let image = camera.image {
                    Image(nsImage: image).resizable().scaledToFit()
                } else {
                    VStack(spacing: 10) {
                        Image(systemName: "video.slash").font(.largeTitle)
                        Text(camera.macViewing ? camera.status : "Camera stopped").multilineTextAlignment(.center)
                    }.foregroundStyle(.white).padding()
                }
            }.aspectRatio(16 / 9, contentMode: .fit)
            HStack {
                Button(camera.macViewing ? "Stop" : "Start", systemImage: camera.macViewing ? "stop.fill" : "play.fill") {
                    camera.viewOnMac(!camera.macViewing)
                }
                Button("Retry", systemImage: "arrow.clockwise") { camera.retry() }
                    .disabled(!camera.macViewing)
                Spacer()
                Button(camera.lightOn == true ? "Light off" : "Light on", systemImage: "lightbulb") {
                    camera.toggleLight()
                }.disabled(!camera.running)
            }
            HStack {
                Text(camera.status)
                Spacer()
                if camera.lightPending { Text("Light cleanup armed").foregroundStyle(.secondary) }
            }.font(.caption)
        }
        .padding(16)
        .frame(minWidth: 480, idealWidth: 640, minHeight: 340)
    }
}
