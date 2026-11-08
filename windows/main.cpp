#include <QApplication>
#include <QIcon>
#include "PlayerWidget.h"

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    app.setApplicationName("FMusic");
    app.setOrganizationName("FMusic");
    app.setApplicationDisplayName("FMusic");
    app.setWindowIcon(QIcon(":/fmusic.png"));

    PlayerWidget w;
    w.setWindowTitle("FMusic");
    w.setMinimumSize(820, 680);
    w.resize(1100, 780);
    w.show();

    QStringList args = app.arguments();
    args.removeFirst();
    if (!args.isEmpty()) w.openFiles(args);
    return app.exec();
}
