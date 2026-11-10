#pragma once
#include <atomic>
#include <memory>
#include <string>
#include <thread>
#include <vector>
#include "AudioDecoder.h"
#include "Equalizer.h"
#include "RingBuffer.h"
#include "SpectrumAnalyzer.h"

// Lõi phát nhạc dùng chung cho Windows & iOS & Android.
//   decode thread:  AudioDecoder -> RingBuffer
//   audio callback: pull() lấy PCM từ RingBuffer, đồng thời đẩy vào SpectrumAnalyzer
//   UI thread:      spectrum(), position(), ...
// v1.1: thêm setSampleRate() để hỗ trợ sample rate động (44.1k, 48k, 96k...).
class Player {
public:
    static constexpr int kSampleRate = AudioDecoder::kOutRate;
    static constexpr int kBands = 64;

    Player();
    ~Player();
    Player(const Player&) = delete;
    Player& operator=(const Player&) = delete;

    bool   load(const std::string& utf8Path, std::string* err = nullptr);
    void   play();
    void   pause();
    void   seek(double seconds);

    // v1.1: thay đổi sample rate đầu ra (dừng audio -> khởi lại decoder -> phát lại)
    void   setSampleRate(int sr);
    int    sampleRate() const { return sampleRate_; }

    bool   playing() const { return playing_; }
    bool   ended() const   { return ended_; }
    double position() const;
    double duration() const { return info_.duration; }
    const TrackInfo& info() const { return info_; }
    const std::vector<uint8_t>& coverArt() const { return cover_; }

    Equalizer& eq() { return eq_; }

    // Audio thread: điền đúng `frames` frame stereo float (thiếu thì chèn im lặng).
    void   pull(float* dst, size_t frames);
    // UI thread: ghi kBands giá trị 0..1 vào `bands`, trả về mức bass 0..1.
    float  spectrum(float* bands);

private:
    void decodeLoop();
    void startThread();
    void stopThread();

    std::unique_ptr<AudioDecoder> dec_;
    RingBuffer ring_;
    Equalizer eq_;
    SpectrumAnalyzer analyzer_;
    std::thread th_;
    std::atomic<bool> quit_{false}, playing_{false}, eof_{false}, ended_{false};
    std::atomic<double> seekReq_{-1.0}, base_{0.0};
    std::atomic<uint64_t> played_{0};
    TrackInfo info_;
    std::vector<uint8_t> cover_;
    std::string currentPath_;
    int sampleRate_ = 44100;   // v1.1: sample rate hiện tại (mặc định 44100)
};
