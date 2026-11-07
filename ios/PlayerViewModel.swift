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

final class PlayerViewModel: NSObject, ObservableObject {
    let spectrum = SpectrumModel()

    @Published var title = "Chưa có bài hát"
    @Published var artist = "Nhấn + để chọn nhạc"
    @Published var cover: UIImage?
    @Published var backdrop: UIImage?
    @Published var coverToken = 0
    @Published var isPlaying = false
    @Published var position: Double = 0
    @Published var duration: Double = 0

    private let core = MusicPlayerCore()
    private let engine = AVAudioEngine()
    private var sourceNode: AVAudioSourceNode!
    private var playlist: [URL] = []
    private var index = 0
    private var scopedURL: URL?
    private var link: CADisplayLink?
    private var bandBuffer = [Float](repeating: 0, count: 64)

    override init() {
        super.init()
        configureAudio()
        configureRemoteCommands()
        link = CADisplayLink(target: self, selector: #selector(tick))
        link?.add(to: .main, forMode: .common)
    }

    deinit {
        link?.invalidate()
        engine.stop()
        scopedURL?.stopAccessingSecurityScopedResource()
    }

    // MARK: Audio engine
    private func configureAudio() {
        let session = AVAudioSession.sharedInstance()
        try? session.setCategory(.playback, mode: .default)
        try? session.setActive(true)

        let format = AVAudioFormat(standardFormatWithSampleRate: 44100, channels: 2)!   // float32 planar
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

    // MARK: Playlist
    func openFiles(_ urls: [URL]) {
        guard !urls.isEmpty else { return }
        playlist = urls
        index = 0
        loadCurrent(autoplay: true)
    }

    private func loadCurrent(autoplay: Bool) {
        scopedURL?.stopAccessingSecurityScopedResource()
        let url = playlist[index]
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

        let img = info.coverArt.flatMap { UIImage(data: $0) }
        cover = img
        backdrop = img.flatMap { Backdrop.make(from: $0) }
        coverToken += 1

        if autoplay { core.play(); isPlaying = true } else { isPlaying = false }
        updateNowPlaying()
    }

    // MARK: Transport
    func togglePlay() {
        if playlist.isEmpty { return }
        if isPlaying {
            core.pause(); isPlaying = false
        } else {
            if core.ended { core.seekToSeconds(0) }
            core.play(); isPlaying = true
        }
        updateNowPlaying()
    }

    func play()  { if !isPlaying { togglePlay() } }
    func pause() { if isPlaying { togglePlay() } }

    func next() {
        if index + 1 < playlist.count { index += 1; loadCurrent(autoplay: true) }
        else { core.pause(); core.seekToSeconds(0); isPlaying = false; position = 0; updateNowPlaying() }
    }

    func previous() {
        if core.position > 3 || index == 0 { core.seekToSeconds(0) }
        else { index -= 1; loadCurrent(autoplay: true) }
    }

    func seek(_ seconds: Double) {
        core.seekToSeconds(seconds)
        position = seconds
        updateNowPlaying()
    }

    // MARK: 60 Hz tick
    @objc private func tick() {
        let bass = core.copySpectrum(&bandBuffer, count: bandBuffer.count)
        spectrum.bands = bandBuffer
        spectrum.bass = bass

        let p = core.position
        if abs(p - position) > 0.03 { position = p }
        if isPlaying && core.ended { next() }              // tự chuyển bài / dừng cuối danh sách
    }

    // MARK: Lock screen / Control Center
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
