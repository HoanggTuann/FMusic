#include "FFT.h"
#include <cmath>
#include <utility>

static constexpr double kPi = 3.14159265358979323846;

FFT::FFT(int n) : n_(n), rev_(n), cos_(n / 2), sin_(n / 2) {
    int bits = 0;
    while ((1 << bits) < n) ++bits;
    rev_[0] = 0;
    for (int i = 1; i < n; ++i) rev_[i] = (rev_[i >> 1] >> 1) | ((i & 1) << (bits - 1));
    for (int i = 0; i < n / 2; ++i) {
        const double a = -2.0 * kPi * i / n;
        cos_[i] = float(std::cos(a));
        sin_[i] = float(std::sin(a));
    }
}

void FFT::forward(float* re, float* im) const {
    for (int i = 0; i < n_; ++i) {
        const int j = rev_[i];
        if (j > i) { std::swap(re[i], re[j]); std::swap(im[i], im[j]); }
    }
    for (int size = 2; size <= n_; size <<= 1) {
        const int half = size / 2, step = n_ / size;
        for (int i = 0; i < n_; i += size) {
            for (int j = 0, k = 0; j < half; ++j, k += step) {
                const int a = i + j, b = a + half;
                const float wr = cos_[k], wi = sin_[k];
                const float tr = re[b] * wr - im[b] * wi;
                const float ti = re[b] * wi + im[b] * wr;
                re[b] = re[a] - tr;  im[b] = im[a] - ti;
                re[a] += tr;         im[a] += ti;
            }
        }
    }
}
