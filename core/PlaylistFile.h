#pragma once
#include <string>
#include <vector>

// Đọc/ghi danh sách phát định dạng văn bản đơn giản của FMusic.
// Mỗi dòng là đường dẫn tuyệt đối đến tệp nhạc; dòng bắt đầu bằng '#' là chú thích.
struct PlaylistFile {
    static constexpr const char* kHeader = "#FMUSIC PLAYLIST 1.1";

    // Đọc danh sách phát từ tệp; bỏ qua dòng chú thích và dòng trắng.
    static bool load(const std::string& path, std::vector<std::string>& outPaths);

    // Ghi danh sách phát ra tệp (mỗi đường dẫn 1 dòng, có header phiên bản).
    static bool save(const std::string& path, const std::vector<std::string>& paths);
};
