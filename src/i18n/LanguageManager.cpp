#include "i18n/LanguageManager.h"

#include <QCoreApplication>
#include <QSettings>
#include <QStringList>
#include <QTranslator>

namespace {
const char *kSettingsKey = "ui/language";
const char *kDefaultLanguage = "zh_CN";
}

LanguageManager &LanguageManager::instance()
{
    static LanguageManager manager;
    return manager;
}

LanguageManager::LanguageManager(QObject *parent)
    : QObject(parent),
      m_currentLanguage(QString::fromLatin1(kDefaultLanguage)),
      m_translator(new QTranslator(this))
{
}

QString LanguageManager::currentLanguage() const
{
    return m_currentLanguage;
}

QStringList LanguageManager::availableLanguages() const
{
    return {QStringLiteral("zh_CN"), QStringLiteral("en_US")};
}

QString LanguageManager::displayName(const QString &languageCode) const
{
    if (languageCode == QLatin1String("en_US"))
        return QStringLiteral("English");
    return QString::fromUtf8("\344\270\255\346\226\207");
}

void LanguageManager::loadSavedLanguage()
{
    QSettings settings;
    const QString saved = settings.value(QString::fromLatin1(kSettingsKey), QString::fromLatin1(kDefaultLanguage)).toString();
    setLanguage(saved);
}

bool LanguageManager::setLanguage(const QString &languageCode)
{
    const QString normalized = availableLanguages().contains(languageCode)
        ? languageCode
        : QString::fromLatin1(kDefaultLanguage);

    QCoreApplication *application = QCoreApplication::instance();
    if (application)
        application->removeTranslator(m_translator);

    bool loaded = true;
    if (normalized == QLatin1String("zh_CN")) {
        loaded = m_translator->load(QStringLiteral(":/i18n/visionselect_zh_CN.qm"));
        if (loaded && application)
            application->installTranslator(m_translator);
    } else if (normalized == QLatin1String("en_US")) {
        loaded = m_translator->load(QStringLiteral(":/i18n/visionselect_en_US.qm"));
        if (loaded && application)
            application->installTranslator(m_translator);
    }

    m_currentLanguage = loaded ? normalized : QString::fromLatin1(kDefaultLanguage);
    QSettings settings;
    settings.setValue(QString::fromLatin1(kSettingsKey), m_currentLanguage);
    emit languageChanged();
    return loaded;
}
