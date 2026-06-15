#include "catalog/CatalogRepository.h"

#include <QDir>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QIODevice>
#include <QRegularExpression>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QStandardPaths>
#include <QSet>
#include <QTextStream>
#include <QtGlobal>
#include <QUuid>
#include <QVariant>

#include <cmath>

namespace {
QString sqlErrorText(const QSqlQuery &query)
{
    return query.lastError().text();
}

QString sqlErrorText(const QSqlDatabase &db)
{
    return db.lastError().text();
}

bool execSql(const QSqlDatabase &db, const QString &sql, QString *errorMessage)
{
    QSqlQuery query(db);
    if (!query.exec(sql)) {
        if (errorMessage)
            *errorMessage = QStringLiteral("SQL failed: %1\n%2").arg(sql, sqlErrorText(query));
        return false;
    }
    return true;
}

QString nowUtcIso()
{
    return QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
}

QString likePattern(const QString &value)
{
    return QLatin1Char('%') + value.trimmed().toLower() + QLatin1Char('%');
}

QString normalizedQueryType(const QString &value)
{
    QString v = value.trimmed();
    if (v.contains(QStringLiteral("Fixed"), Qt::CaseInsensitive)
        || v.contains(QString::fromUtf8("普通")))
        return QStringLiteral("FixedFocal");
    if (v.contains(QStringLiteral("Object"), Qt::CaseInsensitive)
        || v.contains(QString::fromUtf8("物方")))
        return QStringLiteral("ObjectTelecentric");
    if (v.contains(QStringLiteral("Bi"), Qt::CaseInsensitive)
        || v.contains(QString::fromUtf8("双远心")))
        return QStringLiteral("BiTelecentric");
    if (v.contains(QStringLiteral("Backlight"), Qt::CaseInsensitive)
        || v.contains(QString::fromUtf8("背光")))
        return v.contains(QStringLiteral("Telecentric"), Qt::CaseInsensitive)
            || v.contains(QString::fromUtf8("远心")) ? QStringLiteral("TelecentricBacklight") : QStringLiteral("Backlight");
    if (v.contains(QStringLiteral("Ring"), Qt::CaseInsensitive)
        || v.contains(QString::fromUtf8("环")))
        return QStringLiteral("Ring");
    if (v.contains(QStringLiteral("Bar"), Qt::CaseInsensitive)
        || v.contains(QString::fromUtf8("条")))
        return QStringLiteral("Bar");
    if (v.contains(QStringLiteral("Coaxial"), Qt::CaseInsensitive)
        || v.contains(QString::fromUtf8("同轴")))
        return QStringLiteral("Coaxial");
    if (v.contains(QStringLiteral("Dome"), Qt::CaseInsensitive)
        || v.contains(QString::fromUtf8("穹")))
        return QStringLiteral("Dome");
    if (v.contains(QStringLiteral("Dark"), Qt::CaseInsensitive)
        || v.contains(QString::fromUtf8("暗场")))
        return QStringLiteral("DarkField");
    return v;
}

QString lightCandidateCacheKey(const SelectionRequest &request, bool hasTelecentricLens, bool hasCoaxialLens, int limit)
{
    const double fovW = request.objectWidthMm + request.placementMarginMm * 2.0;
    const double fovH = request.objectHeightMm + request.placementMarginMm * 2.0;
    return QStringLiteral("%1|%2|%3|%4|%5|%6|%7|%8")
        .arg(hasTelecentricLens ? 1 : 0)
        .arg(hasCoaxialLens ? 1 : 0)
        .arg(detectionTypeKey(request.detectionType))
        .arg(surfaceTypeKey(request.surfaceType))
        .arg(qRound64(fovW * 10.0))
        .arg(qRound64(fovH * 10.0))
        .arg(qRound64(request.motionSpeedMmS * 10.0))
        .arg(limit > 0 ? limit : 300);
}

QVector<double> fixedFocalTargetSamples(const SelectionRequest &request)
{
    QVector<double> targets;
    const double fovW = request.objectWidthMm + request.placementMarginMm * 2.0;
    const double fovH = request.objectHeightMm + request.placementMarginMm * 2.0;
    if (request.workingDistanceMm <= 0.0 || fovW <= 0.0 || fovH <= 0.0)
        return targets;

    static const double sensorWidths[] = { 3.6, 4.8, 6.4, 8.8, 11.3, 14.1, 17.6, 25.6 };
    static const double sensorHeights[] = { 2.7, 3.6, 4.8, 6.6, 8.5, 10.6, 13.2, 19.2 };
    for (int i = 0; i < static_cast<int>(sizeof(sensorWidths) / sizeof(sensorWidths[0])); ++i) {
        const double sensorW = sensorWidths[i];
        const double sensorH = sensorHeights[i];
        const bool widthLimited = (fovW / fovH) >= (sensorW / sensorH);
        const double fov = widthLimited ? fovW : fovH;
        const double sensor = widthLimited ? sensorW : sensorH;
        const double focal = request.workingDistanceMm * sensor / (fov + sensor);
        if (!std::isfinite(focal) || focal <= 0.0)
            continue;
        bool duplicate = false;
        for (double existing : targets) {
            if (qAbs(existing - focal) < 0.5) {
                duplicate = true;
                break;
            }
        }
        if (!duplicate)
            targets.append(focal);
    }
    return targets;
}

QString focalProximityOrder(const QVector<double> &targets)
{
    if (targets.isEmpty())
        return QString();
    QStringList distances;
    for (int i = 0; i < targets.size(); ++i)
        distances << QStringLiteral("ABS(focal_length_mm - ?)");
    const QString distance = distances.size() == 1
        ? distances.first()
        : QStringLiteral("MIN(%1)").arg(distances.join(QStringLiteral(", ")));
    return distance + QStringLiteral(" ASC, image_circle_mm DESC, megapixel_rating DESC, manufacturer ASC, model ASC, id ASC");
}

bool csvRecordComplete(const QString &record)
{
    bool inQuotes = false;
    for (int i = 0; i < record.size(); ++i) {
        if (record.at(i) != QLatin1Char('"'))
            continue;
        if (inQuotes && i + 1 < record.size() && record.at(i + 1) == QLatin1Char('"')) {
            ++i;
            continue;
        }
        inQuotes = !inQuotes;
    }
    return !inQuotes;
}

bool nearlyEqual(double left, double right)
{
    return qAbs(left - right) <= 0.000001;
}

QString productKey(const QString &manufacturer, const QString &model)
{
    return manufacturer.trimmed().toLower() + QLatin1Char('\n') + model.trimmed().toLower();
}

QString productKey(const CameraSpec &camera)
{
    return productKey(camera.manufacturer, camera.model);
}

QString productKey(const LensSpec &lens)
{
    return productKey(lens.manufacturer, lens.model);
}

QString productKey(const LightSpec &light)
{
    return productKey(light.manufacturer, light.model);
}

bool sameCameraSpec(const CameraSpec &left, const CameraSpec &right)
{
    return left.model == right.model
        && left.manufacturer == right.manufacturer
        && left.resolutionX == right.resolutionX
        && left.resolutionY == right.resolutionY
        && nearlyEqual(left.pixelSizeUm, right.pixelSizeUm)
        && left.sensorFormat == right.sensorFormat
        && left.colorMode == right.colorMode
        && left.shutterType == right.shutterType
        && nearlyEqual(left.maxFps, right.maxFps)
        && left.interfaceType == right.interfaceType
        && nearlyEqual(left.bandwidthMBps, right.bandwidthMBps)
        && nearlyEqual(left.bitDepth, right.bitDepth)
        && nearlyEqual(left.dynamicRangeDb, right.dynamicRangeDb)
        && left.lensMount == right.lensMount;
}

bool sameLensSpec(const LensSpec &left, const LensSpec &right)
{
    return left.model == right.model
        && left.manufacturer == right.manufacturer
        && left.lensType == right.lensType
        && left.lensMount == right.lensMount
        && nearlyEqual(left.focalLengthMm, right.focalLengthMm)
        && nearlyEqual(left.minWorkingDistanceMm, right.minWorkingDistanceMm)
        && nearlyEqual(left.distortionPercent, right.distortionPercent)
        && nearlyEqual(left.imageCircleMm, right.imageCircleMm)
        && nearlyEqual(left.megapixelRating, right.megapixelRating)
        && nearlyEqual(left.recommendedMinPixelUm, right.recommendedMinPixelUm)
        && nearlyEqual(left.pmag, right.pmag)
        && nearlyEqual(left.nominalWorkingDistanceMm, right.nominalWorkingDistanceMm)
        && nearlyEqual(left.workingDistanceToleranceMm, right.workingDistanceToleranceMm)
        && nearlyEqual(left.maxSensorDiagonalMm, right.maxSensorDiagonalMm)
        && nearlyEqual(left.telecentricityDeg, right.telecentricityDeg)
        && nearlyEqual(left.dofMm, right.dofMm)
        && nearlyEqual(left.numericalAperture, right.numericalAperture)
        && nearlyEqual(left.fNumber, right.fNumber)
        && left.coaxialIllumination == right.coaxialIllumination
        && left.notes == right.notes;
}

bool sameLightSpec(const LightSpec &left, const LightSpec &right)
{
    return left.model == right.model
        && left.manufacturer == right.manufacturer
        && left.lightType == right.lightType
        && left.color == right.color
        && left.wavelengthNm == right.wavelengthNm
        && left.mode == right.mode
        && nearlyEqual(left.activeWidthMm, right.activeWidthMm)
        && nearlyEqual(left.activeHeightMm, right.activeHeightMm)
        && left.bestFor == right.bestFor;
}

QHash<QString, CameraSpec> cameraMapByKey(const QVector<CameraSpec> &cameras)
{
    QHash<QString, CameraSpec> specs;
    for (const CameraSpec &camera : cameras)
        specs.insert(productKey(camera), camera);
    return specs;
}

QHash<QString, LensSpec> lensMapByKey(const QVector<LensSpec> &lenses)
{
    QHash<QString, LensSpec> specs;
    for (const LensSpec &lens : lenses)
        specs.insert(productKey(lens), lens);
    return specs;
}

QHash<QString, LightSpec> lightMapByKey(const QVector<LightSpec> &lights)
{
    QHash<QString, LightSpec> specs;
    for (const LightSpec &light : lights)
        specs.insert(productKey(light), light);
    return specs;
}

template <typename T>
void appendUniqueProducts(QVector<T> *target, const QVector<T> &source, int limit)
{
    if (!target)
        return;
    const int maxItems = limit > 0 ? limit : source.size();
    QSet<QString> seen;
    seen.reserve(target->size() + source.size());
    for (const T &item : *target)
        seen.insert(productKey(item));
    for (const T &item : source) {
        if (target->size() >= maxItems)
            return;
        const QString key = productKey(item);
        if (seen.contains(key))
            continue;
        seen.insert(key);
        target->append(item);
    }
}

int ceilPositiveToInt(double value)
{
    if (value <= 0.0)
        return 0;
    return static_cast<int>(std::ceil(value));
}

double requiredFovWidthForRequest(const SelectionRequest &request)
{
    return request.objectWidthMm + request.placementMarginMm * 2.0;
}

double requiredFovHeightForRequest(const SelectionRequest &request)
{
    return request.objectHeightMm + request.placementMarginMm * 2.0;
}

double targetObjectPixelUmForRequest(const SelectionRequest &request)
{
    double featurePixels = 3.0;
    double toleranceFactor = 1.0;
    switch (request.detectionType) {
    case DetectionType::Measurement:
        featurePixels = 5.0;
        toleranceFactor = 1.0 / 5.0;
        break;
    case DetectionType::Positioning:
        featurePixels = 4.0;
        toleranceFactor = 1.5;
        break;
    case DetectionType::DefectInspection:
        featurePixels = 3.0;
        toleranceFactor = 2.0;
        break;
    case DetectionType::OcrCode:
        featurePixels = 4.0;
        toleranceFactor = 2.0;
        break;
    }

    double target = 999999.0;
    if (request.minFeatureUm > 0.0)
        target = qMin(target, request.minFeatureUm / featurePixels);
    if (request.measurementToleranceUm > 0.0)
        target = qMin(target, request.measurementToleranceUm * toleranceFactor);
    return qMax(0.5, target);
}
}

CatalogRepository::CatalogRepository()
{
    m_connectionName = QStringLiteral("visionselect_catalog_%1")
        .arg(QString::fromLatin1(QUuid::createUuid().toRfc4122().toHex()));
}

CatalogRepository::~CatalogRepository()
{
    if (m_db.isValid()) {
        const QString connectionName = m_db.connectionName();
        m_db.close();
        m_db = QSqlDatabase();
        QSqlDatabase::removeDatabase(connectionName);
    }
}

void CatalogRepository::setStorageDirectory(const QString &directory)
{
    m_storageDirectory = directory;
}

QString CatalogRepository::storageDirectory() const
{
    return effectiveStorageDirectory();
}

bool CatalogRepository::loadDefaults(QString *errorMessage)
{
    return initializeDatabase(errorMessage)
        && refreshSnapshots(errorMessage);
}

bool CatalogRepository::initializeDatabase(QString *errorMessage)
{
    return ensureDatabase(errorMessage)
        && appendMissingBuiltInRows(errorMessage);
}

QString CatalogRepository::effectiveStorageDirectory() const
{
    if (!m_storageDirectory.trimmed().isEmpty())
        return m_storageDirectory;
    QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (path.isEmpty())
        path = QDir::homePath() + QStringLiteral("/VisionSelect");
    return path;
}

QString CatalogRepository::databasePath() const
{
    return QDir(effectiveStorageDirectory()).filePath(QStringLiteral("catalog.db"));
}

QString CatalogRepository::cameraStoragePath() const
{
    return QDir(effectiveStorageDirectory()).filePath(QStringLiteral("cameras.csv"));
}

QString CatalogRepository::lensStoragePath() const
{
    return QDir(effectiveStorageDirectory()).filePath(QStringLiteral("lenses.csv"));
}

QString CatalogRepository::lightStoragePath() const
{
    return QDir(effectiveStorageDirectory()).filePath(QStringLiteral("lights.csv"));
}

bool CatalogRepository::openDatabase(QString *errorMessage) const
{
    QDir dir(effectiveStorageDirectory());
    if (!dir.exists() && !QDir().mkpath(dir.absolutePath())) {
        if (errorMessage)
            *errorMessage = QString::fromUtf8("无法创建产品库数据目录：%1").arg(dir.absolutePath());
        return false;
    }

    if (m_connectionName.isEmpty())
        m_connectionName = QStringLiteral("visionselect_catalog_%1")
            .arg(QString::fromLatin1(QUuid::createUuid().toRfc4122().toHex()));

    if (!m_db.isValid()) {
        m_db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
        m_db.setDatabaseName(databasePath());
    }
    if (m_db.isOpen())
        return true;
    if (!m_db.open()) {
        if (errorMessage)
            *errorMessage = QString::fromUtf8("无法打开 SQLite 产品库：%1").arg(sqlErrorText(m_db));
        return false;
    }
    return execSql(m_db, QStringLiteral("PRAGMA foreign_keys = ON"), errorMessage)
        && execSql(m_db, QStringLiteral("PRAGMA journal_mode = WAL"), errorMessage)
        && execSql(m_db, QStringLiteral("PRAGMA synchronous = NORMAL"), errorMessage);
}

bool CatalogRepository::ensureDatabase(QString *errorMessage)
{
    const bool existed = QFileInfo::exists(databasePath());
    if (!openDatabase(errorMessage))
        return false;
    if (!createSchema(errorMessage))
        return false;

    QSqlQuery versionQuery(m_db);
    if (!versionQuery.exec(QStringLiteral("PRAGMA user_version"))) {
        if (errorMessage)
            *errorMessage = QString::fromUtf8("无法读取产品库版本：%1").arg(sqlErrorText(versionQuery));
        return false;
    }
    int version = 0;
    if (versionQuery.next())
        version = versionQuery.value(0).toInt();

    if (!existed || version == 0) {
        if (!migrateInitialDatabase(errorMessage))
            return false;
        if (!execSql(m_db, QStringLiteral("PRAGMA user_version = 1"), errorMessage))
            return false;
    }
    return true;
}

bool CatalogRepository::createSchema(QString *errorMessage) const
{
    if (!openDatabase(errorMessage))
        return false;

    const QString cameraSql = QStringLiteral(
        "CREATE TABLE IF NOT EXISTS camera_products ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "model TEXT NOT NULL,"
        "manufacturer TEXT,"
        "manufacturer_key TEXT NOT NULL,"
        "model_key TEXT NOT NULL,"
        "resolution_x INTEGER NOT NULL,"
        "resolution_y INTEGER NOT NULL,"
        "pixel_size_um REAL NOT NULL,"
        "sensor_format TEXT,"
        "color_mode TEXT,"
        "shutter_type TEXT,"
        "max_fps REAL,"
        "interface TEXT,"
        "bandwidth_mbps REAL,"
        "bit_depth REAL,"
        "dynamic_range_db REAL,"
        "lens_mount TEXT,"
        "search_text TEXT,"
        "source_kind TEXT NOT NULL,"
        "source_version TEXT,"
        "created_at TEXT NOT NULL,"
        "updated_at TEXT NOT NULL,"
        "UNIQUE(manufacturer_key, model_key))");
    const QString lensSql = QStringLiteral(
        "CREATE TABLE IF NOT EXISTS lens_products ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "model TEXT NOT NULL,"
        "manufacturer TEXT,"
        "manufacturer_key TEXT NOT NULL,"
        "model_key TEXT NOT NULL,"
        "lens_type TEXT NOT NULL,"
        "lens_mount TEXT,"
        "focal_length_mm REAL,"
        "min_wd_mm REAL,"
        "distortion_percent REAL,"
        "image_circle_mm REAL NOT NULL,"
        "megapixel_rating REAL,"
        "recommended_min_pixel_um REAL,"
        "pmag REAL,"
        "nominal_wd_mm REAL,"
        "wd_tolerance_mm REAL,"
        "max_sensor_diagonal_mm REAL,"
        "telecentricity_deg REAL,"
        "dof_mm REAL,"
        "numerical_aperture REAL,"
        "f_number REAL,"
        "coaxial_illumination INTEGER,"
        "notes TEXT,"
        "search_text TEXT,"
        "source_kind TEXT NOT NULL,"
        "source_version TEXT,"
        "created_at TEXT NOT NULL,"
        "updated_at TEXT NOT NULL,"
        "UNIQUE(manufacturer_key, model_key))");
    const QString lightSql = QStringLiteral(
        "CREATE TABLE IF NOT EXISTS light_products ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "model TEXT NOT NULL,"
        "manufacturer TEXT,"
        "manufacturer_key TEXT NOT NULL,"
        "model_key TEXT NOT NULL,"
        "light_type TEXT NOT NULL,"
        "color TEXT,"
        "wavelength_nm INTEGER,"
        "mode TEXT,"
        "active_width_mm REAL NOT NULL,"
        "active_height_mm REAL NOT NULL,"
        "best_for TEXT,"
        "search_text TEXT,"
        "source_kind TEXT NOT NULL,"
        "source_version TEXT,"
        "created_at TEXT NOT NULL,"
        "updated_at TEXT NOT NULL,"
        "UNIQUE(manufacturer_key, model_key))");

    const QStringList statements = {
        cameraSql,
        lensSql,
        lightSql,
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_camera_manufacturer ON camera_products(manufacturer_key)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_camera_model ON camera_products(model_key)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_camera_interface ON camera_products(interface)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_camera_lens_mount ON camera_products(lens_mount)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_camera_resolution ON camera_products(resolution_x, resolution_y)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_camera_fps ON camera_products(max_fps)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_lens_manufacturer ON lens_products(manufacturer_key)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_lens_model ON lens_products(model_key)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_lens_type ON lens_products(lens_type)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_lens_mount ON lens_products(lens_mount)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_lens_image_circle ON lens_products(image_circle_mm)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_lens_working_distance ON lens_products(nominal_wd_mm, min_wd_mm)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_light_manufacturer ON light_products(manufacturer_key)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_light_model ON light_products(model_key)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_light_type ON light_products(light_type)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_light_mode ON light_products(mode)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_light_color ON light_products(color)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_light_size ON light_products(active_width_mm, active_height_mm)")
    };

    for (const QString &statement : statements) {
        if (!execSql(m_db, statement, errorMessage))
            return false;
    }
    return true;
}

bool CatalogRepository::backupLocalCsvFiles(QString *errorMessage) const
{
    const QStringList paths = {cameraStoragePath(), lensStoragePath(), lightStoragePath()};
    bool hasAny = false;
    for (const QString &path : paths)
        hasAny = hasAny || QFileInfo::exists(path);
    if (!hasAny)
        return true;

    const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss"));
    QDir dir(effectiveStorageDirectory());
    const QString backupDir = dir.filePath(QStringLiteral("catalog_migration_backup_%1").arg(stamp));
    if (!QDir().mkpath(backupDir)) {
        if (errorMessage)
            *errorMessage = QString::fromUtf8("无法创建产品库迁移备份目录：%1").arg(backupDir);
        return false;
    }
    for (const QString &path : paths) {
        QFileInfo info(path);
        if (!info.exists())
            continue;
        const QString target = QDir(backupDir).filePath(info.fileName());
        if (!QFile::copy(path, target)) {
            if (errorMessage)
                *errorMessage = QString::fromUtf8("无法备份本机参数库文件：%1").arg(path);
            return false;
        }
    }
    return true;
}

bool CatalogRepository::migrateInitialDatabase(QString *errorMessage)
{
    if (!backupLocalCsvFiles(errorMessage))
        return false;

    QVector<Row> cameraRows;
    QVector<Row> lensRows;
    QVector<Row> lightRows;
    QVector<CameraSpec> cameras;
    QVector<LensSpec> lenses;
    QVector<LightSpec> lights;
    if (!readCsvRows(QStringLiteral(":/data/cameras.csv"), &cameraRows, errorMessage)
        || !parseCameraSpecs(cameraRows, QString::fromUtf8("内置相机库"), &cameras, errorMessage)
        || !replaceCamerasInDatabase(cameras, QStringLiteral("builtin"), errorMessage)) {
        return false;
    }
    if (!readCsvRows(QStringLiteral(":/data/lenses.csv"), &lensRows, errorMessage)
        || !parseLensSpecs(lensRows, QString::fromUtf8("内置镜头库"), &lenses, errorMessage)
        || !replaceLensesInDatabase(lenses, QStringLiteral("builtin"), errorMessage)) {
        return false;
    }
    if (!readCsvRows(QStringLiteral(":/data/lights.csv"), &lightRows, errorMessage)
        || !parseLightSpecs(lightRows, QString::fromUtf8("内置光源库"), &lights, errorMessage)
        || !replaceLightsInDatabase(lights, QStringLiteral("builtin"), errorMessage)) {
        return false;
    }

    const QHash<QString, CameraSpec> builtInCameras = cameraMapByKey(cameras);
    const QHash<QString, LensSpec> builtInLenses = lensMapByKey(lenses);
    const QHash<QString, LightSpec> builtInLights = lightMapByKey(lights);

    if (QFileInfo::exists(cameraStoragePath())) {
        QVector<Row> rows;
        QVector<CameraSpec> local;
        if (!readCsvRows(cameraStoragePath(), &rows, errorMessage)
            || !parseCameraSpecs(rows, cameraStoragePath(), &local, errorMessage))
            return false;
        for (const CameraSpec &camera : local) {
            const QString key = productKey(camera);
            if (builtInCameras.contains(key) && sameCameraSpec(builtInCameras.value(key), camera))
                continue;
            if (!insertCameraIntoDatabase(camera, QStringLiteral("local"), true, nullptr, errorMessage))
                return false;
        }
    }
    if (QFileInfo::exists(lensStoragePath())) {
        QVector<Row> rows;
        QVector<LensSpec> local;
        if (!readCsvRows(lensStoragePath(), &rows, errorMessage)
            || !parseLensSpecs(rows, lensStoragePath(), &local, errorMessage))
            return false;
        for (const LensSpec &lens : local) {
            const QString key = productKey(lens);
            if (builtInLenses.contains(key) && sameLensSpec(builtInLenses.value(key), lens))
                continue;
            if (!insertLensIntoDatabase(lens, QStringLiteral("local"), true, nullptr, errorMessage))
                return false;
        }
    }
    if (QFileInfo::exists(lightStoragePath())) {
        QVector<Row> rows;
        QVector<LightSpec> local;
        if (!readCsvRows(lightStoragePath(), &rows, errorMessage)
            || !parseLightSpecs(rows, lightStoragePath(), &local, errorMessage))
            return false;
        for (const LightSpec &light : local) {
            const QString key = productKey(light);
            if (builtInLights.contains(key) && sameLightSpec(builtInLights.value(key), light))
                continue;
            if (!insertLightIntoDatabase(light, QStringLiteral("local"), true, nullptr, errorMessage))
                return false;
        }
    }
    return true;
}

bool CatalogRepository::appendMissingBuiltInRows(QString *errorMessage)
{
    QVector<Row> rows;
    QVector<CameraSpec> cameras;
    if (!readCsvRows(QStringLiteral(":/data/cameras.csv"), &rows, errorMessage)
        || !parseCameraSpecs(rows, QString::fromUtf8("内置相机库"), &cameras, errorMessage))
        return false;
    for (const CameraSpec &camera : cameras) {
        if (!insertCameraIntoDatabase(camera, QStringLiteral("builtin"), false, nullptr, errorMessage))
            return false;
    }

    rows.clear();
    QVector<LensSpec> lenses;
    if (!readCsvRows(QStringLiteral(":/data/lenses.csv"), &rows, errorMessage)
        || !parseLensSpecs(rows, QString::fromUtf8("内置镜头库"), &lenses, errorMessage))
        return false;
    for (const LensSpec &lens : lenses) {
        if (!insertLensIntoDatabase(lens, QStringLiteral("builtin"), false, nullptr, errorMessage))
            return false;
    }

    rows.clear();
    QVector<LightSpec> lights;
    if (!readCsvRows(QStringLiteral(":/data/lights.csv"), &rows, errorMessage)
        || !parseLightSpecs(rows, QString::fromUtf8("内置光源库"), &lights, errorMessage))
        return false;
    for (const LightSpec &light : lights) {
        if (!insertLightIntoDatabase(light, QStringLiteral("builtin"), false, nullptr, errorMessage))
            return false;
    }
    return true;
}

bool CatalogRepository::ensureLocalCatalogs(QString *errorMessage)
{
    QDir dir(effectiveStorageDirectory());
    if (!dir.exists() && !QDir().mkpath(dir.absolutePath())) {
        if (errorMessage)
            *errorMessage = QString::fromUtf8("\346\227\240\346\263\225\345\210\233\345\273\272\344\272\247\345\223\201\345\272\223\346\225\260\346\215\256\347\233\256\345\275\225\357\274\232%1").arg(dir.absolutePath());
        return false;
    }
    if (!QFileInfo::exists(cameraStoragePath())
        && !copyResourceToFile(QStringLiteral(":/data/cameras.csv"), cameraStoragePath(), false, errorMessage)) {
        return false;
    }
    if (!QFileInfo::exists(lensStoragePath())
        && !copyResourceToFile(QStringLiteral(":/data/lenses.csv"), lensStoragePath(), false, errorMessage)) {
        return false;
    }
    if (!QFileInfo::exists(lightStoragePath())
        && !copyResourceToFile(QStringLiteral(":/data/lights.csv"), lightStoragePath(), false, errorMessage)) {
        return false;
    }
    return appendMissingResourceRows(QStringLiteral(":/data/cameras.csv"), cameraStoragePath(), QStringLiteral("model"), errorMessage)
        && appendMissingResourceRows(QStringLiteral(":/data/lenses.csv"), lensStoragePath(), QStringLiteral("model"), errorMessage)
        && appendMissingResourceRows(QStringLiteral(":/data/lights.csv"), lightStoragePath(), QStringLiteral("model"), errorMessage);
}

bool CatalogRepository::copyResourceToFile(const QString &resourcePath, const QString &filePath, bool overwrite, QString *errorMessage) const
{
    QFileInfo targetInfo(filePath);
    if (!QDir().mkpath(targetInfo.absolutePath())) {
        if (errorMessage)
            *errorMessage = QString::fromUtf8("\346\227\240\346\263\225\345\210\233\345\273\272\347\233\256\345\275\225\357\274\232%1").arg(targetInfo.absolutePath());
        return false;
    }
    if (overwrite && QFileInfo::exists(filePath) && !QFile::remove(filePath)) {
        if (errorMessage)
            *errorMessage = QString::fromUtf8("\346\227\240\346\263\225\350\246\206\347\233\226\346\226\207\344\273\266\357\274\232%1").arg(filePath);
        return false;
    }
    if (!overwrite && QFileInfo::exists(filePath))
        return true;
    if (!QFile::copy(resourcePath, filePath)) {
        if (errorMessage)
            *errorMessage = QString::fromUtf8("\346\227\240\346\263\225\345\210\235\345\247\213\345\214\226\344\272\247\345\223\201\345\272\223\346\226\207\344\273\266\357\274\232%1").arg(filePath);
        return false;
    }
    QFile::setPermissions(filePath,
                          QFileDevice::ReadOwner | QFileDevice::WriteOwner
                              | QFileDevice::ReadUser | QFileDevice::WriteUser
                              | QFileDevice::ReadGroup | QFileDevice::ReadOther);
    return true;
}

bool CatalogRepository::appendMissingResourceRows(const QString &resourcePath, const QString &filePath, const QString &keyColumn, QString *errorMessage) const
{
    QVector<Row> localRows;
    QVector<Row> resourceRows;
    if (!readCsvRows(filePath, &localRows, errorMessage)
        || !readCsvRows(resourcePath, &resourceRows, errorMessage)) {
        return false;
    }

    QSet<QString> existingKeys;
    for (const Row &row : localRows) {
        const QString key = row.value(keyColumn).trimmed().toLower();
        if (!key.isEmpty())
            existingKeys.insert(key);
    }

    QFile resourceFile(resourcePath);
    if (!resourceFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (errorMessage)
            *errorMessage = QString::fromUtf8("无法打开内置产品库：%1").arg(resourcePath);
        return false;
    }

    QTextStream in(&resourceFile);
    in.setCodec("UTF-8");
    QStringList headers;
    int keyIndex = -1;
    int lineNumber = 0;
    QStringList rowsToAppend;
    while (!in.atEnd()) {
        QString line = in.readLine();
        ++lineNumber;
        if (lineNumber == 1 && line.startsWith(QChar(0xFEFF)))
            line.remove(0, 1);
        if (line.trimmed().isEmpty())
            continue;

        const QStringList values = parseCsvLine(line);
        if (headers.isEmpty()) {
            headers = values;
            for (QString &header : headers)
                header = header.trimmed().toLower();
            keyIndex = headers.indexOf(keyColumn);
            if (keyIndex < 0) {
                if (errorMessage)
                    *errorMessage = QString::fromUtf8("%1 缺少合并键字段：%2").arg(resourcePath, keyColumn);
                return false;
            }
            continue;
        }

        if (values.size() != headers.size()) {
            if (errorMessage)
                *errorMessage = QString::fromUtf8("%1 第 %2 行字段数量不匹配：期望 %3，实际 %4")
                    .arg(resourcePath)
                    .arg(lineNumber)
                    .arg(headers.size())
                    .arg(values.size());
            return false;
        }

        const QString key = values.at(keyIndex).trimmed().toLower();
        if (!key.isEmpty() && !existingKeys.contains(key)) {
            rowsToAppend.append(line);
            existingKeys.insert(key);
        }
    }

    if (rowsToAppend.isEmpty())
        return true;

    bool endsWithNewline = true;
    QFile existingFile(filePath);
    if (existingFile.open(QIODevice::ReadOnly) && existingFile.size() > 0) {
        existingFile.seek(existingFile.size() - 1);
        const QByteArray last = existingFile.read(1);
        endsWithNewline = last == "\n" || last == "\r";
    }

    QFile outputFile(filePath);
    if (!outputFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        if (errorMessage)
            *errorMessage = QString::fromUtf8("无法更新本地产品库文件：%1").arg(filePath);
        return false;
    }

    QTextStream out(&outputFile);
    out.setCodec("UTF-8");
    if (!endsWithNewline)
        out << "\n";
    for (const QString &line : rowsToAppend)
        out << line << "\n";
    return true;
}

bool CatalogRepository::parseCameraSpecs(const QVector<Row> &rows, const QString &sourceName, QVector<CameraSpec> *cameras, QString *errorMessage)
{
    if (!cameras)
        return false;
    const QVector<CameraSpec> previous = m_cameras;
    if (!loadCameraRows(rows, sourceName, errorMessage)) {
        m_cameras = previous;
        return false;
    }
    *cameras = m_cameras;
    m_cameras = previous;
    return true;
}

bool CatalogRepository::parseLensSpecs(const QVector<Row> &rows, const QString &sourceName, QVector<LensSpec> *lenses, QString *errorMessage)
{
    if (!lenses)
        return false;
    const QVector<LensSpec> previous = m_lenses;
    if (!loadLensRows(rows, sourceName, errorMessage)) {
        m_lenses = previous;
        return false;
    }
    *lenses = m_lenses;
    m_lenses = previous;
    return true;
}

bool CatalogRepository::parseLightSpecs(const QVector<Row> &rows, const QString &sourceName, QVector<LightSpec> *lights, QString *errorMessage)
{
    if (!lights)
        return false;
    const QVector<LightSpec> previous = m_lights;
    if (!loadLightRows(rows, sourceName, errorMessage)) {
        m_lights = previous;
        return false;
    }
    *lights = m_lights;
    m_lights = previous;
    return true;
}

bool CatalogRepository::insertCameraIntoDatabase(const CameraSpec &camera, const QString &sourceKind, bool replaceExisting, qint64 *id, QString *errorMessage) const
{
    if (!openDatabase(errorMessage))
        return false;
    const QString command = replaceExisting ? QStringLiteral("INSERT OR REPLACE") : QStringLiteral("INSERT OR IGNORE");
    QSqlQuery query(m_db);
    query.prepare(command + QStringLiteral(
        " INTO camera_products (model, manufacturer, manufacturer_key, model_key, resolution_x, resolution_y, pixel_size_um,"
        " sensor_format, color_mode, shutter_type, max_fps, interface, bandwidth_mbps, bit_depth, dynamic_range_db, lens_mount,"
        " search_text, source_kind, source_version, created_at, updated_at)"
        " VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));
    const QString now = nowUtcIso();
    query.addBindValue(camera.model);
    query.addBindValue(camera.manufacturer);
    query.addBindValue(keyForText(camera.manufacturer));
    query.addBindValue(keyForText(camera.model));
    query.addBindValue(camera.resolutionX);
    query.addBindValue(camera.resolutionY);
    query.addBindValue(camera.pixelSizeUm);
    query.addBindValue(camera.sensorFormat);
    query.addBindValue(camera.colorMode);
    query.addBindValue(camera.shutterType);
    query.addBindValue(camera.maxFps);
    query.addBindValue(camera.interfaceType);
    query.addBindValue(camera.bandwidthMBps);
    query.addBindValue(camera.bitDepth);
    query.addBindValue(camera.dynamicRangeDb);
    query.addBindValue(camera.lensMount);
    query.addBindValue(QStringList({camera.model, camera.manufacturer, camera.interfaceType, camera.lensMount, camera.sensorFormat, camera.colorMode, camera.shutterType}).join(QLatin1Char(' ')).toLower());
    query.addBindValue(sourceKind);
    query.addBindValue(QStringLiteral("1"));
    query.addBindValue(now);
    query.addBindValue(now);
    if (!query.exec()) {
        if (errorMessage)
            *errorMessage = QString::fromUtf8("无法写入相机产品：%1").arg(sqlErrorText(query));
        return false;
    }
    if (id)
        *id = query.lastInsertId().toLongLong();
    return true;
}

bool CatalogRepository::insertLensIntoDatabase(const LensSpec &lens, const QString &sourceKind, bool replaceExisting, qint64 *id, QString *errorMessage) const
{
    if (!openDatabase(errorMessage))
        return false;
    const QString command = replaceExisting ? QStringLiteral("INSERT OR REPLACE") : QStringLiteral("INSERT OR IGNORE");
    QSqlQuery query(m_db);
    query.prepare(command + QStringLiteral(
        " INTO lens_products (model, manufacturer, manufacturer_key, model_key, lens_type, lens_mount, focal_length_mm,"
        " min_wd_mm, distortion_percent, image_circle_mm, megapixel_rating, recommended_min_pixel_um, pmag,"
        " nominal_wd_mm, wd_tolerance_mm, max_sensor_diagonal_mm, telecentricity_deg, dof_mm, numerical_aperture,"
        " f_number, coaxial_illumination, notes, search_text, source_kind, source_version, created_at, updated_at)"
        " VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));
    const QString now = nowUtcIso();
    query.addBindValue(lens.model);
    query.addBindValue(lens.manufacturer);
    query.addBindValue(keyForText(lens.manufacturer));
    query.addBindValue(keyForText(lens.model));
    query.addBindValue(lensTypeKey(lens.lensType));
    query.addBindValue(lens.lensMount);
    query.addBindValue(lens.focalLengthMm);
    query.addBindValue(lens.minWorkingDistanceMm);
    query.addBindValue(lens.distortionPercent);
    query.addBindValue(lens.imageCircleMm);
    query.addBindValue(lens.megapixelRating);
    query.addBindValue(lens.recommendedMinPixelUm);
    query.addBindValue(lens.pmag);
    query.addBindValue(lens.nominalWorkingDistanceMm);
    query.addBindValue(lens.workingDistanceToleranceMm);
    query.addBindValue(lens.maxSensorDiagonalMm);
    query.addBindValue(lens.telecentricityDeg);
    query.addBindValue(lens.dofMm);
    query.addBindValue(lens.numericalAperture);
    query.addBindValue(lens.fNumber);
    query.addBindValue(lens.coaxialIllumination ? 1 : 0);
    query.addBindValue(lens.notes);
    query.addBindValue(QStringList({lens.model, lens.manufacturer, lensTypeKey(lens.lensType), lens.typeLabel(), lens.lensMount, lens.notes}).join(QLatin1Char(' ')).toLower());
    query.addBindValue(sourceKind);
    query.addBindValue(QStringLiteral("1"));
    query.addBindValue(now);
    query.addBindValue(now);
    if (!query.exec()) {
        if (errorMessage)
            *errorMessage = QString::fromUtf8("无法写入镜头产品：%1").arg(sqlErrorText(query));
        return false;
    }
    if (id)
        *id = query.lastInsertId().toLongLong();
    return true;
}

bool CatalogRepository::insertLightIntoDatabase(const LightSpec &light, const QString &sourceKind, bool replaceExisting, qint64 *id, QString *errorMessage) const
{
    if (!openDatabase(errorMessage))
        return false;
    const QString command = replaceExisting ? QStringLiteral("INSERT OR REPLACE") : QStringLiteral("INSERT OR IGNORE");
    QSqlQuery query(m_db);
    query.prepare(command + QStringLiteral(
        " INTO light_products (model, manufacturer, manufacturer_key, model_key, light_type, color, wavelength_nm,"
        " mode, active_width_mm, active_height_mm, best_for, search_text, source_kind, source_version, created_at, updated_at)"
        " VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));
    const QString now = nowUtcIso();
    query.addBindValue(light.model);
    query.addBindValue(light.manufacturer);
    query.addBindValue(keyForText(light.manufacturer));
    query.addBindValue(keyForText(light.model));
    query.addBindValue(lightTypeKey(light.lightType));
    query.addBindValue(light.color);
    query.addBindValue(light.wavelengthNm);
    query.addBindValue(light.mode);
    query.addBindValue(light.activeWidthMm);
    query.addBindValue(light.activeHeightMm);
    query.addBindValue(light.bestFor);
    query.addBindValue(QStringList({light.model, light.manufacturer, lightTypeKey(light.lightType), light.typeLabel(), light.color, light.mode, light.bestFor}).join(QLatin1Char(' ')).toLower());
    query.addBindValue(sourceKind);
    query.addBindValue(QStringLiteral("1"));
    query.addBindValue(now);
    query.addBindValue(now);
    if (!query.exec()) {
        if (errorMessage)
            *errorMessage = QString::fromUtf8("无法写入光源产品：%1").arg(sqlErrorText(query));
        return false;
    }
    if (id)
        *id = query.lastInsertId().toLongLong();
    m_lightCandidateCache.clear();
    return true;
}

bool CatalogRepository::replaceCamerasInDatabase(const QVector<CameraSpec> &cameras, const QString &sourceKind, QString *errorMessage)
{
    if (!openDatabase(errorMessage))
        return false;
    if (!m_db.transaction()) {
        if (errorMessage)
            *errorMessage = QString::fromUtf8("无法开始相机产品导入事务：%1").arg(sqlErrorText(m_db));
        return false;
    }
    if (!execSql(m_db, QStringLiteral("DELETE FROM camera_products"), errorMessage)) {
        m_db.rollback();
        return false;
    }
    for (const CameraSpec &camera : cameras) {
        if (!insertCameraIntoDatabase(camera, sourceKind, true, nullptr, errorMessage)) {
            m_db.rollback();
            return false;
        }
    }
    if (!m_db.commit()) {
        if (errorMessage)
            *errorMessage = QString::fromUtf8("无法提交相机产品导入事务：%1").arg(sqlErrorText(m_db));
        return false;
    }
    return true;
}

bool CatalogRepository::replaceLensesInDatabase(const QVector<LensSpec> &lenses, const QString &sourceKind, QString *errorMessage)
{
    if (!openDatabase(errorMessage))
        return false;
    if (!m_db.transaction()) {
        if (errorMessage)
            *errorMessage = QString::fromUtf8("无法开始镜头产品导入事务：%1").arg(sqlErrorText(m_db));
        return false;
    }
    if (!execSql(m_db, QStringLiteral("DELETE FROM lens_products"), errorMessage)) {
        m_db.rollback();
        return false;
    }
    for (const LensSpec &lens : lenses) {
        if (!insertLensIntoDatabase(lens, sourceKind, true, nullptr, errorMessage)) {
            m_db.rollback();
            return false;
        }
    }
    if (!m_db.commit()) {
        if (errorMessage)
            *errorMessage = QString::fromUtf8("无法提交镜头产品导入事务：%1").arg(sqlErrorText(m_db));
        return false;
    }
    return true;
}

bool CatalogRepository::replaceLightsInDatabase(const QVector<LightSpec> &lights, const QString &sourceKind, QString *errorMessage)
{
    m_lightCandidateCache.clear();
    if (!openDatabase(errorMessage))
        return false;
    if (!m_db.transaction()) {
        if (errorMessage)
            *errorMessage = QString::fromUtf8("无法开始光源产品导入事务：%1").arg(sqlErrorText(m_db));
        return false;
    }
    if (!execSql(m_db, QStringLiteral("DELETE FROM light_products"), errorMessage)) {
        m_db.rollback();
        return false;
    }
    for (const LightSpec &light : lights) {
        if (!insertLightIntoDatabase(light, sourceKind, true, nullptr, errorMessage)) {
            m_db.rollback();
            return false;
        }
    }
    if (!m_db.commit()) {
        if (errorMessage)
            *errorMessage = QString::fromUtf8("无法提交光源产品导入事务：%1").arg(sqlErrorText(m_db));
        return false;
    }
    return true;
}

bool CatalogRepository::refreshSnapshots(QString *errorMessage)
{
    if (!openDatabase(errorMessage))
        return false;

    m_cameras.clear();
    m_lenses.clear();
    m_lights.clear();
    m_cameraIds.clear();
    m_lensIds.clear();
    m_lightIds.clear();

    QSqlQuery cameraQuery(m_db);
    if (!cameraQuery.exec(QStringLiteral(
            "SELECT id, model, manufacturer, resolution_x, resolution_y, pixel_size_um, sensor_format, color_mode,"
            " shutter_type, max_fps, interface, bandwidth_mbps, bit_depth, dynamic_range_db, lens_mount"
            " FROM camera_products ORDER BY id"))) {
        if (errorMessage)
            *errorMessage = QString::fromUtf8("无法读取相机产品库：%1").arg(sqlErrorText(cameraQuery));
        return false;
    }
    while (cameraQuery.next()) {
        CameraSpec spec;
        m_cameraIds.append(cameraQuery.value(0).toLongLong());
        spec.model = cameraQuery.value(1).toString();
        spec.manufacturer = cameraQuery.value(2).toString();
        spec.resolutionX = cameraQuery.value(3).toInt();
        spec.resolutionY = cameraQuery.value(4).toInt();
        spec.pixelSizeUm = cameraQuery.value(5).toDouble();
        spec.sensorFormat = cameraQuery.value(6).toString();
        spec.colorMode = cameraQuery.value(7).toString();
        spec.shutterType = cameraQuery.value(8).toString();
        spec.maxFps = cameraQuery.value(9).toDouble();
        spec.interfaceType = cameraQuery.value(10).toString();
        spec.bandwidthMBps = cameraQuery.value(11).toDouble();
        spec.bitDepth = cameraQuery.value(12).toDouble();
        spec.dynamicRangeDb = cameraQuery.value(13).toDouble();
        spec.lensMount = cameraQuery.value(14).toString();
        m_cameras.append(spec);
    }

    QSqlQuery lensQuery(m_db);
    if (!lensQuery.exec(QStringLiteral(
            "SELECT id, model, manufacturer, lens_type, lens_mount, focal_length_mm, min_wd_mm, distortion_percent,"
            " image_circle_mm, megapixel_rating, recommended_min_pixel_um, pmag, nominal_wd_mm, wd_tolerance_mm,"
            " max_sensor_diagonal_mm, telecentricity_deg, dof_mm, numerical_aperture, f_number, coaxial_illumination, notes"
            " FROM lens_products ORDER BY id"))) {
        if (errorMessage)
            *errorMessage = QString::fromUtf8("无法读取镜头产品库：%1").arg(sqlErrorText(lensQuery));
        return false;
    }
    while (lensQuery.next()) {
        LensSpec spec;
        m_lensIds.append(lensQuery.value(0).toLongLong());
        spec.model = lensQuery.value(1).toString();
        spec.manufacturer = lensQuery.value(2).toString();
        spec.lensType = lensTypeFromString(lensQuery.value(3).toString());
        spec.lensMount = lensQuery.value(4).toString();
        spec.focalLengthMm = lensQuery.value(5).toDouble();
        spec.minWorkingDistanceMm = lensQuery.value(6).toDouble();
        spec.distortionPercent = lensQuery.value(7).toDouble();
        spec.imageCircleMm = lensQuery.value(8).toDouble();
        spec.megapixelRating = lensQuery.value(9).toDouble();
        spec.recommendedMinPixelUm = lensQuery.value(10).toDouble();
        spec.pmag = lensQuery.value(11).toDouble();
        spec.nominalWorkingDistanceMm = lensQuery.value(12).toDouble();
        spec.workingDistanceToleranceMm = lensQuery.value(13).toDouble();
        spec.maxSensorDiagonalMm = lensQuery.value(14).toDouble();
        spec.telecentricityDeg = lensQuery.value(15).toDouble();
        spec.dofMm = lensQuery.value(16).toDouble();
        spec.numericalAperture = lensQuery.value(17).toDouble();
        spec.fNumber = lensQuery.value(18).toDouble();
        spec.coaxialIllumination = lensQuery.value(19).toInt() != 0;
        spec.notes = lensQuery.value(20).toString();
        m_lenses.append(spec);
    }

    QSqlQuery lightQuery(m_db);
    if (!lightQuery.exec(QStringLiteral(
            "SELECT id, model, manufacturer, light_type, color, wavelength_nm, mode, active_width_mm, active_height_mm, best_for"
            " FROM light_products ORDER BY id"))) {
        if (errorMessage)
            *errorMessage = QString::fromUtf8("无法读取光源产品库：%1").arg(sqlErrorText(lightQuery));
        return false;
    }
    while (lightQuery.next()) {
        LightSpec spec;
        m_lightIds.append(lightQuery.value(0).toLongLong());
        spec.model = lightQuery.value(1).toString();
        spec.manufacturer = lightQuery.value(2).toString();
        spec.lightType = lightTypeFromString(lightQuery.value(3).toString());
        spec.color = lightQuery.value(4).toString();
        spec.wavelengthNm = lightQuery.value(5).toInt();
        spec.mode = lightQuery.value(6).toString();
        spec.activeWidthMm = lightQuery.value(7).toDouble();
        spec.activeHeightMm = lightQuery.value(8).toDouble();
        spec.bestFor = lightQuery.value(9).toString();
        m_lights.append(spec);
    }
    m_snapshotsLoaded = true;
    return true;
}

bool CatalogRepository::refreshSnapshotsIfLoaded(QString *errorMessage)
{
    return m_snapshotsLoaded ? refreshSnapshots(errorMessage) : true;
}

bool CatalogRepository::loadCameraCsv(const QString &filePath, QString *errorMessage)
{
    QVector<Row> rows;
    QVector<CameraSpec> cameras;
    if (!readCsvRows(filePath, &rows, errorMessage)
        || !parseCameraSpecs(rows, filePath, &cameras, errorMessage)
        || !replaceCamerasInDatabase(cameras, QStringLiteral("local"), errorMessage)
        || !refreshSnapshotsIfLoaded(errorMessage)) {
        return false;
    }
    return true;
}

bool CatalogRepository::loadLensCsv(const QString &filePath, QString *errorMessage)
{
    QVector<Row> rows;
    QVector<LensSpec> lenses;
    if (!readCsvRows(filePath, &rows, errorMessage)
        || !parseLensSpecs(rows, filePath, &lenses, errorMessage)
        || !replaceLensesInDatabase(lenses, QStringLiteral("local"), errorMessage)
        || !refreshSnapshotsIfLoaded(errorMessage)) {
        return false;
    }
    return true;
}

bool CatalogRepository::loadLightCsv(const QString &filePath, QString *errorMessage)
{
    QVector<Row> rows;
    QVector<LightSpec> lights;
    if (!readCsvRows(filePath, &rows, errorMessage)
        || !parseLightSpecs(rows, filePath, &lights, errorMessage)
        || !replaceLightsInDatabase(lights, QStringLiteral("local"), errorMessage)
        || !refreshSnapshotsIfLoaded(errorMessage)) {
        return false;
    }
    return true;
}

bool CatalogRepository::exportCameraCsv(const QString &filePath, QString *errorMessage) const
{
    return writeCameraCsv(filePath, m_cameras, errorMessage);
}

bool CatalogRepository::exportLensCsv(const QString &filePath, QString *errorMessage) const
{
    return writeLensCsv(filePath, m_lenses, errorMessage);
}

bool CatalogRepository::exportLightCsv(const QString &filePath, QString *errorMessage) const
{
    return writeLightCsv(filePath, m_lights, errorMessage);
}

bool CatalogRepository::exportCameraCsv(const QString &filePath, const QVector<int> &indexes, QString *errorMessage) const
{
    QVector<CameraSpec> selected;
    for (int index : indexes) {
        if (index >= 0 && index < m_cameras.size())
            selected.append(m_cameras.at(index));
    }
    return writeCameraCsv(filePath, selected, errorMessage);
}

bool CatalogRepository::exportLensCsv(const QString &filePath, const QVector<int> &indexes, QString *errorMessage) const
{
    QVector<LensSpec> selected;
    for (int index : indexes) {
        if (index >= 0 && index < m_lenses.size())
            selected.append(m_lenses.at(index));
    }
    return writeLensCsv(filePath, selected, errorMessage);
}

bool CatalogRepository::exportLightCsv(const QString &filePath, const QVector<int> &indexes, QString *errorMessage) const
{
    QVector<LightSpec> selected;
    for (int index : indexes) {
        if (index >= 0 && index < m_lights.size())
            selected.append(m_lights.at(index));
    }
    return writeLightCsv(filePath, selected, errorMessage);
}

bool CatalogRepository::resetCamerasToBuiltIn(QString *errorMessage)
{
    QVector<Row> rows;
    QVector<CameraSpec> cameras;
    return readCsvRows(QStringLiteral(":/data/cameras.csv"), &rows, errorMessage)
        && parseCameraSpecs(rows, QString::fromUtf8("内置相机库"), &cameras, errorMessage)
        && replaceCamerasInDatabase(cameras, QStringLiteral("builtin"), errorMessage)
        && refreshSnapshotsIfLoaded(errorMessage);
}

bool CatalogRepository::resetLensesToBuiltIn(QString *errorMessage)
{
    QVector<Row> rows;
    QVector<LensSpec> lenses;
    return readCsvRows(QStringLiteral(":/data/lenses.csv"), &rows, errorMessage)
        && parseLensSpecs(rows, QString::fromUtf8("内置镜头库"), &lenses, errorMessage)
        && replaceLensesInDatabase(lenses, QStringLiteral("builtin"), errorMessage)
        && refreshSnapshotsIfLoaded(errorMessage);
}

bool CatalogRepository::resetLightsToBuiltIn(QString *errorMessage)
{
    QVector<Row> rows;
    QVector<LightSpec> lights;
    return readCsvRows(QStringLiteral(":/data/lights.csv"), &rows, errorMessage)
        && parseLightSpecs(rows, QString::fromUtf8("内置光源库"), &lights, errorMessage)
        && replaceLightsInDatabase(lights, QStringLiteral("builtin"), errorMessage)
        && refreshSnapshotsIfLoaded(errorMessage);
}

bool CatalogRepository::addCamera(const CameraSpec &camera, QString *errorMessage)
{
    if (!validateCamera(camera, QString::fromUtf8("\346\226\260\345\242\236\347\233\270\346\234\272"), errorMessage))
        return false;
    if (!openDatabase(errorMessage))
        return false;
    QSqlQuery duplicate(m_db);
    duplicate.prepare(QStringLiteral("SELECT id FROM camera_products WHERE manufacturer_key=? AND model_key=? LIMIT 1"));
    duplicate.addBindValue(keyForText(camera.manufacturer));
    duplicate.addBindValue(keyForText(camera.model));
    if (!duplicate.exec()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Unable to check duplicate camera product: %1").arg(sqlErrorText(duplicate));
        return false;
    }
    if (duplicate.next()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Duplicate camera product: %1 / %2").arg(camera.manufacturer, camera.model);
        return false;
    }
    return insertCameraIntoDatabase(camera, QStringLiteral("local"), false, nullptr, errorMessage)
        && refreshSnapshotsIfLoaded(errorMessage);
}

bool CatalogRepository::updateCamera(int index, const CameraSpec &camera, QString *errorMessage)
{
    if (index < 0 || index >= m_cameras.size()) {
        if (errorMessage)
            *errorMessage = QString::fromUtf8("\347\233\270\346\234\272\350\241\214\345\217\267\346\227\240\346\225\210");
        return false;
    }
    if (!validateCamera(camera, camera.model, errorMessage))
        return false;
    return updateCameraById(m_cameraIds.at(index), camera, errorMessage);
}

bool CatalogRepository::removeCamera(int index, QString *errorMessage)
{
    if (index < 0 || index >= m_cameras.size()) {
        if (errorMessage)
            *errorMessage = QString::fromUtf8("\347\233\270\346\234\272\350\241\214\345\217\267\346\227\240\346\225\210");
        return false;
    }
    return removeCameraById(m_cameraIds.at(index), errorMessage);
}

bool CatalogRepository::addLens(const LensSpec &lens, QString *errorMessage)
{
    if (!validateLens(lens, QString::fromUtf8("\346\226\260\345\242\236\351\225\234\345\244\264"), errorMessage))
        return false;
    if (!openDatabase(errorMessage))
        return false;
    QSqlQuery duplicate(m_db);
    duplicate.prepare(QStringLiteral("SELECT id FROM lens_products WHERE manufacturer_key=? AND model_key=? LIMIT 1"));
    duplicate.addBindValue(keyForText(lens.manufacturer));
    duplicate.addBindValue(keyForText(lens.model));
    if (!duplicate.exec()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Unable to check duplicate lens product: %1").arg(sqlErrorText(duplicate));
        return false;
    }
    if (duplicate.next()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Duplicate lens product: %1 / %2").arg(lens.manufacturer, lens.model);
        return false;
    }
    return insertLensIntoDatabase(lens, QStringLiteral("local"), false, nullptr, errorMessage)
        && refreshSnapshotsIfLoaded(errorMessage);
}

bool CatalogRepository::updateLens(int index, const LensSpec &lens, QString *errorMessage)
{
    if (index < 0 || index >= m_lenses.size()) {
        if (errorMessage)
            *errorMessage = QString::fromUtf8("\351\225\234\345\244\264\350\241\214\345\217\267\346\227\240\346\225\210");
        return false;
    }
    if (!validateLens(lens, lens.model, errorMessage))
        return false;
    return updateLensById(m_lensIds.at(index), lens, errorMessage);
}

bool CatalogRepository::removeLens(int index, QString *errorMessage)
{
    if (index < 0 || index >= m_lenses.size()) {
        if (errorMessage)
            *errorMessage = QString::fromUtf8("\351\225\234\345\244\264\350\241\214\345\217\267\346\227\240\346\225\210");
        return false;
    }
    return removeLensById(m_lensIds.at(index), errorMessage);
}

bool CatalogRepository::addLight(const LightSpec &light, QString *errorMessage)
{
    if (!validateLight(light, QString::fromUtf8("新增光源"), errorMessage))
        return false;
    if (!openDatabase(errorMessage))
        return false;
    QSqlQuery duplicate(m_db);
    duplicate.prepare(QStringLiteral("SELECT id FROM light_products WHERE manufacturer_key=? AND model_key=? LIMIT 1"));
    duplicate.addBindValue(keyForText(light.manufacturer));
    duplicate.addBindValue(keyForText(light.model));
    if (!duplicate.exec()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Unable to check duplicate light product: %1").arg(sqlErrorText(duplicate));
        return false;
    }
    if (duplicate.next()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Duplicate light product: %1 / %2").arg(light.manufacturer, light.model);
        return false;
    }
    return insertLightIntoDatabase(light, QStringLiteral("local"), false, nullptr, errorMessage)
        && refreshSnapshotsIfLoaded(errorMessage);
}

bool CatalogRepository::updateLight(int index, const LightSpec &light, QString *errorMessage)
{
    if (index < 0 || index >= m_lights.size()) {
        if (errorMessage)
            *errorMessage = QString::fromUtf8("光源行号无效");
        return false;
    }
    if (!validateLight(light, light.model, errorMessage))
        return false;
    return updateLightById(m_lightIds.at(index), light, errorMessage);
}

bool CatalogRepository::removeLight(int index, QString *errorMessage)
{
    if (index < 0 || index >= m_lights.size()) {
        if (errorMessage)
            *errorMessage = QString::fromUtf8("光源行号无效");
        return false;
    }
    return removeLightById(m_lightIds.at(index), errorMessage);
}

QString CatalogRepository::summary() const
{
    return QString::fromUtf8("\347\233\270\346\234\272 %1 \346\254\276\357\274\214\351\225\234\345\244\264 %2 \346\254\276\357\274\214\345\205\211\346\272\220 %3 \346\254\276")
        .arg(m_cameras.size())
        .arg(m_lenses.size())
        .arg(m_lights.size());
}

bool CatalogRepository::saveCameras(QString *errorMessage) const
{
    return writeCameraCsv(cameraStoragePath(), m_cameras, errorMessage);
}

bool CatalogRepository::saveLenses(QString *errorMessage) const
{
    return writeLensCsv(lensStoragePath(), m_lenses, errorMessage);
}

bool CatalogRepository::saveLights(QString *errorMessage) const
{
    return writeLightCsv(lightStoragePath(), m_lights, errorMessage);
}

namespace {
QString orderColumn(CatalogDomain domain, const QString &field)
{
    const QString f = field.trimmed().toLower();
    if (domain == CatalogDomain::Camera) {
        if (f == QStringLiteral("manufacturer")) return QStringLiteral("manufacturer");
        if (f == QStringLiteral("resolution_x")) return QStringLiteral("resolution_x");
        if (f == QStringLiteral("resolution_y")) return QStringLiteral("resolution_y");
        if (f == QStringLiteral("max_fps")) return QStringLiteral("max_fps");
        if (f == QStringLiteral("interface")) return QStringLiteral("interface");
        if (f == QStringLiteral("lens_mount")) return QStringLiteral("lens_mount");
        return QStringLiteral("model");
    }
    if (domain == CatalogDomain::Lens) {
        if (f == QStringLiteral("manufacturer")) return QStringLiteral("manufacturer");
        if (f == QStringLiteral("lens_type")) return QStringLiteral("lens_type");
        if (f == QStringLiteral("lens_mount")) return QStringLiteral("lens_mount");
        if (f == QStringLiteral("image_circle_mm")) return QStringLiteral("image_circle_mm");
        if (f == QStringLiteral("pmag")) return QStringLiteral("pmag");
        if (f == QStringLiteral("nominal_wd_mm")) return QStringLiteral("nominal_wd_mm");
        return QStringLiteral("model");
    }
    if (f == QStringLiteral("manufacturer")) return QStringLiteral("manufacturer");
    if (f == QStringLiteral("light_type")) return QStringLiteral("light_type");
    if (f == QStringLiteral("color")) return QStringLiteral("color");
    if (f == QStringLiteral("mode")) return QStringLiteral("mode");
    if (f == QStringLiteral("active_width_mm")) return QStringLiteral("active_width_mm");
    if (f == QStringLiteral("active_height_mm")) return QStringLiteral("active_height_mm");
    return QStringLiteral("model");
}

QString stableOrderClause(CatalogDomain domain, const CatalogSort &sort)
{
    const QString column = orderColumn(domain, sort.field);
    QStringList terms;
    terms << column + (sort.ascending ? QStringLiteral(" ASC") : QStringLiteral(" DESC"));
    if (column != QStringLiteral("manufacturer"))
        terms << QStringLiteral("manufacturer ASC");
    if (column != QStringLiteral("model"))
        terms << QStringLiteral("model ASC");
    terms << QStringLiteral("id ASC");
    return terms.join(QStringLiteral(", "));
}

void bindAll(QSqlQuery *query, const QList<QVariant> &values)
{
    for (const QVariant &value : values)
        query->addBindValue(value);
}

QString buildWhere(CatalogDomain domain, const CatalogQuery &catalogQuery, QList<QVariant> *values)
{
    QStringList clauses;
    if (!catalogQuery.search.trimmed().isEmpty()) {
        clauses.append(QStringLiteral("search_text LIKE ?"));
        values->append(likePattern(catalogQuery.search));
    }
    if (!catalogQuery.manufacturer.trimmed().isEmpty()) {
        clauses.append(QStringLiteral("manufacturer_key = ?"));
        values->append(catalogQuery.manufacturer.trimmed().toLower());
    }
    if (domain == CatalogDomain::Camera) {
        if (!catalogQuery.interfaceType.trimmed().isEmpty()) {
            clauses.append(QStringLiteral("interface = ?"));
            values->append(catalogQuery.interfaceType.trimmed());
        }
        if (!catalogQuery.lensMount.trimmed().isEmpty()) {
            clauses.append(QStringLiteral("lens_mount = ?"));
            values->append(catalogQuery.lensMount.trimmed());
        }
    } else if (domain == CatalogDomain::Lens) {
        if (!catalogQuery.type.trimmed().isEmpty()) {
            clauses.append(QStringLiteral("lens_type = ?"));
            values->append(normalizedQueryType(catalogQuery.type));
        }
        if (!catalogQuery.lensMount.trimmed().isEmpty()) {
            clauses.append(QStringLiteral("lens_mount = ?"));
            values->append(catalogQuery.lensMount.trimmed());
        }
    } else {
        if (!catalogQuery.type.trimmed().isEmpty()) {
            clauses.append(QStringLiteral("light_type = ?"));
            values->append(normalizedQueryType(catalogQuery.type));
        }
        if (!catalogQuery.mode.trimmed().isEmpty()) {
            clauses.append(QStringLiteral("mode = ?"));
            values->append(catalogQuery.mode.trimmed());
        }
    }
    return clauses.isEmpty() ? QString() : QStringLiteral(" WHERE ") + clauses.join(QStringLiteral(" AND "));
}

CameraSpec cameraFromQuery(const QSqlQuery &query)
{
    CameraSpec spec;
    spec.model = query.value(1).toString();
    spec.manufacturer = query.value(2).toString();
    spec.resolutionX = query.value(3).toInt();
    spec.resolutionY = query.value(4).toInt();
    spec.pixelSizeUm = query.value(5).toDouble();
    spec.sensorFormat = query.value(6).toString();
    spec.colorMode = query.value(7).toString();
    spec.shutterType = query.value(8).toString();
    spec.maxFps = query.value(9).toDouble();
    spec.interfaceType = query.value(10).toString();
    spec.bandwidthMBps = query.value(11).toDouble();
    spec.bitDepth = query.value(12).toDouble();
    spec.dynamicRangeDb = query.value(13).toDouble();
    spec.lensMount = query.value(14).toString();
    return spec;
}

LensSpec lensFromQuery(const QSqlQuery &query)
{
    LensSpec spec;
    spec.model = query.value(1).toString();
    spec.manufacturer = query.value(2).toString();
    spec.lensType = lensTypeFromString(query.value(3).toString());
    spec.lensMount = query.value(4).toString();
    spec.focalLengthMm = query.value(5).toDouble();
    spec.minWorkingDistanceMm = query.value(6).toDouble();
    spec.distortionPercent = query.value(7).toDouble();
    spec.imageCircleMm = query.value(8).toDouble();
    spec.megapixelRating = query.value(9).toDouble();
    spec.recommendedMinPixelUm = query.value(10).toDouble();
    spec.pmag = query.value(11).toDouble();
    spec.nominalWorkingDistanceMm = query.value(12).toDouble();
    spec.workingDistanceToleranceMm = query.value(13).toDouble();
    spec.maxSensorDiagonalMm = query.value(14).toDouble();
    spec.telecentricityDeg = query.value(15).toDouble();
    spec.dofMm = query.value(16).toDouble();
    spec.numericalAperture = query.value(17).toDouble();
    spec.fNumber = query.value(18).toDouble();
    spec.coaxialIllumination = query.value(19).toInt() != 0;
    spec.notes = query.value(20).toString();
    return spec;
}

LightSpec lightFromQuery(const QSqlQuery &query)
{
    LightSpec spec;
    spec.model = query.value(1).toString();
    spec.manufacturer = query.value(2).toString();
    spec.lightType = lightTypeFromString(query.value(3).toString());
    spec.color = query.value(4).toString();
    spec.wavelengthNm = query.value(5).toInt();
    spec.mode = query.value(6).toString();
    spec.activeWidthMm = query.value(7).toDouble();
    spec.activeHeightMm = query.value(8).toDouble();
    spec.bestFor = query.value(9).toString();
    return spec;
}
}

CatalogPageResult<CameraSpec> CatalogRepository::queryCameras(const CatalogQuery &catalogQuery, QString *errorMessage) const
{
    CatalogPageResult<CameraSpec> result;
    if (!openDatabase(errorMessage))
        return result;
    QList<QVariant> values;
    const QString where = buildWhere(CatalogDomain::Camera, catalogQuery, &values);
    QSqlQuery count(m_db);
    count.prepare(QStringLiteral("SELECT COUNT(*) FROM camera_products") + where);
    bindAll(&count, values);
    if (!count.exec() || !count.next()) {
        if (errorMessage)
            *errorMessage = QString::fromUtf8("无法查询相机数量：%1").arg(sqlErrorText(count));
        return result;
    }
    result.totalCount = count.value(0).toInt();

    QString sql = QStringLiteral(
        "SELECT id, model, manufacturer, resolution_x, resolution_y, pixel_size_um, sensor_format, color_mode,"
        " shutter_type, max_fps, interface, bandwidth_mbps, bit_depth, dynamic_range_db, lens_mount"
        " FROM camera_products")
        + where
        + QStringLiteral(" ORDER BY ") + stableOrderClause(CatalogDomain::Camera, catalogQuery.sort);
    if (catalogQuery.limit > 0)
        sql += QStringLiteral(" LIMIT ? OFFSET ?");
    QSqlQuery query(m_db);
    query.prepare(sql);
    bindAll(&query, values);
    if (catalogQuery.limit > 0) {
        query.addBindValue(catalogQuery.limit);
        query.addBindValue(qMax(0, catalogQuery.offset));
    }
    if (!query.exec()) {
        if (errorMessage)
            *errorMessage = QString::fromUtf8("无法查询相机产品：%1").arg(sqlErrorText(query));
        return result;
    }
    while (query.next()) {
        result.ids.append(query.value(0).toLongLong());
        result.items.append(cameraFromQuery(query));
    }
    return result;
}

CatalogPageResult<LensSpec> CatalogRepository::queryLenses(const CatalogQuery &catalogQuery, QString *errorMessage) const
{
    CatalogPageResult<LensSpec> result;
    if (!openDatabase(errorMessage))
        return result;
    QList<QVariant> values;
    const QString where = buildWhere(CatalogDomain::Lens, catalogQuery, &values);
    QSqlQuery count(m_db);
    count.prepare(QStringLiteral("SELECT COUNT(*) FROM lens_products") + where);
    bindAll(&count, values);
    if (!count.exec() || !count.next()) {
        if (errorMessage)
            *errorMessage = QString::fromUtf8("无法查询镜头数量：%1").arg(sqlErrorText(count));
        return result;
    }
    result.totalCount = count.value(0).toInt();

    QString sql = QStringLiteral(
        "SELECT id, model, manufacturer, lens_type, lens_mount, focal_length_mm, min_wd_mm, distortion_percent,"
        " image_circle_mm, megapixel_rating, recommended_min_pixel_um, pmag, nominal_wd_mm, wd_tolerance_mm,"
        " max_sensor_diagonal_mm, telecentricity_deg, dof_mm, numerical_aperture, f_number, coaxial_illumination, notes"
        " FROM lens_products")
        + where
        + QStringLiteral(" ORDER BY ") + stableOrderClause(CatalogDomain::Lens, catalogQuery.sort);
    if (catalogQuery.limit > 0)
        sql += QStringLiteral(" LIMIT ? OFFSET ?");
    QSqlQuery query(m_db);
    query.prepare(sql);
    bindAll(&query, values);
    if (catalogQuery.limit > 0) {
        query.addBindValue(catalogQuery.limit);
        query.addBindValue(qMax(0, catalogQuery.offset));
    }
    if (!query.exec()) {
        if (errorMessage)
            *errorMessage = QString::fromUtf8("无法查询镜头产品：%1").arg(sqlErrorText(query));
        return result;
    }
    while (query.next()) {
        result.ids.append(query.value(0).toLongLong());
        result.items.append(lensFromQuery(query));
    }
    return result;
}

CatalogPageResult<LightSpec> CatalogRepository::queryLights(const CatalogQuery &catalogQuery, QString *errorMessage) const
{
    CatalogPageResult<LightSpec> result;
    if (!openDatabase(errorMessage))
        return result;
    QList<QVariant> values;
    const QString where = buildWhere(CatalogDomain::Light, catalogQuery, &values);
    QSqlQuery count(m_db);
    count.prepare(QStringLiteral("SELECT COUNT(*) FROM light_products") + where);
    bindAll(&count, values);
    if (!count.exec() || !count.next()) {
        if (errorMessage)
            *errorMessage = QString::fromUtf8("无法查询光源数量：%1").arg(sqlErrorText(count));
        return result;
    }
    result.totalCount = count.value(0).toInt();

    QString sql = QStringLiteral(
        "SELECT id, model, manufacturer, light_type, color, wavelength_nm, mode, active_width_mm, active_height_mm, best_for"
        " FROM light_products")
        + where
        + QStringLiteral(" ORDER BY ") + stableOrderClause(CatalogDomain::Light, catalogQuery.sort);
    if (catalogQuery.limit > 0)
        sql += QStringLiteral(" LIMIT ? OFFSET ?");
    QSqlQuery query(m_db);
    query.prepare(sql);
    bindAll(&query, values);
    if (catalogQuery.limit > 0) {
        query.addBindValue(catalogQuery.limit);
        query.addBindValue(qMax(0, catalogQuery.offset));
    }
    if (!query.exec()) {
        if (errorMessage)
            *errorMessage = QString::fromUtf8("无法查询光源产品：%1").arg(sqlErrorText(query));
        return result;
    }
    while (query.next()) {
        result.ids.append(query.value(0).toLongLong());
        result.items.append(lightFromQuery(query));
    }
    return result;
}

bool CatalogRepository::exportCameraCsvByQuery(const QString &filePath, const CatalogQuery &query, QString *errorMessage) const
{
    CatalogQuery all = query;
    all.limit = 0;
    QString queryError;
    const CatalogPageResult<CameraSpec> page = queryCameras(all, &queryError);
    if (!queryError.isEmpty()) {
        if (errorMessage)
            *errorMessage = queryError;
        return false;
    }
    return writeCameraCsv(filePath, page.items, errorMessage);
}

bool CatalogRepository::exportLensCsvByQuery(const QString &filePath, const CatalogQuery &query, QString *errorMessage) const
{
    CatalogQuery all = query;
    all.limit = 0;
    QString queryError;
    const CatalogPageResult<LensSpec> page = queryLenses(all, &queryError);
    if (!queryError.isEmpty()) {
        if (errorMessage)
            *errorMessage = queryError;
        return false;
    }
    return writeLensCsv(filePath, page.items, errorMessage);
}

bool CatalogRepository::exportLightCsvByQuery(const QString &filePath, const CatalogQuery &query, QString *errorMessage) const
{
    CatalogQuery all = query;
    all.limit = 0;
    QString queryError;
    const CatalogPageResult<LightSpec> page = queryLights(all, &queryError);
    if (!queryError.isEmpty()) {
        if (errorMessage)
            *errorMessage = queryError;
        return false;
    }
    return writeLightCsv(filePath, page.items, errorMessage);
}

QStringList CatalogRepository::distinctValues(CatalogDomain domain, const QString &field, QString *errorMessage) const
{
    QString table;
    QString column;
    if (domain == CatalogDomain::Camera) {
        table = QStringLiteral("camera_products");
        column = field == QStringLiteral("interface") ? QStringLiteral("interface")
            : field == QStringLiteral("lens_mount") ? QStringLiteral("lens_mount") : QStringLiteral("manufacturer");
    } else if (domain == CatalogDomain::Lens) {
        table = QStringLiteral("lens_products");
        column = field == QStringLiteral("lens_type") ? QStringLiteral("lens_type")
            : field == QStringLiteral("lens_mount") ? QStringLiteral("lens_mount") : QStringLiteral("manufacturer");
    } else {
        table = QStringLiteral("light_products");
        column = field == QStringLiteral("light_type") ? QStringLiteral("light_type")
            : field == QStringLiteral("mode") ? QStringLiteral("mode") : QStringLiteral("manufacturer");
    }
    QStringList values;
    if (!openDatabase(errorMessage))
        return values;
    QSqlQuery query(m_db);
    if (!query.exec(QStringLiteral("SELECT DISTINCT %1 FROM %2 WHERE %1 IS NOT NULL AND TRIM(%1) <> '' ORDER BY %1").arg(column, table))) {
        if (errorMessage)
            *errorMessage = QString::fromUtf8("无法查询筛选项：%1").arg(sqlErrorText(query));
        return values;
    }
    while (query.next()) {
        const QString value = query.value(0).toString();
        if (column == QStringLiteral("lens_type"))
            values.append(lensTypeLabel(lensTypeFromString(value)));
        else if (column == QStringLiteral("light_type"))
            values.append(lightTypeLabel(lightTypeFromString(value)));
        else
            values.append(value);
    }
    values.removeDuplicates();
    values.sort(Qt::CaseInsensitive);
    return values;
}

int CatalogRepository::productCount(CatalogDomain domain, QString *errorMessage) const
{
    QString table;
    if (domain == CatalogDomain::Camera)
        table = QStringLiteral("camera_products");
    else if (domain == CatalogDomain::Lens)
        table = QStringLiteral("lens_products");
    else
        table = QStringLiteral("light_products");

    if (!openDatabase(errorMessage))
        return -1;
    QSqlQuery query(m_db);
    if (!query.exec(QStringLiteral("SELECT COUNT(*) FROM %1").arg(table)) || !query.next()) {
        if (errorMessage)
            *errorMessage = QString::fromUtf8("无法查询产品数量：%1").arg(sqlErrorText(query));
        return -1;
    }
    return query.value(0).toInt();
}

bool CatalogRepository::cameraById(qint64 id, CameraSpec *camera, QString *errorMessage) const
{
    const int index = cameraIndexById(id);
    if (index >= 0 && camera) {
        *camera = m_cameras.at(index);
        return true;
    }
    if (!openDatabase(errorMessage))
        return false;
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "SELECT id, model, manufacturer, resolution_x, resolution_y, pixel_size_um, sensor_format, color_mode,"
        " shutter_type, max_fps, interface, bandwidth_mbps, bit_depth, dynamic_range_db, lens_mount"
        " FROM camera_products WHERE id=?"));
    query.addBindValue(id);
    if (!query.exec() || !query.next()) {
        if (errorMessage)
            *errorMessage = QString::fromUtf8("相机 ID 无效：%1").arg(id);
        return false;
    }
    if (camera)
        *camera = cameraFromQuery(query);
    return true;
}

bool CatalogRepository::lensById(qint64 id, LensSpec *lens, QString *errorMessage) const
{
    const int index = lensIndexById(id);
    if (index >= 0 && lens) {
        *lens = m_lenses.at(index);
        return true;
    }
    if (!openDatabase(errorMessage))
        return false;
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "SELECT id, model, manufacturer, lens_type, lens_mount, focal_length_mm, min_wd_mm, distortion_percent,"
        " image_circle_mm, megapixel_rating, recommended_min_pixel_um, pmag, nominal_wd_mm, wd_tolerance_mm,"
        " max_sensor_diagonal_mm, telecentricity_deg, dof_mm, numerical_aperture, f_number, coaxial_illumination, notes"
        " FROM lens_products WHERE id=?"));
    query.addBindValue(id);
    if (!query.exec() || !query.next()) {
        if (errorMessage)
            *errorMessage = QString::fromUtf8("镜头 ID 无效：%1").arg(id);
        return false;
    }
    if (lens)
        *lens = lensFromQuery(query);
    return true;
}

bool CatalogRepository::lightById(qint64 id, LightSpec *light, QString *errorMessage) const
{
    const int index = lightIndexById(id);
    if (index >= 0 && light) {
        *light = m_lights.at(index);
        return true;
    }
    if (!openDatabase(errorMessage))
        return false;
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "SELECT id, model, manufacturer, light_type, color, wavelength_nm, mode, active_width_mm, active_height_mm, best_for"
        " FROM light_products WHERE id=?"));
    query.addBindValue(id);
    if (!query.exec() || !query.next()) {
        if (errorMessage)
            *errorMessage = QString::fromUtf8("光源 ID 无效：%1").arg(id);
        return false;
    }
    if (light)
        *light = lightFromQuery(query);
    return true;
}

bool CatalogRepository::updateCameraById(qint64 id, const CameraSpec &camera, QString *errorMessage)
{
    if (!validateCamera(camera, camera.model, errorMessage) || !openDatabase(errorMessage))
        return false;
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "UPDATE camera_products SET model=?, manufacturer=?, manufacturer_key=?, model_key=?, resolution_x=?, resolution_y=?,"
        " pixel_size_um=?, sensor_format=?, color_mode=?, shutter_type=?, max_fps=?, interface=?, bandwidth_mbps=?,"
        " bit_depth=?, dynamic_range_db=?, lens_mount=?, search_text=?, source_kind='local', updated_at=? WHERE id=?"));
    query.addBindValue(camera.model);
    query.addBindValue(camera.manufacturer);
    query.addBindValue(keyForText(camera.manufacturer));
    query.addBindValue(keyForText(camera.model));
    query.addBindValue(camera.resolutionX);
    query.addBindValue(camera.resolutionY);
    query.addBindValue(camera.pixelSizeUm);
    query.addBindValue(camera.sensorFormat);
    query.addBindValue(camera.colorMode);
    query.addBindValue(camera.shutterType);
    query.addBindValue(camera.maxFps);
    query.addBindValue(camera.interfaceType);
    query.addBindValue(camera.bandwidthMBps);
    query.addBindValue(camera.bitDepth);
    query.addBindValue(camera.dynamicRangeDb);
    query.addBindValue(camera.lensMount);
    query.addBindValue(QStringList({camera.model, camera.manufacturer, camera.interfaceType, camera.lensMount, camera.sensorFormat, camera.colorMode, camera.shutterType}).join(QLatin1Char(' ')).toLower());
    query.addBindValue(nowUtcIso());
    query.addBindValue(id);
    if (!query.exec()) {
        if (errorMessage)
            *errorMessage = QString::fromUtf8("无法更新相机产品：%1").arg(sqlErrorText(query));
        return false;
    }
    return refreshSnapshotsIfLoaded(errorMessage);
}

bool CatalogRepository::updateLensById(qint64 id, const LensSpec &lens, QString *errorMessage)
{
    if (!validateLens(lens, lens.model, errorMessage) || !openDatabase(errorMessage))
        return false;
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "UPDATE lens_products SET model=?, manufacturer=?, manufacturer_key=?, model_key=?, lens_type=?, lens_mount=?,"
        " focal_length_mm=?, min_wd_mm=?, distortion_percent=?, image_circle_mm=?, megapixel_rating=?,"
        " recommended_min_pixel_um=?, pmag=?, nominal_wd_mm=?, wd_tolerance_mm=?, max_sensor_diagonal_mm=?,"
        " telecentricity_deg=?, dof_mm=?, numerical_aperture=?, f_number=?, coaxial_illumination=?, notes=?,"
        " search_text=?, source_kind='local', updated_at=? WHERE id=?"));
    query.addBindValue(lens.model);
    query.addBindValue(lens.manufacturer);
    query.addBindValue(keyForText(lens.manufacturer));
    query.addBindValue(keyForText(lens.model));
    query.addBindValue(lensTypeKey(lens.lensType));
    query.addBindValue(lens.lensMount);
    query.addBindValue(lens.focalLengthMm);
    query.addBindValue(lens.minWorkingDistanceMm);
    query.addBindValue(lens.distortionPercent);
    query.addBindValue(lens.imageCircleMm);
    query.addBindValue(lens.megapixelRating);
    query.addBindValue(lens.recommendedMinPixelUm);
    query.addBindValue(lens.pmag);
    query.addBindValue(lens.nominalWorkingDistanceMm);
    query.addBindValue(lens.workingDistanceToleranceMm);
    query.addBindValue(lens.maxSensorDiagonalMm);
    query.addBindValue(lens.telecentricityDeg);
    query.addBindValue(lens.dofMm);
    query.addBindValue(lens.numericalAperture);
    query.addBindValue(lens.fNumber);
    query.addBindValue(lens.coaxialIllumination ? 1 : 0);
    query.addBindValue(lens.notes);
    query.addBindValue(QStringList({lens.model, lens.manufacturer, lensTypeKey(lens.lensType), lens.typeLabel(), lens.lensMount, lens.notes}).join(QLatin1Char(' ')).toLower());
    query.addBindValue(nowUtcIso());
    query.addBindValue(id);
    if (!query.exec()) {
        if (errorMessage)
            *errorMessage = QString::fromUtf8("无法更新镜头产品：%1").arg(sqlErrorText(query));
        return false;
    }
    return refreshSnapshotsIfLoaded(errorMessage);
}

bool CatalogRepository::updateLightById(qint64 id, const LightSpec &light, QString *errorMessage)
{
    if (!validateLight(light, light.model, errorMessage) || !openDatabase(errorMessage))
        return false;
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "UPDATE light_products SET model=?, manufacturer=?, manufacturer_key=?, model_key=?, light_type=?, color=?,"
        " wavelength_nm=?, mode=?, active_width_mm=?, active_height_mm=?, best_for=?, search_text=?,"
        " source_kind='local', updated_at=? WHERE id=?"));
    query.addBindValue(light.model);
    query.addBindValue(light.manufacturer);
    query.addBindValue(keyForText(light.manufacturer));
    query.addBindValue(keyForText(light.model));
    query.addBindValue(lightTypeKey(light.lightType));
    query.addBindValue(light.color);
    query.addBindValue(light.wavelengthNm);
    query.addBindValue(light.mode);
    query.addBindValue(light.activeWidthMm);
    query.addBindValue(light.activeHeightMm);
    query.addBindValue(light.bestFor);
    query.addBindValue(QStringList({light.model, light.manufacturer, lightTypeKey(light.lightType), light.typeLabel(), light.color, light.mode, light.bestFor}).join(QLatin1Char(' ')).toLower());
    query.addBindValue(nowUtcIso());
    query.addBindValue(id);
    if (!query.exec()) {
        if (errorMessage)
            *errorMessage = QString::fromUtf8("无法更新光源产品：%1").arg(sqlErrorText(query));
        return false;
    }
    m_lightCandidateCache.clear();
    return refreshSnapshotsIfLoaded(errorMessage);
}

bool CatalogRepository::removeCameraById(qint64 id, QString *errorMessage)
{
    if (!openDatabase(errorMessage))
        return false;
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("DELETE FROM camera_products WHERE id=?"));
    query.addBindValue(id);
    if (!query.exec()) {
        if (errorMessage)
            *errorMessage = QString::fromUtf8("无法删除相机产品：%1").arg(sqlErrorText(query));
        return false;
    }
    return refreshSnapshotsIfLoaded(errorMessage);
}

bool CatalogRepository::removeLensById(qint64 id, QString *errorMessage)
{
    if (!openDatabase(errorMessage))
        return false;
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("DELETE FROM lens_products WHERE id=?"));
    query.addBindValue(id);
    if (!query.exec()) {
        if (errorMessage)
            *errorMessage = QString::fromUtf8("无法删除镜头产品：%1").arg(sqlErrorText(query));
        return false;
    }
    return refreshSnapshotsIfLoaded(errorMessage);
}

bool CatalogRepository::removeLightById(qint64 id, QString *errorMessage)
{
    if (!openDatabase(errorMessage))
        return false;
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("DELETE FROM light_products WHERE id=?"));
    query.addBindValue(id);
    if (!query.exec()) {
        if (errorMessage)
            *errorMessage = QString::fromUtf8("无法删除光源产品：%1").arg(sqlErrorText(query));
        return false;
    }
    m_lightCandidateCache.clear();
    return refreshSnapshotsIfLoaded(errorMessage);
}

int CatalogRepository::cameraIndexById(qint64 id) const
{
    return m_cameraIds.indexOf(id);
}

int CatalogRepository::lensIndexById(qint64 id) const
{
    return m_lensIds.indexOf(id);
}

int CatalogRepository::lightIndexById(qint64 id) const
{
    return m_lightIds.indexOf(id);
}

QVector<CameraSpec> CatalogRepository::selectionCandidateCameras(const SelectionRequest &request, int limit, QString *errorMessage) const
{
    const int effectiveLimit = limit > 0 ? limit : 300;
    if (!openDatabase(errorMessage))
        return QVector<CameraSpec>();

    bool queryFailed = false;
    const auto runCameraQuery = [this, &queryFailed, errorMessage](const QStringList &clauses, const QList<QVariant> &values,
                                                                   const QString &orderBy, int rowLimit) {
        QVector<CameraSpec> cameras;
        QString sql = QStringLiteral(
            "SELECT id, model, manufacturer, resolution_x, resolution_y, pixel_size_um, sensor_format, color_mode,"
            " shutter_type, max_fps, interface, bandwidth_mbps, bit_depth, dynamic_range_db, lens_mount"
            " FROM camera_products");
        if (!clauses.isEmpty())
            sql += QStringLiteral(" WHERE ") + clauses.join(QStringLiteral(" AND "));
        sql += QStringLiteral(" ORDER BY ") + orderBy + QStringLiteral(" LIMIT ?");

        QSqlQuery query(m_db);
        query.prepare(sql);
        bindAll(&query, values);
        query.addBindValue(rowLimit);
        if (!query.exec()) {
            queryFailed = true;
            if (errorMessage)
                *errorMessage = QStringLiteral("Unable to query camera candidates: %1").arg(sqlErrorText(query));
            return cameras;
        }
        while (query.next())
            cameras.append(cameraFromQuery(query));
        return cameras;
    };

    const double targetPixelUm = targetObjectPixelUmForRequest(request);
    const int requiredResolutionX = ceilPositiveToInt(requiredFovWidthForRequest(request) * 1000.0 / targetPixelUm);
    const int requiredResolutionY = ceilPositiveToInt(requiredFovHeightForRequest(request) * 1000.0 / targetPixelUm);

    QStringList resolutionClauses;
    QList<QVariant> resolutionValues;
    if (requiredResolutionX > 0) {
        resolutionClauses << QStringLiteral("resolution_x >= ?");
        resolutionValues << requiredResolutionX;
    }
    if (requiredResolutionY > 0) {
        resolutionClauses << QStringLiteral("resolution_y >= ?");
        resolutionValues << requiredResolutionY;
    }

    QStringList strictClauses = resolutionClauses;
    QList<QVariant> strictValues = resolutionValues;
    if (request.requiredFps > 0.0) {
        strictClauses << QStringLiteral("max_fps >= ?");
        strictValues << request.requiredFps;
    }

    QVector<CameraSpec> cameras;
    appendUniqueProducts(&cameras,
                         runCameraQuery(strictClauses, strictValues,
                                        QStringLiteral("(resolution_x * resolution_y) ASC, max_fps DESC, manufacturer ASC, model ASC, id ASC"),
                                        effectiveLimit),
                         effectiveLimit);
    if (queryFailed)
        return cameras;

    if (cameras.size() < effectiveLimit && request.requiredFps > 0.0) {
        appendUniqueProducts(&cameras,
                             runCameraQuery(resolutionClauses, resolutionValues,
                                            QStringLiteral("(resolution_x * resolution_y) ASC, max_fps DESC, manufacturer ASC, model ASC, id ASC"),
                                            effectiveLimit),
                             effectiveLimit);
        if (queryFailed)
            return cameras;
    }

    if (cameras.size() < effectiveLimit) {
        appendUniqueProducts(&cameras,
                             runCameraQuery(QStringList(), QList<QVariant>(),
                                            QStringLiteral("max_fps DESC, (resolution_x * resolution_y) DESC, manufacturer ASC, model ASC, id ASC"),
                                            effectiveLimit),
                             effectiveLimit);
    }
    return cameras;
}

QVector<LensSpec> CatalogRepository::selectionCandidateLenses(const SelectionRequest &request, int limit, QString *errorMessage) const
{
    const int effectiveLimit = limit > 0 ? limit : 500;
    if (!openDatabase(errorMessage))
        return QVector<LensSpec>();

    bool queryFailed = false;
    const auto runLensQuery = [this, &queryFailed, errorMessage](const QStringList &clauses, const QList<QVariant> &values,
                                                                const QString &orderBy, const QList<QVariant> &orderValues,
                                                                int rowLimit) {
        QVector<LensSpec> lenses;
        QString sql = QStringLiteral(
            "SELECT id, model, manufacturer, lens_type, lens_mount, focal_length_mm, min_wd_mm, distortion_percent,"
            " image_circle_mm, megapixel_rating, recommended_min_pixel_um, pmag, nominal_wd_mm, wd_tolerance_mm,"
            " max_sensor_diagonal_mm, telecentricity_deg, dof_mm, numerical_aperture, f_number, coaxial_illumination, notes"
            " FROM lens_products");
        if (!clauses.isEmpty())
            sql += QStringLiteral(" WHERE ") + clauses.join(QStringLiteral(" AND "));
        sql += QStringLiteral(" ORDER BY ") + orderBy + QStringLiteral(" LIMIT ?");

        QSqlQuery query(m_db);
        query.prepare(sql);
        bindAll(&query, values);
        bindAll(&query, orderValues);
        query.addBindValue(rowLimit);
        if (!query.exec()) {
            queryFailed = true;
            if (errorMessage)
                *errorMessage = QStringLiteral("Unable to query lens candidates: %1").arg(sqlErrorText(query));
            return lenses;
        }
        while (query.next())
            lenses.append(lensFromQuery(query));
        return lenses;
    };

    QStringList strictClauses;
    QList<QVariant> strictValues;
    if (!request.allowTelecentric)
        strictClauses << QStringLiteral("lens_type NOT IN ('ObjectTelecentric','BiTelecentric')");
    if (request.workingDistanceMm > 0.0) {
        strictClauses << QStringLiteral(
            "((lens_type IN ('ObjectTelecentric','BiTelecentric')"
            " AND (nominal_wd_mm <= 0 OR wd_tolerance_mm <= 0 OR ABS(nominal_wd_mm - ?) <= wd_tolerance_mm))"
            " OR (lens_type NOT IN ('ObjectTelecentric','BiTelecentric') AND (min_wd_mm <= 0 OR min_wd_mm <= ?)))");
        strictValues << request.workingDistanceMm << request.workingDistanceMm;
    }

    const QString largeImageCircleOrder = QStringLiteral(
        "image_circle_mm DESC, megapixel_rating DESC, manufacturer ASC, model ASC, id ASC");
    const QString smallImageCircleOrder = QStringLiteral(
        "image_circle_mm ASC, megapixel_rating DESC, manufacturer ASC, model ASC, id ASC");

    QVector<LensSpec> lenses;
    const int largeQuota = qMax(1, effectiveLimit / 4);
    appendUniqueProducts(&lenses, runLensQuery(strictClauses, strictValues, largeImageCircleOrder, QList<QVariant>(), largeQuota), effectiveLimit);
    if (queryFailed)
        return lenses;

    const QVector<double> focalTargets = fixedFocalTargetSamples(request);
    const QString focalOrder = focalProximityOrder(focalTargets);
    if (!focalOrder.isEmpty()) {
        QStringList focalClauses = strictClauses;
        QList<QVariant> focalValues = strictValues;
        QList<QVariant> orderValues;
        focalClauses << QStringLiteral("lens_type = 'FixedFocal'")
                     << QStringLiteral("focal_length_mm > 0");
        for (double target : focalTargets)
            orderValues << target;
        appendUniqueProducts(&lenses, runLensQuery(focalClauses, focalValues, focalOrder, orderValues,
                                                   qMax(1, effectiveLimit / 2)),
                             effectiveLimit);
        if (queryFailed)
            return lenses;
    }

    appendUniqueProducts(&lenses, runLensQuery(strictClauses, strictValues, smallImageCircleOrder, QList<QVariant>(), effectiveLimit), effectiveLimit);
    if (queryFailed)
        return lenses;

    if (lenses.size() < effectiveLimit) {
        QStringList relaxedClauses;
        if (!request.allowTelecentric)
            relaxedClauses << QStringLiteral("lens_type NOT IN ('ObjectTelecentric','BiTelecentric')");
        appendUniqueProducts(&lenses, runLensQuery(relaxedClauses, QList<QVariant>(), largeImageCircleOrder, QList<QVariant>(), effectiveLimit), effectiveLimit);
    }
    if (queryFailed)
        return lenses;

    if (lenses.size() < effectiveLimit)
        appendUniqueProducts(&lenses, runLensQuery(QStringList(), QList<QVariant>(), largeImageCircleOrder, QList<QVariant>(), effectiveLimit), effectiveLimit);
    return lenses;
}

QVector<LightSpec> CatalogRepository::selectionCandidateLights(const SelectionRequest &request, int limit, QString *errorMessage) const
{
    return selectionCandidateLights(request, false, false, limit, errorMessage);
}

QVector<LightSpec> CatalogRepository::selectionCandidateLights(const SelectionRequest &request, bool hasTelecentricLens, bool hasCoaxialLens,
                                                               int limit, QString *errorMessage) const
{
    const int effectiveLimit = limit > 0 ? limit : 300;
    const QString cacheKey = lightCandidateCacheKey(request, hasTelecentricLens, hasCoaxialLens, effectiveLimit);
    if (m_lightCandidateCache.contains(cacheKey))
        return m_lightCandidateCache.value(cacheKey);

    const double fovW = request.objectWidthMm + request.placementMarginMm * 2.0;
    const double fovH = request.objectHeightMm + request.placementMarginMm * 2.0;
    if (!openDatabase(errorMessage))
        return QVector<LightSpec>();

    QString prioritySql = QStringLiteral("CASE");
    if (hasCoaxialLens)
        prioritySql += QStringLiteral(" WHEN light_type='Coaxial' THEN 0");
    if (hasTelecentricLens)
        prioritySql += QStringLiteral(" WHEN light_type='TelecentricBacklight' THEN 1");
    if (request.detectionType == DetectionType::DefectInspection)
        prioritySql += QStringLiteral(" WHEN light_type='DarkField' THEN 2");
    if (request.reflective
        || request.surfaceType == SurfaceType::ReflectiveMetal
        || request.surfaceType == SurfaceType::GlassTransparent) {
        prioritySql += QStringLiteral(" WHEN light_type IN ('Coaxial','Dome') THEN 3");
    }
    if (request.motionSpeedMmS > 0.0)
        prioritySql += QStringLiteral(" WHEN LOWER(mode) LIKE '%strobe%' THEN 4");
    prioritySql += QStringLiteral(" ELSE 10 END");

    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "SELECT id, model, manufacturer, light_type, color, wavelength_nm, mode, active_width_mm, active_height_mm, best_for"
        " FROM light_products WHERE active_width_mm >= ? AND active_height_mm >= ?"
        " ORDER BY %1 ASC, (active_width_mm * active_height_mm) ASC LIMIT ?").arg(prioritySql));
    query.addBindValue(fovW);
    query.addBindValue(fovH);
    query.addBindValue(effectiveLimit);
    QVector<LightSpec> lights;
    if (!query.exec()) {
        if (errorMessage)
            *errorMessage = QString::fromUtf8("无法查询光源候选：%1").arg(sqlErrorText(query));
        return lights;
    }
    while (query.next())
        lights.append(lightFromQuery(query));
    if (!lights.isEmpty()) {
        m_lightCandidateCache.insert(cacheKey, lights);
        return lights;
    }

    CatalogQuery fallback;
    fallback.limit = effectiveLimit;
    fallback.sort.field = QStringLiteral("active_width_mm");
    fallback.sort.ascending = false;
    lights = queryLights(fallback, errorMessage).items;
    m_lightCandidateCache.insert(cacheKey, lights);
    return lights;
}

bool CatalogRepository::writeCameraCsv(const QString &filePath, const QVector<CameraSpec> &cameras, QString *errorMessage) const
{
    QFileInfo info(filePath);
    if (!QDir().mkpath(info.absolutePath())) {
        if (errorMessage)
            *errorMessage = QString::fromUtf8("\346\227\240\346\263\225\345\210\233\345\273\272\347\233\256\345\275\225\357\274\232%1").arg(info.absolutePath());
        return false;
    }
    if (info.exists()) {
        QFile::setPermissions(filePath,
                              QFileDevice::ReadOwner | QFileDevice::WriteOwner
                                  | QFileDevice::ReadUser | QFileDevice::WriteUser
                                  | QFileDevice::ReadGroup | QFileDevice::ReadOther);
    }
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (errorMessage)
            *errorMessage = QString::fromUtf8("\346\227\240\346\263\225\345\206\231\345\205\245 CSV\357\274\232%1").arg(filePath);
        return false;
    }
    QTextStream out(&file);
    out.setCodec("UTF-8");
    const QStringList headers = {
        QStringLiteral("model"), QStringLiteral("manufacturer"), QStringLiteral("resolution_x"), QStringLiteral("resolution_y"),
        QStringLiteral("pixel_size_um"), QStringLiteral("sensor_format"), QStringLiteral("color_mode"), QStringLiteral("shutter_type"),
        QStringLiteral("max_fps"), QStringLiteral("interface"), QStringLiteral("bandwidth_mbps"), QStringLiteral("bit_depth"),
        QStringLiteral("dynamic_range_db"), QStringLiteral("lens_mount")
    };
    out << headers.join(QLatin1Char(',')) << "\n";
    for (const CameraSpec &c : cameras) {
        QStringList row;
        row << c.model << c.manufacturer << QString::number(c.resolutionX) << QString::number(c.resolutionY)
            << QString::number(c.pixelSizeUm, 'g', 12) << c.sensorFormat << c.colorMode << c.shutterType
            << QString::number(c.maxFps, 'g', 12) << c.interfaceType << QString::number(c.bandwidthMBps, 'g', 12)
            << QString::number(c.bitDepth, 'g', 12) << QString::number(c.dynamicRangeDb, 'g', 12) << c.lensMount;
        for (QString &value : row)
            value = csvField(value);
        out << row.join(QLatin1Char(',')) << "\n";
    }
    return true;
}

bool CatalogRepository::writeLensCsv(const QString &filePath, const QVector<LensSpec> &lenses, QString *errorMessage) const
{
    QFileInfo info(filePath);
    if (!QDir().mkpath(info.absolutePath())) {
        if (errorMessage)
            *errorMessage = QString::fromUtf8("\346\227\240\346\263\225\345\210\233\345\273\272\347\233\256\345\275\225\357\274\232%1").arg(info.absolutePath());
        return false;
    }
    if (info.exists()) {
        QFile::setPermissions(filePath,
                              QFileDevice::ReadOwner | QFileDevice::WriteOwner
                                  | QFileDevice::ReadUser | QFileDevice::WriteUser
                                  | QFileDevice::ReadGroup | QFileDevice::ReadOther);
    }
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (errorMessage)
            *errorMessage = QString::fromUtf8("\346\227\240\346\263\225\345\206\231\345\205\245 CSV\357\274\232%1").arg(filePath);
        return false;
    }
    QTextStream out(&file);
    out.setCodec("UTF-8");
    const QStringList headers = {
        QStringLiteral("model"), QStringLiteral("manufacturer"), QStringLiteral("lens_type"), QStringLiteral("lens_mount"),
        QStringLiteral("focal_length_mm"), QStringLiteral("min_wd_mm"), QStringLiteral("distortion_percent"),
        QStringLiteral("image_circle_mm"), QStringLiteral("megapixel_rating"), QStringLiteral("recommended_min_pixel_um"),
        QStringLiteral("pmag"), QStringLiteral("nominal_wd_mm"), QStringLiteral("wd_tolerance_mm"),
        QStringLiteral("max_sensor_diagonal_mm"), QStringLiteral("telecentricity_deg"), QStringLiteral("dof_mm"),
        QStringLiteral("numerical_aperture"), QStringLiteral("f_number"), QStringLiteral("coaxial_illumination"), QStringLiteral("notes")
    };
    out << headers.join(QLatin1Char(',')) << "\n";
    for (const LensSpec &l : lenses) {
        QStringList row;
        row << l.model << l.manufacturer
            << (l.lensType == LensType::FixedFocal ? QStringLiteral("FixedFocal")
                : l.lensType == LensType::ObjectTelecentric ? QStringLiteral("ObjectTelecentric") : QStringLiteral("BiTelecentric"))
            << l.lensMount << QString::number(l.focalLengthMm, 'g', 12)
            << QString::number(l.minWorkingDistanceMm, 'g', 12) << QString::number(l.distortionPercent, 'g', 12)
            << QString::number(l.imageCircleMm, 'g', 12) << QString::number(l.megapixelRating, 'g', 12)
            << QString::number(l.recommendedMinPixelUm, 'g', 12) << QString::number(l.pmag, 'g', 12)
            << QString::number(l.nominalWorkingDistanceMm, 'g', 12) << QString::number(l.workingDistanceToleranceMm, 'g', 12)
            << QString::number(l.maxSensorDiagonalMm, 'g', 12) << QString::number(l.telecentricityDeg, 'g', 12)
            << QString::number(l.dofMm, 'g', 12) << QString::number(l.numericalAperture, 'g', 12)
            << QString::number(l.fNumber, 'g', 12) << (l.coaxialIllumination ? QStringLiteral("true") : QStringLiteral("false"))
            << l.notes;
        for (QString &value : row)
            value = csvField(value);
        out << row.join(QLatin1Char(',')) << "\n";
    }
    return true;
}

bool CatalogRepository::writeLightCsv(const QString &filePath, const QVector<LightSpec> &lights, QString *errorMessage) const
{
    QFileInfo info(filePath);
    if (!QDir().mkpath(info.absolutePath())) {
        if (errorMessage)
            *errorMessage = QString::fromUtf8("\346\227\240\346\263\225\345\210\233\345\273\272\347\233\256\345\275\225\357\274\232%1").arg(info.absolutePath());
        return false;
    }
    if (info.exists()) {
        QFile::setPermissions(filePath,
                              QFileDevice::ReadOwner | QFileDevice::WriteOwner
                                  | QFileDevice::ReadUser | QFileDevice::WriteUser
                                  | QFileDevice::ReadGroup | QFileDevice::ReadOther);
    }
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (errorMessage)
            *errorMessage = QString::fromUtf8("\346\227\240\346\263\225\345\206\231\345\205\245 CSV\357\274\232%1").arg(filePath);
        return false;
    }
    QTextStream out(&file);
    out.setCodec("UTF-8");
    const QStringList headers = {
        QStringLiteral("model"), QStringLiteral("manufacturer"), QStringLiteral("light_type"), QStringLiteral("color"),
        QStringLiteral("wavelength_nm"), QStringLiteral("mode"), QStringLiteral("active_width_mm"),
        QStringLiteral("active_height_mm"), QStringLiteral("best_for")
    };
    out << headers.join(QLatin1Char(',')) << "\n";
    for (const LightSpec &l : lights) {
        QStringList row;
        row << l.model << l.manufacturer
            << (l.lightType == LightType::Backlight ? QStringLiteral("Backlight")
                : l.lightType == LightType::Ring ? QStringLiteral("Ring")
                : l.lightType == LightType::Bar ? QStringLiteral("Bar")
                : l.lightType == LightType::Coaxial ? QStringLiteral("Coaxial")
                : l.lightType == LightType::Dome ? QStringLiteral("Dome")
                : l.lightType == LightType::TelecentricBacklight ? QStringLiteral("TelecentricBacklight") : QStringLiteral("DarkField"))
            << l.color << QString::number(l.wavelengthNm) << l.mode
            << QString::number(l.activeWidthMm, 'g', 12) << QString::number(l.activeHeightMm, 'g', 12)
            << l.bestFor;
        for (QString &value : row)
            value = csvField(value);
        out << row.join(QLatin1Char(',')) << "\n";
    }
    return true;
}

QString CatalogRepository::csvField(const QString &value)
{
    QString escaped = value;
    escaped.replace(QLatin1Char('"'), QStringLiteral("\"\""));
    return QLatin1Char('"') + escaped + QLatin1Char('"');
}

QString CatalogRepository::keyForText(const QString &value)
{
    return value.trimmed().toLower();
}

QString CatalogRepository::lensTypeKey(LensType type)
{
    switch (type) {
    case LensType::FixedFocal: return QStringLiteral("FixedFocal");
    case LensType::ObjectTelecentric: return QStringLiteral("ObjectTelecentric");
    case LensType::BiTelecentric: return QStringLiteral("BiTelecentric");
    }
    return QStringLiteral("FixedFocal");
}

QString CatalogRepository::lightTypeKey(LightType type)
{
    switch (type) {
    case LightType::Backlight: return QStringLiteral("Backlight");
    case LightType::Ring: return QStringLiteral("Ring");
    case LightType::Bar: return QStringLiteral("Bar");
    case LightType::Coaxial: return QStringLiteral("Coaxial");
    case LightType::Dome: return QStringLiteral("Dome");
    case LightType::TelecentricBacklight: return QStringLiteral("TelecentricBacklight");
    case LightType::DarkField: return QStringLiteral("DarkField");
    }
    return QStringLiteral("Ring");
}

bool CatalogRepository::validateCamera(const CameraSpec &camera, const QString &sourceName, QString *errorMessage)
{
    if (camera.model.trimmed().isEmpty() || camera.resolutionX <= 0 || camera.resolutionY <= 0 || camera.pixelSizeUm <= 0.0) {
        if (errorMessage)
            *errorMessage = QString::fromUtf8("%1 \347\233\270\346\234\272\346\225\260\346\215\256\346\227\240\346\225\210\357\274\232\345\236\213\345\217\267\343\200\201\345\210\206\350\276\250\347\216\207\343\200\201\345\203\217\345\205\203\345\260\272\345\257\270\345\277\205\351\241\273\346\234\211\346\225\210").arg(sourceName);
        return false;
    }
    return true;
}

bool CatalogRepository::validateLens(const LensSpec &lens, const QString &sourceName, QString *errorMessage)
{
    if (lens.model.trimmed().isEmpty() || lens.imageCircleMm <= 0.0) {
        if (errorMessage)
            *errorMessage = QString::fromUtf8("%1 \351\225\234\345\244\264\346\225\260\346\215\256\346\227\240\346\225\210\357\274\232\345\236\213\345\217\267\345\222\214\345\203\217\345\234\206\345\277\205\351\241\273\346\234\211\346\225\210").arg(sourceName);
        return false;
    }
    if (lens.isTelecentric() && lens.pmag <= 0.0) {
        if (errorMessage)
            *errorMessage = QString::fromUtf8("%1 \350\277\234\345\277\203\351\225\234\345\244\264\347\274\272\345\260\221 PMAG \346\224\276\345\244\247\345\200\215\347\216\207").arg(sourceName);
        return false;
    }
    if (!lens.isTelecentric() && lens.focalLengthMm <= 0.0) {
        if (errorMessage)
            *errorMessage = QString::fromUtf8("%1 \346\231\256\351\200\232\351\225\234\345\244\264\347\274\272\345\260\221\347\204\246\350\267\235").arg(sourceName);
        return false;
    }
    return true;
}

bool CatalogRepository::validateLight(const LightSpec &light, const QString &sourceName, QString *errorMessage)
{
    if (light.model.trimmed().isEmpty() || light.activeWidthMm <= 0.0 || light.activeHeightMm <= 0.0) {
        if (errorMessage)
            *errorMessage = QString::fromUtf8("%1 光源数据无效：型号和有效照明尺寸必须有效").arg(sourceName);
        return false;
    }
    return true;
}

QString CatalogRepository::normalizeCameraSensorFormat(const QString &value, int resolutionX, int resolutionY, double pixelSizeUm)
{
    QString normalized = value.trimmed();
    normalized.replace(QString::fromUtf8("Ã"), QStringLiteral("x"));
    normalized.replace(QString::fromUtf8("脳"), QStringLiteral("x"));
    normalized.replace(QString::fromUtf8("×"), QStringLiteral("x"));
    normalized.replace(QString::fromUtf8("â"), QStringLiteral("\""));
    normalized.replace(QChar(0x201C), QStringLiteral("\""));
    normalized.replace(QChar(0x201D), QStringLiteral("\""));
    normalized.replace(QChar(0x2033), QStringLiteral("\""));
    normalized.replace(QChar(0xFF02), QStringLiteral("\""));
    normalized.replace(QStringLiteral("''"), QStringLiteral("\""));
    normalized.remove(QRegularExpression(QStringLiteral("\\bInGaAs\\s*,\\s*"), QRegularExpression::CaseInsensitiveOption));
    normalized.remove(QRegularExpression(QStringLiteral("\\bCMOS\\b"), QRegularExpression::CaseInsensitiveOption));
    normalized = normalized.trimmed();

    if (normalized.isEmpty()
        || QRegularExpression(QStringLiteral("^IMX\\d+"), QRegularExpression::CaseInsensitiveOption).match(normalized).hasMatch()) {
        if (resolutionX > 0 && resolutionY > 0 && pixelSizeUm > 0.0) {
            return QStringLiteral("%1 mm x %2 mm")
                .arg(resolutionX * pixelSizeUm / 1000.0, 0, 'f', 2)
                .arg(resolutionY * pixelSizeUm / 1000.0, 0, 'f', 2);
        }
        return QString();
    }

    normalized.replace(QRegularExpression(QString::fromUtf8("\\s*[xX×脳]\\s*")), QStringLiteral(" x "));
    normalized.replace(QRegularExpression(QStringLiteral("(\\d)\\s*mm\\b"), QRegularExpression::CaseInsensitiveOption), QStringLiteral("\\1 mm"));
    normalized.replace(QRegularExpression(QStringLiteral("\\s+")), QStringLiteral(" "));
    normalized.replace(QRegularExpression(QStringLiteral("\\s+\"")), QStringLiteral("\""));
    normalized = normalized.trimmed();

    if (QRegularExpression(QStringLiteral("^(?:\\d+(?:\\.\\d+)?|\\d+/\\d+(?:\\.\\d+)?)$")).match(normalized).hasMatch())
        normalized.append(QLatin1Char('"'));
    return normalized;
}

QString CatalogRepository::normalizeCameraColorMode(const QString &value)
{
    const QString normalized = value.trimmed();
    const QString lower = normalized.toLower();
    if (normalized.isEmpty())
        return normalized;
    if (lower.contains(QStringLiteral("mono"))
        || normalized.contains(QString::fromUtf8("黑白"))
        || normalized.contains(QString::fromUtf8("单色"))) {
        return QStringLiteral("Mono");
    }
    if (lower.contains(QStringLiteral("color"))
        || lower.contains(QStringLiteral("colour"))
        || normalized.contains(QString::fromUtf8("彩色"))) {
        return QStringLiteral("Color");
    }
    return normalized;
}

QString CatalogRepository::normalizeCameraInterface(const QString &value)
{
    QString normalized = value.trimmed();
    normalized.replace(QString::fromUtf8("Ã"), QStringLiteral("x"));
    normalized.replace(QString::fromUtf8("脳"), QStringLiteral("x"));
    normalized.replace(QString::fromUtf8("×"), QStringLiteral("x"));
    normalized.replace(QString::fromUtf8("â"), QStringLiteral("\""));
    normalized.replace(QChar(0x201C), QStringLiteral("\""));
    normalized.replace(QChar(0x201D), QStringLiteral("\""));
    return normalized;
}

QString CatalogRepository::normalizeLensMount(const QString &value)
{
    QString normalized = value.trimmed();
    QString mojibakeTimes;
    mojibakeTimes.append(QChar(0x00C3));
    mojibakeTimes.append(QChar(0x0097));
    normalized.replace(mojibakeTimes, QStringLiteral("x"));
    normalized.replace(QChar(0x00D7), QStringLiteral("x"));
    normalized.replace(QChar(0x8133), QStringLiteral("x"));
    normalized.replace(QRegularExpression(QStringLiteral("\\s*x\\s*"), QRegularExpression::CaseInsensitiveOption), QStringLiteral(" x "));
    normalized.replace(QRegularExpression(QStringLiteral("\\s+")), QStringLiteral(" "));
    return normalized.trimmed();
}

bool CatalogRepository::readCsvRows(const QString &filePath, QVector<Row> *rows, QString *errorMessage)
{
    QFile file(filePath);
    return readCsvRowsFromDevice(&file, filePath, rows, errorMessage);
}

bool CatalogRepository::readCsvRowsFromDevice(QIODevice *device, const QString &sourceName, QVector<Row> *rows, QString *errorMessage)
{
    if (!device->open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (errorMessage)
            *errorMessage = QString::fromUtf8("\346\227\240\346\263\225\346\211\223\345\274\200 CSV\357\274\232%1").arg(sourceName);
        return false;
    }

    QTextStream in(device);
    in.setCodec("UTF-8");

    QStringList headers;
    int physicalLineNumber = 0;
    int recordStartLine = 0;
    QString record;
    while (!in.atEnd()) {
        QString line = in.readLine();
        ++physicalLineNumber;
        if (physicalLineNumber == 1 && line.startsWith(QChar(0xFEFF)))
            line.remove(0, 1);
        if (record.isEmpty() && line.trimmed().isEmpty())
            continue;

        if (record.isEmpty()) {
            recordStartLine = physicalLineNumber;
            record = line;
        } else {
            record += QLatin1Char('\n');
            record += line;
        }

        if (!csvRecordComplete(record))
            continue;

        const QStringList values = parseCsvLine(record);
        if (headers.isEmpty()) {
            headers = values;
            for (QString &header : headers)
                header = header.trimmed().toLower();
            record.clear();
            recordStartLine = 0;
            continue;
        }

        if (values.size() != headers.size()) {
            if (errorMessage) {
                *errorMessage = QString::fromUtf8("%1 \347\254\254 %2 \350\241\214\345\255\227\346\256\265\346\225\260\351\207\217\344\270\215\345\214\271\351\205\215\357\274\232\346\234\237\346\234\233 %3\357\274\214\345\256\236\351\231\205 %4")
                    .arg(sourceName)
                    .arg(recordStartLine)
                    .arg(headers.size())
                    .arg(values.size());
            }
            return false;
        }

        Row row;
        for (int i = 0; i < headers.size(); ++i)
            row.insert(headers.at(i), values.at(i).trimmed());
        rows->append(row);
        record.clear();
        recordStartLine = 0;
    }

    if (!record.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QString::fromUtf8("%1 第 %2 行 CSV 引号字段未闭合")
                .arg(sourceName)
                .arg(recordStartLine);
        }
        return false;
    }

    if (headers.isEmpty()) {
        if (errorMessage)
            *errorMessage = QString::fromUtf8("%1 \346\262\241\346\234\211\350\241\250\345\244\264").arg(sourceName);
        return false;
    }
    return true;
}

QStringList CatalogRepository::parseCsvLine(const QString &line)
{
    QStringList out;
    QString current;
    bool inQuotes = false;

    for (int i = 0; i < line.size(); ++i) {
        const QChar ch = line.at(i);
        if (ch == QLatin1Char('"')) {
            if (inQuotes && i + 1 < line.size() && line.at(i + 1) == QLatin1Char('"')) {
                current.append(ch);
                ++i;
            } else {
                inQuotes = !inQuotes;
            }
        } else if (ch == QLatin1Char(',') && !inQuotes) {
            out.append(current);
            current.clear();
        } else {
            current.append(ch);
        }
    }
    out.append(current);
    return out;
}

bool CatalogRepository::ensureColumns(const QVector<Row> &rows, const QStringList &required, const QString &sourceName, QString *errorMessage)
{
    if (rows.isEmpty()) {
        if (errorMessage)
            *errorMessage = QString::fromUtf8("%1 \346\262\241\346\234\211\346\225\260\346\215\256\350\241\214").arg(sourceName);
        return false;
    }

    for (const QString &column : required) {
        if (!rows.first().contains(column)) {
            if (errorMessage)
                *errorMessage = QString::fromUtf8("%1 \347\274\272\345\260\221\345\277\205\351\234\200\345\255\227\346\256\265\357\274\232%2").arg(sourceName, column);
            return false;
        }
    }
    return true;
}

double CatalogRepository::number(const Row &row, const QString &key, double defaultValue)
{
    bool ok = false;
    const double value = row.value(key).toDouble(&ok);
    return ok ? value : defaultValue;
}

int CatalogRepository::integer(const Row &row, const QString &key, int defaultValue)
{
    bool ok = false;
    const int value = row.value(key).toInt(&ok);
    return ok ? value : defaultValue;
}

QString CatalogRepository::text(const Row &row, const QString &key)
{
    return row.value(key).trimmed();
}

QString CatalogRepository::firstText(const Row &row, const QStringList &keys)
{
    for (const QString &key : keys) {
        const QString value = text(row, key);
        if (!value.isEmpty())
            return value;
    }
    return QString();
}

bool CatalogRepository::loadCameraRows(const QVector<Row> &rows, const QString &sourceName, QString *errorMessage)
{
    const QStringList required = {
        QStringLiteral("model"),
        QStringLiteral("resolution_x"),
        QStringLiteral("resolution_y"),
        QStringLiteral("pixel_size_um"),
        QStringLiteral("color_mode"),
        QStringLiteral("shutter_type"),
        QStringLiteral("max_fps"),
        QStringLiteral("interface"),
        QStringLiteral("bandwidth_mbps"),
        QStringLiteral("bit_depth"),
        QStringLiteral("lens_mount")
    };
    if (!ensureColumns(rows, required, sourceName, errorMessage))
        return false;

    QVector<CameraSpec> imported;
    for (const Row &row : rows) {
        CameraSpec spec;
        spec.model = text(row, QStringLiteral("model"));
        spec.manufacturer = firstText(row, {QStringLiteral("manufacturer"), QStringLiteral("vendor"), QStringLiteral("brand"), QStringLiteral("maker"),
                                            QString::fromUtf8("\345\216\202\345\256\266"), QString::fromUtf8("\345\216\202\345\225\206"),
                                            QString::fromUtf8("\345\210\266\351\200\240\345\225\206")});
        spec.resolutionX = integer(row, QStringLiteral("resolution_x"));
        spec.resolutionY = integer(row, QStringLiteral("resolution_y"));
        spec.pixelSizeUm = number(row, QStringLiteral("pixel_size_um"));
        spec.sensorFormat = normalizeCameraSensorFormat(text(row, QStringLiteral("sensor_format")), spec.resolutionX, spec.resolutionY, spec.pixelSizeUm);
        spec.colorMode = normalizeCameraColorMode(text(row, QStringLiteral("color_mode")));
        spec.shutterType = text(row, QStringLiteral("shutter_type"));
        spec.maxFps = number(row, QStringLiteral("max_fps"));
        spec.interfaceType = normalizeCameraInterface(text(row, QStringLiteral("interface")));
        spec.bandwidthMBps = number(row, QStringLiteral("bandwidth_mbps"));
        spec.bitDepth = number(row, QStringLiteral("bit_depth"), 8.0);
        spec.dynamicRangeDb = number(row, QStringLiteral("dynamic_range_db"));
        spec.lensMount = normalizeLensMount(text(row, QStringLiteral("lens_mount")));

        if (spec.model.isEmpty() || spec.resolutionX <= 0 || spec.resolutionY <= 0 || spec.pixelSizeUm <= 0.0) {
            if (errorMessage)
                *errorMessage = QString::fromUtf8("%1 \344\270\255\347\233\270\346\234\272\346\225\260\346\215\256\346\227\240\346\225\210\357\274\232\345\236\213\345\217\267\343\200\201\345\210\206\350\276\250\347\216\207\343\200\201\345\203\217\345\205\203\345\260\272\345\257\270\345\277\205\351\241\273\346\234\211\346\225\210").arg(sourceName);
            return false;
        }
        imported.append(spec);
    }

    m_cameras = imported;
    return true;
}

bool CatalogRepository::loadLensRows(const QVector<Row> &rows, const QString &sourceName, QString *errorMessage)
{
    const QStringList required = {
        QStringLiteral("model"),
        QStringLiteral("lens_type"),
        QStringLiteral("lens_mount"),
        QStringLiteral("distortion_percent"),
        QStringLiteral("image_circle_mm")
    };
    if (!ensureColumns(rows, required, sourceName, errorMessage))
        return false;

    QVector<LensSpec> imported;
    for (const Row &row : rows) {
        LensSpec spec;
        spec.model = text(row, QStringLiteral("model"));
        spec.manufacturer = firstText(row, {QStringLiteral("manufacturer"), QStringLiteral("vendor"), QStringLiteral("brand"), QStringLiteral("maker"),
                                            QString::fromUtf8("\345\216\202\345\256\266"), QString::fromUtf8("\345\216\202\345\225\206"),
                                            QString::fromUtf8("\345\210\266\351\200\240\345\225\206")});
        spec.lensType = lensTypeFromString(text(row, QStringLiteral("lens_type")));
        spec.lensMount = normalizeLensMount(text(row, QStringLiteral("lens_mount")));
        spec.focalLengthMm = number(row, QStringLiteral("focal_length_mm"));
        spec.minWorkingDistanceMm = number(row, QStringLiteral("min_wd_mm"));
        spec.distortionPercent = number(row, QStringLiteral("distortion_percent"));
        spec.imageCircleMm = number(row, QStringLiteral("image_circle_mm"));
        spec.megapixelRating = number(row, QStringLiteral("megapixel_rating"));
        spec.recommendedMinPixelUm = number(row, QStringLiteral("recommended_min_pixel_um"));
        spec.pmag = number(row, QStringLiteral("pmag"));
        spec.nominalWorkingDistanceMm = number(row, QStringLiteral("nominal_wd_mm"));
        spec.workingDistanceToleranceMm = number(row, QStringLiteral("wd_tolerance_mm"));
        spec.maxSensorDiagonalMm = number(row, QStringLiteral("max_sensor_diagonal_mm"));
        spec.telecentricityDeg = number(row, QStringLiteral("telecentricity_deg"));
        spec.dofMm = number(row, QStringLiteral("dof_mm"));
        spec.numericalAperture = number(row, QStringLiteral("numerical_aperture"));
        spec.fNumber = number(row, QStringLiteral("f_number"));
        spec.coaxialIllumination = parseBool(text(row, QStringLiteral("coaxial_illumination")));
        spec.notes = text(row, QStringLiteral("notes"));

        if (spec.model.isEmpty() || spec.imageCircleMm <= 0.0) {
            if (errorMessage)
                *errorMessage = QString::fromUtf8("%1 \344\270\255\351\225\234\345\244\264\346\225\260\346\215\256\346\227\240\346\225\210\357\274\232\345\236\213\345\217\267\345\222\214\345\203\217\345\234\206\345\277\205\351\241\273\346\234\211\346\225\210").arg(sourceName);
            return false;
        }
        if (spec.isTelecentric() && spec.pmag <= 0.0) {
            if (errorMessage)
                *errorMessage = QString::fromUtf8("%1 \344\270\255\350\277\234\345\277\203\351\225\234\345\244\264 %2 \347\274\272\345\260\221 PMAG \346\224\276\345\244\247\345\200\215\347\216\207").arg(sourceName, spec.model);
            return false;
        }
        if (!spec.isTelecentric() && spec.focalLengthMm <= 0.0) {
            if (errorMessage)
                *errorMessage = QString::fromUtf8("%1 \344\270\255\346\231\256\351\200\232\351\225\234\345\244\264 %2 \347\274\272\345\260\221\347\204\246\350\267\235").arg(sourceName, spec.model);
            return false;
        }
        imported.append(spec);
    }

    m_lenses = imported;
    return true;
}

bool CatalogRepository::loadLightRows(const QVector<Row> &rows, const QString &sourceName, QString *errorMessage)
{
    const QStringList required = {
        QStringLiteral("model"),
        QStringLiteral("light_type"),
        QStringLiteral("active_width_mm"),
        QStringLiteral("active_height_mm")
    };
    if (!ensureColumns(rows, required, sourceName, errorMessage))
        return false;

    QVector<LightSpec> imported;
    for (const Row &row : rows) {
        LightSpec spec;
        spec.model = text(row, QStringLiteral("model"));
        spec.manufacturer = firstText(row, {QStringLiteral("manufacturer"), QStringLiteral("vendor"), QStringLiteral("brand"), QStringLiteral("maker"),
                                            QString::fromUtf8("\345\216\202\345\256\266"), QString::fromUtf8("\345\216\202\345\225\206"),
                                            QString::fromUtf8("\345\210\266\351\200\240\345\225\206")});
        spec.lightType = lightTypeFromString(text(row, QStringLiteral("light_type")));
        spec.color = text(row, QStringLiteral("color"));
        spec.wavelengthNm = integer(row, QStringLiteral("wavelength_nm"));
        spec.mode = text(row, QStringLiteral("mode"));
        spec.activeWidthMm = number(row, QStringLiteral("active_width_mm"));
        spec.activeHeightMm = number(row, QStringLiteral("active_height_mm"));
        spec.bestFor = text(row, QStringLiteral("best_for"));
        if (spec.lightType == LightType::Ring && spec.isDarkFieldLike())
            spec.lightType = LightType::DarkField;

        if (!validateLight(spec, sourceName, errorMessage))
            return false;
        imported.append(spec);
    }

    m_lights = imported;
    return true;
}
