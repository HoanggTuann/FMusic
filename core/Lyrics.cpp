#include "Lyrics.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <sstream>

static std::string trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
    while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
    return s.substr(a, b - a);
}

static bool allDigits(const std::string& s) {
    return !s.empty() && std::all_of(s.begin(), s.end(), [](char c) { return std::isdigit(static_cast<unsigned char>(c)) != 0; });
}

// "mm:ss", "mm:ss.xx", "mm:ss,xx"
static bool parseTimeTag(const std::string& tag, double& out) {
    const size_t colon = tag.find(':');
    if (colon == std::string::npos) return false;
    const std::string m = tag.substr(0, colon);
    std::string s = tag.substr(colon + 1);
    if (!allDigits(m)) return false;
    std::string frac;
    const size_t dot = s.find_first_of(".,");
    if (dot != std::string::npos) { frac = s.substr(dot + 1); s = s.substr(0, dot); }
    if (!allDigits(s) || (!frac.empty() && !allDigits(frac))) return false;
    double t = std::atoi(m.c_str()) * 60.0 + std::atoi(s.c_str());
    if (!frac.empty()) t += std::atoi(frac.c_str()) / std::pow(10.0, double(frac.size()));
    out = t;
    return true;
}

// Bỏ thẻ từng từ của LRC mở rộng: <mm:ss.xx>
static std::string stripWordTags(std::string s) {
    size_t i = 0;
    while ((i = s.find('<', i)) != std::string::npos) {
        const size_t j = s.find('>', i);
        if (j == std::string::npos) break;
        bool ok = j > i + 1;
        for (size_t k = i + 1; k < j && ok; ++k)
            ok = std::isdigit(static_cast<unsigned char>(s[k])) || s[k] == ':' || s[k] == '.';
        if (ok) s.erase(i, j - i + 1); else i = j + 1;
    }
    return s;
}

bool Lyrics::parse(const std::string& textIn) {
    clear();
    std::string text = textIn;
    if (text.size() >= 3 && (unsigned char)text[0] == 0xEF && (unsigned char)text[1] == 0xBB && (unsigned char)text[2] == 0xBF)
        text.erase(0, 3);                                        // bỏ BOM UTF-8

    std::vector<LyricLine> timed;
    std::vector<std::string> plain;
    double offsetMs = 0.0;

    std::istringstream in(text);
    std::string raw;
    while (std::getline(in, raw)) {
        std::string line = trim(raw);
        size_t i = 0;
        std::vector<double> times;
        bool hadTag = false;
        while (i < line.size() && line[i] == '[') {
            const size_t j = line.find(']', i);
            if (j == std::string::npos) break;
            const std::string tag = line.substr(i + 1, j - i - 1);
            double t;
            if (parseTimeTag(tag, t)) times.push_back(t);
            else if (tag.compare(0, 7, "offset:") == 0) offsetMs = std::atof(tag.c_str() + 7);
            hadTag = true;
            i = j + 1;
        }
        const std::string body = trim(stripWordTags(line.substr(i)));
        if (!times.empty()) {
            for (double t : times) timed.push_back({t, body});
        } else if (!hadTag) {
            plain.push_back(line);                               // dòng thường (không thẻ)
        }                                                        // dòng chỉ có thẻ meta ([ar:], [ti:]...) -> bỏ
    }

    if (!timed.empty()) {
        synced_ = true;
        for (auto& l : timed) l.time = std::max(0.0, l.time - offsetMs / 1000.0);
        std::stable_sort(timed.begin(), timed.end(), [](const LyricLine& a, const LyricLine& b) { return a.time < b.time; });
        lines_ = std::move(timed);
    } else {
        while (!plain.empty() && plain.back().empty()) plain.pop_back();
        size_t start = 0;
        while (start < plain.size() && plain[start].empty()) ++start;
        for (size_t k = start; k < plain.size(); ++k) lines_.push_back({-1.0, plain[k]});
    }
    return !lines_.empty();
}

int Lyrics::indexAt(double t) const {
    if (!synced_ || lines_.empty()) return -1;
    int lo = 0, hi = int(lines_.size()) - 1, ans = -1;
    while (lo <= hi) {
        const int mid = (lo + hi) / 2;
        if (lines_[mid].time <= t) { ans = mid; lo = mid + 1; } else hi = mid - 1;
    }
    return ans;
}
