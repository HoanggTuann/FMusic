#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

// Giải mã ảnh bìa (JPEG/PNG/BMP...) bằng libavcodec và scale về RGBA bằng libswscale.
// Ảnh được thu nhỏ để cạnh dài <= maxSide (không phóng to). Trả về false nếu decoder không có trong build.
bool decodeCoverArt(int codecId, const uint8_t* data, size_t size, int maxSide,
                    std::vector<uint8_t>& rgba, int& outW, int& outH);
