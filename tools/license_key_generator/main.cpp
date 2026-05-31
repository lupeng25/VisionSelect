#include "LicenseGeneratorWindow.h"

#include <QApplication>
#include <QFont>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setOrganizationName(QStringLiteral("VisionSelect"));
    app.setApplicationName(QStringLiteral("LicenseKeyGenerator"));
    app.setFont(QFont(QStringLiteral("Microsoft YaHei UI"), 9));

    LicenseGeneratorWindow window;
    window.resize(860, 620);
    window.show();

    return app.exec();
}
