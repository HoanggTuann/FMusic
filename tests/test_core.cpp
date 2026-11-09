// Kiểm thử Equalizer + Lyrics (không cần FFmpeg).
#include <cmath>
#include <cstdio>
#include <vector>
#include "../core/Equalizer.h"
#include "../core/Lyrics.h"

static int fails = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL line %d: %s\n", __LINE__, #c); ++fails; } } while (0)

static double rmsAfter(Equalizer& eq, double freq) {
    const int sr = 44100, N = sr;
    std::vector<float> pcm(N * 2);
    for (int i = 0; i < N; ++i) pcm[2*i] = pcm[2*i+1] = 0.1f * float(std::sin(2 * 3.14159265358979 * freq * i / sr));
    eq.process(pcm.data(), N);
    double s = 0; int n = 0;
    for (int i = N / 2; i < N; ++i, ++n) s += pcm[2*i] * pcm[2*i];
    return std::sqrt(s / n);
}

int main() {
    const double ref = 0.1 / std::sqrt(2.0);
    { Equalizer eq; eq.setEnabled(true); eq.setGain(5, 12.f);                      // +12 dB @1k, preamp -6 dB
      double a = rmsAfter(eq, 1000) / ref; std::printf("1k @+12dB net gain x%.2f (mong ~2.0)\n", a);
      CHECK(a > 1.8 && a < 2.2); }
    { Equalizer eq; eq.setEnabled(true); eq.setGain(5, 12.f);
      double a = rmsAfter(eq, 100) / ref; std::printf("100 Hz net gain x%.2f (mong ~0.5, chỉ preamp)\n", a);
      CHECK(a > 0.4 && a < 0.65); }
    { Equalizer eq; eq.setEnabled(true); eq.setGain(5, -12.f);
      double a = rmsAfter(eq, 1000) / ref; std::printf("1k @-12dB gain x%.2f (mong ~0.25)\n", a);
      CHECK(a > 0.2 && a < 0.3); }
    { Equalizer eq; double a = rmsAfter(eq, 1000) / ref; CHECK(a > 0.99 && a < 1.01); }   // tắt = bypass
    { Equalizer eq; eq.applyPreset(1); CHECK(eq.gain(0) == 6.f); CHECK(std::string(Equalizer::presetName(1)) == "Bass"); }

    Lyrics ly;
    CHECK(ly.parse("\xEF\xBB\xBF[ar:Ca si]\n[ti:Bai hat]\n[offset:500]\n[00:12.00]Dong mot\n[00:05.50][01:00.00]Dong <00:06.00>hai\n\n[00:30]\n"));
    CHECK(ly.synced());
    CHECK(ly.lines().size() == 4);
    CHECK(ly.lines()[0].text == "Dong hai");                       // 5.5 - 0.5 = 5.0 s
    CHECK(std::fabs(ly.lines()[0].time - 5.0) < 1e-6);
    CHECK(ly.indexAt(1.0) == -1);
    CHECK(ly.indexAt(5.0) == 0);
    CHECK(ly.indexAt(11.6) == 1);                                    // 12.0-0.5 = 11.5
    CHECK(ly.indexAt(31.0) == 2 && ly.lines()[2].text.empty());      // dòng trống [00:30]
    CHECK(ly.indexAt(500) == 3);
    CHECK(ly.parse("Loi 1\nLoi 2\n\nLoi 3\n\n") && !ly.synced() && ly.lines().size() == 4 && ly.indexAt(10) == -1);
    CHECK(!ly.parse("[ar:x]\n[ti:y]\n") && ly.empty());
    std::printf(fails ? "CO LOI\n" : "TAT CA OK\n");
    return fails ? 1 : 0;
}
