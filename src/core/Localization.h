#ifndef LOCALIZATION_H
#define LOCALIZATION_H

#include <QString>
#include <QStringList>

namespace CoreI18n {

QString localizedText(const char *zhUtf8, const char *enUtf8);
QString localizedDiagnostic(const QString &value);
QString localizedDiagnosticForLanguage(const QString &value, const QString &languageCode);
QStringList localizedDiagnostics(const QStringList &values);
QStringList localizedDiagnosticsForLanguage(const QStringList &values, const QString &languageCode);

}

#endif
