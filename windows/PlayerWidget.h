#pragma once
#include <QAudioSink>
#include <QElapsedTimer>
#include <QIODevice>
#include <QPixmap>
#include <QRectF>
#include <QStringList>
#include <QTimer>
#include <QVector>
#include <QWidget>
#include "Lyrics.h"
#include "MediaSession.h"
#include "Player.h"

// Thiết bị "pull": QAudioSink gọi readData() -> Player::pull() (luôn trả đủ dữ liệu, thiếu thì im lặng).
class PcmDevice : public QIODevice {
public:
    explicit PcmDevice(Player& p, QObject* parent = nullptr) : QIODevice(parent), p_(p) {}
    qint64 readData(char* data, qint64 maxlen) override {
        const qint64 frames = maxlen / qint64(2 * sizeof(float));
        p_.pull(reinterpret_cast<float*>(data), size_t(frames));
        return frames * qint64(2 * sizeof(float));
    }
    qint64 writeData(const char*, qint64) override { return -1; }
    bool isSequential() const override { return true; }
    qint64 bytesAvailable() const override { return qint64(1 << 20) + QIODevice::bytesAvailable(); }
private:
    Player& p_;
};

class PlayerWidget : public QWidget {
    Q_OBJECT
public:
    explicit PlayerWidget(QWidget* parent = nullptr);
    ~PlayerWidget() override;
    void openFiles(const QStringList& files);   // thay danh sách phát và phát bài đầu
    void addFiles(const QStringList& files);    // thêm vào cuối danh sách

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void showEvent(QShowEvent*) override;
    bool nativeEvent(const QByteArray& eventType, void* message, qintptr* result) override;
    void mouseDoubleClickEvent(QMouseEvent*) override;
    void wheelEvent(QWheelEvent*) override;
    void keyPressEvent(QKeyEvent*) override;
    void dragEnterEvent(QDragEnterEvent*) override;
    void dropEvent(QDropEvent*) override;

private:
    struct Track { QString path, title, artist; double duration = 0.0; bool probed = false; };
    struct EqGeom { double left, top, h, colW; };
    enum class Panel { None, Lyrics, Queue, Eq, Settings };

    // --- phát nhạc / danh sách ---
    void loadIndex(int i, bool autoplay);
    void loadLyrics(const QString& path, const TrackInfo& ti);
    void setCover(const QImage& img);
    void next(bool automatic = false);
    void prev();
    void togglePlay();
    void openDialog(bool append);
    void removeTrack(int i);
    void clearPlaylist();
    void addTracks(const QStringList& files);
    void onTick();
    void updateDrag(double x);

    // --- panel ---
    void togglePanel(Panel p);
    void handlePanelPress(const QPointF& pt, Qt::MouseButton b);
    QRectF panelInner() const;
    double listTop() const;
    int    rowAt(const QPointF& pt) const;
    QRectF queueAddRect() const;
    QRectF queueClearRect() const;
    QRectF presetRect(int i) const;
    QRectF eqToggleRect() const;
    EqGeom eqGeom() const;
    void   setEqFromY(int band, double y);
    void   loadEq();
    void   saveEq();
    void   layoutLyrics(double width);
    void   loadSettings();
    void   saveSettings();
    QRectF settingsRow(int i) const;
    void   setSettingFromX(int row, double x);
    void   applyVolume();
    void   updateMedia();
    void   registerHotkeys();
    void   unregisterHotkeys();

    // --- vẽ ---
    void computeLayout();
    void ensureShadow();
    void drawBackdrop(QPainter& p, const QPixmap& pix, double t, double opacity);
    void drawBackground(QPainter& p, double t);
    void drawArtAndSpectrum(QPainter& p, const float* bands);
    void drawTexts(QPainter& p);
    void drawProgress(QPainter& p);
    void drawControls(QPainter& p);
    void drawSidebar(QPainter& p);
    void drawSettings(QPainter& p);
    void drawPanel(QPainter& p);
    void drawLyrics(QPainter& p);
    void drawQueue(QPainter& p);
    void drawEq(QPainter& p);

    Player player_;
    MediaSession media_;
    bool mediaInit_ = false, hotkeysOn_ = false;
    QString lastMediaKey_;
    int lastMediaState_ = -1;
    QAudioSink* sink_ = nullptr;
    PcmDevice*  pcm_  = nullptr;
    QTimer tick_, frame_;
    QElapsedTimer clock_;
    qint64 lastMs_ = 0;
    double dt_ = 0.016;

    // danh sách phát
    QVector<Track> playlist_;
    int index_ = -1, probeCursor_ = 0;
    bool shuffle_ = false;
    int repeat_ = 0;                   // 0 tắt, 1 lặp danh sách, 2 lặp 1 bài
    QString title_, artist_;

    // hình ảnh
    QPixmap cover_, bg_, bgPrev_, shadowPix_;
    int shadowArt_ = 0, shadowPad_ = 0;
    float bgFade_ = 1.f, bassSmooth_ = 0.f, artScale_ = 0.86f, progThick_ = 5.f;

    // tương tác
    bool dragging_ = false, hoverProg_ = false;
    double dragPos_ = 0.0;
    int hoverRow_ = -1, eqDrag_ = -1, eqPreset_ = 0;

    // panel bên phải
    Panel panel_ = Panel::None, shownPanel_ = Panel::Lyrics;
    float panelAnim_ = 0.f;
    Lyrics lyrics_;
    QVector<double> lyricH_, lyricY_;
    int lyricLayoutW_ = -1;
    double lyricScroll_ = 0.0, lyricManual_ = 0.0, lyricTotalH_ = 0.0;
    bool lyricSnap_ = true;
    double queueScroll_ = 0.0;

    // layout
    double mainX_ = 0.0, mainW_ = 0.0, titleY_ = 0.0;

    // cài đặt
    bool setAmbient_ = true, setSpectrum_ = true;
    double setLyricOffsetMs_ = 0.0, volume_ = 1.0;
    int setDrag_ = -1;
    QRectF artRect_, progRect_, prevRect_, playRect_, nextRect_, openRect_, shufRect_, repRect_, panelRect_;
    QRectF navRects_[5];
};
