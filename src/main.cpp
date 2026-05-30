#include "ui/MainWindow.h"

#include <QApplication>
#include <QFile>
#include <QFont>
#include <QIcon>
#include <QTextStream>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("VisionSelect"));
    app.setOrganizationName(QStringLiteral("VisionSelect"));
    app.setFont(QFont(QStringLiteral("Microsoft YaHei UI"), 9));
    app.setWindowIcon(QIcon(QStringLiteral(":/icons/visionselect_icon_256.png")));

    QFile styleFile(QStringLiteral(":/style.qss"));
    if (styleFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream stream(&styleFile);
        stream.setCodec("UTF-8");
        app.setStyleSheet(stream.readAll());
    }

    MainWindow window;
    window.resize(1280, 820);
    window.show();

    return app.exec();
}
