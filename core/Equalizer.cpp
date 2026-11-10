#include "Equalizer.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

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

// v1.1: cập nhật sample rate và tính lại hệ số biquad ngay lập tức
void Equalizer::setSampleRate(int sr) {
    if (sr <= 0) return;
    sr_ = sr;
    updateCoeffs();
    // Đánh dấu version để process() không tính lại thừa
    applied_ = version_.load();
}

// v1.1: lưu preset tùy chỉnh ra tệp JSON
bool Equalizer::savePreset(const std::string& path, const std::string& name) const {
    FILE* f = std::fopen(path.c_str(), "w");
    if (!f) return false;

    std::fprintf(f, "{\n");
    std::fprintf(f, "  \"version\": 1,\n");
    std::fprintf(f, "  \"name\": \"%s\",\n", name.c_str());
    std::fprintf(f, "  \"enabled\": %s,\n", enabled_.load() ? "true" : "false");
    std::fprintf(f, "  \"gains\": [");
    for (int i = 0; i < kBands; ++i) {
        std::fprintf(f, "%.4f", gains_[i].load());
        if (i < kBands - 1) std::fprintf(f, ", ");
    }
    std::fprintf(f, "]\n");
    std::fprintf(f, "}\n");

    std::fclose(f);
    return true;
}

// v1.1: đọc preset từ tệp JSON (parser thủ công, không phụ thuộc lib ngoài)
bool Equalizer::loadPreset(const std::string& path) {
    FILE* f = std::fopen(path.c_str(), "r");
    if (!f) return false;

    // Đọc toàn bộ nội dung vào chuỗi
    std::string content;
    {
        char buf[256];
        while (std::fgets(buf, sizeof buf, f)) content += buf;
    }
    std::fclose(f);

    // --- Parser JSON đơn giản: trích xuất từng trường theo tên key ---
    auto findValue = [&](const std::string& key) -> std::string {
        const std::string search = "\"" + key + "\"";
        auto pos = content.find(search);
        if (pos == std::string::npos) return {};
        pos = content.find(':', pos + search.size());
        if (pos == std::string::npos) return {};
        ++pos;
        while (pos < content.size() && (content[pos] == ' ' || content[pos] == '\t')) ++pos;
        size_t end = pos;
        while (end < content.size() && content[end] != ',' && content[end] != '\n' && content[end] != '}') ++end;
        std::string val = content.substr(pos, end - pos);
        // Xóa dấu ngoặc kép nếu có
        if (!val.empty() && val.front() == '"') {
            val.erase(0, 1);
            auto q = val.find('"');
            if (q != std::string::npos) val.erase(q);
        }
        // Xóa khoảng trắng cuối
        while (!val.empty() && (val.back() == ' ' || val.back() == '\r')) val.pop_back();
        return val;
    };

    // name
    customPresetName_ = findValue("name");

    // enabled
    {
        std::string en = findValue("enabled");
        if (en == "true")  enabled_ = true;
        if (en == "false") enabled_ = false;
    }

    // gains: tìm mảng [...] sau key "gains"
    {
        auto pos = content.find("\"gains\"");
        if (pos != std::string::npos) {
            pos = content.find('[', pos);
            if (pos != std::string::npos) {
                auto endArr = content.find(']', pos);
                if (endArr != std::string::npos) {
                    std::string arr = content.substr(pos + 1, endArr - pos - 1);
                    // Phân tách theo dấu phẩy
                    int band = 0;
                    size_t cur = 0;
                    while (band < kBands && cur < arr.size()) {
                        while (cur < arr.size() && (arr[cur] == ' ' || arr[cur] == '\t' || arr[cur] == '\n' || arr[cur] == '\r')) ++cur;
                        if (cur >= arr.size()) break;
                        size_t next = arr.find(',', cur);
                        std::string token = (next == std::string::npos) ? arr.substr(cur) : arr.substr(cur, next - cur);
                        try {
                            float db = std::stof(token);
                            gains_[band].store(std::clamp(db, -12.f, 12.f));
                        } catch (...) {}
                        ++band;
                        cur = (next == std::string::npos) ? arr.size() : next + 1;
                    }
                }
            }
        }
    }

    version_.fetch_add(1);   // báo cho process() tính lại hệ số
    return true;
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
