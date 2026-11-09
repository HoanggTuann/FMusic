#include "Equalizer.h"
#include <algorithm>
#include <cmath>

static constexpr double kPi = 3.14159265358979323846;

const float Equalizer::kFreqs[Equalizer::kBands] = {31, 62, 125, 250, 500, 1000, 2000, 4000, 8000, 16000};

static const char* kNames[Equalizer::kPresetCount] = {
    "Flat", "Bass", "Treble", "Vocal", "Rock", "Pop", "Jazz", "Classical", "Electronic"};

static const float kPresets[Equalizer::kPresetCount][Equalizer::kBands] = {
    {  0,  0,  0,  0,  0,  0,  0,  0,  0,  0},   // Flat
    {  6,  5,  4,  2,  0,  0,  0,  0,  0,  0},   // Bass
    {  0,  0,  0,  0,  0,  1,  2,  4,  5,  6},   // Treble
    { -2, -3, -2,  1,  3,  3,  2,  1,  0, -1},   // Vocal
    {  4,  3,  2,  0, -1, -1,  0,  2,  3,  4},   // Rock
    { -1,  1,  3,  4,  3,  0, -1, -1,  1,  2},   // Pop
    {  3,  2,  1,  2, -2, -2,  0,  1,  2,  3},   // Jazz
    {  4,  3,  2,  2, -1, -1,  0,  2,  3,  4},   // Classical
    {  4,  3,  0, -2, -2,  0,  1,  3,  4,  4},   // Electronic
};

const char* Equalizer::presetName(int i) { return (i >= 0 && i < kPresetCount) ? kNames[i] : ""; }
const float* Equalizer::presetGains(int i) { return kPresets[std::clamp(i, 0, kPresetCount - 1)]; }

Equalizer::Equalizer(int sampleRate) : sr_(sampleRate) {
    for (auto& g : gains_) g.store(0.f);
}

void Equalizer::setGain(int band, float db) {
    if (band < 0 || band >= kBands) return;
    gains_[band].store(std::clamp(db, -12.f, 12.f));
    version_.fetch_add(1);
}

float Equalizer::gain(int band) const {
    return (band >= 0 && band < kBands) ? gains_[band].load() : 0.f;
}

void Equalizer::applyPreset(int idx) {
    const float* g = presetGains(idx);
    for (int i = 0; i < kBands; ++i) gains_[i].store(g[i]);
    version_.fetch_add(1);
}

void Equalizer::updateCoeffs() {
    float maxPos = 0.f;
    for (int i = 0; i < kBands; ++i) {
        const float db = gains_[i].load();
        maxPos = std::max(maxPos, db);
        Biquad& b = bq_[i];
        b.active = std::fabs(db) > 0.01f;
        if (!b.active) continue;
        const double A = std::pow(10.0, db / 40.0);
        const double w0 = 2.0 * kPi * std::min<double>(kFreqs[i], sr_ * 0.45) / sr_;
        const double alpha = std::sin(w0) / (2.0 * 1.41);
        const double a0 = 1.0 + alpha / A;
        b.b0 = float((1.0 + alpha * A) / a0);
        b.b1 = float((-2.0 * std::cos(w0)) / a0);
        b.b2 = float((1.0 - alpha * A) / a0);
        b.a1 = b.b1;
        b.a2 = float((1.0 - alpha / A) / a0);
    }
    preamp_ = std::pow(10.f, -0.5f * maxPos / 20.f);      // hạ nhẹ gain tổng khi boost để tránh clip
}

static inline float softClip(float x) {
    const float t = 0.92f, a = std::fabs(x);
    if (a <= t) return x;
    const float s = t + (1.f - t) * std::tanh((a - t) / (1.f - t));
    return x < 0 ? -s : s;
}

void Equalizer::process(float* s, size_t frames) {
    if (!enabled_.load()) return;
    const uint32_t v = version_.load();
    if (v != applied_) { updateCoeffs(); applied_ = v; }
    for (size_t f = 0; f < frames; ++f) {
        for (int ch = 0; ch < 2; ++ch) {
            float x = s[2 * f + ch] * preamp_;
            for (int i = 0; i < kBands; ++i) {
                Biquad& b = bq_[i];
                if (!b.active) continue;
                const float y = b.b0 * x + b.z1[ch];              // Direct Form II transposed
                b.z1[ch] = b.b1 * x - b.a1 * y + b.z2[ch];
                b.z2[ch] = b.b2 * x - b.a2 * y;
                x = y;
            }
            s[2 * f + ch] = softClip(x);
        }
    }
}
