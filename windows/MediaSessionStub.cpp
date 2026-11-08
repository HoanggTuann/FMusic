// Bản dự phòng khi toolchain không có header WinRT/SMTC: FMusic dùng phím media toàn cục (RegisterHotKey) thay thế.
#include "MediaSession.h"

struct MediaSession::Impl {};
MediaSession::MediaSession() : d(new Impl) {}
MediaSession::~MediaSession() = default;
bool MediaSession::init(void*, MediaCallbacks) { return false; }
bool MediaSession::available() const { return false; }
void MediaSession::setMetadata(const std::string&, const std::string&) {}
void MediaSession::setPlaybackState(int) {}
