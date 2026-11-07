// Kiểm tra nhanh FFT + SpectrumAnalyzer (không cần FFmpeg):  sin 1 kHz phải làm sáng đúng dải chứa 1 kHz.
#include <cmath>
#include <cstdio>
#include <vector>
#include "../core/SpectrumAnalyzer.h"

int main() {
    const int sr = 44100, bands = 64;
    SpectrumAnalyzer an(4096, bands, sr);
    std::vector<float> pcm(4096 * 2);
    for (int i = 0; i < 4096; ++i) {
        float s = 0.5f * std::sin(2.0 * 3.14159265358979 * 1000.0 * i / sr);
        pcm[2 * i] = pcm[2 * i + 1] = s;
    }
    an.push(pcm.data(), 4096);
    std::vector<float> out(bands);
    for (int k = 0; k < 20; ++k) an.compute(out.data());   // cho smoothing hội tụ
    int peak = 0;
    for (int b = 1; b < bands; ++b) if (out[b] > out[peak]) peak = b;
    const double fmax = 16000 < sr * 0.475 ? 16000 : sr * 0.475;
    const double lo = 40.0 * std::pow(fmax / 40.0, double(peak) / bands);
    const double hi = 40.0 * std::pow(fmax / 40.0, double(peak + 1) / bands);
    std::printf("peak band=%d  (%.0f-%.0f Hz)  value=%.2f\n", peak, lo, hi, out[peak]);
    return (lo <= 1000.0 && 1000.0 <= hi * 1.001) ? 0 : 1;
}
