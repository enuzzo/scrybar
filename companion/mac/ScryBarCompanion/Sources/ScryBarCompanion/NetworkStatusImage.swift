import AppKit
import CoreText

/// NSButton's multiline title layout uses text metrics rather than the visible
/// two-row block. A template image lets the status button center that block as
/// one native icon, including on menu bars taller than the standard 22 points.
enum NetworkStatusImage {
    static func make(download: String, upload: String) -> NSImage {
        let image = NSImage(size: NSSize(width: 82, height: 22), flipped: false) { bounds in
            guard let context = NSGraphicsContext.current?.cgContext else { return false }
            let font = NSFont.monospacedDigitSystemFont(ofSize: 9.5, weight: .medium)
            let attributes: [NSAttributedString.Key: Any] = [.font: font, .foregroundColor: NSColor.black]
            let down = CTLineCreateWithAttributedString(NSAttributedString(string: "↓ \(download)", attributes: attributes))
            let up = CTLineCreateWithAttributedString(NSAttributedString(string: "↑ \(upload)", attributes: attributes))
            let lineSpacing: CGFloat = 10
            let downInk = CTLineGetImageBounds(down, nil).offsetBy(dx: 0, dy: lineSpacing)
            let upInk = CTLineGetImageBounds(up, nil)
            let ink = downInk.union(upInk)
            // Center visible glyphs, including arrows/descenders, rather than
            // adding a font-dependent baseline offset to a multiline title.
            let originY = ((bounds.midY - ink.midY) * 2).rounded() / 2
            context.saveGState()
            defer { context.restoreGState() }
            context.textMatrix = .identity
            context.textPosition = CGPoint(x: -ink.minX, y: originY + lineSpacing)
            CTLineDraw(down, context)
            context.textPosition = CGPoint(x: -ink.minX, y: originY)
            CTLineDraw(up, context)
            return true
        }
        image.isTemplate = true
        return image
    }
}
