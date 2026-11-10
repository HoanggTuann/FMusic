#pragma once
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <string>

// EQ đồ hoạ 10 băng tần (31 Hz … 16 kHz), mỗi băng là 1 bộ lọc peaking biquad (RBJ), Q ≈ 1.41 (1 octave).
// setGain()/setEnabled() gọi từ UI thread; process() gọi từ audio thread (lock-free).
// v1.1: thêm setSampleRate() để hỗ trợ sample rate động, và savePreset()/loadPreset() cho preset tùy chỉnh.
class Equalizer {
public:
    static constexpr int kBands = 10;
    static constexpr int kPresetCount = 9;
    static const float kFreqs[kBands];

    static const char* presetName(int idx);
    static const float* presetGains(int idx);          // kBands giá trị dB

    explicit Equalizer(int sampleRate = 44100);

    void  setEnabled(bool on) { enabled_ = on; }
    bool  enabled() const { return enabled_; }
    void  setGain(int band, float db);                  // -12 … +12 dB
    float gain(int band) const;
    void  applyPreset(int idx);

    // v1.1: cập nhật sample rate và tính lại hệ số biquad
    void  setSampleRate(int sr);

    // v1.1: lưu / đọc preset tùy chỉnh dạng JSON
    bool savePreset(const std::string& path, const std::string& name) const;
    bool loadPreset(const std::string& path);

    // Tên của preset đã tải gần nhất (rỗng nếu chưa tải preset nào)
    const std::string& presetName() const { return customPresetName_; }

    // Xử lý tại chỗ PCM stereo interleaved float.
    void  process(float* stereo, size_t frames);

private:
    struct Biquad {
        float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
        float z1[2] = {0, 0}, z2[2] = {0, 0};
        bool active = false;
    };
    void updateCoeffs();

    int sr_;
    std::atomic<bool> enabled_{false};
    std::atomic<float> gains_[kBands];
    std::atomic<uint32_t> version_{1};
    uint32_t applied_ = 0;
    Biquad bq_[kBands];
    float preamp_ = 1.f;
    std::string customPresetName_;   // v1.1: tên preset tùy chỉnh đã tải
};
