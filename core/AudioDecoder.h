#pragma once
#include <cstdint>
#include <string>
#include <vector>

struct AVFormatContext; struct AVCodecContext; struct SwrContext; struct AVPacket; struct AVFrame;

struct TrackInfo {
    std::string title, artist, album;
    std::string lyrics;         // lời nhúng trong tag (ID3 USLT / Vorbis LYRICS / ©lyr), có thể rỗng
    double duration = 0.0;      // giây
    int sampleRate = 0;         // của file gốc
    int channels = 0;
    int coverCodecId = 0;       // AVCodecID của ảnh bìa (mjpeg/png/...), 0 nếu không có
};

// Mở + demux + decode 1 file âm thanh bất kỳ FFmpeg hỗ trợ, xuất PCM float32 interleaved stereo.
// Output sample rate có thể cấu hình (mặc định 44100 Hz cho tương thích ngược).
class AudioDecoder {
public:
    static constexpr int kOutRate = 44100;   // giữ lại để tương thích; dùng outRate_ khi thực thi
    static constexpr int kOutChannels = 2;

    explicit AudioDecoder(int outRate = 44100);
    ~AudioDecoder();
    AudioDecoder(const AudioDecoder&) = delete;
    AudioDecoder& operator=(const AudioDecoder&) = delete;

    bool open(const std::string& utf8Path, std::string* err = nullptr);
    // Đọc nhanh metadata + thời lượng mà không mở decoder (dùng cho danh sách phát). Thread-safe.
    static bool probe(const std::string& utf8Path, TrackInfo& out);
    void close();

    // Giải mã thêm 1 khối PCM. Trả về số frame (>0), 0 = hết file, <0 = lỗi.
    int  readFrames(std::vector<float>& out);
    bool seek(double seconds);

    const TrackInfo& info() const { return info_; }
    const std::vector<uint8_t>& coverArt() const { return cover_; }   // bytes ảnh gốc (JPEG/PNG...)

    // Trả về sample rate đầu ra thực tế đang dùng
    int outRate() const { return outRate_; }

private:
    bool convert(const AVFrame* f, std::vector<float>& out);
    void flushResampler(std::vector<float>& out);

    int  outRate_;               // sample rate đầu ra cấu hình được (mặc định 44100)

    AVFormatContext* fmt_   = nullptr;
    AVCodecContext*  codec_ = nullptr;
    SwrContext*      swr_   = nullptr;
    AVPacket*        pkt_   = nullptr;
    AVFrame*         frame_ = nullptr;
    int  streamIdx_ = -1;
    bool flushed_   = false;
    TrackInfo info_;
    std::vector<uint8_t> cover_;
};
