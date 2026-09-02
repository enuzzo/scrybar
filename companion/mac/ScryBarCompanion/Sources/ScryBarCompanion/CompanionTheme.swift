import SwiftUI

enum CompanionPalette {
    enum Layout {
        static let windowPadding: CGFloat = 20
        static let sectionSpacing: CGFloat = 14
        static let cardSpacing: CGFloat = 16
        static let cardCornerRadius: CGFloat = 18
        static let insetCornerRadius: CGFloat = 14
        static let pillCornerRadius: CGFloat = 999
        static let controlCornerRadius: CGFloat = 10
        static let buttonMinHeight: CGFloat = 28
        static let inputMinHeight: CGFloat = 32
        static let hairline: CGFloat = 1
    }

    enum Typography {
        static let appTitle = Font.system(size: 26, weight: .semibold, design: .rounded)
        static let sectionTitle = Font.system(size: 12, weight: .semibold, design: .rounded)
        static let label = Font.system(size: 13, weight: .medium, design: .default)
        static let body = Font.system(size: 13.5, weight: .regular, design: .default)
        static let bodyEmphasis = Font.system(size: 13.5, weight: .medium, design: .default)
        static let caption = Font.system(size: 12, weight: .regular, design: .default)
        static let mono = Font.system(size: 12, weight: .regular, design: .monospaced)
        static let monoEmphasis = Font.system(size: 12, weight: .medium, design: .monospaced)
    }

    enum Tone {
        case neutral
        case accent
        case success
        case warning
        case danger
    }

    static let preferredColorScheme: ColorScheme = .dark

    static let windowBackground = Color(red: 0.07, green: 0.08, blue: 0.10)
    static let windowBackgroundAlt = Color(red: 0.09, green: 0.10, blue: 0.12)
    static let surface = Color(red: 0.11, green: 0.12, blue: 0.15)
    static let surfaceElevated = Color(red: 0.14, green: 0.15, blue: 0.19)
    static let surfacePressed = Color(red: 0.18, green: 0.19, blue: 0.23)
    static let inputSurface = Color(red: 0.14, green: 0.15, blue: 0.18)
    static let pillSurface = Color(red: 0.16, green: 0.17, blue: 0.21)
    static let divider = Color.white.opacity(0.08)
    static let dividerStrong = Color.white.opacity(0.14)
    static let border = Color.white.opacity(0.09)
    static let borderStrong = Color.white.opacity(0.16)
    static let shadow = Color.black.opacity(0.35)
    static let textPrimary = Color.white.opacity(0.96)
    static let textSecondary = Color.white.opacity(0.78)
    static let textTertiary = Color.white.opacity(0.62)
    static let textDisabled = Color.white.opacity(0.46)
    static let accent = Color(red: 0.22, green: 0.62, blue: 0.98)
    static let accentSoft = Color(red: 0.18, green: 0.45, blue: 0.74)
    // BambuSphere reference tokens used for the printer telemetry surface.
    // Normal printer telemetry deliberately stays inside a black / white /
    // grey / Bambu-green palette so it remains recognisable in every theme.
    static let bambuSurface = Color.black                         // #000000
    static let bambuSurfaceElevated = Color(white: 0.063)         // #101010
    static let bambuTrack = Color(white: 0.102)                   // #1A1A1A
    static let bambuBorder = Color(white: 0.188)                  // #303030
    static let bambuTextPrimary = Color.white                    // #FFFFFF
    static let bambuTextSecondary = Color(white: 0.867)          // #DDDDDD
    static let bambuTextMuted = Color(white: 0.533)              // #888888
    static let bambuAccent = Color(red: 0.00, green: 1.00, blue: 0.00) // #00FF00
    static let bambuAccentSoft = Color(red: 0.00, green: 0.18, blue: 0.00)
    static let bambuDone = bambuAccent
    static let bambuPaused = Color(red: 1.00, green: 0.647, blue: 0.00) // #FFA500
    static let bambuError = Color(red: 1.00, green: 0.20, blue: 0.20)   // #FF3333
    static let success = Color(red: 0.24, green: 0.76, blue: 0.46)
    static let warning = Color(red: 0.90, green: 0.64, blue: 0.18)
    static let danger = Color(red: 0.90, green: 0.33, blue: 0.30)

    static func toneColor(_ tone: Tone) -> Color {
        switch tone {
        case .neutral: return textSecondary
        case .accent: return accent
        case .success: return success
        case .warning: return warning
        case .danger: return danger
        }
    }
}

typealias CompanionTheme = CompanionPalette

extension View {
    func companionWindowCanvas() -> some View {
        background(CompanionTheme.windowBackground)
    }

    func companionCard(
        elevated: Bool = false,
        padding: CGFloat = CompanionPalette.Layout.cardSpacing
    ) -> some View {
        modifier(CompanionCardModifier(elevated: elevated, padding: padding))
    }

    func companionInputChrome() -> some View {
        modifier(CompanionInputChromeModifier())
    }

    func companionMutedText() -> some View {
        foregroundStyle(CompanionTheme.textSecondary)
    }

    func companionTertiaryText() -> some View {
        foregroundStyle(CompanionTheme.textTertiary)
    }
}

struct CompanionCardModifier: ViewModifier {
    var elevated: Bool = false
    var padding: CGFloat = CompanionPalette.Layout.cardSpacing

    func body(content: Content) -> some View {
        content
            .padding(padding)
            .frame(maxWidth: .infinity, alignment: .leading)
            .background {
                RoundedRectangle(cornerRadius: CompanionPalette.Layout.cardCornerRadius, style: .continuous)
                    .fill(elevated ? CompanionPalette.surfaceElevated : CompanionPalette.surface)
            }
            .overlay {
                RoundedRectangle(cornerRadius: CompanionPalette.Layout.cardCornerRadius, style: .continuous)
                    .strokeBorder(elevated ? CompanionPalette.borderStrong : CompanionPalette.border, lineWidth: CompanionPalette.Layout.hairline)
            }
            .shadow(color: CompanionPalette.shadow, radius: elevated ? 12 : 8, x: 0, y: elevated ? 6 : 3)
    }
}

struct CompanionInputChromeModifier: ViewModifier {
    func body(content: Content) -> some View {
        content
            .padding(.horizontal, 10)
            .frame(minHeight: CompanionPalette.Layout.inputMinHeight, alignment: .leading)
            .background {
                RoundedRectangle(cornerRadius: CompanionPalette.Layout.controlCornerRadius, style: .continuous)
                    .fill(CompanionPalette.inputSurface)
            }
            .overlay {
                RoundedRectangle(cornerRadius: CompanionPalette.Layout.controlCornerRadius, style: .continuous)
                    .strokeBorder(CompanionPalette.borderStrong, lineWidth: CompanionPalette.Layout.hairline)
            }
    }
}

struct CompanionPrimaryButtonStyle: ButtonStyle {
    var tint: Color = CompanionPalette.accent
    var foreground: Color = CompanionPalette.textPrimary

    func makeBody(configuration: Configuration) -> some View {
        configuration.label
            .font(CompanionPalette.Typography.label)
            .foregroundStyle(foreground)
            .padding(.horizontal, 12)
            .frame(minHeight: CompanionPalette.Layout.buttonMinHeight)
            .background {
                RoundedRectangle(cornerRadius: CompanionPalette.Layout.controlCornerRadius, style: .continuous)
                    .fill(configuration.isPressed ? tint.opacity(0.72) : tint)
            }
            .overlay {
                RoundedRectangle(cornerRadius: CompanionPalette.Layout.controlCornerRadius, style: .continuous)
                    .strokeBorder(configuration.isPressed ? CompanionPalette.borderStrong : CompanionPalette.border, lineWidth: CompanionPalette.Layout.hairline)
            }
            .contentShape(RoundedRectangle(cornerRadius: CompanionPalette.Layout.controlCornerRadius, style: .continuous))
    }
}

struct CompanionSecondaryButtonStyle: ButtonStyle {
    func makeBody(configuration: Configuration) -> some View {
        configuration.label
            .font(CompanionPalette.Typography.label)
            .foregroundStyle(CompanionPalette.textPrimary)
            .padding(.horizontal, 12)
            .frame(minHeight: CompanionPalette.Layout.buttonMinHeight)
            .background {
                RoundedRectangle(cornerRadius: CompanionPalette.Layout.controlCornerRadius, style: .continuous)
                    .fill(configuration.isPressed ? CompanionPalette.surfacePressed : CompanionPalette.surfaceElevated)
            }
            .overlay {
                RoundedRectangle(cornerRadius: CompanionPalette.Layout.controlCornerRadius, style: .continuous)
                    .strokeBorder(configuration.isPressed ? CompanionPalette.borderStrong : CompanionPalette.border, lineWidth: CompanionPalette.Layout.hairline)
            }
            .contentShape(RoundedRectangle(cornerRadius: CompanionPalette.Layout.controlCornerRadius, style: .continuous))
    }
}

struct CompanionCard<Content: View>: View {
    var elevated: Bool = false
    var padding: CGFloat = CompanionPalette.Layout.cardSpacing
    @ViewBuilder var content: () -> Content

    init(
        elevated: Bool = false,
        padding: CGFloat = CompanionPalette.Layout.cardSpacing,
        @ViewBuilder content: @escaping () -> Content
    ) {
        self.elevated = elevated
        self.padding = padding
        self.content = content
    }

    var body: some View {
        content()
            .padding(padding)
            .frame(maxWidth: .infinity, alignment: .leading)
            .background {
                RoundedRectangle(cornerRadius: CompanionPalette.Layout.cardCornerRadius, style: .continuous)
                    .fill(elevated ? CompanionPalette.surfaceElevated : CompanionPalette.surface)
            }
            .overlay {
                RoundedRectangle(cornerRadius: CompanionPalette.Layout.cardCornerRadius, style: .continuous)
                    .strokeBorder(elevated ? CompanionPalette.borderStrong : CompanionPalette.border, lineWidth: CompanionPalette.Layout.hairline)
            }
            .shadow(color: CompanionPalette.shadow, radius: elevated ? 12 : 8, x: 0, y: elevated ? 6 : 3)
    }
}

struct CompanionStatusPill: View {
    let title: String
    var systemImage: String?
    var tone: CompanionPalette.Tone = .neutral
    var isSelected: Bool = false

    var body: some View {
        HStack(spacing: 6) {
            if let systemImage {
                Image(systemName: systemImage)
                    .font(.system(size: 11, weight: .semibold))
            }
            Text(title)
        }
        .font(CompanionPalette.Typography.caption)
        .foregroundStyle(isSelected ? CompanionPalette.textPrimary : CompanionPalette.toneColor(tone))
        .padding(.horizontal, 10)
        .padding(.vertical, 5)
        .background {
            RoundedRectangle(cornerRadius: CompanionPalette.Layout.pillCornerRadius, style: .continuous)
                .fill(isSelected ? CompanionPalette.accentSoft.opacity(0.28) : CompanionPalette.pillSurface)
        }
        .overlay {
            RoundedRectangle(cornerRadius: CompanionPalette.Layout.pillCornerRadius, style: .continuous)
                .strokeBorder(isSelected ? CompanionPalette.accent.opacity(0.45) : CompanionPalette.border, lineWidth: CompanionPalette.Layout.hairline)
        }
    }
}

typealias CompanionPill = CompanionStatusPill
typealias CompanionCompactActionButtonStyle = CompanionSecondaryButtonStyle

struct CompanionSectionHeader<Accessory: View>: View {
    let title: String
    var subtitle: String?
    @ViewBuilder var accessory: () -> Accessory

    init(
        title: String,
        subtitle: String? = nil,
        @ViewBuilder accessory: @escaping () -> Accessory
    ) {
        self.title = title
        self.subtitle = subtitle
        self.accessory = accessory
    }

    var body: some View {
        HStack(alignment: .firstTextBaseline, spacing: 12) {
            VStack(alignment: .leading, spacing: 3) {
                Text(title)
                    .font(CompanionTheme.Typography.sectionTitle)
                    .foregroundStyle(CompanionTheme.textPrimary)
                    .textCase(.uppercase)
                    .tracking(0.8)

                if let subtitle {
                    Text(subtitle)
                        .font(CompanionTheme.Typography.caption)
                        .foregroundStyle(CompanionTheme.textSecondary)
                }
            }

            Spacer(minLength: 8)
            accessory()
        }
    }
}

extension CompanionSectionHeader where Accessory == EmptyView {
    init(title: String, subtitle: String? = nil) {
        self.init(title: title, subtitle: subtitle) {
            EmptyView()
        }
    }
}

struct CompanionMetricRow<Value: View>: View {
    let label: String
    var tone: CompanionTheme.Tone = .neutral
    @ViewBuilder let value: () -> Value

    var body: some View {
        HStack(alignment: .firstTextBaseline, spacing: 12) {
            Text(label)
                .font(CompanionTheme.Typography.label)
                .foregroundStyle(CompanionTheme.textSecondary)
            Spacer(minLength: 12)
            value()
        }
        .padding(.vertical, 4)
    }
}

extension CompanionMetricRow where Value == Text {
    init(_ label: String, _ value: String, tone: CompanionTheme.Tone = .neutral) {
        self.label = label
        self.tone = tone
        self.value = {
            Text(value)
                .font(CompanionTheme.Typography.bodyEmphasis)
                .foregroundStyle(CompanionTheme.toneColor(tone))
        }
    }
}

struct CompanionCodeBlock: View {
    let text: String
    var minHeight: CGFloat = 200

    var body: some View {
        ScrollView {
            Text(text)
                .font(CompanionTheme.Typography.mono)
                .foregroundStyle(CompanionTheme.textPrimary)
                .frame(maxWidth: .infinity, alignment: .leading)
                .textSelection(.enabled)
                .padding(12)
        }
        .frame(minHeight: minHeight)
        .background {
            RoundedRectangle(cornerRadius: CompanionTheme.Layout.insetCornerRadius, style: .continuous)
                .fill(CompanionTheme.windowBackgroundAlt)
        }
        .overlay {
            RoundedRectangle(cornerRadius: CompanionTheme.Layout.insetCornerRadius, style: .continuous)
                .strokeBorder(CompanionTheme.border, lineWidth: CompanionTheme.Layout.hairline)
        }
    }
}
