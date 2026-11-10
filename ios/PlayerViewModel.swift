import SwiftUI
import AVFoundation
import MediaPlayer
import CoreImage
import CoreImage.CIFilterBuiltins

/// Model riêng cho spectrum: chỉ view vẽ sóng nhạc quan sát nó nên 60 fps không làm render lại cả màn hình.
final class SpectrumModel: ObservableObject {
    @Published var bands = [Float](repeating: 0, count: 64)
    @Published var bass: Float = 0
}

/// Nền ambient: thu nhỏ ảnh bìa, Gaussian blur bằng Core Image (chạy trên GPU/Metal), tăng bão hoà, hạ sáng.
enum Backdrop {
    private static let context = CIContext()   // Metal-backed mặc định

    static func make(from image: UIImage) -> UIImage? {
        guard let cg = image.cgImage else { return nil }
        var ci = CIImage(cgImage: cg)
        let scale = 128 / max(ci.extent.width, ci.extent.height)
        ci = ci.transformed(by: CGAffineTransform(scaleX: scale, y: scale))
        let rect = ci.extent

        let blur = CIFilter.gaussianBlur()
        blur.inputImage = ci.clampedToExtent()
        blur.radius = 10
        guard var out = blur.outputImage else { return nil }

        let color = CIFilter.colorControls()
        color.inputImage = out
        color.saturation = 1.5
        color.brightness = -0.12
        color.contrast = 1.05
        out = color.outputImage ?? out

        guard let result = context.createCGImage(out, from: rect) else { return nil }
        return UIImage(cgImage: result)
    }
}

struct TrackItem: Identifiable, Equatable {
    let id = UUID()
    let url: URL
    var title: String
    var artist: String = ""
    var duration: Double = 0
}

struct LyricLine: Identifiable {
    let id: Int
    let time: Double      // -1 nếu lời không có mốc thời gian
    let text: String
}

enum RepeatMode { case off, all, one }

final class PlayerViewModel: NSObject, ObservableObject {
    let spectrum = SpectrumModel()

    @Published var title = "Chưa có bài hát"
    // TASK 1 & 5: Cập nhật văn bản placeholder
    @Published var artist = "Mở Danh sách phát để thêm nhạc"
    @Published var cover: UIImage?
    @Published var backdrop: UIImage?
    @Published var coverToken = 0
    @Published var isPlaying = false
    @Published var position: Double = 0
    @Published var duration: Double = 0

    // danh sách phát
    @Published var tracks: [TrackItem] = []
    @Published var currentIndex = -1
    @Published var shuffle = false
    @Published var repeatMode: RepeatMode = .off

    // lời bài hát
    @Published var lyrics: [LyricLine] = []
    @Published var lyricsSynced = false
    @Published var currentLyric = -1

    // equalizer
    @Published var eqEnabled = false
    @Published var eqGains = [Float](repeating: 0, count: 10)
    @Published var eqPreset = 0          // -1 = tuỳ chỉnh

    // TASK 3: User preset management
    @Published var userPresets: [(name: String, gains: [Float])] = []

    // TASK 2: Dynamic sample rate
    @Published var sampleRate: Int = 44100

    private let core = MusicPlayerCore()
    private let engine = AVAudioEngine()
    private var sourceNode: AVAudioSourceNode!
    private var scopedURL: URL?
    private var lrcByName: [String: URL] = [:]
    private var link: CADisplayLink?
    private var bandBuffer = [Float](repeating: 0, count: 64)

    override init() {
        super.init()
        // TASK 2: load persisted sample rate
        let storedSR = UserDefaults.standard.integer(forKey: "sampleRate")
        if storedSR > 0 { sampleRate = storedSR }
        loadEq()
        // TASK 3: load user presets on startup
        loadUserPresets()
        configureAudio()
        configureRemoteCommands()
        link = CADisplayLink(target: self, selector: #selector(tick))
        link?.add(to: .main, forMode: .common)
    }

    deinit {
        link?.invalidate()
        engine.stop()
        scopedURL?.stopAccessingSecurityScopedResource()
        NotificationCenter.default.removeObserver(self)
    }

    // MARK: Audio engine
    // TASK 1: Background playback + interruption/route-change handling
    // TASK 2: Uses dynamic sampleRate instead of hardcoded 44100
    private func configureAudio() {
        let session = AVAudioSession.sharedInstance()
        try? session.setCategory(.playback, mode: .default, options: [.allowBluetooth, .allowAirPlay])
        try? session.setActive(true, options: .notifyOthersOnDeactivation)

        // Handle interruptions (phone calls, Siri, etc.)
        NotificationCenter.default.addObserver(self, selector: #selector(handleInterruption),
            name: AVAudioSession.interruptionNotification, object: session)
        // Handle route changes (headphones unplugged, etc.)
        NotificationCenter.default.addObserver(self, selector: #selector(handleRouteChange),
            name: AVAudioSession.routeChangeNotification, object: session)

        // TASK 2: Use dynamic sample rate
        let format = AVAudioFormat(standardFormatWithSampleRate: Double(sampleRate), channels: 2)!   // float32 planar
        let core = self.core
        sourceNode = AVAudioSourceNode(format: format) { _, _, frameCount, abl -> OSStatus in
            let list = UnsafeMutableAudioBufferListPointer(abl)
            guard list.count >= 2, let l = list[0].mData, let r = list[1].mData else { return noErr }
            core.pullLeft(l.assumingMemoryBound(to: Float.self),
                          right: r.assumingMemoryBound(to: Float.self),
                          frames: Int(frameCount))
            return noErr
        }
        engine.attach(sourceNode)
        engine.connect(sourceNode, to: engine.mainMixerNode, format: format)
        try? engine.start()
    }

    // TASK 1: Xử lý ngắt (gọi điện, Siri, v.v.)
    @objc private func handleInterruption(_ n: Notification) {
        guard let info = n.userInfo,
              let typeVal = info[AVAudioSessionInterruptionTypeKey] as? UInt,
              let type = AVAudioSession.InterruptionType(rawValue: typeVal) else { return }
        if type == .began {
            pause()
        } else if type == .ended {
            if let opts = info[AVAudioSessionInterruptionOptionKey] as? UInt,
               AVAudioSession.InterruptionOptions(rawValue: opts).contains(.shouldResume) {
                play()
            }
        }
    }

    // TASK 1: Dừng khi rút tai nghe
    @objc private func handleRouteChange(_ n: Notification) {
        if let reason = n.userInfo?[AVAudioSessionRouteChangeReasonKey] as? UInt,
           reason == AVAudioSession.RouteChangeReason.oldDeviceUnavailable.rawValue {
            pause()
        }
    }

    // TASK 2: Set sample rate dynamically, reinitialise engine
    func setSampleRate(_ sr: Int) {
        sampleRate = sr
        engine.stop()
        // Detach old sourceNode before reconfiguring
        if let node = sourceNode { engine.detach(node) }
        configureAudio() // reinit with new SR
        UserDefaults.standard.set(sr, forKey: "sampleRate")
    }

    // MARK: Danh sách phát
    /// Thay danh sách phát và phát bài đầu. Có thể kèm tệp .lrc (trùng tên với tệp nhạc) để có lời.
    func openFiles(_ urls: [URL]) { ingest(urls, append: false) }
    /// Thêm vào cuối danh sách.
    func addFiles(_ urls: [URL]) { ingest(urls, append: true) }

    /// Sao chép tệp được chọn vào vùng riêng của app (Documents/Imported) rồi phát từ bản sao.
    /// Nhờ vậy vẫn chạy khi chứng chỉ ký lại không cấp quyền đọc tệp bên ngoài sandbox.
    private func ingest(_ urls: [URL], append: Bool) {
        DispatchQueue.global(qos: .userInitiated).async { [weak self] in
            let local = urls.compactMap { Self.importToSandbox($0) }
            DispatchQueue.main.async {
                guard let self else { return }
                if local.isEmpty && !urls.isEmpty {
                    self.title = "Không nhập được tệp"
                    self.artist = "Hãy chép nhạc vào Tệp ▸ Trên iPhone ▸ FMusic"
                    return
                }
                self.finishIngest(local, append: append)
            }
        }
    }

    private static func documentsDir() -> URL? {
        FileManager.default.urls(for: .documentDirectory, in: .userDomainMask).first
    }

    private static func importToSandbox(_ url: URL) -> URL? {
        let fm = FileManager.default
        guard let docs = documentsDir() else { return nil }
        if url.standardizedFileURL.path.hasPrefix(docs.standardizedFileURL.path) { return url }   // đã nằm trong sandbox
        let dir = docs.appendingPathComponent("Imported", isDirectory: true)
        try? fm.createDirectory(at: dir, withIntermediateDirectories: true)
        let dest = dir.appendingPathComponent(url.lastPathComponent)

        let scoped = url.startAccessingSecurityScopedResource()
        defer { if scoped { url.stopAccessingSecurityScopedResource() } }

        let srcSize = (try? url.resourceValues(forKeys: [.fileSizeKey]).fileSize) ?? -1
        let dstSize = (try? dest.resourceValues(forKeys: [.fileSizeKey]).fileSize) ?? -2
        if fm.fileExists(atPath: dest.path), srcSize == dstSize { return dest }                  // đã có bản sao
        try? fm.removeItem(at: dest)
        do {
            try fm.copyItem(at: url, to: dest)
            return dest
        } catch {
            if let data = try? Data(contentsOf: url), (try? data.write(to: dest)) != nil { return dest }
            return nil
        }
    }

    /// Quét thư mục Documents của app (Tệp ▸ Trên iPhone ▸ FMusic): thêm nhạc/.lrc mà người dùng chép vào bằng app Tệp.
    func scanDocuments() {
        let fm = FileManager.default
        guard let docs = Self.documentsDir() else { return }
        try? fm.createDirectory(at: docs.appendingPathComponent("Imported", isDirectory: true), withIntermediateDirectories: true)
        let exts: Set<String> = ["mp3", "flac", "m4a", "aac", "wav", "ogg", "opus", "wma", "ape", "aiff", "wv"]
        var found: [URL] = []
        if let en = fm.enumerator(at: docs, includingPropertiesForKeys: nil, options: [.skipsHiddenFiles]) {
            for case let u as URL in en {
                let e = u.pathExtension.lowercased()
                if exts.contains(e) { found.append(u) }
                else if e == "lrc" { lrcByName[u.deletingPathExtension().lastPathComponent.lowercased()] = u }
            }
        }
        let known = Set(tracks.map { $0.url.standardizedFileURL.path })
        let fresh = found.filter { !known.contains($0.standardizedFileURL.path) }
                         .sorted { $0.lastPathComponent < $1.lastPathComponent }
        guard !fresh.isEmpty else { return }
        let items = fresh.map { TrackItem(url: $0, title: $0.deletingPathExtension().lastPathComponent) }
        tracks.append(contentsOf: items)
        probe(items)
        if currentIndex < 0 { loadTrack(0, autoplay: false) }
    }

    private func finishIngest(_ urls: [URL], append: Bool) {
        for u in urls where u.pathExtension.lowercased() == "lrc" {
            lrcByName[u.deletingPathExtension().lastPathComponent.lowercased()] = u
        }
        let audio = urls.filter { $0.pathExtension.lowercased() != "lrc" }
        guard !audio.isEmpty else {                                   // chỉ chọn .lrc -> nạp lại lời bài đang phát
            if tracks.indices.contains(currentIndex) { loadLyrics(for: tracks[currentIndex].url, embedded: core.trackInfo.lyrics) }
            return
        }
        if !append { tracks.removeAll(); currentIndex = -1 }
        let items = audio.map { TrackItem(url: $0, title: $0.deletingPathExtension().lastPathComponent) }
        tracks.append(contentsOf: items)
        probe(items)
        if currentIndex < 0 { loadTrack(0, autoplay: true) }
    }

    /// Đọc metadata nền (FFmpeg) cho từng bài để danh sách hiện đúng tên, nghệ sĩ, thời lượng.
    private func probe(_ items: [TrackItem]) {
        DispatchQueue.global(qos: .utility).async { [weak self] in
            for item in items {
                let ok = item.url.startAccessingSecurityScopedResource()
                let info = MusicPlayerCore.probePath(item.url.path)
                if ok { item.url.stopAccessingSecurityScopedResource() }
                guard let info else { continue }
                DispatchQueue.main.async {
                    guard let self, let i = self.tracks.firstIndex(where: { $0.id == item.id }) else { return }
                    self.tracks[i].title = info.title
                    self.tracks[i].artist = info.artist
                    self.tracks[i].duration = info.duration
                }
            }
        }
    }

    func select(_ index: Int) { loadTrack(index, autoplay: true) }

    func removeTracks(at offsets: IndexSet) { for i in offsets.sorted(by: >) { removeTrack(i) } }

    func removeTrack(_ i: Int) {
        guard tracks.indices.contains(i) else { return }
        if tracks.count == 1 { clearPlaylist(); return }
        let wasPlaying = isPlaying
        tracks.remove(at: i)
        if i < currentIndex { currentIndex -= 1 }
        else if i == currentIndex { loadTrack(min(i, tracks.count - 1), autoplay: wasPlaying) }
    }

    func clearPlaylist() {
        core.pause()
        isPlaying = false
        tracks.removeAll()
        currentIndex = -1
        lyrics = []; lyricsSynced = false; currentLyric = -1
        // TASK 1 & 5: Updated placeholder text
        title = "Chưa có bài hát"; artist = "Mở Danh sách phát để thêm nhạc"
        duration = 0; position = 0
        cover = nil; backdrop = nil; coverToken += 1
    }

    private func loadTrack(_ index: Int, autoplay: Bool) {
        guard tracks.indices.contains(index) else { return }
        scopedURL?.stopAccessingSecurityScopedResource()
        currentIndex = index
        let url = tracks[index].url
        _ = url.startAccessingSecurityScopedResource()     // giữ quyền truy cập suốt thời gian phát
        scopedURL = url

        do {
            try core.loadPath(url.path)
        } catch {
            title = "Không thể mở tệp"
            artist = error.localizedDescription
            return
        }
        let info = core.trackInfo
        title = info.title
        artist = info.artist
        duration = info.duration
        position = 0
        tracks[index].title = info.title
        tracks[index].artist = info.artist
        tracks[index].duration = info.duration

        let img = info.coverArt.flatMap { UIImage(data: $0) }
        cover = img
        backdrop = img.flatMap { Backdrop.make(from: $0) }
        coverToken += 1

        loadLyrics(for: url, embedded: info.lyrics)

        if autoplay { core.play(); isPlaying = true } else { isPlaying = false }
        // TASK 1: Cập nhật Now Playing ngay sau khi tải bài
        updateNowPlaying()
    }

    // MARK: Lời bài hát
    private func loadLyrics(for url: URL, embedded: String) {
        var text = ""
        if let lrc = lrcByName[url.deletingPathExtension().lastPathComponent.lowercased()] {
            let ok = lrc.startAccessingSecurityScopedResource()
            text = (try? String(contentsOf: lrc, encoding: .utf8)) ?? ""
            if ok { lrc.stopAccessingSecurityScopedResource() }
        }
        if text.isEmpty { text = embedded }                          // không có .lrc -> dùng lời nhúng trong tag
        let parsed = MusicPlayerCore.parseLyrics(text)               // parser C++ dùng chung với Windows
        lyricsSynced = parsed.synced
        lyrics = parsed.lines.enumerated().map { LyricLine(id: $0.offset, time: $0.element.time, text: $0.element.text) }
        currentLyric = -1
    }

    private func lyricIndex(at t: Double) -> Int {
        var lo = 0, hi = lyrics.count - 1, ans = -1
        while lo <= hi {
            let mid = (lo + hi) / 2
            if lyrics[mid].time <= t { ans = mid; lo = mid + 1 } else { hi = mid - 1 }
        }
        return ans
    }

    // MARK: Equalizer
    func setEqEnabled(_ on: Bool) {
        eqEnabled = on
        core.eqEnabled = on
        saveEq()
    }

    func setEqGain(_ band: Int, _ db: Float) {
        guard eqGains.indices.contains(band) else { return }
        eqGains[band] = db
        core.setEqGain(db, forBand: band)
        eqPreset = -1
        if !eqEnabled { setEqEnabled(true) } else { saveEq() }
    }

    func applyPreset(_ index: Int) {
        core.applyEqPreset(index)
        eqGains = (0..<10).map { core.eqGain(forBand: $0) }
        eqPreset = index
        setEqEnabled(true)
    }

    private func loadEq() {
        let d = UserDefaults.standard
        if let g = d.array(forKey: "eq.gains") as? [Double], g.count == 10 {
            eqGains = g.map { Float($0) }
            for (i, v) in eqGains.enumerated() { core.setEqGain(v, forBand: i) }
        }
        eqEnabled = d.bool(forKey: "eq.enabled")
        core.eqEnabled = eqEnabled
        eqPreset = d.object(forKey: "eq.preset") as? Int ?? 0
    }

    private func saveEq() {
        let d = UserDefaults.standard
        d.set(eqGains.map { Double($0) }, forKey: "eq.gains")
        d.set(eqEnabled, forKey: "eq.enabled")
        d.set(eqPreset, forKey: "eq.preset")
    }

    // MARK: TASK 3 - EQ preset save/load/export/import

    /// Lưu preset hiện tại với tên tuỳ chỉnh vào thư mục EQPresets trong Documents.
    func saveCurrentAsPreset(name: String) {
        let preset: [String: Any] = [
            "version": 1,
            "name": name,
            "enabled": eqEnabled,
            "gains": eqGains.map { Double($0) }
        ]
        if let data = try? JSONSerialization.data(withJSONObject: preset, options: .prettyPrinted),
           let dir = eqPresetsDir() {
            let url = dir.appendingPathComponent("\(name).feq")
            try? data.write(to: url)
            loadUserPresets()
        }
    }

    /// Xuất preset hiện tại ra file .feq tạm thời để share sheet có thể gửi đi.
    func exportPreset(name: String) -> URL? {
        let preset: [String: Any] = [
            "version": 1,
            "name": name,
            "gains": eqGains.map { Double($0) }
        ]
        guard let data = try? JSONSerialization.data(withJSONObject: preset, options: .prettyPrinted) else { return nil }
        let url = FileManager.default.temporaryDirectory.appendingPathComponent("\(name).feq")
        try? data.write(to: url)
        return url
    }

    /// Nhập preset .feq từ URL (file importer hoặc AirDrop, v.v.).
    func importPreset(from url: URL) {
        guard let data = try? Data(contentsOf: url),
              let json = try? JSONSerialization.jsonObject(with: data) as? [String: Any],
              let gains = json["gains"] as? [Double], gains.count == 10 else { return }
        eqGains = gains.map { Float($0) }
        for (i, g) in eqGains.enumerated() { core.setEqGain(g, forBand: i) }
        eqPreset = -1
        setEqEnabled(true)
    }

    /// Trả về URL thư mục chứa các preset người dùng; tạo thư mục nếu chưa có.
    private func eqPresetsDir() -> URL? {
        guard let docs = FileManager.default.urls(for: .documentDirectory, in: .userDomainMask).first else { return nil }
        let dir = docs.appendingPathComponent("EQPresets")
        try? FileManager.default.createDirectory(at: dir, withIntermediateDirectories: true)
        return dir
    }

    /// Nạp danh sách preset người dùng từ đĩa.
    func loadUserPresets() {
        guard let dir = eqPresetsDir(),
              let files = try? FileManager.default.contentsOfDirectory(at: dir, includingPropertiesForKeys: nil)
        else { return }
        userPresets = files.filter { $0.pathExtension == "feq" }.compactMap { url in
            guard let data = try? Data(contentsOf: url),
                  let json = try? JSONSerialization.jsonObject(with: data) as? [String: Any],
                  let gains = json["gains"] as? [Double], gains.count == 10,
                  let name = json["name"] as? String else { return nil }
            return (name: name, gains: gains.map { Float($0) })
        }.sorted { $0.name < $1.name }
    }

    /// Áp dụng user preset theo tên.
    func applyUserPreset(_ preset: (name: String, gains: [Float])) {
        eqGains = preset.gains
        for (i, g) in eqGains.enumerated() { core.setEqGain(g, forBand: i) }
        eqPreset = -1
        setEqEnabled(true)
    }

    /// Xoá user preset theo tên.
    func deleteUserPreset(name: String) {
        guard let dir = eqPresetsDir() else { return }
        let url = dir.appendingPathComponent("\(name).feq")
        try? FileManager.default.removeItem(at: url)
        loadUserPresets()
    }

    // MARK: Transport
    func togglePlay() {
        if tracks.isEmpty { return }
        if isPlaying {
            core.pause(); isPlaying = false
        } else {
            if core.ended { core.seekToSeconds(0) }
            core.play(); isPlaying = true
        }
        // TASK 1: Cập nhật Now Playing khi play/pause
        updateNowPlaying()
    }

    func play()  { if !isPlaying { togglePlay() } }
    func pause() { if isPlaying { togglePlay() } }

    func next(automatic: Bool = false) {
        guard !tracks.isEmpty else { return }
        if automatic && repeatMode == .one {                         // lặp 1 bài
            core.seekToSeconds(0); core.play(); isPlaying = true
            return
        }
        var target: Int?
        if shuffle && tracks.count > 1 {
            repeat { target = Int.random(in: 0..<tracks.count) } while target == currentIndex
        } else if currentIndex + 1 < tracks.count {
            target = currentIndex + 1
        } else if repeatMode == .all {
            target = 0
        }
        if let t = target { loadTrack(t, autoplay: true) }
        else { core.pause(); core.seekToSeconds(0); isPlaying = false; position = 0; updateNowPlaying() }
    }

    func previous() {
        if core.position > 3 || currentIndex <= 0 { core.seekToSeconds(0) }
        else { loadTrack(currentIndex - 1, autoplay: true) }
    }

    func toggleShuffle() { shuffle.toggle() }

    func cycleRepeat() {
        switch repeatMode {
        case .off: repeatMode = .all
        case .all: repeatMode = .one
        case .one: repeatMode = .off
        }
    }

    func seek(_ seconds: Double) {
        core.seekToSeconds(seconds)
        position = seconds
        // TASK 1: Cập nhật Now Playing sau khi tua
        updateNowPlaying()
    }

    // MARK: 60 Hz tick
    @objc private func tick() {
        let bass = core.copySpectrum(&bandBuffer, count: bandBuffer.count)
        spectrum.bands = bandBuffer
        spectrum.bass = bass

        let p = core.position
        if abs(p - position) > 0.03 { position = p }
        if lyricsSynced && !lyrics.isEmpty {
            let offset = UserDefaults.standard.double(forKey: "lyricOffsetMs") / 1000     // chỉnh trong Cài đặt
            let idx = lyricIndex(at: p + 0.05 + offset)
            if idx != currentLyric { currentLyric = idx }
        }
        if isPlaying && core.ended { next(automatic: true) }         // tự chuyển bài / dừng cuối danh sách
    }

    // MARK: Lock screen / Control Center
    // TASK 1: updateNowPlaying được gọi sau seek, play, pause, loadTrack
    private func updateNowPlaying() {
        var info: [String: Any] = [
            MPMediaItemPropertyTitle: title,
            MPMediaItemPropertyArtist: artist,
            MPMediaItemPropertyPlaybackDuration: duration,
            MPNowPlayingInfoPropertyElapsedPlaybackTime: core.position,
            MPNowPlayingInfoPropertyPlaybackRate: isPlaying ? 1.0 : 0.0
        ]
        if let cover {
            info[MPMediaItemPropertyArtwork] = MPMediaItemArtwork(boundsSize: cover.size) { _ in cover }
        }
        MPNowPlayingInfoCenter.default().nowPlayingInfo = info
    }

    private func configureRemoteCommands() {
        let c = MPRemoteCommandCenter.shared()
        c.playCommand.addTarget { [weak self] _ in DispatchQueue.main.async { self?.play() }; return .success }
        c.pauseCommand.addTarget { [weak self] _ in DispatchQueue.main.async { self?.pause() }; return .success }
        c.nextTrackCommand.addTarget { [weak self] _ in DispatchQueue.main.async { self?.next() }; return .success }
        c.previousTrackCommand.addTarget { [weak self] _ in DispatchQueue.main.async { self?.previous() }; return .success }
        c.changePlaybackPositionCommand.addTarget { [weak self] event in
            if let e = event as? MPChangePlaybackPositionCommandEvent {
                DispatchQueue.main.async { self?.seek(e.positionTime) }
            }
            return .success
        }
    }
}
