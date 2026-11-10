#pragma once
#include <mutex>
#include <vector>
#include "FFT.h"

// Nhận PCM stereo float, giữ cửa sổ mono gần nhất, tính phổ theo dải log (40 Hz – 16 kHz)
// và trả về giá trị 0..1 đã làm mượt (attack nhanh / release chậm) cho UI.
// v1.1: hỗ trợ setSampleRate() khi đổi tần số lấy mẫu runtime.
class SpectrumAnalyzer {
public:
    SpectrumAnalyzer(int fftSize = 4096, int bands = 64, int sampleRate = 44100);
    void  push(const float* stereo, size_t frames);   // gọi từ audio thread
    void  reset();                                    // xoá cửa sổ mẫu (không xoá smoothing)
    float compute(float* out);                        // gọi từ UI thread, ~60 Hz. Trả về mức bass 0..1
    int   bands() const { return bands_; }
    void  setSampleRate(int sr);

private:
    int n_, bands_, sr_;
    FFT fft_;
    std::mutex m_;
    std::vector<float> ring_;
    size_t pos_ = 0;
    std::vector<float> window_, re_, im_, smooth_;
    std::vector<double> edges_;
};
