#ifndef LANGUAGEMANAGER_H
#define LANGUAGEMANAGER_H

#include <QObject>
#include <QString>
#include <QStringList>

class QTranslator;

class LanguageManager : public QObject
{
    Q_OBJECT

public:
    static LanguageManager &instance();

    QString currentLanguage() const;
    QStringList availableLanguages() const;
    QString displayName(const QString &languageCode) const;

    void loadSavedLanguage();
    bool setLanguage(const QString &languageCode);

signals:
    void languageChanged();

private:
    explicit LanguageManager(QObject *parent = nullptr);
    LanguageManager(const LanguageManager &) = delete;
    LanguageManager &operator=(const LanguageManager &) = delete;

    QString m_currentLanguage;
    QTranslator *m_translator = nullptr;
};

#endif
