#include "i18n/LanguageManager.h"
#include "license/LicenseManager.h"
#include "ui/LicenseDialog.h"
#include "ui/MainWindow.h"
#include "ui/UiSettings.h"
#include "ui/UiThemeManager.h"

#include <QApplication>
#include <QFontDatabase>
#include <QIcon>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("VisionSelect"));
    app.setOrganizationName(QStringLiteral("VisionSelect"));
    app.setFont(QFontDatabase::systemFont(QFontDatabase::GeneralFont));
    app.setWindowIcon(QIcon(QStringLiteral(":/icons/visionselect_icon_256.png")));
    UiSettings::instance().initialize();
    LanguageManager::instance().loadSavedLanguage();
    UiThemeManager::instance().initialize(&app);

    LicenseManager licenseManager;
    if (!licenseManager.currentStatus().isValid()) {
        LicenseDialog dialog(&licenseManager);
        if (dialog.exec() != QDialog::Accepted)
            return 0;
    }

    MainWindow window;
    window.setMinimumSize(1024, 700);
    window.resize(1280, 820);
    UiSettings::instance().restoreWindow(&window);
    window.show();

    return app.exec();
}
