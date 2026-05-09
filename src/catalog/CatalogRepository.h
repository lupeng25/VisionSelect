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
    void setStorageDirectory(const QString &directory);
    QString storageDirectory() const;

    bool loadDefaults(QString *errorMessage = nullptr);
    bool loadCameraCsv(const QString &filePath, QString *errorMessage = nullptr);
    bool loadLensCsv(const QString &filePath, QString *errorMessage = nullptr);
    bool loadLightCsv(const QString &filePath, QString *errorMessage = nullptr);
    bool exportCameraCsv(const QString &filePath, QString *errorMessage = nullptr) const;
    bool exportLensCsv(const QString &filePath, QString *errorMessage = nullptr) const;
    bool resetCamerasToBuiltIn(QString *errorMessage = nullptr);
    bool resetLensesToBuiltIn(QString *errorMessage = nullptr);
    bool addCamera(const CameraSpec &camera, QString *errorMessage = nullptr);
    bool updateCamera(int index, const CameraSpec &camera, QString *errorMessage = nullptr);
    bool removeCamera(int index, QString *errorMessage = nullptr);
    bool addLens(const LensSpec &lens, QString *errorMessage = nullptr);
    bool updateLens(int index, const LensSpec &lens, QString *errorMessage = nullptr);
    bool removeLens(int index, QString *errorMessage = nullptr);

    const QVector<CameraSpec> &cameras() const { return m_cameras; }
    const QVector<LensSpec> &lenses() const { return m_lenses; }
    const QVector<LightSpec> &lights() const { return m_lights; }

    QString summary() const;

private:
    using Row = QHash<QString, QString>;

    QVector<CameraSpec> m_cameras;
    QVector<LensSpec> m_lenses;
    QVector<LightSpec> m_lights;
    QString m_storageDirectory;

    QString effectiveStorageDirectory() const;
    QString cameraStoragePath() const;
    QString lensStoragePath() const;
    bool ensureLocalCatalogs(QString *errorMessage);
    bool copyResourceToFile(const QString &resourcePath, const QString &filePath, bool overwrite, QString *errorMessage) const;
    bool saveCameras(QString *errorMessage) const;
    bool saveLenses(QString *errorMessage) const;
    bool writeCameraCsv(const QString &filePath, const QVector<CameraSpec> &cameras, QString *errorMessage) const;
    bool writeLensCsv(const QString &filePath, const QVector<LensSpec> &lenses, QString *errorMessage) const;
    static bool readCsvRows(const QString &filePath, QVector<Row> *rows, QString *errorMessage);
    static bool readCsvRowsFromDevice(QIODevice *device, const QString &sourceName, QVector<Row> *rows, QString *errorMessage);
    static QStringList parseCsvLine(const QString &line);
    static bool ensureColumns(const QVector<Row> &rows, const QStringList &required, const QString &sourceName, QString *errorMessage);
    static QString csvField(const QString &value);
    static bool validateCamera(const CameraSpec &camera, const QString &sourceName, QString *errorMessage);
    static bool validateLens(const LensSpec &lens, const QString &sourceName, QString *errorMessage);

    static double number(const Row &row, const QString &key, double defaultValue = 0.0);
    static int integer(const Row &row, const QString &key, int defaultValue = 0);
    static QString text(const Row &row, const QString &key);
    static QString firstText(const Row &row, const QStringList &keys);

    bool loadCameraRows(const QVector<Row> &rows, const QString &sourceName, QString *errorMessage);
    bool loadLensRows(const QVector<Row> &rows, const QString &sourceName, QString *errorMessage);
    bool loadLightRows(const QVector<Row> &rows, const QString &sourceName, QString *errorMessage);
};

#endif
