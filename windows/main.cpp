#include <QApplication>
#include "PlayerWidget.h"

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    PlayerWidget w;
    w.setWindowTitle("Music Player");
    w.setMinimumSize(480, 660);
    w.resize(900, 780);
    w.show();

    QStringList args = app.arguments();
    args.removeFirst();
    if (!args.isEmpty()) w.openFiles(args);
    return app.exec();
}
