#pragma once
#include <string>
#include <vector>

// Cấu trúc dữ liệu cho một preset EQ tùy chỉnh của người dùng.
// Dùng FEQPreset::load() / FEQPreset::save() để đọc/ghi tệp JSON.
struct FEQPreset {
    int version = 1;
    std::string name;
    bool enabled = true;
    std::vector<float> gains;   // 10 băng tần, đơn vị dB (-12 … +12)

    // Đọc preset từ tệp JSON (trả về false nếu thất bại)
    static bool load(const std::string& path, FEQPreset& out);

    // Ghi preset ra tệp JSON (trả về false nếu thất bại)
    static bool save(const std::string& path, const FEQPreset& in);
};
