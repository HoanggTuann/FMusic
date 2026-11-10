#include "PlaylistFile.h"
#include <cstdio>
#include <string>

// ─── save ────────────────────────────────────────────────────────────────────
// Ghi header phiên bản và một đường dẫn tuyệt đối mỗi dòng.
bool PlaylistFile::save(const std::string& path, const std::vector<std::string>& paths) {
    FILE* f = std::fopen(path.c_str(), "w");
    if (!f) return false;

    std::fprintf(f, "%s\n", kHeader);
    for (const auto& p : paths) {
        // Bỏ qua các mục rỗng
        if (!p.empty()) std::fprintf(f, "%s\n", p.c_str());
    }

    std::fclose(f);
    return true;
}

// ─── load ────────────────────────────────────────────────────────────────────
// Đọc tệp dòng-theo-dòng; bỏ qua dòng bắt đầu bằng '#' (chú thích) và dòng trắng.
bool PlaylistFile::load(const std::string& path, std::vector<std::string>& outPaths) {
    FILE* f = std::fopen(path.c_str(), "r");
    if (!f) return false;

    outPaths.clear();
    char buf[4096];
    while (std::fgets(buf, sizeof buf, f)) {
        std::string line(buf);
        // Xóa ký tự xuống dòng cuối
        while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) line.pop_back();
        // Bỏ qua dòng trắng và dòng chú thích
        if (line.empty() || line.front() == '#') continue;
        outPaths.push_back(line);
    }

    std::fclose(f);
    return true;
}
