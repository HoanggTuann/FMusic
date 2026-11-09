#include "Player.h"
#include <algorithm>
#include <chrono>
#include <cstring>

Player::Player()
    : ring_(size_t(kSampleRate) * 2 * 2),          // ~2 giây PCM
      eq_(kSampleRate),
      analyzer_(4096, kBands, kSampleRate) {}

Player::~Player() { stopThread(); }

bool Player::load(const std::string& path, std::string* err) {
    stopThread();
    playing_ = false;
    auto dec = std::make_unique<AudioDecoder>();
    if (!dec->open(path, err)) {
        dec_.reset();
        info_ = TrackInfo();
        cover_.clear();
        return false;
    }
    dec_ = std::move(dec);
    info_ = dec_->info();
    cover_ = dec_->coverArt();
    ring_.clear();
    analyzer_.reset();
    eof_ = false; ended_ = false; seekReq_ = -1.0; base_ = 0.0; played_ = 0;
    startThread();
    return true;
}

void Player::startThread() {
    quit_ = false;
    th_ = std::thread(&Player::decodeLoop, this);
}

void Player::stopThread() {
    if (th_.joinable()) { quit_ = true; th_.join(); }
    quit_ = false;
}

void Player::play()  { if (dec_) playing_ = true; }
void Player::pause() { playing_ = false; }

void Player::seek(double s) {
    if (!dec_) return;
    const double hi = info_.duration > 0 ? info_.duration : 1e9;
    s = std::clamp(s, 0.0, hi);
    base_ = s; played_ = 0; ended_ = false;         // UI thấy vị trí mới ngay lập tức
    seekReq_ = s;                                   // decode thread sẽ thực hiện seek thật
}

double Player::position() const {
    double p = base_.load() + double(played_.load()) / kSampleRate;
    if (info_.duration > 0) p = std::min(p, info_.duration);
    return p;
}

void Player::decodeLoop() {
    using namespace std::chrono_literals;
    std::vector<float> chunk;
    size_t off = 0;
    while (!quit_) {
        const double s = seekReq_.exchange(-1.0);
        if (s >= 0.0) {
            dec_->seek(s);
            ring_.clear();
            analyzer_.reset();
            chunk.clear(); off = 0;
            base_ = s; played_ = 0;
            eof_ = false; ended_ = false;
        }
        if (eof_) { std::this_thread::sleep_for(10ms); continue; }

        if (off >= chunk.size()) {
            off = 0;
            if (dec_->readFrames(chunk) <= 0) { chunk.clear(); eof_ = true; continue; }
        }
        size_t room = ring_.writable() & ~size_t(1);          // luôn ghi theo cặp L/R
        const size_t n = std::min(room, chunk.size() - off);
        if (n > 0) off += ring_.write(chunk.data() + off, n);
        if (off < chunk.size()) std::this_thread::sleep_for(5ms);   // ring đầy -> chờ
    }
}

void Player::pull(float* dst, size_t frames) {
    if (!playing_) { std::memset(dst, 0, frames * 2 * sizeof(float)); return; }
    const size_t got = ring_.read(dst, frames * 2) / 2;
    if (got < frames) {
        std::memset(dst + got * 2, 0, (frames - got) * 2 * sizeof(float));
        if (eof_) ended_ = true;                              // eof + ring cạn = hết bài
    }
    if (got) {
        eq_.process(dst, got);                                // EQ trước khi phân tích phổ -> thanh sóng phản ánh EQ
        analyzer_.push(dst, got);
        played_ += got;
    }
}

float Player::spectrum(float* bands) {
    if (!playing_) analyzer_.reset();                         // tạm dừng -> thanh sóng hạ dần về 0
    return analyzer_.compute(bands);
}
