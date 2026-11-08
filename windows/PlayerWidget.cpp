#include "PlayerWidget.h"

#include <QAudioDevice>
#include <QAudioFormat>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QKeyEvent>
#include <QLinearGradient>
#include <QMediaDevices>
#include <QMimeData>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QUrl>
#include <algorithm>
#include <cmath>
#include <vector>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRandomGenerator>
#include <QSettings>
#include <QWheelEvent>

#include "AudioDecoder.h"
#include "CoverArt.h"
#include "Equalizer.h"

namespace {
constexpr int    kBands = Player::kBands;
constexpr double kPi    = 3.14159265358979323846;
constexpr double kCorner = 0.045;      // bán kính bo góc = 4.5% cạnh ảnh bìa

// ---------- Box blur (trượt cửa sổ O(n)) cho ảnh ARGB32 premultiplied ----------
void boxBlurPass(QImage& img, int r, bool horiz) {
    const int w = img.width(), h = img.height();
    const int len = horiz ? w : h, lines = horiz ? h : w;
    const int stride = int(img.bytesPerLine() / 4);
    const int step = horiz ? 1 : stride, lineStep = horiz ? stride : 1;
    auto* base = reinterpret_cast<uint32_t*>(img.bits());
    std::vector<uint32_t> tmp(static_cast<size_t>(len));
    const int win = 2 * r + 1;
    for (int l = 0; l < lines; ++l) {
        uint32_t* line = base + size_t(l) * lineStep;
        auto px = [&](int i) { return line[size_t(std::clamp(i, 0, len - 1)) * step]; };
        int s0 = 0, s1 = 0, s2 = 0, s3 = 0;
        for (int i = -r; i <= r; ++i) {
            const uint32_t v = px(i);
            s0 += v & 0xFF; s1 += (v >> 8) & 0xFF; s2 += (v >> 16) & 0xFF; s3 += v >> 24;
        }
        for (int i = 0; i < len; ++i) {
            tmp[size_t(i)] = uint32_t(s0 / win) | (uint32_t(s1 / win) << 8) |
                             (uint32_t(s2 / win) << 16) | (uint32_t(s3 / win) << 24);
            const uint32_t a = px(i + r + 1), b = px(i - r);
            s0 += int(a & 0xFF) - int(b & 0xFF);
            s1 += int((a >> 8) & 0xFF) - int((b >> 8) & 0xFF);
            s2 += int((a >> 16) & 0xFF) - int((b >> 16) & 0xFF);
            s3 += int(a >> 24) - int(b >> 24);
        }
        for (int i = 0; i < len; ++i) line[size_t(i) * step] = tmp[size_t(i)];
    }
}

void boxBlur(QImage& img, int r, int passes = 3) {   // 3 lần box ≈ Gaussian
    for (int i = 0; i < passes; ++i) { boxBlurPass(img, r, true); boxBlurPass(img, r, false); }
}

// Ảnh nền ambient: thu nhỏ còn 40x40, blur, tăng bão hoà, hạ sáng nhẹ. Khi phóng to toàn màn hình sẽ rất mịn.
QImage makeBackdrop(const QImage& cover) {
    QImage s = cover.scaled(40, 40, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation)
                    .convertToFormat(QImage::Format_ARGB32_Premultiplied);
    s = s.copy((s.width() - 40) / 2, (s.height() - 40) / 2, 40, 40);
    boxBlur(s, 3, 3);
    for (int y = 0; y < s.height(); ++y)
        for (int x = 0; x < s.width(); ++x) {
            QColor c = s.pixelColor(x, y);
            float h, sat, v, a;
            c.getHsvF(&h, &sat, &v, &a);
            c.setHsvF(h < 0 ? 0 : h, std::min(1.f, sat * 1.45f), std::min(1.f, v * 0.95f), a);
            s.setPixelColor(x, y, c);
        }
    return s;
}

// ---------- Điểm + pháp tuyến ngoài trên chu vi hình chữ nhật bo góc ----------
// s = độ dài cung tính từ giữa cạnh dưới, đi qua phải -> trên -> trái.
struct PerimPt { QPointF p, n; };

PerimPt perimeterPoint(const QRectF& rc, double r, double s) {
    const double hw = rc.width() - 2 * r, hh = rc.height() - 2 * r, q = kPi * r / 2;
    const double lens[9] = { hw / 2, q, hh, q, hw, q, hh, q, hw / 2 };
    int i = 0;
    while (i < 8 && s > lens[i]) { s -= lens[i]; ++i; }
    const double u = lens[i] > 0 ? std::clamp(s / lens[i], 0.0, 1.0) : 0.0;
    auto arc = [&](QPointF c, double deg) {
        const double a = deg * kPi / 180.0;
        const QPointF n(std::cos(a), std::sin(a));
        return PerimPt{ c + n * r, n };
    };
    switch (i) {
    case 0: return { { rc.center().x() + u * hw / 2, rc.bottom() }, { 0, 1 } };
    case 1: return arc({ rc.right() - r, rc.bottom() - r }, 90 - 90 * u);
    case 2: return { { rc.right(), rc.bottom() - r - u * hh }, { 1, 0 } };
    case 3: return arc({ rc.right() - r, rc.top() + r }, -90 * u);
    case 4: return { { rc.right() - r - u * hw, rc.top() }, { 0, -1 } };
    case 5: return arc({ rc.left() + r, rc.top() + r }, -90 - 90 * u);
    case 6: return { { rc.left(), rc.top() + r + u * hh }, { -1, 0 } };
    case 7: return arc({ rc.left() + r, rc.bottom() - r }, 180 - 90 * u);
    default: return { { rc.left() + r + u * hw / 2, rc.bottom() }, { 0, 1 } };
    }
}

QString formatTime(double s) {
    const int t = std::max(0, int(s + 0.5));
    return QString("%1:%2").arg(t / 60).arg(t % 60, 2, 10, QChar('0'));
}
} // namespace
// =====================================================================================

namespace {
constexpr double kRowH = 58.0;
const char* const kFreqLabels[Equalizer::kBands] = {"31", "62", "125", "250", "500", "1k", "2k", "4k", "8k", "16k"};

QFont uiFont(int px, bool semibold = false) {
    QFont f(semibold ? "Segoe UI Semibold" : "Segoe UI");
    f.setPixelSize(px);
    return f;
}

QString fromUtf8(const std::string& s) { return QString::fromUtf8(s.c_str()); }
} // namespace

PlayerWidget::PlayerWidget(QWidget* parent) : QWidget(parent) {
    setAcceptDrops(true);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_OpaquePaintEvent);          // paintEvent luôn phủ kín toàn bộ cửa sổ

    title_  = QStringLiteral("Chưa có bài hát");
    artist_ = QStringLiteral("Kéo thả tệp nhạc vào đây hoặc nhấn O");

    // ----- Audio output (Qt Multimedia / WASAPI), float32 stereo 44.1 kHz -----
    QAudioFormat fmt;
    fmt.setSampleRate(Player::kSampleRate);
    fmt.setChannelCount(2);
    fmt.setSampleFormat(QAudioFormat::Float);
    const QAudioDevice dev = QMediaDevices::defaultAudioOutput();
    if (!dev.isFormatSupported(fmt)) qWarning("Thiết bị âm thanh không hỗ trợ float32 44.1k trực tiếp (Qt sẽ chuyển đổi).");
    sink_ = new QAudioSink(dev, fmt, this);
    sink_->setBufferSize(Player::kSampleRate / 10 * 2 * int(sizeof(float)));   // ~100 ms
    pcm_ = new PcmDevice(player_, this);
    pcm_->open(QIODevice::ReadOnly);
    sink_->start(pcm_);                          // chạy liên tục; Player tự xuất im lặng khi pause

    loadEq();

    connect(&tick_, &QTimer::timeout, this, [this] { onTick(); });
    tick_.start(100);
    connect(&frame_, &QTimer::timeout, this, [this] { update(); });
    frame_.start(16);
    clock_.start();

    setCover(QImage());
}

PlayerWidget::~PlayerWidget() {
    if (sink_) sink_->stop();    // dừng audio thread trước khi Player bị huỷ
}

// ------------------------------- danh sách phát -------------------------------

void PlayerWidget::addTracks(const QStringList& files) {
    for (const QString& f : files) {
        Track t;
        t.path = f;
        t.title = QFileInfo(f).completeBaseName();      // tạm dùng tên tệp, metadata thật được đọc nền ở onTick()
        playlist_.push_back(t);
    }
}

void PlayerWidget::openFiles(const QStringList& files) {
    if (files.isEmpty()) return;
    playlist_.clear();
    probeCursor_ = 0;
    queueScroll_ = 0;
    addTracks(files);
    loadIndex(0, true);
}

void PlayerWidget::addFiles(const QStringList& files) {
    if (files.isEmpty()) return;
    const bool wasEmpty = playlist_.isEmpty();
    addTracks(files);
    if (wasEmpty) loadIndex(0, true);
}

void PlayerWidget::removeTrack(int i) {
    if (i < 0 || i >= playlist_.size()) return;
    if (playlist_.size() == 1) { clearPlaylist(); return; }
    const bool wasPlaying = player_.playing();
    playlist_.remove(i);
    if (i < probeCursor_) --probeCursor_;
    if (i < index_) { --index_; }
    else if (i == index_) {
        index_ = std::min(index_, int(playlist_.size()) - 1);
        loadIndex(index_, wasPlaying);
    }
}

void PlayerWidget::clearPlaylist() {
    player_.pause();
    playlist_.clear();
    index_ = -1;
    probeCursor_ = 0;
    queueScroll_ = 0;
    lyrics_.clear();
    lyricLayoutW_ = -1;
    title_  = QStringLiteral("Chưa có bài hát");
    artist_ = QStringLiteral("Kéo thả tệp nhạc vào đây hoặc nhấn O");
    setWindowTitle(QStringLiteral("Music Player"));
    setCover(QImage());
}

void PlayerWidget::loadIndex(int i, bool autoplay) {
    if (i < 0 || i >= playlist_.size()) return;
    index_ = i;
    Track& tr = playlist_[i];
    std::string err;
    if (!player_.load(tr.path.toUtf8().toStdString(), &err)) {
        title_ = QStringLiteral("Không thể mở tệp");
        artist_ = fromUtf8(err);
        lyrics_.clear();
        lyricLayoutW_ = -1;
        return;
    }
    const TrackInfo& ti = player_.info();
    title_  = fromUtf8(ti.title);
    artist_ = fromUtf8(ti.artist);
    tr.title = title_; tr.artist = artist_; tr.duration = ti.duration; tr.probed = true;
    setWindowTitle(artist_.isEmpty() ? title_ : title_ + " — " + artist_);

    QImage img;
    const auto& raw = player_.coverArt();
    if (!raw.empty()) {
        std::vector<uint8_t> rgba;
        int w = 0, h = 0;
        if (decodeCoverArt(ti.coverCodecId, raw.data(), raw.size(), 1024, rgba, w, h))   // libavcodec + libswscale
            img = QImage(rgba.data(), w, h, w * 4, QImage::Format_RGBA8888).copy();
        if (img.isNull()) img.loadFromData(raw.data(), uint(raw.size()));                // fallback: Qt tự giải mã
    }
    setCover(img);
    loadLyrics(tr.path, ti);

    // cuộn danh sách tới bài đang phát
    queueScroll_ = std::max(0.0, i * kRowH - 3 * kRowH);

    if (autoplay) player_.play();
}

void PlayerWidget::loadLyrics(const QString& path, const TrackInfo& ti) {
    std::string text;
    const QFileInfo fi(path);
    QFile f(fi.dir().filePath(fi.completeBaseName() + QStringLiteral(".lrc")));      // ưu tiên .lrc cạnh tệp nhạc
    if (f.open(QIODevice::ReadOnly)) {
        const QByteArray b = f.readAll();
        text.assign(b.constData(), size_t(b.size()));
    }
    if (text.empty()) text = ti.lyrics;                                               // rồi tới lời nhúng trong tag
    lyrics_.parse(text);
    lyricLayoutW_ = -1;
    lyricSnap_ = true;
    lyricManual_ = 0.0;
    lyricScroll_ = 0.0;
}

void PlayerWidget::setCover(const QImage& src) {
    QImage img = src;
    if (img.isNull()) {                           // ảnh bìa mặc định
        img = QImage(512, 512, QImage::Format_ARGB32_Premultiplied);
        QPainter q(&img);
        q.setRenderHint(QPainter::Antialiasing);
        QLinearGradient g(0, 0, 512, 512);
        g.setColorAt(0, QColor(70, 70, 96));
        g.setColorAt(1, QColor(22, 22, 32));
        q.fillRect(img.rect(), g);
        q.setPen(QColor(255, 255, 255, 110));
        QFont f("Segoe UI Symbol"); f.setPixelSize(260); q.setFont(f);
        q.drawText(img.rect(), Qt::AlignCenter, QStringLiteral("♪"));
    }
    // Cắt vuông giữa + bo góc anti-alias một lần (tỉ lệ bo không đổi khi đổi kích thước)
    const int minSide = std::min(img.width(), img.height());
    const int side = std::min(minSide, 900);
    const QImage sq = img.copy((img.width() - minSide) / 2, (img.height() - minSide) / 2, minSide, minSide)
                          .scaled(side, side, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    QImage rounded(side, side, QImage::Format_ARGB32_Premultiplied);
    rounded.fill(Qt::transparent);
    {
        QPainter q(&rounded);
        q.setRenderHint(QPainter::Antialiasing);
        q.setPen(Qt::NoPen);
        q.setBrush(QBrush(sq));
        q.drawRoundedRect(QRectF(0, 0, side, side), side * kCorner, side * kCorner);
    }
    cover_ = QPixmap::fromImage(rounded);

    bgPrev_ = bg_;
    bg_ = QPixmap::fromImage(makeBackdrop(img));
    bgFade_ = bgPrev_.isNull() ? 1.f : 0.f;
}

void PlayerWidget::next(bool automatic) {
    if (playlist_.isEmpty()) return;
    if (automatic && repeat_ == 2) { player_.seek(0); player_.play(); return; }       // lặp 1 bài
    const int n = playlist_.size();
    int target = -1;
    if (shuffle_ && n > 1) {
        do { target = int(QRandomGenerator::global()->bounded(n)); } while (target == index_);
    } else if (index_ + 1 < n) {
        target = index_ + 1;
    } else if (repeat_ == 1) {
        target = 0;
    }
    if (target >= 0) loadIndex(target, true);
    else { player_.pause(); player_.seek(0); }                                       // hết danh sách
}

void PlayerWidget::prev() {
    if (player_.position() > 3.0 || index_ <= 0) player_.seek(0);
    else loadIndex(index_ - 1, true);
}

void PlayerWidget::togglePlay() {
    if (playlist_.isEmpty()) { openDialog(false); return; }
    if (player_.playing()) { player_.pause(); return; }
    if (player_.ended()) player_.seek(0);
    player_.play();
}

void PlayerWidget::openDialog(bool append) {
    const QStringList f = QFileDialog::getOpenFileNames(
        this, QStringLiteral("Chọn nhạc"), QString(),
        QStringLiteral("Âm thanh (*.mp3 *.flac *.m4a *.aac *.wav *.ogg *.opus *.wma *.ape *.aiff *.wv);;Tất cả (*)"));
    if (append) addFiles(f); else openFiles(f);
}

void PlayerWidget::onTick() {
    if (player_.playing() && player_.ended()) next(true);                            // tự chuyển bài

    // Đọc metadata nền cho danh sách phát: tối đa 2 tệp mỗi 100 ms
    int budget = 2;
    while (budget > 0 && probeCursor_ < playlist_.size()) {
        Track& t = playlist_[probeCursor_++];
        if (t.probed) continue;
        TrackInfo ti;
        if (AudioDecoder::probe(t.path.toUtf8().toStdString(), ti)) {
            t.title = fromUtf8(ti.title);
            t.artist = fromUtf8(ti.artist);
            t.duration = ti.duration;
        }
        t.probed = true;
        --budget;
    }
}

// ------------------------------- EQ lưu/đọc -------------------------------

void PlayerWidget::loadEq() {
    QSettings s("FMusic", "Player");
    Equalizer& eq = player_.eq();
    const QStringList g = s.value("eq/gains").toStringList();
    for (int i = 0; i < Equalizer::kBands && i < g.size(); ++i) eq.setGain(i, g[i].toFloat());
    eq.setEnabled(s.value("eq/enabled", false).toBool());
    eqPreset_ = s.value("eq/preset", 0).toInt();
}

void PlayerWidget::saveEq() {
    QSettings s("FMusic", "Player");
    Equalizer& eq = player_.eq();
    QStringList g;
    for (int i = 0; i < Equalizer::kBands; ++i) g << QString::number(double(eq.gain(i)));
    s.setValue("eq/gains", g);
    s.setValue("eq/enabled", eq.enabled());
    s.setValue("eq/preset", eqPreset_);
}

void PlayerWidget::setEqFromY(int band, double y) {
    const EqGeom g = eqGeom();
    const double f = std::clamp((y - g.top) / g.h, 0.0, 1.0);
    double db = 12.0 - 24.0 * f;
    db = std::round(db * 2.0) / 2.0;                 // bước 0.5 dB
    if (std::abs(db) < 0.4) db = 0.0;
    Equalizer& eq = player_.eq();
    eq.setGain(band, float(db));
    eq.setEnabled(true);
    eqPreset_ = -1;
}

// ------------------------------- input -------------------------------

void PlayerWidget::togglePanel(Panel p) {
    panel_ = (panel_ == p) ? Panel::None : p;
    if (panel_ != Panel::None) shownPanel_ = panel_;
}

void PlayerWidget::updateDrag(double x) {
    const double f = std::clamp((x - progRect_.left()) / progRect_.width(), 0.0, 1.0);
    dragPos_ = f * player_.duration();
}

void PlayerWidget::handlePanelPress(const QPointF& pt, Qt::MouseButton b) {
    Equalizer& eq = player_.eq();
    switch (panel_) {
    case Panel::Lyrics: {
        if (b != Qt::LeftButton || !lyrics_.synced() || lyricY_.size() != int(lyrics_.lines().size())) break;
        const double y = pt.y() - panelInner().top() + lyricScroll_;
        for (int i = 0; i < lyricY_.size(); ++i)
            if (y >= lyricY_[i] && y < lyricY_[i] + lyricH_[i] + 22.0) { player_.seek(lyrics_.lines()[size_t(i)].time); break; }
        break;
    }
    case Panel::Queue: {
        if (b == Qt::LeftButton && queueAddRect().contains(pt))   { openDialog(true); break; }
        if (b == Qt::LeftButton && queueClearRect().contains(pt)) { clearPlaylist(); break; }
        const int r = rowAt(pt);
        if (r >= 0) { if (b == Qt::LeftButton) loadIndex(r, true); else if (b == Qt::RightButton) removeTrack(r); }
        break;
    }
    case Panel::Eq: {
        if (b != Qt::LeftButton) break;
        if (eqToggleRect().contains(pt)) { eq.setEnabled(!eq.enabled()); saveEq(); break; }
        for (int i = 0; i < Equalizer::kPresetCount; ++i)
            if (presetRect(i).contains(pt)) { eq.applyPreset(i); eq.setEnabled(true); eqPreset_ = i; saveEq(); return; }
        const EqGeom g = eqGeom();
        for (int k = 0; k < Equalizer::kBands; ++k)
            if (QRectF(g.left + k * g.colW, g.top - 12, g.colW, g.h + 24).contains(pt)) { eqDrag_ = k; setEqFromY(k, pt.y()); break; }
        break;
    }
    default: break;
    }
}

void PlayerWidget::mousePressEvent(QMouseEvent* e) {
    const QPointF pt = e->position();
    static const Panel ps[3] = { Panel::Lyrics, Panel::Queue, Panel::Eq };
    for (int k = 0; k < 3; ++k)
        if (tabRects_[k].contains(pt)) { if (e->button() == Qt::LeftButton) togglePanel(ps[k]); return; }
    if (openRect_.contains(pt)) { if (e->button() == Qt::LeftButton) openDialog(false); return; }
    if (panelAnim_ > 0.5f && panelRect_.contains(pt)) { handlePanelPress(pt, e->button()); return; }
    if (e->button() != Qt::LeftButton) return;
    if (prevRect_.contains(pt)) { prev(); return; }
    if (nextRect_.contains(pt)) { next(false); return; }
    if (playRect_.contains(pt)) { togglePlay(); return; }
    if (shufRect_.contains(pt)) { shuffle_ = !shuffle_; return; }
    if (repRect_.contains(pt))  { repeat_ = (repeat_ + 1) % 3; return; }
    if (progRect_.adjusted(-8, -14, 8, 14).contains(pt) && player_.duration() > 0) {
        dragging_ = true;
        updateDrag(pt.x());
    }
}

void PlayerWidget::mouseDoubleClickEvent(QMouseEvent* e) {
    if (panel_ == Panel::Eq && panelAnim_ > 0.5f && e->button() == Qt::LeftButton) {
        const EqGeom g = eqGeom();
        for (int k = 0; k < Equalizer::kBands; ++k)
            if (QRectF(g.left + k * g.colW, g.top - 12, g.colW, g.h + 24).contains(e->position())) {
                player_.eq().setGain(k, 0.f);                 // nhấp đúp = đưa băng về 0 dB
                eqPreset_ = -1;
                saveEq();
                return;
            }
    }
    mousePressEvent(e);
}

void PlayerWidget::mouseMoveEvent(QMouseEvent* e) {
    const QPointF pt = e->position();
    if (dragging_) updateDrag(pt.x());
    if (eqDrag_ >= 0) setEqFromY(eqDrag_, pt.y());
    hoverProg_ = progRect_.adjusted(-8, -14, 8, 14).contains(pt);
    hoverRow_ = (panel_ == Panel::Queue && panelAnim_ > 0.5f) ? rowAt(pt) : -1;
    bool hand = hoverProg_ || hoverRow_ >= 0 || openRect_.contains(pt) || prevRect_.contains(pt) ||
                nextRect_.contains(pt) || playRect_.contains(pt) || shufRect_.contains(pt) || repRect_.contains(pt);
    for (const QRectF& r : tabRects_) hand = hand || r.contains(pt);
    setCursor(hand ? Qt::PointingHandCursor : Qt::ArrowCursor);
}

void PlayerWidget::mouseReleaseEvent(QMouseEvent*) {
    if (dragging_) { player_.seek(dragPos_); dragging_ = false; }
    if (eqDrag_ >= 0) { eqDrag_ = -1; saveEq(); }
}

void PlayerWidget::wheelEvent(QWheelEvent* e) {
    if (panelAnim_ > 0.5f && panelRect_.contains(e->position())) {
        const double dy = e->angleDelta().y() * 0.6;
        const QRectF in = panelInner();
        if (panel_ == Panel::Queue) {
            const double maxScroll = std::max(0.0, playlist_.size() * kRowH - (in.bottom() - listTop()));
            queueScroll_ = std::clamp(queueScroll_ - dy, 0.0, maxScroll);
        } else if (panel_ == Panel::Lyrics && !lyrics_.synced()) {
            lyricManual_ = std::clamp(lyricManual_ - dy, 0.0, std::max(0.0, lyricTotalH_ - in.height()));
        }
        e->accept();
        return;
    }
    QWidget::wheelEvent(e);
}

void PlayerWidget::keyPressEvent(QKeyEvent* e) {
    switch (e->key()) {
    case Qt::Key_Space: togglePlay(); break;
    case Qt::Key_Right: player_.seek(player_.position() + 5); break;
    case Qt::Key_Left:  player_.seek(player_.position() - 5); break;
    case Qt::Key_N:     next(false); break;
    case Qt::Key_P:     prev(); break;
    case Qt::Key_O:     openDialog(false); break;
    case Qt::Key_L:     togglePanel(Panel::Lyrics); break;
    case Qt::Key_Q:     togglePanel(Panel::Queue); break;
    case Qt::Key_E:     togglePanel(Panel::Eq); break;
    case Qt::Key_S:     shuffle_ = !shuffle_; break;
    case Qt::Key_R:     repeat_ = (repeat_ + 1) % 3; break;
    case Qt::Key_Escape: panel_ = Panel::None; break;
    case Qt::Key_Up:    sink_->setVolume(std::min(1.0, double(sink_->volume()) + 0.05)); break;
    case Qt::Key_Down:  sink_->setVolume(std::max(0.0, double(sink_->volume()) - 0.05)); break;
    default: QWidget::keyPressEvent(e);
    }
}

void PlayerWidget::dragEnterEvent(QDragEnterEvent* e) {
    if (e->mimeData()->hasUrls()) e->acceptProposedAction();
}

void PlayerWidget::dropEvent(QDropEvent* e) {
    QStringList files;
    for (const QUrl& u : e->mimeData()->urls())
        if (u.isLocalFile()) files << u.toLocalFile();
    openFiles(files);
}

// ------------------------------- layout -------------------------------

QRectF PlayerWidget::panelInner() const { return panelRect_.adjusted(20, 18, -20, -18); }
double PlayerWidget::listTop() const { return panelInner().top() + 58.0; }

int PlayerWidget::rowAt(const QPointF& pt) const {
    const QRectF in = panelInner();
    if (pt.x() < in.left() - 6 || pt.x() > in.right() + 6 || pt.y() < listTop() || pt.y() > in.bottom()) return -1;
    const int r = int((pt.y() - listTop() + queueScroll_) / kRowH);
    return (r >= 0 && r < playlist_.size()) ? r : -1;
}

QRectF PlayerWidget::queueAddRect() const   { const QRectF in = panelInner(); return QRectF(in.right() - 64, in.top() + 3, 64, 28); }
QRectF PlayerWidget::queueClearRect() const { const QRectF in = panelInner(); return QRectF(in.right() - 64 - 8 - 84, in.top() + 3, 84, 28); }
QRectF PlayerWidget::eqToggleRect() const   { const QRectF in = panelInner(); return QRectF(in.right() - 48, in.top() + 3, 48, 28); }

QRectF PlayerWidget::presetRect(int i) const {
    const QRectF in = panelInner();
    const double w = (in.width() - 16.0) / 3.0;
    return QRectF(in.left() + (i % 3) * (w + 8.0), in.top() + 52.0 + (i / 3) * 38.0, w, 30.0);
}

PlayerWidget::EqGeom PlayerWidget::eqGeom() const {
    const QRectF in = panelInner();
    EqGeom g;
    g.left = in.left();
    g.colW = in.width() / Equalizer::kBands;
    g.top = in.top() + 52.0 + 3 * 38.0 + 36.0;
    g.h = std::clamp(in.bottom() - g.top - 40.0, 80.0, 260.0);
    return g;
}

void PlayerWidget::computeLayout() {
    const double W = width(), H = height();
    const double panelW = std::min(430.0, W * 0.42);
    mainW_ = W - panelW * panelAnim_;                       // vùng chính co lại khi panel trượt ra
    const double cx = mainW_ / 2;

    const double art  = std::clamp(std::min(mainW_ * 0.52, H * 0.40), 140.0, 560.0);
    const double ring = art * 0.18;                         // chỗ cho vòng spectrum
    const double textH = 64, progH = 46, ctrlH = 92;
    const double total = ring + art + ring + textH + progH + ctrlH;
    const double top = std::max(24.0, (H - total) / 2.0) + ring;

    artRect_ = QRectF(cx - art / 2, top, art, art);
    double y = artRect_.bottom() + ring + 6;
    titleY_ = y;
    y += textH;
    const double pw = std::min(mainW_ * 0.8, std::max(art * 1.5, 320.0));
    progRect_ = QRectF(cx - pw / 2, y + 8, pw, 6);
    y += progH;
    const double cy = y + 38;
    playRect_ = QRectF(cx - 32, cy - 32, 64, 64);
    prevRect_ = QRectF(cx - 130, cy - 28, 56, 56);
    nextRect_ = QRectF(cx + 74, cy - 28, 56, 56);
    const double off = std::clamp(mainW_ / 2 - 24, 150.0, 215.0);
    shufRect_ = QRectF(cx - off - 20, cy - 20, 40, 40);
    repRect_  = QRectF(cx + off - 20, cy - 20, 40, 40);
    openRect_ = QRectF(18, 18, 40, 40);

    panelRect_ = QRectF(W - panelW * panelAnim_ + 4, 70, panelW - 22, H - 70 - 20);
    const double tw[3] = { 56, 100, 52 };
    double x = W - 18;
    for (int k = 2; k >= 0; --k) { x -= tw[k]; tabRects_[k] = QRectF(x, 22, tw[k], 32); x -= 8; }
}

void PlayerWidget::ensureShadow() {
    const int art = int(artRect_.width());
    if (art == shadowArt_) return;
    shadowArt_ = art;
    shadowPad_ = int(art * 0.30);
    const int S = art + 2 * shadowPad_;
    QImage img(S, S, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    {
        QPainter q(&img);
        q.setRenderHint(QPainter::Antialiasing);
        q.setPen(Qt::NoPen);
        q.setBrush(QColor(0, 0, 0, 190));
        q.drawRoundedRect(QRectF(shadowPad_, shadowPad_, art, art), art * kCorner, art * kCorner);
    }
    boxBlur(img, std::max(2, art / 16), 3);               // đổ bóng mềm & sâu, chỉ tính lại khi đổi kích thước
    shadowPix_ = QPixmap::fromImage(img);
}

// ------------------------------- painting -------------------------------

void PlayerWidget::paintEvent(QPaintEvent*) {
    const qint64 ms = clock_.elapsed();
    dt_ = std::min(0.1, (ms - lastMs_) / 1000.0);
    lastMs_ = ms;
    const double t = ms / 1000.0;

    float bands[kBands];
    const float bass = player_.spectrum(bands);
    bassSmooth_ += (bass - bassSmooth_) * 0.25f;
    const float target = player_.playing() ? 1.0f : 0.86f;       // ảnh bìa thu nhỏ khi tạm dừng (như Apple Music)
    artScale_ += (target - artScale_) * float(std::min(1.0, dt_ * 9.0));
    bgFade_ = std::min(1.f, bgFade_ + float(dt_ / 0.9));

    const float panelTarget = panel_ != Panel::None ? 1.f : 0.f;
    panelAnim_ += (panelTarget - panelAnim_) * float(std::min(1.0, dt_ * 10.0));
    if (std::abs(panelTarget - panelAnim_) < 0.004f) panelAnim_ = panelTarget;

    QPainter p(this);
    p.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform | QPainter::TextAntialiasing);
    computeLayout();
    drawBackground(p, t);
    drawArtAndSpectrum(p, bands);
    drawTexts(p);
    drawProgress(p);
    drawControls(p);
    drawTabs(p);
    drawPanel(p);
}

void PlayerWidget::drawBackdrop(QPainter& p, const QPixmap& pix, double t, double opacity) {
    if (pix.isNull() || opacity <= 0.0) return;
    const double W = width(), H = height(), S = std::max(W, H) * 1.5;
    p.save();
    p.setOpacity(opacity);
    p.translate(W / 2 + std::sin(t * 0.07) * W * 0.04, H / 2 + std::cos(t * 0.05) * H * 0.04);
    p.rotate(std::sin(t * 0.04) * 10.0);
    p.drawPixmap(QRectF(-S / 2, -S / 2, S, S), pix, pix.rect());
    p.setOpacity(opacity * 0.55);                       // lớp thứ hai xoay ngược -> màu "loang" chuyển động
    p.rotate(-std::sin(t * 0.06) * 30.0 + 55.0);
    p.scale(1.15, 1.15);
    p.drawPixmap(QRectF(-S / 2, -S / 2, S, S), pix, pix.rect());
    p.restore();
}

void PlayerWidget::drawBackground(QPainter& p, double t) {
    p.fillRect(rect(), QColor(8, 8, 10));
    if (bgFade_ < 1.f) drawBackdrop(p, bgPrev_, t, 1.0);            // cross-fade giữa 2 bài
    drawBackdrop(p, bg_, t, bgFade_);
    p.fillRect(rect(), QColor(0, 0, 0, 95));                        // dim
    QLinearGradient g(0, 0, 0, height());                           // vignette
    g.setColorAt(0.0, QColor(0, 0, 0, 20));
    g.setColorAt(1.0, QColor(0, 0, 0, 130));
    p.fillRect(rect(), g);
}

void PlayerWidget::drawArtAndSpectrum(QPainter& p, const float* bands) {
    ensureShadow();
    const double s = artScale_ * (1.0 + 0.02 * bassSmooth_);        // nhịp bass làm ảnh "thở" nhẹ
    const QPointF c = artRect_.center();
    const double art = artRect_.width();

    p.save();
    p.translate(c);
    p.scale(s, s);
    p.translate(-c);

    // Drop shadow
    p.drawPixmap(QPointF(artRect_.left() - shadowPad_, artRect_.top() - shadowPad_ + art * 0.06), shadowPix_);

    // Album art (đã bo góc sẵn)
    p.drawPixmap(artRect_, cover_, cover_.rect());
    p.setPen(QPen(QColor(255, 255, 255, 26), 1));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(artRect_.adjusted(0.5, 0.5, -0.5, -0.5), art * kCorner, art * kCorner);

    // Spectrum bám theo đường viền bo góc: bass ở giữa cạnh trên, treble ở giữa cạnh dưới, đối xứng trái/phải
    const double gap = art * 0.035, maxLen = art * 0.15;
    const QRectF ring = artRect_.adjusted(-gap, -gap, gap, gap);
    const double r = art * kCorner + gap;
    const double P = 2 * (ring.width() - 2 * r) + 2 * (ring.height() - 2 * r) + 2 * kPi * r;
    const int N = 96;
    const double bw = std::max(2.0, P / N * 0.52);
    for (int pass = 0; pass < 2; ++pass) {                          // pass 0: vầng sáng, pass 1: thanh chính
        QPen pen;
        pen.setCapStyle(Qt::RoundCap);
        pen.setWidthF(pass == 0 ? bw * 2.4 : bw);
        for (int k = 0; k < N; ++k) {
            const double f = (k + 0.5) / N;
            const double g = std::abs(f - 0.5) * 2.0;
            const double x = g * (kBands - 1);
            const int i0 = int(x), i1 = std::min(i0 + 1, kBands - 1);
            const double v = bands[i0] * (1.0 - (x - i0)) + bands[i1] * (x - i0);
            const PerimPt pt = perimeterPoint(ring, r, P * f);
            const double len = 3.0 + std::pow(v, 1.2) * maxLen;
            pen.setColor(QColor(255, 255, 255, pass == 0 ? int(38 * v) : int(70 + 160 * v)));
            p.setPen(pen);
            p.drawLine(pt.p, pt.p + pt.n * len);
        }
    }
    p.restore();
}


void PlayerWidget::drawTexts(QPainter& p) {
    const double cx = mainW_ / 2, art = artRect_.width();
    const double tw = std::min(mainW_ - 40, std::max(art * 1.5, 320.0));
    QFont tf("Segoe UI Semibold");
    tf.setPixelSize(int(std::max(18.0, art * 0.055)));
    QFont af("Segoe UI");
    af.setPixelSize(int(std::max(15.0, art * 0.042)));

    p.setFont(tf);
    p.setPen(QColor(255, 255, 255, 240));
    p.drawText(QRectF(cx - tw / 2, titleY_, tw, tf.pixelSize() * 1.5), Qt::AlignCenter,
               QFontMetrics(tf).elidedText(title_, Qt::ElideRight, int(tw)));
    p.setFont(af);
    p.setPen(QColor(255, 255, 255, 150));
    p.drawText(QRectF(cx - tw / 2, titleY_ + tf.pixelSize() * 1.55, tw, af.pixelSize() * 1.5), Qt::AlignCenter,
               QFontMetrics(af).elidedText(artist_, Qt::ElideRight, int(tw)));
}

void PlayerWidget::drawProgress(QPainter& p) {
    const double dur = player_.duration();
    const double pos = dragging_ ? dragPos_ : player_.position();
    const double frac = dur > 0 ? std::clamp(pos / dur, 0.0, 1.0) : 0.0;
    const float want = (dragging_ || hoverProg_) ? 9.f : 5.f;
    progThick_ += (want - progThick_) * 0.3f;

    const double cy = progRect_.center().y(), h = progThick_;
    const QRectF track(progRect_.left(), cy - h / 2, progRect_.width(), h);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(255, 255, 255, 64));
    p.drawRoundedRect(track, h / 2, h / 2);
    QRectF fill = track;
    fill.setWidth(std::max(h, track.width() * frac));
    if (frac > 0) {
        p.setBrush(QColor(255, 255, 255, 220));
        p.drawRoundedRect(fill, h / 2, h / 2);
    }
    QFont f("Segoe UI");
    f.setPixelSize(12);
    p.setFont(f);
    p.setPen(QColor(255, 255, 255, 140));
    p.drawText(QRectF(track.left(), cy + 10, 80, 18), Qt::AlignLeft, formatTime(pos));
    p.drawText(QRectF(track.right() - 80, cy + 10, 80, 18), Qt::AlignRight,
               dur > 0 ? "-" + formatTime(dur - pos) : QString("-:--"));
}


void PlayerWidget::drawControls(QPainter& p) {
    const QColor col(255, 255, 255, 235);
    p.setPen(QPen(col, 3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));   // join tròn -> tam giác bo mềm
    p.setBrush(col);

    auto skip = [&](QPointF c, bool forward) {            // 2 tam giác liên tiếp (⏮ / ⏭)
        const double d = forward ? 1.0 : -1.0, w = 15, h = 22;
        const double start = c.x() - d * w * 0.975;
        for (int i = 0; i < 2; ++i) {
            const double x0 = start + d * i * w * 0.95;
            QPainterPath path;
            path.moveTo(x0, c.y() - h / 2);
            path.lineTo(x0, c.y() + h / 2);
            path.lineTo(x0 + d * w, c.y());
            path.closeSubpath();
            p.drawPath(path);
        }
    };
    skip(prevRect_.center(), false);
    skip(nextRect_.center(), true);

    const QPointF c = playRect_.center();
    if (player_.playing()) {
        p.setPen(Qt::NoPen);
        p.drawRoundedRect(QRectF(c.x() - 15, c.y() - 16, 10, 32), 3, 3);
        p.drawRoundedRect(QRectF(c.x() + 5, c.y() - 16, 10, 32), 3, 3);
    } else {
        QPainterPath tri;
        tri.moveTo(c.x() - 11, c.y() - 17);
        tri.lineTo(c.x() - 11, c.y() + 17);
        tri.lineTo(c.x() + 18, c.y());
        tri.closeSubpath();
        p.drawPath(tri);
    }

    // Shuffle / Repeat: sáng + chấm nhỏ bên dưới khi đang bật
    auto toggleGlyph = [&](const QRectF& r, const QString& g, bool on, const QString& badge) {
        QFont f("Segoe UI Symbol");
        f.setPixelSize(24);
        p.setFont(f);
        p.setPen(QColor(255, 255, 255, on ? 255 : 105));
        p.drawText(r, Qt::AlignCenter, g);
        if (!badge.isEmpty()) {
            p.setFont(uiFont(11, true));
            p.drawText(QRectF(r.right() - 12, r.top() - 2, 12, 14), Qt::AlignCenter, badge);
        }
        if (on) {
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(255, 255, 255, 230));
            p.drawEllipse(QPointF(r.center().x(), r.bottom() + 2), 2.2, 2.2);
            p.setBrush(Qt::NoBrush);
        }
    };
    toggleGlyph(shufRect_, QStringLiteral("⇄"), shuffle_, QString());
    toggleGlyph(repRect_, QStringLiteral("↻"), repeat_ != 0, repeat_ == 2 ? QStringLiteral("1") : QString());

    // nút "+" mở nhạc
    p.setPen(QPen(QColor(255, 255, 255, 200), 2, Qt::SolidLine, Qt::RoundCap));
    p.setBrush(QColor(255, 255, 255, 28));
    p.drawEllipse(openRect_);
    const QPointF o = openRect_.center();
    p.drawLine(o + QPointF(-7, 0), o + QPointF(7, 0));
    p.drawLine(o + QPointF(0, -7), o + QPointF(0, 7));
    p.setBrush(Qt::NoBrush);
}

void PlayerWidget::drawTabs(QPainter& p) {
    static const char* const labels[3] = { "Lời", "Danh sách", "EQ" };
    static const Panel ps[3] = { Panel::Lyrics, Panel::Queue, Panel::Eq };
    p.setFont(uiFont(14, true));
    for (int k = 0; k < 3; ++k) {
        const bool on = panel_ == ps[k];
        p.setPen(Qt::NoPen);
        p.setBrush(on ? QColor(255, 255, 255, 235) : QColor(255, 255, 255, 40));
        p.drawRoundedRect(tabRects_[k], 16, 16);
        p.setPen(on ? QColor(20, 20, 24) : QColor(255, 255, 255, 225));
        p.drawText(tabRects_[k], Qt::AlignCenter, QString::fromUtf8(labels[k]));
    }
    p.setBrush(Qt::NoBrush);
}

void PlayerWidget::drawPanel(QPainter& p) {
    if (panelAnim_ < 0.01f) return;
    p.save();
    p.setOpacity(panelAnim_);
    QPainterPath bg;
    bg.addRoundedRect(panelRect_, 20, 20);
    p.fillPath(bg, QColor(0, 0, 0, 85));
    p.setPen(QPen(QColor(255, 255, 255, 28), 1));
    p.setBrush(Qt::NoBrush);
    p.drawPath(bg);
    p.setClipPath(bg);
    switch (shownPanel_) {
    case Panel::Lyrics: drawLyrics(p); break;
    case Panel::Queue:  drawQueue(p);  break;
    case Panel::Eq:     drawEq(p);     break;
    default: break;
    }
    p.restore();
}

// ---- Lời bài hát ----
void PlayerWidget::layoutLyrics(double width) {
    const QFontMetrics fm(uiFont(26, true));
    const auto& lines = lyrics_.lines();
    lyricH_.resize(int(lines.size()));
    lyricY_.resize(int(lines.size()));
    double y = 0.0;
    for (size_t i = 0; i < lines.size(); ++i) {
        const QString s = fromUtf8(lines[i].text);
        const double h = s.isEmpty() ? fm.height() * 0.6
                                     : double(fm.boundingRect(QRect(0, 0, int(width), 100000), Qt::TextWordWrap | Qt::AlignLeft, s).height());
        lyricY_[int(i)] = y;
        lyricH_[int(i)] = h;
        y += h + 22.0;
    }
    lyricTotalH_ = y;
    lyricLayoutW_ = int(width);
}

void PlayerWidget::drawLyrics(QPainter& p) {
    const QRectF in = panelInner();
    if (lyrics_.empty()) {
        p.setPen(QColor(255, 255, 255, 200));
        p.setFont(uiFont(18, true));
        p.drawText(in.adjusted(0, 0, 0, -30), Qt::AlignCenter, QStringLiteral("Không có lời bài hát"));
        p.setPen(QColor(255, 255, 255, 120));
        p.setFont(uiFont(13));
        p.drawText(in.adjusted(10, 40, -10, 0), Qt::AlignHCenter | Qt::AlignVCenter | Qt::TextWordWrap,
                   QStringLiteral("Đặt tệp .lrc cùng tên với tệp nhạc\n(vd: bai-hat.mp3 và bai-hat.lrc) hoặc dùng tệp có lời nhúng trong tag."));
        return;
    }
    if (lyricLayoutW_ != int(in.width())) layoutLyrics(in.width());

    const bool synced = lyrics_.synced();
    const int cur = synced ? lyrics_.indexAt(player_.position() + 0.05) : -1;
    double target;
    if (synced) {
        const int c = std::max(cur, 0);
        target = lyricY_[c] + lyricH_[c] / 2 - in.height() * 0.4;
    } else {
        target = lyricManual_;
    }
    target = std::max(0.0, target);
    if (lyricSnap_) { lyricScroll_ = target; lyricSnap_ = false; }
    else lyricScroll_ += (target - lyricScroll_) * std::min(1.0, dt_ * 6.0);        // cuộn mượt

    p.save();
    p.setClipRect(in, Qt::IntersectClip);
    p.setFont(uiFont(26, true));
    for (int i = 0; i < lyricY_.size(); ++i) {
        const double y = in.top() + lyricY_[i] - lyricScroll_;
        if (y + lyricH_[i] < in.top() || y > in.bottom()) continue;
        int alpha = 215;
        if (synced) alpha = (i == cur) ? 255 : std::max(60, 160 - 40 * std::abs(i - cur));
        p.setPen(QColor(255, 255, 255, alpha));
        p.drawText(QRectF(in.left(), y, in.width(), lyricH_[i]), Qt::TextWordWrap | Qt::AlignLeft | Qt::AlignTop,
                   fromUtf8(lyrics_.lines()[size_t(i)].text));
    }
    p.restore();
}

// ---- Danh sách phát ----
void PlayerWidget::drawQueue(QPainter& p) {
    const QRectF in = panelInner();
    p.setPen(QColor(255, 255, 255, 240));
    p.setFont(uiFont(20, true));
    p.drawText(QRectF(in.left(), in.top(), in.width() * 0.5, 30), Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("Danh sách phát"));
    p.setPen(QColor(255, 255, 255, 125));
    p.setFont(uiFont(12));
    p.drawText(QRectF(in.left(), in.top() + 30, in.width() * 0.6, 20), Qt::AlignLeft | Qt::AlignVCenter,
               QString("%1 bài").arg(playlist_.size()));

    auto pill = [&](const QRectF& r, const QString& text) {
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(255, 255, 255, 45));
        p.drawRoundedRect(r, 14, 14);
        p.setBrush(Qt::NoBrush);
        p.setPen(QColor(255, 255, 255, 230));
        p.setFont(uiFont(13, true));
        p.drawText(r, Qt::AlignCenter, text);
    };
    pill(queueClearRect(), QStringLiteral("Xoá hết"));
    pill(queueAddRect(), QStringLiteral("Thêm"));

    const double top = listTop();
    if (playlist_.isEmpty()) {
        p.setPen(QColor(255, 255, 255, 130));
        p.setFont(uiFont(14));
        p.drawText(QRectF(in.left(), top, in.width(), 120), Qt::AlignCenter | Qt::TextWordWrap,
                   QStringLiteral("Chưa có bài hát\nNhấn “Thêm” hoặc kéo thả tệp nhạc vào cửa sổ"));
        return;
    }

    p.save();
    p.setClipRect(QRectF(in.left() - 8, top, in.width() + 16, in.bottom() - top), Qt::IntersectClip);
    const QFontMetrics fmTitle(uiFont(15, true)), fmArtist(uiFont(13));
    for (int i = 0; i < playlist_.size(); ++i) {
        const QRectF r(in.left(), top + i * kRowH - queueScroll_, in.width(), kRowH - 4);
        if (r.bottom() < top || r.top() > in.bottom()) continue;
        const Track& t = playlist_[i];
        if (i == index_ || i == hoverRow_) {
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(255, 255, 255, i == index_ ? 48 : 22));
            p.drawRoundedRect(r, 10, 10);
            p.setBrush(Qt::NoBrush);
        }
        // số thứ tự / biểu tượng đang phát
        p.setPen(QColor(255, 255, 255, i == index_ ? 240 : 120));
        if (i == index_) { QFont sf("Segoe UI Symbol"); sf.setPixelSize(16); p.setFont(sf); p.drawText(QRectF(r.left() + 6, r.top(), 28, r.height()), Qt::AlignCenter, QStringLiteral("♪")); }
        else { p.setFont(uiFont(13)); p.drawText(QRectF(r.left() + 6, r.top(), 28, r.height()), Qt::AlignCenter, QString::number(i + 1)); }

        const double tw = r.width() - 40 - 56;
        p.setFont(uiFont(15, true));
        p.setPen(QColor(255, 255, 255, i == index_ ? 255 : 225));
        p.drawText(QRectF(r.left() + 40, r.top() + 8, tw, 22), Qt::AlignLeft | Qt::AlignVCenter,
                   fmTitle.elidedText(t.title, Qt::ElideRight, int(tw)));
        p.setFont(uiFont(13));
        p.setPen(QColor(255, 255, 255, 125));
        p.drawText(QRectF(r.left() + 40, r.top() + 30, tw, 18), Qt::AlignLeft | Qt::AlignVCenter,
                   fmArtist.elidedText(t.artist, Qt::ElideRight, int(tw)));
        if (t.duration > 0)
            p.drawText(QRectF(r.right() - 54, r.top(), 48, r.height()), Qt::AlignRight | Qt::AlignVCenter, formatTime(t.duration));
    }
    p.restore();
}

// ---- Equalizer ----
void PlayerWidget::drawEq(QPainter& p) {
    const QRectF in = panelInner();
    Equalizer& eq = player_.eq();
    const bool on = eq.enabled();

    p.setPen(QColor(255, 255, 255, 240));
    p.setFont(uiFont(20, true));
    p.drawText(QRectF(in.left(), in.top(), in.width() * 0.6, 34), Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("Equalizer"));

    // công tắc bật/tắt
    const QRectF sw = eqToggleRect();
    p.setPen(Qt::NoPen);
    p.setBrush(on ? QColor(52, 199, 89) : QColor(255, 255, 255, 70));
    p.drawRoundedRect(sw, sw.height() / 2, sw.height() / 2);
    p.setBrush(Qt::white);
    p.drawEllipse(QPointF(on ? sw.right() - sw.height() / 2 : sw.left() + sw.height() / 2, sw.center().y()), sw.height() / 2 - 3, sw.height() / 2 - 3);
    p.setBrush(Qt::NoBrush);

    // preset
    p.setFont(uiFont(13, true));
    for (int i = 0; i < Equalizer::kPresetCount; ++i) {
        const QRectF r = presetRect(i);
        const bool act = (i == eqPreset_);
        p.setPen(Qt::NoPen);
        p.setBrush(act ? QColor(255, 255, 255, 235) : QColor(255, 255, 255, 38));
        p.drawRoundedRect(r, 15, 15);
        p.setBrush(Qt::NoBrush);
        p.setPen(act ? QColor(20, 20, 24) : QColor(255, 255, 255, 220));
        p.drawText(r, Qt::AlignCenter, QString::fromUtf8(Equalizer::presetName(i)));
    }

    // 10 thanh trượt dọc
    const EqGeom g = eqGeom();
    const double dim = on ? 1.0 : 0.45;
    const double zeroY = g.top + g.h / 2;
    p.setPen(QPen(QColor(255, 255, 255, int(40 * dim)), 1, Qt::DashLine));
    p.drawLine(QPointF(g.left + 6, zeroY), QPointF(g.left + g.colW * Equalizer::kBands - 6, zeroY));    // vạch 0 dB
    for (int k = 0; k < Equalizer::kBands; ++k) {
        const double x = g.left + (k + 0.5) * g.colW;
        const float db = eq.gain(k);
        const double ky = g.top + g.h * (1.0 - (db + 12.0) / 24.0);
        p.setPen(QPen(QColor(255, 255, 255, int(70 * dim)), 4, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(QPointF(x, g.top), QPointF(x, g.top + g.h));
        p.setPen(QPen(QColor(255, 255, 255, int(235 * dim)), 4, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(QPointF(x, zeroY), QPointF(x, ky));
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(255, 255, 255, int(255 * dim)));
        p.drawEllipse(QPointF(x, ky), 8.5, 8.5);
        p.setBrush(Qt::NoBrush);
        p.setFont(uiFont(11));
        p.setPen(QColor(255, 255, 255, int(170 * dim)));
        p.drawText(QRectF(x - g.colW / 2, g.top - 22, g.colW, 16), Qt::AlignCenter,
                   (db > 0 ? "+" : "") + QString::number(double(db), 'f', std::abs(db - std::round(db)) > 0.01f ? 1 : 0));
        p.setPen(QColor(255, 255, 255, 150));
        p.drawText(QRectF(x - g.colW / 2, g.top + g.h + 12, g.colW, 16), Qt::AlignCenter, QString::fromUtf8(kFreqLabels[k]));
    }
    p.setPen(QColor(255, 255, 255, 105));
    p.setFont(uiFont(12));
    p.drawText(QRectF(in.left(), in.bottom() - 22, in.width(), 20), Qt::AlignCenter,
               QStringLiteral("Kéo để chỉnh · nhấp đúp để đưa về 0 dB"));
}
