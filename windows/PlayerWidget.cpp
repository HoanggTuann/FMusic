#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "PlayerWidget.h"

#include <QAudioDevice>
#include <QAudioFormat>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QInputDialog>
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

#include <QShowEvent>
#ifdef Q_OS_WIN
#include <windows.h>
#endif

#include "AudioDecoder.h"
#include "CoverArt.h"
#include "Equalizer.h"
#include "EqPreset.h"
#include "PlaylistFile.h"

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

// v1.1 Blur nâng cấp: thu nhỏ 120x120, blur bán kính 12 (3 lượt ≈ Gaussian sâu), tăng bão hoà 1.5, hạ sáng -12% (đồng bộ chuẩn CoreImage của iOS)
QImage makeBackdrop(const QImage& cover) {
    if (cover.isNull()) return QImage();
    QImage s = cover.scaled(120, 120, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation)
                    .convertToFormat(QImage::Format_ARGB32_Premultiplied);
    s = s.copy((s.width() - 120) / 2, (s.height() - 120) / 2, 120, 120);
    boxBlur(s, 12, 3);
    for (int y = 0; y < s.height(); ++y) {
        for (int x = 0; x < s.width(); ++x) {
            QColor c = s.pixelColor(x, y);
            float h, sat, v, a;
            c.getHsvF(&h, &sat, &v, &a);
            c.setHsvF(h < 0 ? 0 : h, std::min(1.f, sat * 1.50f), std::min(1.f, v * 0.88f), a);
            s.setPixelColor(x, y, c);
        }
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

namespace {
constexpr double kRowH = 58.0;
constexpr double kSidebarW = 76.0;
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

    loadSettings();
    initAudioSink();
    loadEq();

    connect(&tick_, &QTimer::timeout, this, [this] { onTick(); });
    tick_.start(100);
    connect(&frame_, &QTimer::timeout, this, [this] { update(); });
    frame_.start(16);
    clock_.start();

    setCover(QImage());
}

PlayerWidget::~PlayerWidget() {
    unregisterHotkeys();
    if (sink_) sink_->stop();    // dừng audio thread trước khi Player bị huỷ
}

void PlayerWidget::initAudioSink() {
    if (sink_) {
        sink_->stop();
        delete sink_;
        sink_ = nullptr;
    }
    if (pcm_) {
        delete pcm_;
        pcm_ = nullptr;
    }
    QAudioFormat fmt;
    fmt.setSampleRate(sampleRate_);
    fmt.setChannelCount(2);
    fmt.setSampleFormat(QAudioFormat::Float);
    const QAudioDevice dev = QMediaDevices::defaultAudioOutput();
    if (!dev.isFormatSupported(fmt)) {
        qWarning("Thiết bị âm thanh không hỗ trợ float32 %d trực tiếp (Qt sẽ chuyển đổi).", sampleRate_);
    }
    sink_ = new QAudioSink(dev, fmt, this);
    sink_->setBufferSize(sampleRate_ / 10 * 2 * int(sizeof(float)));   // ~100 ms
    pcm_ = new PcmDevice(player_, this);
    pcm_->open(QIODevice::ReadOnly);
    sink_->start(pcm_);
    applyVolume();
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
    setWindowTitle(QStringLiteral("FMusic"));
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
    // Cắt vuông giữa + bo góc anti-alias một lần
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
    const QString filter = QStringLiteral(
        "Tất cả được hỗ trợ (*.mp3 *.flac *.m4a *.aac *.wav *.ogg *.opus *.wma *.ape *.aiff *.wv *.fpl);;"
        "Danh sách phát FMusic (*.fpl);;"
        "Tệp âm thanh (*.mp3 *.flac *.m4a *.aac *.wav *.ogg *.opus *.wma *.ape *.aiff *.wv);;"
        "Mọi tệp (*)");
    const QStringList files = QFileDialog::getOpenFileNames(this, QStringLiteral("Mở nhạc hoặc danh sách phát"), QString(), filter);
    if (files.isEmpty()) return;

    if (files.size() == 1 && files.first().endsWith(QStringLiteral(".fpl"), Qt::CaseInsensitive)) {
        loadPlaylist(files.first());
        return;
    }
    if (append) addFiles(files); else openFiles(files);
}

// v1.1 Streaming qua localhost / HTTP / HTTPS / Cloud direct links
void PlayerWidget::openUrlDialog() {
    bool ok = false;
    const QString url = QInputDialog::getText(
        this, QStringLiteral("Phát trực tuyến (Streaming)"),
        QStringLiteral("Nhập URL stream (HTTP, HTTPS, Localhost, Google Drive direct link...):"),
        QLineEdit::Normal, QStringLiteral("http://localhost:8080/"), &ok);
    if (ok && !url.trimmed().isEmpty()) {
        Track t;
        t.path = url.trimmed();
        t.title = t.path;
        playlist_.push_back(t);
        loadIndex(playlist_.size() - 1, true);
    }
}

// v1.1 Lưu danh sách phát thành file .fpl (chỉ lưu đường dẫn, không sao chép nhạc)
void PlayerWidget::savePlaylist() {
    if (playlist_.isEmpty()) return;
    const QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("Lưu danh sách phát"), QString(),
        QStringLiteral("Danh sách phát FMusic (*.fpl)"));
    if (path.isEmpty()) return;

    std::vector<std::string> paths;
    paths.reserve(playlist_.size());
    for (const Track& t : playlist_) paths.push_back(t.path.toUtf8().toStdString());
    if (PlaylistFile::save(path.toUtf8().toStdString(), paths)) {
        lastPlaylistPath_ = path;
    }
}

void PlayerWidget::loadPlaylist(const QString& path) {
    std::vector<std::string> paths;
    if (PlaylistFile::load(path.toUtf8().toStdString(), paths)) {
        QStringList list;
        for (const auto& p : paths) list.append(QString::fromUtf8(p.c_str()));
        openFiles(list);
        lastPlaylistPath_ = path;
    }
}

// v1.1 Lưu / Tải EQ Preset định dạng FEQ (JSON)
void PlayerWidget::saveEqPreset() {
    const QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("Xuất Preset Equalizer"), QStringLiteral("Preset.feq"),
        QStringLiteral("Preset FMusic EQ (*.feq)"));
    if (path.isEmpty()) return;

    const QString baseName = QFileInfo(path).completeBaseName();
    player_.eq().savePreset(path.toUtf8().toStdString(), baseName.toUtf8().toStdString());
}

void PlayerWidget::loadEqPreset(const QString& path) {
    QString p = path;
    if (p.isEmpty()) {
        p = QFileDialog::getOpenFileName(
            this, QStringLiteral("Nhập Preset Equalizer"), QString(),
            QStringLiteral("Preset FMusic EQ (*.feq);;Tất cả (*)"));
    }
    if (p.isEmpty()) return;
    if (player_.eq().loadPreset(p.toUtf8().toStdString())) {
        eqPreset_ = -1;     // chế độ preset tuỳ chỉnh
        player_.eq().setEnabled(true);
        saveEq();
    }
}

void PlayerWidget::onTick() {
    // Probe metadata ngầm
    if (probeCursor_ < playlist_.size()) {
        Track& t = playlist_[probeCursor_++];
        if (!t.probed) {
            TrackInfo info;
            if (AudioDecoder::probe(t.path.toUtf8().toStdString(), info)) {
                if (!info.title.empty()) t.title = fromUtf8(info.title);
                t.artist = fromUtf8(info.artist);
                t.duration = info.duration;
            }
            t.probed = true;
        }
    }
    if (player_.playing() && player_.ended()) next(true);
    updateMedia();
}

void PlayerWidget::updateDrag(double x) {
    if (player_.duration() <= 0) return;
    const double f = std::clamp((x - progRect_.left()) / progRect_.width(), 0.0, 1.0);
    dragPos_ = f * player_.duration();
}

void PlayerWidget::togglePanel(Panel p) {
    if (panel_ == p) panel_ = Panel::None;
    else { panel_ = p; shownPanel_ = p; }
}

void PlayerWidget::handlePanelPress(const QPointF& pt, Qt::MouseButton b) {
    switch (shownPanel_) {
    case Panel::Lyrics: {
        if (b != Qt::LeftButton) break;
        const auto& lines = lyrics_.lines();
        for (int i = 0; i < lyricH_.size() && i < int(lines.size()); ++i) {
            const QRectF r(panelInner().left(), listTop() + lyricY_[i] - lyricScroll_, panelInner().width(), lyricH_[i]);
            if (r.contains(pt) && lines[i].time >= 0) {
                player_.seek(std::max(0.0, lines[i].time - setLyricOffsetMs_ / 1000.0));
                lyricSnap_ = true;
                break;
            }
        }
        break;
    }
    case Panel::Queue: {
        if (b == Qt::LeftButton) {
            if (queueClearRect().contains(pt)) { clearPlaylist(); break; }
            if (queueAddRect().contains(pt))   { openDialog(true); break; }
            const int r = rowAt(pt);
            if (r >= 0) loadIndex(r, true);
        } else if (b == Qt::RightButton) {
            const int r = rowAt(pt);
            if (r >= 0) removeTrack(r);
        }
        break;
    }
    case Panel::Eq: {
        if (b != Qt::LeftButton) break;
        if (eqToggleRect().contains(pt)) {
            player_.eq().setEnabled(!player_.eq().enabled());
            saveEq();
            break;
        }
        if (eqSaveRect_.contains(pt)) {
            saveEqPreset();
            break;
        }
        if (eqExportRect_.contains(pt)) {
            loadEqPreset(QString());
            break;
        }
        for (int i = 0; i < Equalizer::kPresetCount; ++i) {
            if (presetRect(i).contains(pt)) {
                eqPreset_ = i;
                player_.eq().applyPreset(i);
                player_.eq().setEnabled(true);
                saveEq();
                break;
            }
        }
        const EqGeom g = eqGeom();
        for (int k = 0; k < Equalizer::kBands; ++k) {
            if (QRectF(g.left + k * g.colW, g.top - 12, g.colW, g.h + 24).contains(pt)) {
                eqDrag_ = k;
                setEqFromY(k, pt.y());
                break;
            }
        }
        break;
    }
    case Panel::Settings: {
        if (b != Qt::LeftButton) break;
        // Kiểm tra nút sample rate (hàng 4)
        for (int i = 0; i < 3; ++i) {
            if (srRects_[i].contains(pt)) {
                const int rates[3] = { 44100, 48000, 96000 };
                if (sampleRate_ != rates[i]) {
                    sampleRate_ = rates[i];
                    player_.setSampleRate(sampleRate_);
                    initAudioSink();
                    saveSettings();
                }
                return;
            }
        }
        for (int i = 0; i < 4; ++i) {
            const QRectF r = settingsRow(i);
            if (!r.contains(pt)) continue;
            if (i == 0) { setAmbient_ = !setAmbient_; saveSettings(); }
            else if (i == 1) { setSpectrum_ = !setSpectrum_; saveSettings(); }
            else { setDrag_ = i; setSettingFromX(i, pt.x()); }
            break;
        }
        break;
    }
    default: break;
    }
}

void PlayerWidget::mousePressEvent(QMouseEvent* e) {
    const QPointF pt = e->position();
    // v1.1: Thứ tự nav chuẩn: 0=Đang phát, 1=Lời, 2=Danh sách, 3=EQ, 4=Cài đặt
    static const Panel navPanels[5] = { Panel::None, Panel::Lyrics, Panel::Queue, Panel::Eq, Panel::Settings };
    for (int k = 0; k < 5; ++k) {
        if (navRects_[k].contains(pt)) {
            if (e->button() == Qt::LeftButton) {
                if (k == 0) panel_ = Panel::None;
                else togglePanel(navPanels[k]);
            }
            return;
        }
    }
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
        for (int k = 0; k < Equalizer::kBands; ++k) {
            if (QRectF(g.left + k * g.colW, g.top - 12, g.colW, g.h + 24).contains(e->position())) {
                player_.eq().setGain(k, 0.f);                 // nhấp đúp = đưa băng về 0 dB
                eqPreset_ = -1;
                saveEq();
                return;
            }
        }
    }
}

void PlayerWidget::mouseMoveEvent(QMouseEvent* e) {
    const QPointF pt = e->position();
    if (dragging_) { updateDrag(pt.x()); return; }
    if (eqDrag_ >= 0) { setEqFromY(eqDrag_, pt.y()); return; }
    if (setDrag_ >= 0) { setSettingFromX(setDrag_, pt.x()); return; }

    hoverProg_ = progRect_.adjusted(-8, -14, 8, 14).contains(pt);
    hoverRow_ = (panel_ == Panel::Queue && panelAnim_ > 0.5f) ? rowAt(pt) : -1;
    bool hand = hoverProg_ || hoverRow_ >= 0 || prevRect_.contains(pt) ||
                nextRect_.contains(pt) || playRect_.contains(pt) || shufRect_.contains(pt) || repRect_.contains(pt);
    for (const QRectF& r : navRects_) hand = hand || r.contains(pt);
    setCursor(hand ? Qt::PointingHandCursor : Qt::ArrowCursor);
}

void PlayerWidget::mouseReleaseEvent(QMouseEvent* e) {
    if (dragging_ && e->button() == Qt::LeftButton) {
        dragging_ = false;
        player_.seek(dragPos_);
    }
    eqDrag_ = -1;
    setDrag_ = -1;
}

void PlayerWidget::wheelEvent(QWheelEvent* e) {
    const double d = e->angleDelta().y();
    if (panel_ == Panel::Lyrics && panelAnim_ > 0.5f) {
        lyricScroll_ = std::clamp(lyricScroll_ - d * 0.7, 0.0, std::max(0.0, lyricTotalH_ - panelInner().height() + 100));
        lyricSnap_ = false;
        lyricManual_ = 3.0; // tạm dừng snap trong 3 giây
    } else if (panel_ == Panel::Queue && panelAnim_ > 0.5f) {
        queueScroll_ = std::clamp(queueScroll_ - d * 0.7, 0.0, std::max(0.0, playlist_.size() * kRowH - panelInner().height() + 80));
    } else {
        volume_ = std::clamp(volume_ + (d > 0 ? 0.04 : -0.04), 0.0, 1.0);
        applyVolume();
        saveSettings();
    }
}

void PlayerWidget::keyPressEvent(QKeyEvent* e) {
    if (e->modifiers() & Qt::ControlModifier) {
        if (e->key() == Qt::Key_S) { savePlaylist(); return; }
        if (e->key() == Qt::Key_O) { openDialog(false); return; }
        if (e->key() == Qt::Key_U) { openUrlDialog(); return; }
    }
    switch (e->key()) {
    case Qt::Key_Space: togglePlay(); break;
    case Qt::Key_Right: player_.seek(player_.position() + 5); break;
    case Qt::Key_Left:  player_.seek(player_.position() - 5); break;
    case Qt::Key_N:     next(false); break;
    case Qt::Key_P:     prev(); break;
    case Qt::Key_O:     openDialog(false); break;
    case Qt::Key_U:     openUrlDialog(); break;
    case Qt::Key_L:     togglePanel(Panel::Lyrics); break;
    case Qt::Key_Q:     togglePanel(Panel::Queue); break;
    case Qt::Key_E:     togglePanel(Panel::Eq); break;
    case Qt::Key_S:     shuffle_ = !shuffle_; break;
    case Qt::Key_R:     repeat_ = (repeat_ + 1) % 3; break;
    case Qt::Key_Escape: panel_ = Panel::None; break;
    case Qt::Key_Comma: togglePanel(Panel::Settings); break;
    case Qt::Key_Up:    volume_ = std::min(1.0, volume_ + 0.05); applyVolume(); saveSettings(); break;
    case Qt::Key_Down:  volume_ = std::max(0.0, volume_ - 0.05); applyVolume(); saveSettings(); break;
    default: QWidget::keyPressEvent(e);
    }
}

void PlayerWidget::dragEnterEvent(QDragEnterEvent* e) {
    if (e->mimeData()->hasUrls()) e->acceptProposedAction();
}

void PlayerWidget::dropEvent(QDropEvent* e) {
    QStringList files;
    for (const QUrl& u : e->mimeData()->urls()) {
        if (u.isLocalFile()) {
            const QString f = u.toLocalFile();
            if (f.endsWith(QStringLiteral(".fpl"), Qt::CaseInsensitive)) {
                loadPlaylist(f);
                return;
            }
            if (f.endsWith(QStringLiteral(".feq"), Qt::CaseInsensitive)) {
                loadEqPreset(f);
                return;
            }
            files << f;
        }
    }
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

QRectF PlayerWidget::queueAddRect() const {
    const QRectF in = panelInner();
    return QRectF(in.right() - 74, in.top() + 8, 74, 30);
}

QRectF PlayerWidget::queueClearRect() const {
    const QRectF in = panelInner();
    return QRectF(in.right() - 162, in.top() + 8, 80, 30);
}

QRectF PlayerWidget::presetRect(int i) const {
    const QRectF in = panelInner();
    const double w = (in.width() - 8 * 4) / 5.0;
    const int row = i / 5, col = i % 5;
    return QRectF(in.left() + col * (w + 8), in.top() + 46 + row * 34, w, 28);
}

QRectF PlayerWidget::eqToggleRect() const {
    const QRectF in = panelInner();
    return QRectF(in.right() - 48, in.top() + 4, 48, 28);
}

PlayerWidget::EqGeom PlayerWidget::eqGeom() const {
    const QRectF in = panelInner();
    const double top = in.top() + 148.0;
    const double h = std::max(120.0, in.bottom() - top - 70.0);
    const double colW = in.width() / Equalizer::kBands;
    return EqGeom{ in.left(), top, h, colW };
}

void PlayerWidget::setEqFromY(int band, double y) {
    const EqGeom g = eqGeom();
    const double f = std::clamp((y - g.top) / g.h, 0.0, 1.0);
    const float db = float((1.0 - f) * 24.0 - 12.0);
    player_.eq().setGain(band, db);
    eqPreset_ = -1;
    saveEq();
}

void PlayerWidget::loadEq() {
    QSettings s("FMusic", "Player");
    const bool on = s.value("eq/enabled", false).toBool();
    player_.eq().setEnabled(on);
    eqPreset_ = s.value("eq/preset", 0).toInt();
    for (int i = 0; i < Equalizer::kBands; ++i) {
        const float db = s.value(QString("eq/band_%1").arg(i), 0.0).toFloat();
        player_.eq().setGain(i, db);
    }
}

void PlayerWidget::saveEq() {
    QSettings s("FMusic", "Player");
    s.setValue("eq/enabled", player_.eq().enabled());
    s.setValue("eq/preset", eqPreset_);
    for (int i = 0; i < Equalizer::kBands; ++i)
        s.setValue(QString("eq/band_%1").arg(i), player_.eq().gain(i));
}

void PlayerWidget::computeLayout() {
    const double W = width(), H = height();
    const double panelW = std::clamp(W * 0.46, 360.0, 520.0);
    mainW_ = W - kSidebarW - panelW * panelAnim_;
    mainX_ = kSidebarW;

    const double availH = H - 220.0;
    const double artSide = std::clamp(std::min(mainW_ * 0.58, availH), 180.0, 480.0);
    const double cx = mainX_ + mainW_ / 2;
    const double cy = 40.0 + artSide / 2;
    artRect_ = QRectF(cx - artSide / 2, cy - artSide / 2, artSide, artSide);

    titleY_ = artRect_.bottom() + 18.0;
    progRect_ = QRectF(cx - std::min(mainW_ * 0.72, 440.0) / 2, titleY_ + 62.0, std::min(mainW_ * 0.72, 440.0), 24.0);

    const double ccy = progRect_.bottom() + 38.0;
    playRect_ = QRectF(cx - 30, ccy - 30, 60, 60);
    prevRect_ = QRectF(cx - 104, ccy - 22, 44, 44);
    nextRect_ = QRectF(cx + 60, ccy - 22, 44, 44);
    shufRect_ = QRectF(cx - 170, ccy - 18, 36, 36);
    repRect_  = QRectF(cx + 134, ccy - 18, 36, 36);

    // v1.1: Bỏ nút "+" ở trên đầu ứng dụng (đã có nút Thêm trong Queue)
    openRect_ = QRectF();

    panelRect_ = QRectF(W - panelW * panelAnim_ + 4, 18, panelW - 22, H - 36);
    for (int k = 0; k < 5; ++k) navRects_[k] = QRectF(8, 74 + k * 68, kSidebarW - 16, 62);
}

void PlayerWidget::ensureShadow() {
    const int art = int(artRect_.width());
    if (art == shadowArt_ && !shadowPix_.isNull()) return;
    shadowArt_ = art;
    shadowPad_ = int(art * 0.18);
    const int full = art + 2 * shadowPad_;
    QImage sh(full, full, QImage::Format_ARGB32_Premultiplied);
    sh.fill(Qt::transparent);
    {
        QPainter q(&sh);
        q.setRenderHint(QPainter::Antialiasing);
        q.setPen(Qt::NoPen);
        q.setBrush(QColor(0, 0, 0, 150));
        q.drawRoundedRect(QRectF(shadowPad_, shadowPad_ + art * 0.06, art, art), art * kCorner, art * kCorner);
    }
    boxBlur(sh, int(art * 0.08), 3);
    shadowPix_ = QPixmap::fromImage(sh);
}

// ------------------------------- vẽ giao diện -------------------------------

void PlayerWidget::paintEvent(QPaintEvent*) {
    const qint64 now = clock_.elapsed();
    dt_ = lastMs_ == 0 ? 0.016 : std::clamp(double(now - lastMs_) / 1000.0, 0.001, 0.1);
    lastMs_ = now;
    const double t = double(now) / 1000.0;

    // v1.1: Hoạt ảnh siêu mượt, loại bỏ khựng
    bgAngle_ += float(dt_ * 3.5); // góc xoay nền ambient tăng liên tục
    if (bgAngle_ > 3600.f) bgAngle_ -= 3600.f;

    float bands[kBands] = {0};
    const float bass = player_.spectrum(bands);
    bassSmooth_ += (bass - bassSmooth_) * float(std::min(1.0, dt_ * 12.0));

    const float target = player_.playing() ? 1.0f : 0.86f;
    artScale_ += (target - artScale_) * float(std::min(1.0, dt_ * 9.0));
    bgFade_ = std::min(1.f, bgFade_ + float(dt_ / 0.85));

    const float panelTarget = panel_ != Panel::None ? 1.f : 0.f;
    panelAnim_ += (panelTarget - panelAnim_) * float(std::min(1.0, dt_ * 10.0));
    if (std::abs(panelTarget - panelAnim_) < 0.003f) panelAnim_ = panelTarget;

    QPainter p(this);
    p.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform | QPainter::TextAntialiasing);
    computeLayout();
    drawBackground(p, t);
    drawArtAndSpectrum(p, bands);
    drawTexts(p);
    drawProgress(p);
    drawControls(p);
    drawSidebar(p);
    drawPanel(p);
}

// v1.1: Ambient Background xoay 2 lớp counter-rotating mịn màng như iOS
void PlayerWidget::drawBackdrop(QPainter& p, const QPixmap& pix, double t, double opacity) {
    if (pix.isNull() || opacity <= 0.0) return;
    const double W = width(), H = height(), S = std::max(W, H) * 1.65;
    p.save();
    p.setOpacity(opacity);
    // Lớp 1: xoay thuận với trôi dạt nhẹ
    p.translate(W / 2 + std::sin(t * 0.06) * W * 0.03, H / 2 + std::cos(t * 0.04) * H * 0.03);
    p.rotate(bgAngle_ * 0.35f + std::sin(t * 0.03) * 6.0);
    p.drawPixmap(QRectF(-S / 2, -S / 2, S, S), pix, QRectF(pix.rect()));
    // Lớp 2: xoay ngược, tỷ lệ lớn hơn để màu loang trộn
    p.setOpacity(opacity * 0.55);
    p.rotate(-bgAngle_ * 0.55f + 45.0);
    p.scale(1.22, 1.22);
    p.drawPixmap(QRectF(-S / 2, -S / 2, S, S), pix, QRectF(pix.rect()));
    p.restore();
}

void PlayerWidget::drawBackground(QPainter& p, double t) {
    p.fillRect(rect(), QColor(8, 8, 10));
    if (setAmbient_) {
        if (bgFade_ < 1.f) drawBackdrop(p, bgPrev_, t, 1.0);
        drawBackdrop(p, bg_, t, bgFade_);
    } else {
        QLinearGradient base(0, 0, width(), height());
        base.setColorAt(0, QColor(38, 34, 52));
        base.setColorAt(1, QColor(12, 12, 16));
        p.fillRect(rect(), base);
    }
    p.fillRect(rect(), QColor(0, 0, 0, 95));                        // dim tối
    QLinearGradient g(0, 0, 0, height());                           // vignette
    g.setColorAt(0.0, QColor(0, 0, 0, 25));
    g.setColorAt(1.0, QColor(0, 0, 0, 135));
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
    if (!shadowPix_.isNull()) {
        p.drawPixmap(QPointF(artRect_.left() - shadowPad_, artRect_.top() - shadowPad_ + art * 0.06), shadowPix_);
    }

    // Ảnh bìa
    if (!cover_.isNull()) {
        p.drawPixmap(artRect_, cover_, QRectF(cover_.rect()));
    }

    // Viền trắng mờ quanh bìa
    p.setPen(QPen(QColor(255, 255, 255, 28), 1));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(artRect_, art * kCorner, art * kCorner);

    // Spectrum ring (96 radial bars)
    if (setSpectrum_) {
        const double gap = art * 0.035, maxLen = art * 0.15;
        const QRectF ring = artRect_.adjusted(-gap, -gap, gap, gap);
        const double r = art * kCorner + gap;
        const double P = 2 * (ring.width() - 2 * r) + 2 * (ring.height() - 2 * r) + 2 * kPi * r;
        const int N = 96;
        const double bw = std::max(2.0, P / N * 0.52);

        for (int pass = 0; pass < 2; ++pass) {
            QPen pen(Qt::white, pass == 0 ? bw * 2.4 : bw, Qt::SolidLine, Qt::RoundCap);
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
    }
    p.restore();
}

void PlayerWidget::drawTexts(QPainter& p) {
    const double cx = mainX_ + mainW_ / 2, art = artRect_.width();
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
    p.setPen(QPen(col, 3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(col);

    auto skip = [&](QPointF c, bool forward) {
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
}

// v1.1: Thứ tự Sidebar chuẩn: [Đang phát] -> [Lời] -> [Danh sách] -> [EQ] -> [Cài đặt]
void PlayerWidget::drawSidebar(QPainter& p) {
    p.fillRect(QRectF(0, 0, kSidebarW, height()), QColor(0, 0, 0, 95));
    p.setPen(QPen(QColor(255, 255, 255, 24), 1));
    p.drawLine(QPointF(kSidebarW - 0.5, 0), QPointF(kSidebarW - 0.5, height()));

    p.setFont(uiFont(13, true));
    p.setPen(QColor(255, 255, 255, 235));
    p.drawText(QRectF(0, 20, kSidebarW, 22), Qt::AlignCenter, QStringLiteral("FMusic"));

    static const char* const labels[5] = { "Đang phát", "Lời", "Danh sách", "EQ", "Cài đặt" };
    static const Panel navPanels[5] = { Panel::None, Panel::Lyrics, Panel::Queue, Panel::Eq, Panel::Settings };

    for (int k = 0; k < 5; ++k) {
        const QRectF r = navRects_[k];
        const bool on = panel_ == navPanels[k];
        if (on) {
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(255, 255, 255, 40));
            p.drawRoundedRect(r, 14, 14);
            p.setBrush(Qt::NoBrush);
        }
        const QColor col(255, 255, 255, on ? 255 : 150);
        const QPointF c(r.center().x(), r.top() + 22);
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(col, 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        switch (k) {
        case 0: {                                           // nốt nhạc
            p.setBrush(col);
            p.drawEllipse(c + QPointF(-5, 6), 4.5, 3.6);
            p.drawEllipse(c + QPointF(7, 3), 4.5, 3.6);
            p.setBrush(Qt::NoBrush);
            p.drawLine(c + QPointF(-1, 5), c + QPointF(-1, -8));
            p.drawLine(c + QPointF(11, 2), c + QPointF(11, -11));
            p.drawLine(c + QPointF(-1, -8), c + QPointF(11, -11));
            break;
        }
        case 1:                                             // lời: dấu trích dẫn "Lời"
            p.setFont(uiFont(28, true));
            p.setPen(col);
            p.drawText(QRectF(c.x() - 16, c.y() - 15, 32, 34), Qt::AlignCenter, QStringLiteral("“"));
            break;
        case 2:                                             // danh sách: 3 vạch
            for (int i = 0; i < 3; ++i) {
                p.drawLine(c + QPointF(-3, -7 + i * 7), c + QPointF(10, -7 + i * 7));
                p.drawPoint(c + QPointF(-10, -7 + i * 7));
            }
            break;
        case 3:                                             // EQ: 3 thanh trượt
            for (int i = 0; i < 3; ++i) {
                const double x = c.x() - 8 + i * 8, ky = c.y() + (i == 1 ? -5 : (i == 0 ? 3 : 0));
                p.drawLine(QPointF(x, c.y() - 10), QPointF(x, c.y() + 10));
                p.setBrush(col);
                p.drawEllipse(QPointF(x, ky), 2.8, 2.8);
                p.setBrush(Qt::NoBrush);
            }
            break;
        default:                                            // bánh răng cài đặt
            p.drawEllipse(c, 4.5, 4.5);
            for (int i = 0; i < 8; ++i) {
                const double a = i * 3.14159265358979 / 4;
                p.drawLine(c + QPointF(std::cos(a) * 8, std::sin(a) * 8), c + QPointF(std::cos(a) * 11, std::sin(a) * 11));
            }
            p.drawEllipse(c, 8, 8);
            break;
        }
        p.setFont(uiFont(10, on));
        p.setPen(col);
        p.drawText(QRectF(r.left() - 4, r.top() + 40, r.width() + 8, 16), Qt::AlignCenter, QString::fromUtf8(labels[k]));
    }
    p.setBrush(Qt::NoBrush);
}

void PlayerWidget::drawPanel(QPainter& p) {
    if (panelAnim_ <= 0.005f) return;
    p.save();
    p.setOpacity(panelAnim_);
    // Nền kính mờ của panel
    p.setPen(QPen(QColor(255, 255, 255, 30), 1));
    p.setBrush(QColor(18, 18, 24, 215));
    p.drawRoundedRect(panelRect_, 18, 18);

    switch (shownPanel_) {
    case Panel::Lyrics:   drawLyrics(p);   break;
    case Panel::Queue:    drawQueue(p);    break;
    case Panel::Eq:       drawEq(p);       break;
    case Panel::Settings: drawSettings(p); break;
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
                                     : fm.boundingRect(QRect(0, 0, int(width), 1000), Qt::TextWordWrap, s).height() + 18.0;
        lyricH_[int(i)] = h;
        lyricY_[int(i)] = y;
        y += h;
    }
    lyricTotalH_ = y;
    lyricLayoutW_ = int(width);
}

void PlayerWidget::drawLyrics(QPainter& p) {
    const QRectF in = panelInner();
    if (int(in.width()) != lyricLayoutW_) layoutLyrics(in.width());

    const auto& lines = lyrics_.lines();
    if (lines.empty()) {
        p.setPen(QColor(255, 255, 255, 120));
        p.setFont(uiFont(15));
        p.drawText(in, Qt::AlignCenter | Qt::TextWordWrap,
                   QStringLiteral("Không có lời bài hát\nThêm tệp .lrc cùng tên cạnh tệp nhạc hoặc dùng tệp có lời nhúng"));
        return;
    }

    const double curT = player_.position() + setLyricOffsetMs_ / 1000.0;
    int curIdx = -1;
    if (lyrics_.synced()) {
        for (int i = 0; i < int(lines.size()); ++i)
            if (lines[size_t(i)].time <= curT) curIdx = i; else break;
    }

    // Tự cuộn mượt
    if (lyricSnap_ && curIdx >= 0 && curIdx < lyricY_.size()) {
        const double targetY = lyricY_[curIdx] - in.height() * 0.35;
        lyricScroll_ += (targetY - lyricScroll_) * std::min(1.0, dt_ * 5.0);
    }
    if (lyricManual_ > 0.0) {
        lyricManual_ -= dt_;
        if (lyricManual_ <= 0.0) lyricSnap_ = true;
    }

    p.save();
    p.setClipRect(in);
    for (int i = 0; i < int(lines.size()); ++i) {
        const double y = listTop() + lyricY_[i] - lyricScroll_;
        if (y + lyricH_[i] < in.top() || y > in.bottom()) continue;

        const bool act = (i == curIdx);
        p.setFont(uiFont(act ? 26 : 22, act));
        p.setPen(act ? QColor(255, 255, 255, 255) : QColor(255, 255, 255, 80));
        const QString s = fromUtf8(lines[size_t(i)].text);
        p.drawText(QRectF(in.left(), y, in.width(), lyricH_[i]),
                   Qt::AlignLeft | Qt::TextWordWrap, s.isEmpty() ? QStringLiteral("♪") : s);
    }
    p.restore();
}

// ---- Danh sách phát ----
void PlayerWidget::drawQueue(QPainter& p) {
    const QRectF in = panelInner();
    p.setPen(QColor(255, 255, 255, 240));
    p.setFont(uiFont(20, true));
    p.drawText(QRectF(in.left(), in.top(), in.width() * 0.5, 30), Qt::AlignLeft | Qt::AlignVCenter,
               QStringLiteral("Danh sách phát"));
    p.setPen(QColor(255, 255, 255, 120));
    p.setFont(uiFont(12));
    p.drawText(QRectF(in.left(), in.top() + 30, in.width() * 0.5, 20), Qt::AlignLeft | Qt::AlignVCenter,
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
        p.setPen(QColor(255, 255, 255, i == index_ ? 240 : 120));
        if (i == index_) {
            QFont sf("Segoe UI Symbol"); sf.setPixelSize(16); p.setFont(sf);
            p.drawText(QRectF(r.left() + 6, r.top(), 28, r.height()), Qt::AlignCenter, QStringLiteral("♪"));
        } else {
            p.setFont(uiFont(13));
            p.drawText(QRectF(r.left() + 6, r.top(), 28, r.height()), Qt::AlignCenter, QString::number(i + 1));
        }

        const double tw = r.width() - 40 - 56;
        p.setFont(uiFont(15, true));
        p.setPen(QColor(255, 255, 255, i == index_ ? 255 : 225));
        p.drawText(QRectF(r.left() + 40, r.top() + 8, tw, 22), Qt::AlignLeft | Qt::AlignVCenter,
                   fmTitle.elidedText(t.title, Qt::ElideRight, int(tw)));
        p.setFont(uiFont(13));
        p.setPen(QColor(255, 255, 255, 125));
        p.drawText(QRectF(r.left() + 40, r.top() + 30, tw, 18), Qt::AlignLeft | Qt::AlignVCenter,
                   fmArtist.elidedText(t.artist, Qt::ElideRight, int(tw)));

        p.setFont(uiFont(12));
        p.setPen(QColor(255, 255, 255, 110));
        p.drawText(QRectF(r.right() - 56, r.top(), 52, r.height()), Qt::AlignRight | Qt::AlignVCenter,
                   t.duration > 0 ? formatTime(t.duration) : QStringLiteral("--:--"));
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
    p.drawText(QRectF(in.left(), in.top(), in.width() * 0.6, 34), Qt::AlignLeft | Qt::AlignVCenter,
               QStringLiteral("Equalizer"));

    // Công tắc bật/tắt
    const QRectF sw = eqToggleRect();
    p.setPen(Qt::NoPen);
    p.setBrush(on ? QColor(52, 199, 89) : QColor(255, 255, 255, 70));
    p.drawRoundedRect(sw, sw.height() / 2, sw.height() / 2);
    p.setBrush(Qt::white);
    p.drawEllipse(QPointF(on ? sw.right() - sw.height() / 2 : sw.left() + sw.height() / 2, sw.center().y()),
                  sw.height() / 2 - 3, sw.height() / 2 - 3);
    p.setBrush(Qt::NoBrush);

    // Presets sẵn
    p.setFont(uiFont(12, true));
    for (int i = 0; i < Equalizer::kPresetCount; ++i) {
        const QRectF r = presetRect(i);
        const bool act = (i == eqPreset_);
        p.setPen(Qt::NoPen);
        p.setBrush(act ? QColor(255, 255, 255, 235) : QColor(255, 255, 255, 38));
        p.drawRoundedRect(r, 14, 14);
        p.setBrush(Qt::NoBrush);
        p.setPen(act ? QColor(20, 20, 24) : QColor(255, 255, 255, 220));
        p.drawText(r, Qt::AlignCenter, QString::fromUtf8(Equalizer::presetName(i)));
    }

    // 10 thanh trượt dọc
    const EqGeom g = eqGeom();
    const double dim = on ? 1.0 : 0.45;
    const double zeroY = g.top + g.h / 2;
    p.setPen(QPen(QColor(255, 255, 255, int(40 * dim)), 1, Qt::DashLine));
    p.drawLine(QPointF(g.left + 6, zeroY), QPointF(g.left + g.colW * Equalizer::kBands - 6, zeroY));
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

    // v1.1: Nút Lưu Preset FEQ và Tải FEQ
    eqSaveRect_ = QRectF(in.left() + 10, in.bottom() - 32, 120, 28);
    eqExportRect_ = QRectF(in.left() + 140, in.bottom() - 32, 120, 28);
    auto pillBtn = [&](const QRectF& r, const QString& title) {
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(255, 255, 255, 38));
        p.drawRoundedRect(r, 14, 14);
        p.setBrush(Qt::NoBrush);
        p.setPen(QColor(255, 255, 255, 220));
        p.setFont(uiFont(12, true));
        p.drawText(r, Qt::AlignCenter, title);
    };
    pillBtn(eqSaveRect_, QStringLiteral("💾 Lưu FEQ"));
    pillBtn(eqExportRect_, QStringLiteral("📂 Tải FEQ"));

    p.setPen(QColor(255, 255, 255, 105));
    p.setFont(uiFont(11));
    p.drawText(QRectF(in.left() + 270, in.bottom() - 30, in.width() - 270, 24), Qt::AlignRight | Qt::AlignVCenter,
               QStringLiteral("Nhấp đúp thanh: về 0 dB"));
}

// ------------------------------- Cài đặt -------------------------------

void PlayerWidget::loadSettings() {
    QSettings s("FMusic", "Player");
    setAmbient_ = s.value("ui/ambient", true).toBool();
    setSpectrum_ = s.value("ui/spectrum", true).toBool();
    setLyricOffsetMs_ = s.value("ui/lyricOffsetMs", 0.0).toDouble();
    volume_ = std::clamp(s.value("audio/volume", 1.0).toDouble(), 0.0, 1.0);
    sampleRate_ = s.value("audio/sampleRate", 44100).toInt();
    applyVolume();
}

void PlayerWidget::saveSettings() {
    QSettings s("FMusic", "Player");
    s.setValue("ui/ambient", setAmbient_);
    s.setValue("ui/spectrum", setSpectrum_);
    s.setValue("ui/lyricOffsetMs", setLyricOffsetMs_);
    s.setValue("audio/volume", volume_);
    s.setValue("audio/sampleRate", sampleRate_);
}

void PlayerWidget::applyVolume() {
    if (sink_) sink_->setVolume(float(volume_ * volume_));
}

QRectF PlayerWidget::settingsRow(int i) const {
    const QRectF in = panelInner();
    return QRectF(in.left(), in.top() + 56.0 + i * 64.0, in.width(), 56.0);
}

void PlayerWidget::setSettingFromX(int row, double x) {
    const QRectF r = settingsRow(row);
    const double f = std::clamp((x - r.left()) / r.width(), 0.0, 1.0);
    if (row == 2) { volume_ = f; applyVolume(); }
    else if (row == 3) { setLyricOffsetMs_ = std::round((f * 4000.0 - 2000.0) / 50.0) * 50.0; }
}

void PlayerWidget::drawSettings(QPainter& p) {
    const QRectF in = panelInner();
    p.setPen(QColor(255, 255, 255, 240));
    p.setFont(uiFont(20, true));
    p.drawText(QRectF(in.left(), in.top(), in.width(), 34), Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("Cài đặt"));

    auto sw = [&](const QRectF& r, bool on) {
        p.setPen(Qt::NoPen);
        p.setBrush(on ? QColor(52, 199, 89) : QColor(255, 255, 255, 70));
        p.drawRoundedRect(r, r.height() / 2, r.height() / 2);
        p.setBrush(Qt::white);
        p.drawEllipse(QPointF(on ? r.right() - r.height() / 2 : r.left() + r.height() / 2, r.center().y()),
                      r.height() / 2 - 3, r.height() / 2 - 3);
        p.setBrush(Qt::NoBrush);
    };
    auto label = [&](const QRectF& r, const QString& title, const QString& sub) {
        p.setPen(QColor(255, 255, 255, 235));
        p.setFont(uiFont(15, true));
        p.drawText(QRectF(r.left(), r.top() + 2, r.width() - 60, 22), Qt::AlignLeft | Qt::AlignVCenter, title);
        if (!sub.isEmpty()) {
            p.setPen(QColor(255, 255, 255, 120));
            p.setFont(uiFont(12));
            p.drawText(QRectF(r.left(), r.top() + 26, r.width() - 60, 18), Qt::AlignLeft | Qt::AlignVCenter, sub);
        }
    };
    auto slider = [&](const QRectF& r, const QString& title, const QString& value, double frac) {
        p.setPen(QColor(255, 255, 255, 235));
        p.setFont(uiFont(15, true));
        p.drawText(QRectF(r.left(), r.top() + 2, r.width() * 0.6, 22), Qt::AlignLeft | Qt::AlignVCenter, title);
        p.setPen(QColor(255, 255, 255, 150));
        p.setFont(uiFont(13));
        p.drawText(QRectF(r.right() - 110, r.top() + 2, 110, 22), Qt::AlignRight | Qt::AlignVCenter, value);
        const QRectF tr(r.left(), r.bottom() - 18, r.width(), 6);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(255, 255, 255, 60));
        p.drawRoundedRect(tr, 3, 3);
        QRectF fill = tr;
        fill.setWidth(std::max(6.0, tr.width() * frac));
        p.setBrush(QColor(255, 255, 255, 220));
        p.drawRoundedRect(fill, 3, 3);
        p.setBrush(Qt::white);
        p.drawEllipse(QPointF(tr.left() + tr.width() * frac, tr.center().y()), 8, 8);
        p.setBrush(Qt::NoBrush);
    };

    QRectF r = settingsRow(0);
    label(r, QStringLiteral("Nền ambient động"), QStringLiteral("Ảnh bìa làm mờ phủ toàn cửa sổ (chuẩn iOS)"));
    sw(QRectF(r.right() - 48, r.center().y() - 14, 48, 28), setAmbient_);

    r = settingsRow(1);
    label(r, QStringLiteral("Sóng nhạc quanh ảnh bìa"), QStringLiteral("Hiệu ứng spectrum 96 dải theo nhịp"));
    sw(QRectF(r.right() - 48, r.center().y() - 14, 48, 28), setSpectrum_);

    slider(settingsRow(2), QStringLiteral("Âm lượng"), QString("%1%").arg(int(std::round(volume_ * 100))), volume_);
    slider(settingsRow(3), QStringLiteral("Lệch thời gian lời"),
           QString("%1%2 ms").arg(setLyricOffsetMs_ > 0 ? "+" : "").arg(int(setLyricOffsetMs_)),
           (setLyricOffsetMs_ + 2000.0) / 4000.0);

    // v1.1: Tần số lấy mẫu (Sample Rate)
    r = settingsRow(4);
    label(r, QStringLiteral("Tần số lấy mẫu"), QStringLiteral("Tần số DAC xuất âm thanh (Hz)"));
    const int rates[3] = { 44100, 48000, 96000 };
    const QString rateLabels[3] = { QStringLiteral("44.1k"), QStringLiteral("48k"), QStringLiteral("96k") };
    const double bw = 54.0;
    for (int i = 0; i < 3; ++i) {
        srRects_[i] = QRectF(r.right() - 180 + i * (bw + 6), r.center().y() - 14, bw, 28);
        const bool act = (sampleRate_ == rates[i]);
        p.setPen(Qt::NoPen);
        p.setBrush(act ? QColor(52, 199, 89) : QColor(255, 255, 255, 38));
        p.drawRoundedRect(srRects_[i], 14, 14);
        p.setBrush(Qt::NoBrush);
        p.setPen(act ? Qt::white : QColor(255, 255, 255, 200));
        p.setFont(uiFont(12, true));
        p.drawText(srRects_[i], Qt::AlignCenter, rateLabels[i]);
    }

    // Giới thiệu
    r = settingsRow(5);
    p.setPen(QPen(QColor(255, 255, 255, 30), 1));
    p.drawLine(QPointF(r.left(), r.top() - 6), QPointF(r.right(), r.top() - 6));
    p.setPen(QColor(255, 255, 255, 235));
    p.setFont(uiFont(15, true));
    p.drawText(QRectF(r.left(), r.top() + 4, r.width(), 22), Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("FMusic 1.1"));
    p.setPen(QColor(255, 255, 255, 125));
    p.setFont(uiFont(12));
    const QString media = media_.available() ? QStringLiteral("Điều khiển media Windows: bật (SMTC)")
                        : hotkeysOn_         ? QStringLiteral("Phím media: dùng phím toàn cục (chế độ dự phòng)")
                                             : QStringLiteral("Phím media: chưa đăng ký được");
    p.drawText(QRectF(r.left(), r.top() + 28, r.width(), 18), Qt::AlignLeft | Qt::AlignVCenter, media);
    p.drawText(QRectF(r.left(), r.top() + 46, r.width(), 18), Qt::AlignLeft | Qt::AlignVCenter,
               QStringLiteral("Lõi C++ · FFmpeg · Hỗ trợ Streaming & Playlist .fpl & Preset .feq"));
}

// ------------------------------- Media Windows (SMTC / phím media) -------------------------------

void PlayerWidget::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    if (mediaInit_) return;
    mediaInit_ = true;

    MediaCallbacks cb;
    cb.play = [this] {
        QMetaObject::invokeMethod(this, [this] { if (!playlist_.isEmpty() && !player_.playing()) togglePlay(); }, Qt::QueuedConnection);
    };
    cb.pause = [this] {
        QMetaObject::invokeMethod(this, [this] { if (player_.playing()) togglePlay(); }, Qt::QueuedConnection);
    };
    cb.next = [this] {
        QMetaObject::invokeMethod(this, [this] { next(false); }, Qt::QueuedConnection);
    };
    cb.prev = [this] {
        QMetaObject::invokeMethod(this, [this] { prev(); }, Qt::QueuedConnection);
    };
    media_.init(reinterpret_cast<void*>(winId()), cb);
    if (!media_.available()) registerHotkeys();
    lastMediaKey_.clear();
    lastMediaState_ = -1;
    updateMedia();
}

void PlayerWidget::updateMedia() {
    if (!mediaInit_ || !media_.available()) return;
    const QString key = title_ + QChar(0x1f) + artist_;
    if (key != lastMediaKey_) {
        lastMediaKey_ = key;
        media_.setMetadata(title_.toUtf8().toStdString(), artist_.toUtf8().toStdString());
    }
    const int st = playlist_.isEmpty() ? 0 : (player_.playing() ? 2 : 1);
    if (st != lastMediaState_) {
        lastMediaState_ = st;
        media_.setPlaybackState(st);
    }
}

void PlayerWidget::registerHotkeys() {
#ifdef Q_OS_WIN
    const HWND h = reinterpret_cast<HWND>(winId());
    bool any = false;
    any = RegisterHotKey(h, 1, MOD_NOREPEAT, VK_MEDIA_PLAY_PAUSE) || any;
    any = RegisterHotKey(h, 2, MOD_NOREPEAT, VK_MEDIA_NEXT_TRACK) || any;
    any = RegisterHotKey(h, 3, MOD_NOREPEAT, VK_MEDIA_PREV_TRACK) || any;
    any = RegisterHotKey(h, 4, MOD_NOREPEAT, VK_MEDIA_STOP) || any;
    hotkeysOn_ = any;
#endif
}

void PlayerWidget::unregisterHotkeys() {
#ifdef Q_OS_WIN
    if (!hotkeysOn_) return;
    const HWND h = reinterpret_cast<HWND>(winId());
    for (int id = 1; id <= 4; ++id) UnregisterHotKey(h, id);
    hotkeysOn_ = false;
#endif
}

bool PlayerWidget::nativeEvent(const QByteArray& type, void* message, qintptr* result) {
#ifdef Q_OS_WIN
    if (hotkeysOn_ && type == "windows_generic_MSG") {
        const MSG* m = static_cast<const MSG*>(message);
        if (m->message == WM_HOTKEY) {
            switch (int(m->wParam)) {
            case 1: if (!playlist_.isEmpty()) togglePlay(); break;
            case 2: next(false); break;
            case 3: prev(); break;
            case 4: player_.pause(); break;
            default: break;
            }
            if (result) *result = 0;
            return true;
        }
    }
#endif
    return QWidget::nativeEvent(type, message, result);
}
