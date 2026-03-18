// swift-tools-version: 6.2
import PackageDescription

let package = Package(
    name: "ScryBarCompanion",
    platforms: [
        .macOS(.v14),
    ],
    products: [
        .executable(
            name: "ScryBarCompanion",
            targets: ["ScryBarCompanion"]
        ),
    ],
    targets: [
        .executableTarget(
            name: "ScryBarCompanion"
        ),
    ]
)
