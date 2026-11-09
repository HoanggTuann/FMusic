import SwiftUI
import UniformTypeIdentifiers

// MARK: - Liquid Glass
extension View {
    /// iOS 26+: Liquid Glass thật (cần Xcode 26). Các bản cũ hơn: kính mờ (material) có viền sáng mảnh.
    @ViewBuilder
    func glassBackground<S: Shape>(in shape: S, interactive: Bool = false) -> some View {
        #if compiler(>=6.2)
        if #available(iOS 26.0, *) {
            self.glassEffect(interactive ? .regular.interactive() : .regular, in: shape)
        } else {
            self.background(.ultraThinMaterial, in: shape)
                .overlay(shape.stroke(Color.white.opacity(0.14), lineWidth: 0.5))
        }
        #else
        self.background(.ultraThinMaterial, in: shape)
            .overlay(shape.stroke(Color.white.opacity(0.14), lineWidth: 0.5))
        #endif
    }
}

// MARK: - Các mục của thanh dưới
enum AppTab: Int, CaseIterable {
    case nowPlaying, queue, lyrics, eq, settings

    var title: String {
        switch self {
        case .nowPlaying: return "Đang phát"
        case .queue:      return "Danh sách"
        case .lyrics:     return "Lời"
        case .eq:         return "EQ"
        case .settings:   return "Cài đặt"
        }
    }
    var symbol: String {
        switch self {
        case .nowPlaying: return "music.note"
        case .queue:      return "list.bullet"
        case .lyrics:     return "quote.bubble"
        case .eq:         return "slider.horizontal.3"
        case .settings:   return "gearshape"
        }
    }
}

struct GlassTabBar: View {
    @Binding var tab: AppTab
    @Namespace private var ns

    var body: some View {
        HStack(spacing: 0) {
            ForEach(AppTab.allCases, id: \.self) { t in
                Button {
                    withAnimation(.spring(response: 0.35, dampingFraction: 0.8)) { tab = t }
                } label: {
                    VStack(spacing: 3) {
                        Image(systemName: t.symbol).font(.system(size: 20, weight: .semibold))
                        Text(t.title).font(.system(size: 10, weight: .semibold))
                    }
                    .frame(maxWidth: .infinity)
                    .padding(.vertical, 8)
                    .foregroundStyle(tab == t ? Color.white : Color.white.opacity(0.55))
                    .background {
                        if tab == t {
                            Capsule().fill(Color.white.opacity(0.18)).matchedGeometryEffect(id: "selection", in: ns)
                        }
                    }
                }
                .buttonStyle(.plain)
            }
        }
        .padding(6)
        .glassBackground(in: Capsule(), interactive: true)
        .padding(.horizontal, 16)
    }
}

struct MiniPlayer: View {
    @ObservedObject var vm: PlayerViewModel
    let onTap: () -> Void

    var body: some View {
        HStack(spacing: 12) {
            Group {
                if let c = vm.cover { Image(uiImage: c).resizable().scaledToFill() }
                else { Color.white.opacity(0.12).overlay(Image(systemName: "music.note").opacity(0.7)) }
            }
            .frame(width: 44, height: 44)
            .clipShape(RoundedRectangle(cornerRadius: 9, style: .continuous))

            VStack(alignment: .leading, spacing: 1) {
                Text(vm.title).font(.system(size: 15, weight: .semibold)).lineLimit(1)
                Text(vm.artist).font(.system(size: 12)).opacity(0.6).lineLimit(1)
            }
            Spacer(minLength: 8)
            Button { vm.togglePlay() } label: {
                Image(systemName: vm.isPlaying ? "pause.fill" : "play.fill").font(.system(size: 22)).frame(width: 36, height: 36)
            }
            Button { vm.next() } label: {
                Image(systemName: "forward.fill").font(.system(size: 20)).frame(width: 36, height: 36)
            }
        }
        .buttonStyle(.plain)
        .padding(.horizontal, 12)
        .padding(.vertical, 8)
        .glassBackground(in: Capsule())
        .padding(.horizontal, 16)
        .contentShape(Rectangle())
        .onTapGesture(perform: onTap)
    }
}

// MARK: - Root
struct ContentView: View {
    @StateObject private var vm = PlayerViewModel()
    @State private var tab: AppTab = .nowPlaying
    @State private var showImporter = false
    @State private var importAppend = false
    @AppStorage("ambientBackground") private var ambient = true
    @AppStorage("spectrumRing") private var spectrumRing = true

    private var importTypes: [UTType] { [.audio, UTType(filenameExtension: "lrc") ?? .plainText] }

    var body: some View {
        ZStack {
            AmbientBackground(image: ambient ? vm.backdrop : nil, token: vm.coverToken)

            Group {
                switch tab {
                case .nowPlaying: nowPlaying
                case .queue:      QueueView(vm: vm) { importAppend = true; showImporter = true }
                case .lyrics:     LyricsView(vm: vm)
                case .eq:         EQView(vm: vm)
                case .settings:   SettingsView(vm: vm)
                }
            }
            .frame(maxWidth: .infinity, maxHeight: .infinity)
            .padding(.top, 8)
            .transition(.opacity)
        }
        .animation(.easeInOut(duration: 0.25), value: tab)
        .safeAreaInset(edge: .bottom, spacing: 8) {
            VStack(spacing: 8) {
                if tab != .nowPlaying && !vm.tracks.isEmpty {
                    MiniPlayer(vm: vm) { withAnimation { tab = .nowPlaying } }
                }
                GlassTabBar(tab: $tab)
            }
            .padding(.bottom, 4)
        }
        .foregroundStyle(.white)
        .preferredColorScheme(.dark)
        .onAppear { vm.scanDocuments() }
        .fileImporter(isPresented: $showImporter, allowedContentTypes: importTypes, allowsMultipleSelection: true) { result in
            if case .success(let urls) = result { importAppend ? vm.addFiles(urls) : vm.openFiles(urls) }
        }
    }

    private var nowPlaying: some View {
        VStack(spacing: 0) {
            HStack {
                Button { importAppend = false; showImporter = true } label: {
                    Image(systemName: "plus").font(.system(size: 18, weight: .semibold)).frame(width: 40, height: 40)
                }
                .buttonStyle(.plain)
                .glassBackground(in: Circle(), interactive: true)
                Spacer()
            }
            .padding(.horizontal, 20)

            ArtworkStage(cover: vm.cover, isPlaying: vm.isPlaying, showSpectrum: spectrumRing, spectrum: vm.spectrum)
                .padding(.horizontal, 12)
                .frame(maxHeight: .infinity)

            VStack(spacing: 20) {
                VStack(spacing: 4) {
                    Text(vm.title).font(.system(size: 22, weight: .semibold)).lineLimit(1)
                    Text(vm.artist).font(.system(size: 17)).opacity(0.6).lineLimit(1)
                }
                PlayerProgressBar(position: vm.position, duration: vm.duration) { vm.seek($0) }
                controls
            }
            .padding(.horizontal, 28)
            .padding(.bottom, 8)
        }
    }

    private var controls: some View {
        HStack {
            Button { vm.toggleShuffle() } label: {
                Image(systemName: "shuffle").font(.system(size: 20, weight: .semibold)).opacity(vm.shuffle ? 1 : 0.4)
            }
            Spacer()
            HStack(spacing: 44) {
                Button { vm.previous() } label: { Image(systemName: "backward.fill").font(.system(size: 30)) }
                Button { vm.togglePlay() } label: {
                    Image(systemName: vm.isPlaying ? "pause.fill" : "play.fill")
                        .font(.system(size: 44)).frame(width: 56, height: 56)
                }
                Button { vm.next() } label: { Image(systemName: "forward.fill").font(.system(size: 30)) }
            }
            Spacer()
            Button { vm.cycleRepeat() } label: {
                Image(systemName: vm.repeatMode == .one ? "repeat.1" : "repeat")
                    .font(.system(size: 20, weight: .semibold)).opacity(vm.repeatMode == .off ? 0.4 : 1)
            }
        }
        .buttonStyle(.plain)
    }
}

// MARK: - Cài đặt
struct SettingsView: View {
    @ObservedObject var vm: PlayerViewModel
    @AppStorage("ambientBackground") private var ambient = true
    @AppStorage("spectrumRing") private var spectrumRing = true
    @AppStorage("lyricOffsetMs") private var lyricOffset = 0.0

    var body: some View {
        ScrollView(showsIndicators: false) {
            VStack(spacing: 14) {
                Text("Cài đặt").font(.title2.bold()).frame(maxWidth: .infinity, alignment: .leading)

                card {
                    Toggle("Nền ambient động", isOn: $ambient)
                    Divider().opacity(0.3)
                    Toggle("Sóng nhạc quanh ảnh bìa", isOn: $spectrumRing)
                }

                card {
                    HStack {
                        Text("Lệch thời gian lời")
                        Spacer()
                        Text("\(lyricOffset > 0 ? "+" : "")\(Int(lyricOffset)) ms").opacity(0.6).monospacedDigit()
                    }
                    Slider(value: $lyricOffset, in: -2000...2000, step: 50).tint(.white)
                    Text("Dương: lời hiện sớm hơn · Âm: lời hiện muộn hơn").font(.caption).opacity(0.5)
                }

                card {
                    Button(role: .destructive) { vm.clearPlaylist() } label: {
                        Text("Xoá danh sách phát").frame(maxWidth: .infinity, alignment: .leading)
                    }
                }

                card {
                    Text("FMusic 0.3.0").font(.system(size: 16, weight: .semibold))
                    Text("Giải mã bằng FFmpeg. Điều khiển từ màn hình khoá, Control Center và tai nghe đã được bật. Tệp nhạc được sao chép vào Tệp ▸ Trên iPhone ▸ FMusic; bạn cũng có thể tự chép nhạc và tệp .lrc vào đó.")
                        .font(.footnote).opacity(0.6)
                }
            }
            .tint(.green)
            .padding(.horizontal, 20)
            .padding(.bottom, 24)
        }
    }

    private func card<Content: View>(@ViewBuilder _ content: () -> Content) -> some View {
        VStack(alignment: .leading, spacing: 12, content: content)
            .padding(16)
            .frame(maxWidth: .infinity, alignment: .leading)
            .glassBackground(in: RoundedRectangle(cornerRadius: 22, style: .continuous))
    }
}
