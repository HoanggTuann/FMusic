import SwiftUI

// MARK: - Lời bài hát
struct LyricsView: View {
    @ObservedObject var vm: PlayerViewModel

    var body: some View {
        if vm.lyrics.isEmpty {
            VStack(spacing: 8) {
                Text("Không có lời bài hát").font(.title3.weight(.semibold))
                Text("Chọn kèm tệp .lrc cùng tên với tệp nhạc (vd: bai-hat.mp3 + bai-hat.lrc), hoặc dùng tệp có lời nhúng trong tag.")
                    .font(.footnote).multilineTextAlignment(.center).opacity(0.6)
            }
            .padding(32)
            .frame(maxWidth: .infinity, maxHeight: .infinity)
        } else {
            ScrollViewReader { proxy in
                ScrollView(showsIndicators: false) {
                    LazyVStack(alignment: .leading, spacing: 22) {
                        ForEach(vm.lyrics) { line in
                            Text(line.text.isEmpty ? "♪" : line.text)
                                .font(.system(size: 28, weight: .bold))
                                .opacity(opacity(for: line.id))
                                .blur(radius: vm.lyricsSynced ? CGFloat(min(3, abs(line.id - vm.currentLyric))) : 0)
                                .frame(maxWidth: .infinity, alignment: .leading)
                                .contentShape(Rectangle())
                                .id(line.id)
                                .onTapGesture { if vm.lyricsSynced && line.time >= 0 { vm.seek(line.time) } }   // chạm 1 dòng để tua tới đó
                        }
                    }
                    .padding(.horizontal, 24)
                    .padding(.vertical, 120)
                }
                .onChange(of: vm.currentLyric) { idx in
                    guard vm.lyricsSynced, idx >= 0 else { return }
                    withAnimation(.easeInOut(duration: 0.5)) { proxy.scrollTo(idx, anchor: UnitPoint(x: 0.5, y: 0.35)) }
                }
                .onAppear {
                    if vm.lyricsSynced, vm.currentLyric >= 0 { proxy.scrollTo(vm.currentLyric, anchor: UnitPoint(x: 0.5, y: 0.35)) }
                }
            }
        }
    }

    private func opacity(for id: Int) -> Double {
        guard vm.lyricsSynced else { return 0.9 }
        return id == vm.currentLyric ? 1.0 : 0.35
    }
}

// MARK: - Danh sách phát
struct QueueView: View {
    @ObservedObject var vm: PlayerViewModel
    let onAdd: () -> Void

    var body: some View {
        VStack(spacing: 10) {
            HStack {
                VStack(alignment: .leading, spacing: 2) {
                    Text("Danh sách phát").font(.title3.bold())
                    Text("\(vm.tracks.count) bài").font(.caption).opacity(0.6)
                }
                Spacer()
                chip("Xoá hết") { vm.clearPlaylist() }
                chip("Thêm", action: onAdd)
            }
            .padding(.horizontal, 20)

            if vm.tracks.isEmpty {
                Text("Chưa có bài hát\nNhấn “Thêm” để chọn nhạc")
                    .multilineTextAlignment(.center).opacity(0.6)
                    .frame(maxWidth: .infinity, maxHeight: .infinity)
            } else {
                List {
                    ForEach(Array(vm.tracks.enumerated()), id: \.element.id) { i, t in
                        HStack(spacing: 12) {
                            Group {
                                if i == vm.currentIndex { Image(systemName: "music.note") } else { Text("\(i + 1)").opacity(0.5) }
                            }
                            .frame(width: 24)
                            VStack(alignment: .leading, spacing: 2) {
                                Text(t.title).font(.system(size: 16, weight: .semibold)).lineLimit(1)
                                if !t.artist.isEmpty { Text(t.artist).font(.system(size: 13)).opacity(0.6).lineLimit(1) }
                            }
                            Spacer()
                            if t.duration > 0 { Text(PlayerProgressBar.format(t.duration)).font(.caption.monospacedDigit()).opacity(0.6) }
                        }
                        .contentShape(Rectangle())
                        .onTapGesture { vm.select(i) }
                        .listRowBackground(i == vm.currentIndex ? Color.white.opacity(0.16) : Color.clear)
                        .listRowSeparator(.hidden)
                    }
                    .onDelete { vm.removeTracks(at: $0) }          // vuốt trái để xoá
                }
                .listStyle(.plain)
                .scrollContentBackground(.hidden)
            }
        }
    }

    private func chip(_ text: String, action: @escaping () -> Void) -> some View {
        Button(action: action) {
            Text(text).font(.system(size: 13, weight: .semibold))
                .padding(.horizontal, 12).padding(.vertical, 7)
                .background(Color.white.opacity(0.16), in: Capsule())
        }
        .buttonStyle(.plain)
    }
}

// MARK: - Equalizer
struct EQView: View {
    @ObservedObject var vm: PlayerViewModel
    private let freqs = ["31", "62", "125", "250", "500", "1k", "2k", "4k", "8k", "16k"]
    private let presets = MusicPlayerCore.eqPresetNames()

    var body: some View {
        VStack(spacing: 16) {
            HStack {
                Text("Equalizer").font(.title3.bold())
                Spacer()
                Toggle("", isOn: Binding(get: { vm.eqEnabled }, set: { vm.setEqEnabled($0) }))
                    .labelsHidden().tint(.green)
            }
            ScrollView(.horizontal, showsIndicators: false) {
                HStack(spacing: 8) {
                    ForEach(presets.indices, id: \.self) { i in
                        Button { vm.applyPreset(i) } label: {
                            Text(presets[i]).font(.system(size: 14, weight: .semibold))
                                .padding(.horizontal, 14).padding(.vertical, 8)
                                .background(vm.eqPreset == i ? Color.white : Color.white.opacity(0.16), in: Capsule())
                                .foregroundStyle(vm.eqPreset == i ? Color.black : Color.white)
                        }
                        .buttonStyle(.plain)
                    }
                }
            }
            HStack(spacing: 0) {
                ForEach(0..<10, id: \.self) { b in
                    EQBandSlider(gain: vm.eqGains[b], label: freqs[b],
                                 onChange: { vm.setEqGain(b, $0) },
                                 onReset: { vm.setEqGain(b, 0) })
                }
            }
            .frame(maxHeight: 320)
            .opacity(vm.eqEnabled ? 1 : 0.45)
            Text("Kéo để chỉnh · chạm hai lần để về 0 dB").font(.footnote).opacity(0.5)
            Spacer(minLength: 0)
        }
        .padding(.horizontal, 20)
    }
}

struct EQBandSlider: View {
    let gain: Float
    let label: String
    let onChange: (Float) -> Void
    let onReset: () -> Void

    var body: some View {
        VStack(spacing: 6) {
            Text(String(format: gain == gain.rounded() ? "%+.0f" : "%+.1f", gain))
                .font(.system(size: 10).monospacedDigit()).opacity(0.7)
                .frame(height: 14)
            GeometryReader { geo in
                let w = geo.size.width, h = geo.size.height
                let y = h * (1 - CGFloat((gain + 12) / 24))
                ZStack {
                    Capsule().fill(Color.white.opacity(0.25)).frame(width: 4, height: h)
                    Capsule().fill(Color.white.opacity(0.9))
                        .frame(width: 4, height: max(2, abs(y - h / 2)))
                        .position(x: w / 2, y: (y + h / 2) / 2)
                    Circle().fill(Color.white).frame(width: 20, height: 20).position(x: w / 2, y: y)
                }
                .frame(width: w, height: h)
                .contentShape(Rectangle())
                .gesture(
                    DragGesture(minimumDistance: 0).onChanged { v in
                        let f = 1 - min(max(v.location.y / h, 0), 1)
                        var db = Float(f) * 24 - 12
                        db = (db * 2).rounded() / 2                      // bước 0.5 dB
                        if abs(db) < 0.4 { db = 0 }
                        onChange(db)
                    }
                )
                .simultaneousGesture(TapGesture(count: 2).onEnded { onReset() })
            }
            Text(label).font(.system(size: 10)).opacity(0.6).frame(height: 14)
        }
        .frame(maxWidth: .infinity)
    }
}
