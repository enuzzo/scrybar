// swift-tools-version: 6.0
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
        .testTarget(
            name: "ScryBarCompanionTests",
            dependencies: ["ScryBarCompanion"]
        ),
    ]
)
