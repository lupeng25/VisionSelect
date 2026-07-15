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
      m_currentLanguage(QString::fromLatin1(kDefaultLanguage))
{
}

QString LanguageManager::currentLanguage() const
{
    QReadLocker locker(&m_languageLock);
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

    if (normalized == currentLanguage() && m_translatorInstalled)
        return true;

    QTranslator *candidate = new QTranslator(this);
    bool loaded = false;
    if (normalized == QLatin1String("zh_CN")) {
        loaded = candidate->load(QStringLiteral(":/i18n/visionselect_zh_CN.qm"));
    } else if (normalized == QLatin1String("en_US")) {
        loaded = candidate->load(QStringLiteral(":/i18n/visionselect_en_US.qm"));
    }

    if (!loaded) {
        delete candidate;
        return false;
    }

    QCoreApplication *application = QCoreApplication::instance();
    if (application && m_translatorInstalled && m_translator)
        application->removeTranslator(m_translator);
    if (application)
        application->installTranslator(candidate);
    delete m_translator;
    m_translator = candidate;
    m_translatorInstalled = application != nullptr;
    {
        QWriteLocker locker(&m_languageLock);
        m_currentLanguage = normalized;
    }
    QSettings settings;
    settings.setValue(QString::fromLatin1(kSettingsKey), normalized);
    emit languageChanged();
    return true;
}
