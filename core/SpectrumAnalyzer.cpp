#include "SpectrumAnalyzer.h"
#include <algorithm>
#include <cmath>

static constexpr double kPi = 3.14159265358979323846;

SpectrumAnalyzer::SpectrumAnalyzer(int fftSize, int bands, int sampleRate)
    : n_(fftSize), bands_(bands), sr_(sampleRate), fft_(fftSize),
      ring_(fftSize, 0.f), window_(fftSize), re_(fftSize), im_(fftSize),
      smooth_(bands, 0.f), edges_(bands + 1) {
    for (int i = 0; i < n_; ++i)                                   // cửa sổ Hann
        window_[i] = 0.5f * (1.f - float(std::cos(2.0 * kPi * i / (n_ - 1))));
    const double fmin = 40.0, fmax = std::min(16000.0, sr_ * 0.5 * 0.95);
    for (int i = 0; i <= bands_; ++i)
        edges_[i] = fmin * std::pow(fmax / fmin, double(i) / bands_);
}

void SpectrumAnalyzer::setSampleRate(int sr) {
    std::lock_guard<std::mutex> lk(m_);
    sr_ = sr;
    const double fmin = 40.0, fmax = std::min(16000.0, sr_ * 0.5 * 0.95);
    for (int i = 0; i <= bands_; ++i)
        edges_[i] = fmin * std::pow(fmax / fmin, double(i) / bands_);
}

void SpectrumAnalyzer::push(const float* s, size_t frames) {
    std::lock_guard<std::mutex> lk(m_);
    const size_t N = size_t(n_);
    for (size_t i = 0; i < frames; ++i) {
        ring_[pos_] = 0.5f * (s[2 * i] + s[2 * i + 1]);            // mix mono
        pos_ = (pos_ + 1) % N;
    }
}

void SpectrumAnalyzer::reset() {
    std::lock_guard<std::mutex> lk(m_);
    std::fill(ring_.begin(), ring_.end(), 0.f);
}

float SpectrumAnalyzer::compute(float* out) {
    {
        std::lock_guard<std::mutex> lk(m_);
        const size_t N = size_t(n_);
        for (size_t i = 0; i < N; ++i) re_[i] = ring_[(pos_ + i) % N] * window_[i];
    }
    std::fill(im_.begin(), im_.end(), 0.f);
    fft_.forward(re_.data(), im_.data());

    const double binHz = double(sr_) / n_;
    const float norm = 4.0f / float(n_);                           // sin biên độ A -> |X| = A*N/4 (Hann)
    for (int b = 0; b < bands_; ++b) {
        const double f0 = edges_[b], f1 = edges_[b + 1];
        const int k0 = std::max(1, int(f0 / binHz));
        const int k1 = std::min(std::max(k0 + 1, int(std::ceil(f1 / binHz))), n_ / 2);
        float m = 0.f;
        for (int k = k0; k < k1; ++k) m = std::max(m, std::hypot(re_[k], im_[k]));
        m *= norm;
        float db = 20.f * std::log10(m + 1e-7f);
        db += 3.f * float(std::log2(std::sqrt(f0 * f1) / 1000.0));  // bù tilt +3 dB/octave
        const float v = std::min(1.f, std::max(0.f, (db + 80.f) / 70.f));
        float& s = smooth_[b];
        s += (v - s) * (v > s ? 0.55f : 0.14f);                     // attack nhanh, release chậm
        out[b] = s;
    }
    const int nb = std::min(8, bands_);
    float bass = 0.f;
    for (int i = 0; i < nb; ++i) bass += smooth_[i];
    return bass / float(nb);
}
