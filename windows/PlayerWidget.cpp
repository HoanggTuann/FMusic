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
#include <QPainter>
#include <QPainterPath>
#include <QUrl>
#include <algorithm>
#include <cmath>
#include <vector>

#include "CoverArt.h"

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

PlayerWidget::PlayerWidget(QWidget* parent) : QOpenGLWidget(parent) {
    setAcceptDrops(true);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);

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
    sink_->setBufferSize(Player::kSampleRate / 10 * 2 * int(sizeof(float)));   // ~100 ms: độ trễ hình/tiếng thấp
    pcm_ = new PcmDevice(player_, this);
    pcm_->open(QIODevice::ReadOnly);
    sink_->start(pcm_);                          // chạy liên tục; Player tự xuất im lặng khi pause

    connect(&tick_, &QTimer::timeout, this, [this] { onTick(); });
    tick_.start(100);
    clock_.start();

    // Vẽ liên tục theo vsync
    connect(this, &QOpenGLWidget::frameSwapped, this, [this] { update(); });

    setCover(QImage());          // ảnh bìa mặc định cho tới khi có bài hát
}

PlayerWidget::~PlayerWidget() {
    if (sink_) sink_->stop();    // dừng audio thread trước khi Player bị huỷ
}

void PlayerWidget::openFiles(const QStringList& files) {
    if (files.isEmpty()) return;
    playlist_ = files;
    loadIndex(0, true);
}

void PlayerWidget::loadIndex(int i, bool autoplay) {
    if (i < 0 || i >= playlist_.size()) return;
    index_ = i;
    std::string err;
    if (!player_.load(playlist_[i].toUtf8().toStdString(), &err)) {
        title_ = QStringLiteral("Không thể mở tệp");
        artist_ = QString::fromUtf8(err.c_str());
        return;
    }
    const TrackInfo& ti = player_.info();
    title_  = QString::fromUtf8(ti.title.c_str());
    artist_ = QString::fromUtf8(ti.artist.c_str());
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
    if (autoplay) player_.play();
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
    const int side = std::min(std::min(img.width(), img.height()), 900);
    const QImage sq = img.copy((img.width() - std::min(img.width(), img.height())) / 2,
                               (img.height() - std::min(img.width(), img.height())) / 2,
                               std::min(img.width(), img.height()), std::min(img.width(), img.height()))
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

void PlayerWidget::next() {
    if (index_ + 1 < playlist_.size()) loadIndex(index_ + 1, true);
    else { player_.pause(); player_.seek(0); }
}

void PlayerWidget::prev() {
    if (player_.position() > 3.0 || index_ <= 0) player_.seek(0);
    else loadIndex(index_ - 1, true);
}

void PlayerWidget::togglePlay() {
    if (playlist_.isEmpty()) { openDialog(); return; }
    if (player_.playing()) { player_.pause(); return; }
    if (player_.ended()) player_.seek(0);
    player_.play();
}

void PlayerWidget::openDialog() {
    const QStringList f = QFileDialog::getOpenFileNames(
        this, QStringLiteral("Chọn nhạc"), QString(),
        QStringLiteral("Âm thanh (*.mp3 *.flac *.m4a *.aac *.wav *.ogg *.opus *.wma *.ape *.aiff *.wv);;Tất cả (*)"));
    openFiles(f);
}

void PlayerWidget::onTick() {
    if (player_.playing() && player_.ended()) next();     // tự chuyển bài
}

// ------------------------------- input -------------------------------

void PlayerWidget::updateDrag(double x) {
    const double f = std::clamp((x - progRect_.left()) / progRect_.width(), 0.0, 1.0);
    dragPos_ = f * player_.duration();
}

void PlayerWidget::mousePressEvent(QMouseEvent* e) {
    const QPointF pt = e->position();
    if (openRect_.contains(pt))  { openDialog(); return; }
    if (prevRect_.contains(pt))  { prev(); return; }
    if (nextRect_.contains(pt))  { next(); return; }
    if (playRect_.contains(pt))  { togglePlay(); return; }
    if (progRect_.adjusted(-8, -14, 8, 14).contains(pt) && player_.duration() > 0) {
        dragging_ = true;
        updateDrag(pt.x());
    }
}

void PlayerWidget::mouseMoveEvent(QMouseEvent* e) {
    const QPointF pt = e->position();
    if (dragging_) updateDrag(pt.x());
    hoverProg_ = progRect_.adjusted(-8, -14, 8, 14).contains(pt);
    const bool hand = hoverProg_ || openRect_.contains(pt) || prevRect_.contains(pt) ||
                      nextRect_.contains(pt) || playRect_.contains(pt);
    setCursor(hand ? Qt::PointingHandCursor : Qt::ArrowCursor);
}

void PlayerWidget::mouseReleaseEvent(QMouseEvent*) {
    if (dragging_) { player_.seek(dragPos_); dragging_ = false; }
}

void PlayerWidget::keyPressEvent(QKeyEvent* e) {
    switch (e->key()) {
    case Qt::Key_Space: togglePlay(); break;
    case Qt::Key_Right: player_.seek(player_.position() + 5); break;
    case Qt::Key_Left:  player_.seek(player_.position() - 5); break;
    case Qt::Key_N:     next(); break;
    case Qt::Key_P:     prev(); break;
    case Qt::Key_O:     openDialog(); break;
    case Qt::Key_Up:    sink_->setVolume(std::min(1.0, sink_->volume() + 0.05)); break;
    case Qt::Key_Down:  sink_->setVolume(std::max(0.0, sink_->volume() - 0.05)); break;
    default: QOpenGLWidget::keyPressEvent(e);
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

void PlayerWidget::computeLayout() {
    const double W = width(), H = height(), cx = W / 2;
    const double art  = std::clamp(std::min(W * 0.52, H * 0.40), 140.0, 560.0);
    const double ring = art * 0.18;                       // chỗ cho vòng spectrum
    const double textH = 64, progH = 46, ctrlH = 92;
    const double total = ring + art + ring + textH + progH + ctrlH;
    const double top = std::max(24.0, (H - total) / 2.0) + ring;

    artRect_ = QRectF(cx - art / 2, top, art, art);
    double y = artRect_.bottom() + ring + 6;
    titleY_ = y;
    y += textH;
    const double pw = std::min(W * 0.8, std::max(art * 1.5, 320.0));
    progRect_ = QRectF(cx - pw / 2, y + 8, pw, 6);
    y += progH;
    const double cy = y + 38;
    playRect_ = QRectF(cx - 32, cy - 32, 64, 64);
    prevRect_ = QRectF(cx - 130, cy - 28, 56, 56);
    nextRect_ = QRectF(cx + 74, cy - 28, 56, 56);
    openRect_ = QRectF(18, 18, 40, 40);
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

void PlayerWidget::paintGL() {
    const qint64 ms = clock_.elapsed();
    const double dt = std::min(0.1, (ms - lastMs_) / 1000.0);
    lastMs_ = ms;
    const double t = ms / 1000.0;

    float bands[kBands];
    const float bass = player_.spectrum(bands);
    bassSmooth_ += (bass - bassSmooth_) * 0.25f;
    const float target = player_.playing() ? 1.0f : 0.86f;       // ảnh bìa thu nhỏ khi tạm dừng (như Apple Music)
    artScale_ += (target - artScale_) * float(std::min(1.0, dt * 9.0));
    bgFade_ = std::min(1.f, bgFade_ + float(dt / 0.9));

    QPainter p(this);
    p.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform | QPainter::TextAntialiasing);
    computeLayout();
    drawBackground(p, t);
    drawArtAndSpectrum(p, bands);
    drawTexts(p);
    drawProgress(p);
    drawControls(p);
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
    const double W = width(), art = artRect_.width();
    const double tw = std::min(W - 40, std::max(art * 1.5, 320.0));
    QFont tf("Segoe UI Semibold");
    tf.setPixelSize(int(std::max(18.0, art * 0.055)));
    QFont af("Segoe UI");
    af.setPixelSize(int(std::max(15.0, art * 0.042)));

    p.setFont(tf);
    p.setPen(QColor(255, 255, 255, 240));
    p.drawText(QRectF(W / 2 - tw / 2, titleY_, tw, tf.pixelSize() * 1.5), Qt::AlignCenter,
               QFontMetrics(tf).elidedText(title_, Qt::ElideRight, int(tw)));
    p.setFont(af);
    p.setPen(QColor(255, 255, 255, 150));
    p.drawText(QRectF(W / 2 - tw / 2, titleY_ + tf.pixelSize() * 1.55, tw, af.pixelSize() * 1.5), Qt::AlignCenter,
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

    // nút "+" mở nhạc
    p.setPen(QPen(QColor(255, 255, 255, 200), 2, Qt::SolidLine, Qt::RoundCap));
    p.setBrush(QColor(255, 255, 255, 28));
    p.drawEllipse(openRect_);
    const QPointF o = openRect_.center();
    p.drawLine(o + QPointF(-7, 0), o + QPointF(7, 0));
    p.drawLine(o + QPointF(0, -7), o + QPointF(0, 7));
}