#pragma once
#include <algorithm>
#include <cstring>
#include <mutex>
#include <vector>

// FIFO float (interleaved stereo). 1 producer (decode thread) + 1 consumer (audio callback).
// Dùng mutex rất ngắn (chỉ memcpy) nên đơn giản và an toàn hơn lock-free khi cần clear() lúc seek.
class RingBuffer {
public:
    explicit RingBuffer(size_t capacity) : buf_(capacity) {}

    size_t write(const float* src, size_t n) {
        std::lock_guard<std::mutex> lk(m_);
        const size_t cap = buf_.size();
        n = std::min(n, cap - size_);
        if (n == 0) return 0;
        const size_t w = (head_ + size_) % cap;
        const size_t first = std::min(n, cap - w);
        std::memcpy(&buf_[w], src, first * sizeof(float));
        if (n > first) std::memcpy(&buf_[0], src + first, (n - first) * sizeof(float));
        size_ += n;
        return n;
    }

    size_t read(float* dst, size_t n) {
        std::lock_guard<std::mutex> lk(m_);
        const size_t cap = buf_.size();
        n = std::min(n, size_);
        if (n == 0) return 0;
        const size_t first = std::min(n, cap - head_);
        std::memcpy(dst, &buf_[head_], first * sizeof(float));
        if (n > first) std::memcpy(dst + first, &buf_[0], (n - first) * sizeof(float));
        head_ = (head_ + n) % cap;
        size_ -= n;
        return n;
    }

    size_t writable() const { std::lock_guard<std::mutex> lk(m_); return buf_.size() - size_; }
    size_t readable() const { std::lock_guard<std::mutex> lk(m_); return size_; }
    void clear() { std::lock_guard<std::mutex> lk(m_); head_ = 0; size_ = 0; }

private:
    mutable std::mutex m_;
    std::vector<float> buf_;
    size_t head_ = 0, size_ = 0;
};
