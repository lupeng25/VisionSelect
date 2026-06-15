#ifndef CATALOGREPOSITORY_H
#define CATALOGREPOSITORY_H

#include "core/SelectionTypes.h"

#include <QHash>
#include <QList>
#include <QSqlDatabase>
#include <QString>
#include <QStringList>
#include <QVector>

class QIODevice;

enum class CatalogDomain {
    Camera,
    Lens,
    Light
};

struct CatalogSort {
    QString field;
    bool ascending = true;
};

struct CatalogQuery {
    QString search;
    QString manufacturer;
    QString type;
    QString interfaceType;
    QString lensMount;
    QString mode;
    CatalogSort sort;
    int offset = 0;
    int limit = 500;
};

template <typename T>
struct CatalogPageResult {
    int totalCount = 0;
    QVector<qint64> ids;
    QVector<T> items;
};

class CatalogRepository
{
public:
    CatalogRepository();
    ~CatalogRepository();

    void setStorageDirectory(const QString &directory);
    QString storageDirectory() const;

    bool initializeDatabase(QString *errorMessage = nullptr);
    bool loadDefaults(QString *errorMessage = nullptr);
    bool loadCameraCsv(const QString &filePath, QString *errorMessage = nullptr);
    bool loadLensCsv(const QString &filePath, QString *errorMessage = nullptr);
    bool loadLightCsv(const QString &filePath, QString *errorMessage = nullptr);
    bool exportCameraCsv(const QString &filePath, QString *errorMessage = nullptr) const;
    bool exportLensCsv(const QString &filePath, QString *errorMessage = nullptr) const;
    bool exportLightCsv(const QString &filePath, QString *errorMessage = nullptr) const;
    bool exportCameraCsv(const QString &filePath, const QVector<int> &indexes, QString *errorMessage = nullptr) const;
    bool exportLensCsv(const QString &filePath, const QVector<int> &indexes, QString *errorMessage = nullptr) const;
    bool exportLightCsv(const QString &filePath, const QVector<int> &indexes, QString *errorMessage = nullptr) const;
    bool exportCameraCsvByQuery(const QString &filePath, const CatalogQuery &query, QString *errorMessage = nullptr) const;
    bool exportLensCsvByQuery(const QString &filePath, const CatalogQuery &query, QString *errorMessage = nullptr) const;
    bool exportLightCsvByQuery(const QString &filePath, const CatalogQuery &query, QString *errorMessage = nullptr) const;
    bool resetCamerasToBuiltIn(QString *errorMessage = nullptr);
    bool resetLensesToBuiltIn(QString *errorMessage = nullptr);
    bool resetLightsToBuiltIn(QString *errorMessage = nullptr);
    bool addCamera(const CameraSpec &camera, QString *errorMessage = nullptr);
    bool updateCamera(int index, const CameraSpec &camera, QString *errorMessage = nullptr);
    bool removeCamera(int index, QString *errorMessage = nullptr);
    bool addLens(const LensSpec &lens, QString *errorMessage = nullptr);
    bool updateLens(int index, const LensSpec &lens, QString *errorMessage = nullptr);
    bool removeLens(int index, QString *errorMessage = nullptr);
    bool addLight(const LightSpec &light, QString *errorMessage = nullptr);
    bool updateLight(int index, const LightSpec &light, QString *errorMessage = nullptr);
    bool removeLight(int index, QString *errorMessage = nullptr);

    CatalogPageResult<CameraSpec> queryCameras(const CatalogQuery &query, QString *errorMessage = nullptr) const;
    CatalogPageResult<LensSpec> queryLenses(const CatalogQuery &query, QString *errorMessage = nullptr) const;
    CatalogPageResult<LightSpec> queryLights(const CatalogQuery &query, QString *errorMessage = nullptr) const;
    QStringList distinctValues(CatalogDomain domain, const QString &field, QString *errorMessage = nullptr) const;
    int productCount(CatalogDomain domain, QString *errorMessage = nullptr) const;

    bool cameraById(qint64 id, CameraSpec *camera, QString *errorMessage = nullptr) const;
    bool lensById(qint64 id, LensSpec *lens, QString *errorMessage = nullptr) const;
    bool lightById(qint64 id, LightSpec *light, QString *errorMessage = nullptr) const;
    bool updateCameraById(qint64 id, const CameraSpec &camera, QString *errorMessage = nullptr);
    bool updateLensById(qint64 id, const LensSpec &lens, QString *errorMessage = nullptr);
    bool updateLightById(qint64 id, const LightSpec &light, QString *errorMessage = nullptr);
    bool removeCameraById(qint64 id, QString *errorMessage = nullptr);
    bool removeLensById(qint64 id, QString *errorMessage = nullptr);
    bool removeLightById(qint64 id, QString *errorMessage = nullptr);
    int cameraIndexById(qint64 id) const;
    int lensIndexById(qint64 id) const;
    int lightIndexById(qint64 id) const;

    QVector<CameraSpec> selectionCandidateCameras(const SelectionRequest &request, int limit, QString *errorMessage = nullptr) const;
    QVector<LensSpec> selectionCandidateLenses(const SelectionRequest &request, int limit, QString *errorMessage = nullptr) const;
    QVector<LightSpec> selectionCandidateLights(const SelectionRequest &request, int limit, QString *errorMessage = nullptr) const;
    QVector<LightSpec> selectionCandidateLights(const SelectionRequest &request, bool hasTelecentricLens, bool hasCoaxialLens,
                                                int limit, QString *errorMessage = nullptr) const;

    const QVector<CameraSpec> &cameras() const { return m_cameras; }
    const QVector<LensSpec> &lenses() const { return m_lenses; }
    const QVector<LightSpec> &lights() const { return m_lights; }

    QString summary() const;

private:
    using Row = QHash<QString, QString>;

    QVector<CameraSpec> m_cameras;
    QVector<LensSpec> m_lenses;
    QVector<LightSpec> m_lights;
    QVector<qint64> m_cameraIds;
    QVector<qint64> m_lensIds;
    QVector<qint64> m_lightIds;
    QString m_storageDirectory;
    mutable QSqlDatabase m_db;
    mutable QString m_connectionName;
    bool m_snapshotsLoaded = false;
    mutable QHash<QString, QVector<LightSpec>> m_lightCandidateCache;

    QString effectiveStorageDirectory() const;
    QString databasePath() const;
    QString cameraStoragePath() const;
    QString lensStoragePath() const;
    QString lightStoragePath() const;
    bool openDatabase(QString *errorMessage = nullptr) const;
    bool ensureDatabase(QString *errorMessage = nullptr);
    bool createSchema(QString *errorMessage = nullptr) const;
    bool migrateInitialDatabase(QString *errorMessage = nullptr);
    bool appendMissingBuiltInRows(QString *errorMessage = nullptr);
    bool backupLocalCsvFiles(QString *errorMessage = nullptr) const;
    bool refreshSnapshots(QString *errorMessage = nullptr);
    bool refreshSnapshotsIfLoaded(QString *errorMessage = nullptr);
    bool ensureLocalCatalogs(QString *errorMessage);
    bool copyResourceToFile(const QString &resourcePath, const QString &filePath, bool overwrite, QString *errorMessage) const;
    bool appendMissingResourceRows(const QString &resourcePath, const QString &filePath, const QString &keyColumn, QString *errorMessage) const;
    bool saveCameras(QString *errorMessage) const;
    bool saveLenses(QString *errorMessage) const;
    bool saveLights(QString *errorMessage) const;
    bool writeCameraCsv(const QString &filePath, const QVector<CameraSpec> &cameras, QString *errorMessage) const;
    bool writeLensCsv(const QString &filePath, const QVector<LensSpec> &lenses, QString *errorMessage) const;
    bool writeLightCsv(const QString &filePath, const QVector<LightSpec> &lights, QString *errorMessage) const;
    bool replaceCamerasInDatabase(const QVector<CameraSpec> &cameras, const QString &sourceKind, QString *errorMessage);
    bool replaceLensesInDatabase(const QVector<LensSpec> &lenses, const QString &sourceKind, QString *errorMessage);
    bool replaceLightsInDatabase(const QVector<LightSpec> &lights, const QString &sourceKind, QString *errorMessage);
    bool insertCameraIntoDatabase(const CameraSpec &camera, const QString &sourceKind, bool replaceExisting, qint64 *id, QString *errorMessage) const;
    bool insertLensIntoDatabase(const LensSpec &lens, const QString &sourceKind, bool replaceExisting, qint64 *id, QString *errorMessage) const;
    bool insertLightIntoDatabase(const LightSpec &light, const QString &sourceKind, bool replaceExisting, qint64 *id, QString *errorMessage) const;
    bool parseCameraSpecs(const QVector<Row> &rows, const QString &sourceName, QVector<CameraSpec> *cameras, QString *errorMessage);
    bool parseLensSpecs(const QVector<Row> &rows, const QString &sourceName, QVector<LensSpec> *lenses, QString *errorMessage);
    bool parseLightSpecs(const QVector<Row> &rows, const QString &sourceName, QVector<LightSpec> *lights, QString *errorMessage);
    static bool readCsvRows(const QString &filePath, QVector<Row> *rows, QString *errorMessage);
    static bool readCsvRowsFromDevice(QIODevice *device, const QString &sourceName, QVector<Row> *rows, QString *errorMessage);
    static QStringList parseCsvLine(const QString &line);
    static bool ensureColumns(const QVector<Row> &rows, const QStringList &required, const QString &sourceName, QString *errorMessage);
    static QString csvField(const QString &value);
    static bool validateCamera(const CameraSpec &camera, const QString &sourceName, QString *errorMessage);
    static bool validateLens(const LensSpec &lens, const QString &sourceName, QString *errorMessage);
    static bool validateLight(const LightSpec &light, const QString &sourceName, QString *errorMessage);
    static QString normalizeCameraSensorFormat(const QString &value, int resolutionX, int resolutionY, double pixelSizeUm);
    static QString normalizeCameraColorMode(const QString &value);
    static QString normalizeCameraInterface(const QString &value);
    static QString normalizeLensMount(const QString &value);
    static QString keyForText(const QString &value);
    static QString lightTypeKey(LightType type);
    static QString lensTypeKey(LensType type);

    static double number(const Row &row, const QString &key, double defaultValue = 0.0);
    static int integer(const Row &row, const QString &key, int defaultValue = 0);
    static QString text(const Row &row, const QString &key);
    static QString firstText(const Row &row, const QStringList &keys);

    bool loadCameraRows(const QVector<Row> &rows, const QString &sourceName, QString *errorMessage);
    bool loadLensRows(const QVector<Row> &rows, const QString &sourceName, QString *errorMessage);
    bool loadLightRows(const QVector<Row> &rows, const QString &sourceName, QString *errorMessage);
};

#endif
