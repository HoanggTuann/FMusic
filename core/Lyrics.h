#pragma once
#include <string>
#include <vector>

struct LyricLine {
    double time = -1.0;      // giây; -1 nếu lời không có mốc thời gian
    std::string text;        // UTF-8
};

// Phân tích lời bài hát: LRC có mốc thời gian ([mm:ss.xx], nhiều mốc / dòng, [offset:ms]) hoặc văn bản thường.
class Lyrics {
public:
    bool parse(const std::string& text);                 // true nếu có ít nhất 1 dòng
    void clear() { lines_.clear(); synced_ = false; }
    bool empty() const { return lines_.empty(); }
    bool synced() const { return synced_; }
    const std::vector<LyricLine>& lines() const { return lines_; }
    int  indexAt(double seconds) const;                  // dòng đang hát; -1 nếu chưa tới dòng đầu / lời không có mốc
private:
    std::vector<LyricLine> lines_;
    bool synced_ = false;
};
