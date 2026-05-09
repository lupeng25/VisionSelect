#ifndef CATALOGREPOSITORY_H
#define CATALOGREPOSITORY_H

#include "core/SelectionTypes.h"

#include <QHash>
#include <QString>
#include <QStringList>
#include <QVector>

class QIODevice;

class CatalogRepository
{
public:
    bool loadDefaults(QString *errorMessage = nullptr);
    bool loadCameraCsv(const QString &filePath, QString *errorMessage = nullptr);
    bool loadLensCsv(const QString &filePath, QString *errorMessage = nullptr);
    bool loadLightCsv(const QString &filePath, QString *errorMessage = nullptr);

    const QVector<CameraSpec> &cameras() const { return m_cameras; }
    const QVector<LensSpec> &lenses() const { return m_lenses; }
    const QVector<LightSpec> &lights() const { return m_lights; }

    QString summary() const;

private:
    using Row = QHash<QString, QString>;

    QVector<CameraSpec> m_cameras;
    QVector<LensSpec> m_lenses;
    QVector<LightSpec> m_lights;

    static bool readCsvRows(const QString &filePath, QVector<Row> *rows, QString *errorMessage);
    static bool readCsvRowsFromDevice(QIODevice *device, const QString &sourceName, QVector<Row> *rows, QString *errorMessage);
    static QStringList parseCsvLine(const QString &line);
    static bool ensureColumns(const QVector<Row> &rows, const QStringList &required, const QString &sourceName, QString *errorMessage);

    static double number(const Row &row, const QString &key, double defaultValue = 0.0);
    static int integer(const Row &row, const QString &key, int defaultValue = 0);
    static QString text(const Row &row, const QString &key);

    bool loadCameraRows(const QVector<Row> &rows, const QString &sourceName, QString *errorMessage);
    bool loadLensRows(const QVector<Row> &rows, const QString &sourceName, QString *errorMessage);
    bool loadLightRows(const QVector<Row> &rows, const QString &sourceName, QString *errorMessage);
};

#endif
