#include "EqPreset.h"
#include <algorithm>
#include <cstdio>
#include <stdexcept>

// ─── save ───────────────────────────────────────────────────────────────────
// Ghi preset ra tệp JSON có định dạng đẹp.
bool FEQPreset::save(const std::string& path, const FEQPreset& in) {
    FILE* f = std::fopen(path.c_str(), "w");
    if (!f) return false;

    std::fprintf(f, "{\n");
    std::fprintf(f, "  \"version\": %d,\n", in.version);
    std::fprintf(f, "  \"name\": \"%s\",\n", in.name.c_str());
    std::fprintf(f, "  \"enabled\": %s,\n", in.enabled ? "true" : "false");
    std::fprintf(f, "  \"gains\": [");
    for (size_t i = 0; i < in.gains.size(); ++i) {
        std::fprintf(f, "%.4f", in.gains[i]);
        if (i + 1 < in.gains.size()) std::fprintf(f, ", ");
    }
    std::fprintf(f, "]\n");
    std::fprintf(f, "}\n");

    std::fclose(f);
    return true;
}

// ─── load ────────────────────────────────────────────────────────────────────
// Parser JSON thủ công dòng-theo-dòng; không dùng thư viện ngoài.
bool FEQPreset::load(const std::string& path, FEQPreset& out) {
    FILE* f = std::fopen(path.c_str(), "r");
    if (!f) return false;

    std::string content;
    {
        char buf[256];
        while (std::fgets(buf, sizeof buf, f)) content += buf;
    }
    std::fclose(f);

    // Helper: trả về chuỗi giá trị thô ngay sau "key": (không bao gồm dấu phẩy hoặc cuối dòng)
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
        // Bỏ dấu ngoặc kép nếu là chuỗi
        if (!val.empty() && val.front() == '"') {
            val.erase(0, 1);
            auto q = val.find('"');
            if (q != std::string::npos) val.erase(q);
        }
        // Xóa khoảng trắng cuối
        while (!val.empty() && (val.back() == ' ' || val.back() == '\r')) val.pop_back();
        return val;
    };

    // version
    {
        std::string v = findValue("version");
        if (!v.empty()) {
            try { out.version = std::stoi(v); } catch (...) {}
        }
    }

    // name
    out.name = findValue("name");

    // enabled
    {
        std::string en = findValue("enabled");
        if (en == "true")  out.enabled = true;
        else if (en == "false") out.enabled = false;
    }

    // gains: trích xuất mảng [...]
    out.gains.clear();
    {
        auto pos = content.find("\"gains\"");
        if (pos != std::string::npos) {
            pos = content.find('[', pos);
            if (pos != std::string::npos) {
                auto endArr = content.find(']', pos);
                if (endArr != std::string::npos) {
                    std::string arr = content.substr(pos + 1, endArr - pos - 1);
                    size_t cur = 0;
                    while (cur < arr.size()) {
                        // Bỏ qua khoảng trắng
                        while (cur < arr.size() && (arr[cur] == ' ' || arr[cur] == '\t' ||
                               arr[cur] == '\n' || arr[cur] == '\r')) ++cur;
                        if (cur >= arr.size()) break;
                        size_t next = arr.find(',', cur);
                        std::string token = (next == std::string::npos)
                                            ? arr.substr(cur)
                                            : arr.substr(cur, next - cur);
                        try {
                            out.gains.push_back(std::stof(token));
                        } catch (...) {}
                        cur = (next == std::string::npos) ? arr.size() : next + 1;
                    }
                }
            }
        }
    }

    return true;
}
