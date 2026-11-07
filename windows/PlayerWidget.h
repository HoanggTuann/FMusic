#pragma once
#include <QAudioSink>
#include <QElapsedTimer>
#include <QIODevice>
#include <QWidget>
#include <QPixmap>
#include <QStringList>
#include <QTimer>
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
    void openFiles(const QStringList& files);

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void keyPressEvent(QKeyEvent*) override;
    void dragEnterEvent(QDragEnterEvent*) override;
    void dropEvent(QDropEvent*) override;

private:
    void loadIndex(int i, bool autoplay);
    void setCover(const QImage& img);
    void next();
    void prev();
    void togglePlay();
    void openDialog();
    void onTick();
    void updateDrag(double x);

    void computeLayout();
    void ensureShadow();
    void drawBackdrop(QPainter& p, const QPixmap& pix, double t, double opacity);
    void drawBackground(QPainter& p, double t);
    void drawArtAndSpectrum(QPainter& p, const float* bands);
    void drawTexts(QPainter& p);
    void drawProgress(QPainter& p);
    void drawControls(QPainter& p);

    Player player_;
    QAudioSink* sink_ = nullptr;
    PcmDevice*  pcm_  = nullptr;
    QTimer tick_, frame_;
    QElapsedTimer clock_;
    qint64 lastMs_ = 0;

    QStringList playlist_;
    int index_ = -1;
    QString title_, artist_;

    QPixmap cover_, bg_, bgPrev_, shadowPix_;
    int shadowArt_ = 0, shadowPad_ = 0;
    float bgFade_ = 1.f, bassSmooth_ = 0.f, artScale_ = 0.86f, progThick_ = 5.f;

    bool dragging_ = false, hoverProg_ = false;
    double dragPos_ = 0.0;

    QRectF artRect_, progRect_, prevRect_, playRect_, nextRect_, openRect_;
    double titleY_ = 0.0;
};
