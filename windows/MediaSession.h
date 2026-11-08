#pragma once
#include <functional>
#include <memory>
#include <string>

// Khai báo với Windows rằng FMusic đang phát media (System Media Transport Controls):
// hiện tên bài/nghệ sĩ trong bảng âm lượng + màn hình khoá, và nhận phím Play/Pause/Next/Previous trên bàn phím.
// Các callback có thể được gọi từ thread khác của Windows -> bên nhận phải tự chuyển về UI thread.
struct MediaCallbacks {
    std::function<void()> play, pause, next, prev;
};

class MediaSession {
public:
    MediaSession();
    ~MediaSession();
    bool init(void* hwnd, MediaCallbacks cb);               // false nếu SMTC không dùng được
    bool available() const;
    void setMetadata(const std::string& utf8Title, const std::string& utf8Artist);
    void setPlaybackState(int state);                        // 0 dừng, 1 tạm dừng, 2 đang phát
private:
    struct Impl;
    std::unique_ptr<Impl> d;
};
