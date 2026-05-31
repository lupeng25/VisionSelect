#include "i18n/LanguageManager.h"
#include "license/LicenseManager.h"
#include "ui/LicenseDialog.h"
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
    LanguageManager::instance().loadSavedLanguage();

    QFile styleFile(QStringLiteral(":/style.qss"));
    if (styleFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream stream(&styleFile);
        stream.setCodec("UTF-8");
        app.setStyleSheet(stream.readAll());
    }

    LicenseManager licenseManager;
    if (!licenseManager.currentStatus().isValid()) {
        LicenseDialog dialog(&licenseManager);
        if (dialog.exec() != QDialog::Accepted)
            return 0;
    }

    MainWindow window;
    window.resize(1280, 820);
    window.show();

    return app.exec();
}
