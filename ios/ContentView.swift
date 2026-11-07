import SwiftUI
import UniformTypeIdentifiers

// MARK: - Root
struct ContentView: View {
    @StateObject private var vm = PlayerViewModel()
    @State private var showImporter = false

    var body: some View {
        ZStack {
            AmbientBackground(image: vm.backdrop, token: vm.coverToken)

            VStack(spacing: 0) {
                HStack {
                    Button { showImporter = true } label: {
                        Image(systemName: "plus")
                            .font(.system(size: 18, weight: .semibold))
                            .frame(width: 40, height: 40)
                            .background(.ultraThinMaterial, in: Circle())
                    }
                    Spacer()
                }
                .padding(.horizontal, 20)
                .padding(.top, 8)

                ArtworkStage(cover: vm.cover, isPlaying: vm.isPlaying, spectrum: vm.spectrum)
                    .padding(.horizontal, 12)
                    .frame(maxHeight: .infinity)

                VStack(spacing: 20) {
                    VStack(spacing: 4) {
                        Text(vm.title).font(.system(size: 22, weight: .semibold)).lineLimit(1)
                        Text(vm.artist).font(.system(size: 17)).opacity(0.6).lineLimit(1)
                    }
                    PlayerProgressBar(position: vm.position, duration: vm.duration) { vm.seek($0) }
                    HStack(spacing: 56) {
                        Button { vm.previous() } label: { Image(systemName: "backward.fill").font(.system(size: 30)) }
                        Button { vm.togglePlay() } label: {
                            Image(systemName: vm.isPlaying ? "pause.fill" : "play.fill")
                                .font(.system(size: 44))
                                .frame(width: 56, height: 56)
                        }
                        Button { vm.next() } label: { Image(systemName: "forward.fill").font(.system(size: 30)) }
                    }
                    .buttonStyle(.plain)
                }
                .padding(.horizontal, 28)
                .padding(.bottom, 28)
            }
        }
        .foregroundStyle(.white)
        .preferredColorScheme(.dark)
        .fileImporter(isPresented: $showImporter, allowedContentTypes: [.audio], allowsMultipleSelection: true) { result in
            if case .success(let urls) = result { vm.openFiles(urls) }
        }
    }
}

// MARK: - Ambient background
struct AmbientBackground: View {
    let image: UIImage?     // đã blur sẵn bằng Core Image (xem Backdrop.make)
    let token: Int
    @State private var animate = false

    var body: some View {
        GeometryReader { geo in
            ZStack {
                Color.black
                if let image {
                    ZStack {
                        layer(image, geo, rotation: animate ? 14 : -14, scale: animate ? 2.0 : 1.6, opacity: 1)
                        layer(image, geo, rotation: animate ? -40 : 30, scale: animate ? 1.7 : 2.2, opacity: 0.55)
                    }
                    .id(token)
                    .transition(.opacity)
                }
                Color.black.opacity(0.38)
            }
            .animation(.easeInOut(duration: 0.9), value: token)
        }
        .ignoresSafeArea()
        .onAppear {
            withAnimation(.easeInOut(duration: 20).repeatForever(autoreverses: true)) { animate = true }
        }
    }

    private func layer(_ img: UIImage, _ geo: GeometryProxy, rotation: Double, scale: CGFloat, opacity: Double) -> some View {
        Image(uiImage: img)
            .resizable()
            .interpolation(.high)
            .scaledToFill()
            .frame(width: geo.size.width, height: geo.size.height)
            .scaleEffect(scale)
            .rotationEffect(.degrees(rotation))
            .opacity(opacity)
            .clipped()
    }
}

// MARK: - Artwork + spectrum ring
struct ArtworkStage: View {
    let cover: UIImage?
    let isPlaying: Bool
    @ObservedObject var spectrum: SpectrumModel

    var body: some View {
        GeometryReader { geo in
            let full = min(geo.size.width, geo.size.height)
            let art = full * 0.72
            let radius = art * 0.045

            ZStack {
                SpectrumRing(bands: spectrum.bands, artSide: art)
                    .frame(width: full, height: full)

                Group {
                    if let cover {
                        Image(uiImage: cover).resizable().scaledToFill()
                    } else {
                        LinearGradient(colors: [Color(white: 0.28), Color(white: 0.1)],
                                       startPoint: .topLeading, endPoint: .bottomTrailing)
                            .overlay(Image(systemName: "music.note")
                                .font(.system(size: art * 0.3))
                                .foregroundStyle(.white.opacity(0.5)))
                    }
                }
                .frame(width: art, height: art)
                .clipShape(RoundedRectangle(cornerRadius: radius, style: .continuous))
                .overlay(RoundedRectangle(cornerRadius: radius, style: .continuous)
                    .strokeBorder(.white.opacity(0.10), lineWidth: 0.5))
                .shadow(color: .black.opacity(0.55), radius: art * 0.09, x: 0, y: art * 0.06)   // drop shadow sâu
            }
            .frame(width: geo.size.width, height: geo.size.height)
            .scaleEffect((isPlaying ? 1.0 : 0.86) + CGFloat(spectrum.bass) * 0.02)   // thu nhỏ khi pause, "thở" theo bass
            .animation(.spring(response: 0.5, dampingFraction: 0.7), value: isPlaying)
        }
    }
}

struct SpectrumRing: View {
    let bands: [Float]
    let artSide: CGFloat

    var body: some View {
        Canvas { ctx, size in
            let c = CGPoint(x: size.width / 2, y: size.height / 2)
            let gap = artSide * 0.035
            let maxLen = artSide * 0.15
            let ring = CGRect(x: c.x - artSide / 2 - gap, y: c.y - artSide / 2 - gap,
                              width: artSide + 2 * gap, height: artSide + 2 * gap)
            let r = artSide * 0.045 + gap
            let hw = ring.width - 2 * r, hh = ring.height - 2 * r
            let perimeter = 2 * hw + 2 * hh + 2 * .pi * r
            let n = 96
            let bw = max(2, perimeter / CGFloat(n) * 0.52)
            let count = bands.count

            for pass in 0..<2 {                               // pass 0: vầng sáng, pass 1: thanh chính
                for k in 0..<n {
                    let f = (CGFloat(k) + 0.5) / CGFloat(n)
                    let g = abs(f - 0.5) * 2                  // bass ở giữa cạnh trên, treble ở giữa cạnh dưới
                    let x = Float(g) * Float(count - 1)
                    let i0 = Int(x), i1 = min(i0 + 1, count - 1)
                    let v = CGFloat(bands[i0] * (1 - (x - Float(i0))) + bands[i1] * (x - Float(i0)))
                    let pt = perimeterPoint(ring, radius: r, s: perimeter * f)
                    let len = 3 + pow(v, 1.2) * maxLen
                    var path = Path()
                    path.move(to: pt.p)
                    path.addLine(to: CGPoint(x: pt.p.x + pt.n.x * len, y: pt.p.y + pt.n.y * len))
                    let alpha = pass == 0 ? 0.15 * Double(v) : 0.28 + 0.62 * Double(v)
                    ctx.stroke(path, with: .color(.white.opacity(alpha)),
                               style: StrokeStyle(lineWidth: pass == 0 ? bw * 2.4 : bw, lineCap: .round))
                }
            }
        }
        .allowsHitTesting(false)
    }

    /// Điểm + pháp tuyến ngoài trên chu vi hình chữ nhật bo góc. s: độ dài cung từ giữa cạnh dưới, đi qua phải -> trên -> trái.
    private func perimeterPoint(_ rc: CGRect, radius r: CGFloat, s sIn: CGFloat) -> (p: CGPoint, n: CGPoint) {
        let hw = rc.width - 2 * r, hh = rc.height - 2 * r, q = CGFloat.pi * r / 2
        let lens: [CGFloat] = [hw / 2, q, hh, q, hw, q, hh, q, hw / 2]
        var s = sIn
        var i = 0
        while i < 8 && s > lens[i] { s -= lens[i]; i += 1 }
        let u = lens[i] > 0 ? min(max(s / lens[i], 0), 1) : 0

        func arc(_ c: CGPoint, _ deg: CGFloat) -> (p: CGPoint, n: CGPoint) {
            let a = deg * .pi / 180
            let n = CGPoint(x: cos(a), y: sin(a))
            return (CGPoint(x: c.x + r * n.x, y: c.y + r * n.y), n)
        }
        switch i {
        case 0: return (CGPoint(x: rc.midX + u * hw / 2, y: rc.maxY), CGPoint(x: 0, y: 1))
        case 1: return arc(CGPoint(x: rc.maxX - r, y: rc.maxY - r), 90 - 90 * u)
        case 2: return (CGPoint(x: rc.maxX, y: rc.maxY - r - u * hh), CGPoint(x: 1, y: 0))
        case 3: return arc(CGPoint(x: rc.maxX - r, y: rc.minY + r), -90 * u)
        case 4: return (CGPoint(x: rc.maxX - r - u * hw, y: rc.minY), CGPoint(x: 0, y: -1))
        case 5: return arc(CGPoint(x: rc.minX + r, y: rc.minY + r), -90 - 90 * u)
        case 6: return (CGPoint(x: rc.minX, y: rc.minY + r + u * hh), CGPoint(x: -1, y: 0))
        case 7: return arc(CGPoint(x: rc.minX + r, y: rc.maxY - r), 180 - 90 * u)
        default: return (CGPoint(x: rc.minX + r + u * hw / 2, y: rc.maxY), CGPoint(x: 0, y: 1))
        }
    }
}

// MARK: - Progress bar
struct PlayerProgressBar: View {
    let position: Double
    let duration: Double
    let onSeek: (Double) -> Void
    @State private var dragFraction: Double?

    var body: some View {
        let frac = dragFraction ?? (duration > 0 ? position / duration : 0)
        let shown = frac * duration

        VStack(spacing: 4) {
            GeometryReader { geo in
                ZStack(alignment: .leading) {
                    Capsule().fill(.white.opacity(0.25))
                    Capsule().fill(.white.opacity(0.9))
                        .frame(width: max(0, geo.size.width * CGFloat(min(max(frac, 0), 1))))
                }
                .frame(height: dragFraction == nil ? 6 : 10)
                .frame(maxHeight: .infinity)
                .contentShape(Rectangle())
                .gesture(
                    DragGesture(minimumDistance: 0)
                        .onChanged { v in dragFraction = min(max(v.location.x / geo.size.width, 0), 1) }
                        .onEnded { v in
                            let f = min(max(v.location.x / geo.size.width, 0), 1)
                            if duration > 0 { onSeek(f * duration) }
                            dragFraction = nil
                        }
                )
            }
            .frame(height: 24)
            .animation(.easeOut(duration: 0.15), value: dragFraction == nil)

            HStack {
                Text(Self.format(shown))
                Spacer()
                Text("-" + Self.format(max(0, duration - shown)))
            }
            .font(.caption2.monospacedDigit())
            .opacity(0.6)
        }
    }

    static func format(_ s: Double) -> String {
        let t = max(0, Int(s.rounded()))
        return String(format: "%d:%02d", t / 60, t % 60)
    }
}
