#include "three_d/ThreeDCameraRepository.h"

#include "i18n/LanguageManager.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>
#include <QSet>

namespace {
QString text(const char *zhUtf8, const char *enUtf8)
{
    return LanguageManager::instance().currentLanguage() == QLatin1String("en_US")
        ? QString::fromUtf8(enUtf8)
        : QString::fromUtf8(zhUtf8);
}

void setError(QString *error, const QString &message);

bool quarantineCorruptUserFile(const QString &path, QString *error)
{
    const QString quarantinePath = path + QStringLiteral(".corrupt-")
        + QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyyMMddHHmmsszzz"));
    if (QFile::rename(path, quarantinePath))
        return true;
    setError(error, text("自定义 3D 相机 JSON 已损坏，且无法隔离文件：%1",
                         "Custom 3D camera JSON is corrupt and could not be quarantined: %1").arg(path));
    return false;
}

double numberValue(const QJsonObject &object, const char *key)
{
    const QJsonValue value = object.value(QLatin1String(key));
    return value.isDouble() ? value.toDouble() : -1.0;
}

int intValue(const QJsonObject &object, const char *key)
{
    const QJsonValue value = object.value(QLatin1String(key));
    return value.isDouble() ? value.toInt() : -1;
}

int tristateValue(const QJsonObject &object, const char *key)
{
    const QJsonValue value = object.value(QLatin1String(key));
    if (value.isBool())
        return value.toBool() ? 1 : 0;
    return -1;
}

QString stringValue(const QJsonObject &object, const char *key)
{
    return object.value(QLatin1String(key)).toString().trimmed();
}

QStringList stringListValue(const QJsonObject &object, const char *key)
{
    QStringList result;
    const QJsonArray values = object.value(QLatin1String(key)).toArray();
    for (const QJsonValue &value : values) {
        const QString text = value.toString().trimmed();
        if (!text.isEmpty())
            result.append(text);
    }
    return result;
}

void setError(QString *error, const QString &message)
{
    if (error)
        *error = message;
}

QStringList uniqueSorted(QStringList values)
{
    values.removeDuplicates();
    values.sort(Qt::CaseInsensitive);
    return values;
}

QString cameraKey(const ThreeDCameraSpec &spec)
{
    return spec.manufacturer.trimmed().toLower() + QLatin1Char('\n')
        + spec.series.trimmed().toLower() + QLatin1Char('\n')
        + spec.model.trimmed().toLower();
}

int indexOfCameraKey(const QVector<ThreeDCameraSpec> &cameras, const QString &key)
{
    for (int i = 0; i < cameras.size(); ++i) {
        if (cameraKey(cameras.at(i)) == key)
            return i;
    }
    return -1;
}

bool validateCameraSpec(const ThreeDCameraSpec &spec, const QString &sourceName, bool requireSourceUrl, QString *error)
{
    if (spec.manufacturer.trimmed().isEmpty() || spec.series.trimmed().isEmpty()
        || spec.model.trimmed().isEmpty() || spec.technologyLabel.trimmed().isEmpty()
        || spec.status.trimmed().isEmpty() || (requireSourceUrl && spec.sourceUrl.trimmed().isEmpty())) {
        setError(error, text("%1 缺少品牌、系列、型号、技术路线、状态或来源。",
                             "%1 is missing brand, series, model, technology, status, or source.").arg(sourceName));
        return false;
    }
    return true;
}

ThreeDCameraSpec specFromJson(const QJsonObject &object, bool userDefined)
{
    ThreeDCameraSpec spec;
    spec.manufacturer = stringValue(object, "manufacturer");
    spec.series = stringValue(object, "series");
    spec.model = stringValue(object, "model");
    spec.technologyLabel = stringValue(object, "technology");
    spec.technology = threeDTechnologyFromLabel(spec.technologyLabel);
    spec.status = stringValue(object, "status");
    spec.sourceUrl = stringValue(object, "sourceUrl");
    spec.sourceDate = stringValue(object, "sourceDate");
    spec.referenceDistanceMm = numberValue(object, "referenceDistanceMm");
    spec.workingDistanceMinMm = numberValue(object, "workingDistanceMinMm");
    spec.workingDistanceMaxMm = numberValue(object, "workingDistanceMaxMm");
    spec.zMeasurementRangeMm = numberValue(object, "zMeasurementRangeMm");
    spec.xFovNearMm = numberValue(object, "xFovNearMm");
    spec.xFovReferenceMm = numberValue(object, "xFovReferenceMm");
    spec.xFovFarMm = numberValue(object, "xFovFarMm");
    spec.yFovNearMm = numberValue(object, "yFovNearMm");
    spec.yFovReferenceMm = numberValue(object, "yFovReferenceMm");
    spec.yFovFarMm = numberValue(object, "yFovFarMm");
    spec.zRepeatabilityUm = numberValue(object, "zRepeatabilityUm");
    spec.xRepeatabilityUm = numberValue(object, "xRepeatabilityUm");
    spec.zResolutionUm = numberValue(object, "zResolutionUm");
    spec.zLinearityPercentOfRange = numberValue(object, "zLinearityPercentOfRange");
    spec.measurementAccuracyUm = numberValue(object, "measurementAccuracyUm");
    spec.xyResolutionUm = numberValue(object, "xyResolutionUm");
    spec.profileDataIntervalUm = numberValue(object, "profileDataIntervalUm");
    spec.accuracyCondition = stringValue(object, "accuracyCondition");
    spec.profilePoints = intValue(object, "profilePoints");
    spec.scanRateMinHz = numberValue(object, "scanRateMinHz");
    spec.scanRateMaxHz = numberValue(object, "scanRateMaxHz");
    spec.acquisitionTimeMs = numberValue(object, "acquisitionTimeMs");
    spec.frameRateHz = numberValue(object, "frameRateHz");
    spec.encoderRateMaxHz = numberValue(object, "encoderRateMaxHz");
    spec.exposureTimeMinUs = numberValue(object, "exposureTimeMinUs");
    spec.exposureTimeMaxUs = numberValue(object, "exposureTimeMaxUs");
    spec.readoutTimeUs = numberValue(object, "readoutTimeUs");
    spec.requiresExternalMotion = tristateValue(object, "requiresExternalMotion");
    spec.supportsEncoder = tristateValue(object, "supportsEncoder");
    spec.supportsExternalTrigger = tristateValue(object, "supportsExternalTrigger");
    spec.lightSource = stringValue(object, "lightSource");
    spec.wavelengthNm = numberValue(object, "wavelengthNm");
    spec.laserClass = stringValue(object, "laserClass");
    spec.structure = stringValue(object, "structure");
    spec.ipRating = stringValue(object, "ipRating");
    spec.weightG = numberValue(object, "weightG");
    spec.dimensions = stringValue(object, "dimensions");
    spec.temperature = stringValue(object, "temperature");
    spec.power = stringValue(object, "power");
    spec.interfaces = stringListValue(object, "interfaces");
    spec.outputTypes = stringListValue(object, "outputTypes");
    spec.materialScenarios = stringListValue(object, "materialScenarios");
    spec.notes = stringListValue(object, "notes");
    spec.rawSpecs = object.value(QStringLiteral("rawSpecs")).toObject();
    spec.userDefined = userDefined;
    return spec;
}

void addNumber(QJsonObject *object, const QString &key, double value)
{
    if (threeDHasValue(value))
        object->insert(key, value);
}

void addInt(QJsonObject *object, const QString &key, int value)
{
    if (value >= 0)
        object->insert(key, value);
}

void addTristate(QJsonObject *object, const QString &key, int value)
{
    if (value >= 0)
        object->insert(key, value > 0);
}

void addStringList(QJsonObject *object, const QString &key, const QStringList &values)
{
    if (values.isEmpty())
        return;
    QJsonArray array;
    for (const QString &value : values) {
        if (!value.trimmed().isEmpty())
            array.append(value.trimmed());
    }
    if (!array.isEmpty())
        object->insert(key, array);
}

QJsonObject specToJson(const ThreeDCameraSpec &spec)
{
    QJsonObject object;
    object.insert(QStringLiteral("manufacturer"), spec.manufacturer);
    object.insert(QStringLiteral("series"), spec.series);
    object.insert(QStringLiteral("model"), spec.model);
    object.insert(QStringLiteral("technology"), spec.technologyLabel.isEmpty() ? threeDTechnologyLabel(spec.technology) : spec.technologyLabel);
    object.insert(QStringLiteral("status"), spec.status);
    if (!spec.sourceUrl.isEmpty())
        object.insert(QStringLiteral("sourceUrl"), spec.sourceUrl);
    if (!spec.sourceDate.isEmpty())
        object.insert(QStringLiteral("sourceDate"), spec.sourceDate);
    addNumber(&object, QStringLiteral("referenceDistanceMm"), spec.referenceDistanceMm);
    addNumber(&object, QStringLiteral("workingDistanceMinMm"), spec.workingDistanceMinMm);
    addNumber(&object, QStringLiteral("workingDistanceMaxMm"), spec.workingDistanceMaxMm);
    addNumber(&object, QStringLiteral("zMeasurementRangeMm"), spec.zMeasurementRangeMm);
    addNumber(&object, QStringLiteral("xFovNearMm"), spec.xFovNearMm);
    addNumber(&object, QStringLiteral("xFovReferenceMm"), spec.xFovReferenceMm);
    addNumber(&object, QStringLiteral("xFovFarMm"), spec.xFovFarMm);
    addNumber(&object, QStringLiteral("yFovNearMm"), spec.yFovNearMm);
    addNumber(&object, QStringLiteral("yFovReferenceMm"), spec.yFovReferenceMm);
    addNumber(&object, QStringLiteral("yFovFarMm"), spec.yFovFarMm);
    addNumber(&object, QStringLiteral("zRepeatabilityUm"), spec.zRepeatabilityUm);
    addNumber(&object, QStringLiteral("xRepeatabilityUm"), spec.xRepeatabilityUm);
    addNumber(&object, QStringLiteral("zResolutionUm"), spec.zResolutionUm);
    addNumber(&object, QStringLiteral("zLinearityPercentOfRange"), spec.zLinearityPercentOfRange);
    addNumber(&object, QStringLiteral("measurementAccuracyUm"), spec.measurementAccuracyUm);
    addNumber(&object, QStringLiteral("xyResolutionUm"), spec.xyResolutionUm);
    addNumber(&object, QStringLiteral("profileDataIntervalUm"), spec.profileDataIntervalUm);
    if (!spec.accuracyCondition.isEmpty())
        object.insert(QStringLiteral("accuracyCondition"), spec.accuracyCondition);
    addInt(&object, QStringLiteral("profilePoints"), spec.profilePoints);
    addNumber(&object, QStringLiteral("scanRateMinHz"), spec.scanRateMinHz);
    addNumber(&object, QStringLiteral("scanRateMaxHz"), spec.scanRateMaxHz);
    addNumber(&object, QStringLiteral("acquisitionTimeMs"), spec.acquisitionTimeMs);
    addNumber(&object, QStringLiteral("frameRateHz"), spec.frameRateHz);
    addNumber(&object, QStringLiteral("encoderRateMaxHz"), spec.encoderRateMaxHz);
    addNumber(&object, QStringLiteral("exposureTimeMinUs"), spec.exposureTimeMinUs);
    addNumber(&object, QStringLiteral("exposureTimeMaxUs"), spec.exposureTimeMaxUs);
    addNumber(&object, QStringLiteral("readoutTimeUs"), spec.readoutTimeUs);
    addTristate(&object, QStringLiteral("requiresExternalMotion"), spec.requiresExternalMotion);
    addTristate(&object, QStringLiteral("supportsEncoder"), spec.supportsEncoder);
    addTristate(&object, QStringLiteral("supportsExternalTrigger"), spec.supportsExternalTrigger);
    if (!spec.lightSource.isEmpty())
        object.insert(QStringLiteral("lightSource"), spec.lightSource);
    addNumber(&object, QStringLiteral("wavelengthNm"), spec.wavelengthNm);
    if (!spec.laserClass.isEmpty())
        object.insert(QStringLiteral("laserClass"), spec.laserClass);
    if (!spec.structure.isEmpty())
        object.insert(QStringLiteral("structure"), spec.structure);
    if (!spec.ipRating.isEmpty())
        object.insert(QStringLiteral("ipRating"), spec.ipRating);
    addNumber(&object, QStringLiteral("weightG"), spec.weightG);
    if (!spec.dimensions.isEmpty())
        object.insert(QStringLiteral("dimensions"), spec.dimensions);
    if (!spec.temperature.isEmpty())
        object.insert(QStringLiteral("temperature"), spec.temperature);
    if (!spec.power.isEmpty())
        object.insert(QStringLiteral("power"), spec.power);
    addStringList(&object, QStringLiteral("interfaces"), spec.interfaces);
    addStringList(&object, QStringLiteral("outputTypes"), spec.outputTypes);
    addStringList(&object, QStringLiteral("materialScenarios"), spec.materialScenarios);
    addStringList(&object, QStringLiteral("notes"), spec.notes);
    if (!spec.rawSpecs.isEmpty())
        object.insert(QStringLiteral("rawSpecs"), spec.rawSpecs);
    return object;
}
}

void ThreeDCameraRepository::setStorageDirectory(const QString &directory)
{
    m_storageDirectory = directory;
}

QString ThreeDCameraRepository::storageDirectory() const
{
    return effectiveStorageDirectory();
}

bool ThreeDCameraRepository::loadFromResource(const QString &resourcePath, QString *error)
{
    QFile file(resourcePath);
    if (!file.open(QIODevice::ReadOnly)) {
        setError(error, text("无法打开 3D 相机数据：%1", "Unable to open 3D camera data: %1").arg(resourcePath));
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        setError(error, text("3D 相机 JSON 格式无效：%1", "Invalid 3D camera JSON: %1").arg(parseError.errorString()));
        return false;
    }

    const QJsonArray array = document.object().value(QStringLiteral("cameras")).toArray();
    if (array.isEmpty()) {
        setError(error, text("3D 相机数据为空。", "3D camera data is empty."));
        return false;
    }

    QVector<ThreeDCameraSpec> loaded;
    loaded.reserve(array.size());
    for (int i = 0; i < array.size(); ++i) {
        ThreeDCameraSpec spec = specFromJson(array.at(i).toObject(), false);
        if (!validateCameraSpec(spec, text("第 %1 条 3D 相机数据", "3D camera record %1").arg(i + 1), true, error))
            return false;
        loaded.append(spec);
    }

    m_builtInCameras = loaded;
    if (!loadUserCameras(error))
        return false;
    rebuildMergedCameras();
    setError(error, QString());
    return true;
}

bool ThreeDCameraRepository::addCamera(const ThreeDCameraSpec &camera, QString *error)
{
    ThreeDCameraSpec normalized = camera;
    normalized.userDefined = true;
    normalized.technologyLabel = normalized.technologyLabel.isEmpty() ? threeDTechnologyLabel(normalized.technology) : normalized.technologyLabel;
    normalized.technology = threeDTechnologyFromLabel(normalized.technologyLabel);
    if (normalized.status.trimmed().isEmpty())
        normalized.status = text("用户录入", "User entered");
    if (!validateCameraSpec(normalized, text("新增 3D 相机", "New 3D camera"), false, error))
        return false;

    const QVector<ThreeDCameraSpec> previousUser = m_userCameras;
    const QString key = cameraKey(normalized);
    const int existing = indexOfCameraKey(m_userCameras, key);
    if (existing >= 0)
        m_userCameras[existing] = normalized;
    else
        m_userCameras.append(normalized);
    if (!saveUserCameras(error)) {
        m_userCameras = previousUser;
        rebuildMergedCameras();
        return false;
    }
    rebuildMergedCameras();
    return true;
}

bool ThreeDCameraRepository::updateCamera(int index, const ThreeDCameraSpec &camera, QString *error)
{
    if (index < 0 || index >= m_cameras.size()) {
        setError(error, text("3D 相机行号无效。", "Invalid 3D camera row."));
        return false;
    }

    ThreeDCameraSpec normalized = camera;
    normalized.userDefined = true;
    normalized.technologyLabel = normalized.technologyLabel.isEmpty() ? threeDTechnologyLabel(normalized.technology) : normalized.technologyLabel;
    normalized.technology = threeDTechnologyFromLabel(normalized.technologyLabel);
    if (normalized.status.trimmed().isEmpty())
        normalized.status = text("用户录入", "User entered");
    if (!validateCameraSpec(normalized, normalized.model, false, error))
        return false;

    const QVector<ThreeDCameraSpec> previousUser = m_userCameras;
    const QString oldKey = cameraKey(m_cameras.at(index));
    const QString newKey = cameraKey(normalized);
    const int oldUserIndex = indexOfCameraKey(m_userCameras, oldKey);
    if (oldUserIndex >= 0)
        m_userCameras.removeAt(oldUserIndex);
    const int newUserIndex = indexOfCameraKey(m_userCameras, newKey);
    if (newUserIndex >= 0)
        m_userCameras[newUserIndex] = normalized;
    else
        m_userCameras.append(normalized);

    if (!saveUserCameras(error)) {
        m_userCameras = previousUser;
        rebuildMergedCameras();
        return false;
    }
    rebuildMergedCameras();
    return true;
}

bool ThreeDCameraRepository::removeCamera(int index, QString *error)
{
    if (index < 0 || index >= m_cameras.size()) {
        setError(error, text("3D 相机行号无效。", "Invalid 3D camera row."));
        return false;
    }
    const ThreeDCameraSpec selected = m_cameras.at(index);
    if (!selected.userDefined) {
        setError(error, text("内置 3D 相机不能删除；可先复制为自定义型号后维护。",
                             "Built-in 3D cameras cannot be deleted; copy one as a custom model first."));
        return false;
    }

    const QVector<ThreeDCameraSpec> previousUser = m_userCameras;
    const int userIndex = indexOfCameraKey(m_userCameras, cameraKey(selected));
    if (userIndex < 0) {
        setError(error, text("未找到对应的自定义 3D 相机。", "The matching custom 3D camera was not found."));
        return false;
    }
    m_userCameras.removeAt(userIndex);
    if (!saveUserCameras(error)) {
        m_userCameras = previousUser;
        rebuildMergedCameras();
        return false;
    }
    rebuildMergedCameras();
    return true;
}

QStringList ThreeDCameraRepository::manufacturers() const
{
    QStringList values;
    for (const ThreeDCameraSpec &spec : m_cameras)
        values.append(spec.manufacturer);
    return uniqueSorted(values);
}

QStringList ThreeDCameraRepository::interfaces() const
{
    QStringList values;
    for (const ThreeDCameraSpec &spec : m_cameras)
        values.append(spec.interfaces);
    return uniqueSorted(values);
}

QStringList ThreeDCameraRepository::ipRatings() const
{
    QStringList values;
    for (const ThreeDCameraSpec &spec : m_cameras) {
        if (!spec.ipRating.isEmpty())
            values.append(spec.ipRating);
    }
    return uniqueSorted(values);
}

QStringList ThreeDCameraRepository::materialScenarios() const
{
    QStringList values;
    for (const ThreeDCameraSpec &spec : m_cameras)
        values.append(spec.materialScenarios);
    return uniqueSorted(values);
}

QString ThreeDCameraRepository::effectiveStorageDirectory() const
{
    if (!m_storageDirectory.trimmed().isEmpty())
        return m_storageDirectory;
    QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (path.isEmpty())
        path = QDir::homePath() + QStringLiteral("/VisionSelect");
    return path;
}

QString ThreeDCameraRepository::userCameraStoragePath() const
{
    return QDir(effectiveStorageDirectory()).filePath(QStringLiteral("three_d_cameras.json"));
}

bool ThreeDCameraRepository::loadUserCameras(QString *error)
{
    m_userCameras.clear();
    const QString path = userCameraStoragePath();
    if (!QFileInfo::exists(path))
        return true;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        setError(error, text("无法打开自定义 3D 相机数据：%1", "Unable to open custom 3D camera data: %1").arg(path));
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        file.close();
        return quarantineCorruptUserFile(path, error);
    }

    const QJsonArray array = document.object().value(QStringLiteral("cameras")).toArray();
    for (int i = 0; i < array.size(); ++i) {
        ThreeDCameraSpec spec = specFromJson(array.at(i).toObject(), true);
        if (!validateCameraSpec(spec, text("第 %1 条自定义 3D 相机", "Custom 3D camera record %1").arg(i + 1), false, error))
            return false;
        m_userCameras.append(spec);
    }
    return true;
}

bool ThreeDCameraRepository::saveUserCameras(QString *error) const
{
    const QString path = userCameraStoragePath();
    QFileInfo info(path);
    if (!QDir().mkpath(info.absolutePath())) {
        setError(error, text("无法创建自定义 3D 相机目录：%1", "Unable to create custom 3D camera directory: %1").arg(info.absolutePath()));
        return false;
    }
    if (info.exists()) {
        QFile::setPermissions(path,
                              QFileDevice::ReadOwner | QFileDevice::WriteOwner
                                  | QFileDevice::ReadUser | QFileDevice::WriteUser
                                  | QFileDevice::ReadGroup | QFileDevice::ReadOther);
    }

    QJsonArray array;
    for (const ThreeDCameraSpec &camera : m_userCameras)
        array.append(specToJson(camera));
    QJsonObject root;
    root.insert(QStringLiteral("schemaVersion"), 1);
    root.insert(QStringLiteral("cameras"), array);

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        setError(error, text("无法写入自定义 3D 相机数据：%1", "Unable to write custom 3D camera data: %1").arg(path));
        return false;
    }
    const QByteArray payload = QJsonDocument(root).toJson(QJsonDocument::Indented);
    if (file.write(payload) != payload.size()) {
        setError(error, text("无法写入自定义 3D 相机数据：%1", "Unable to write custom 3D camera data: %1").arg(file.errorString()));
        return false;
    }
    if (!file.commit()) {
        setError(error, text("无法保存自定义 3D 相机数据：%1", "Unable to save custom 3D camera data: %1").arg(file.errorString()));
        return false;
    }
    QFile::setPermissions(path,
                          QFileDevice::ReadOwner | QFileDevice::WriteOwner
                              | QFileDevice::ReadUser | QFileDevice::WriteUser
                              | QFileDevice::ReadGroup | QFileDevice::ReadOther);
    return true;
}

void ThreeDCameraRepository::rebuildMergedCameras()
{
    m_cameras = m_builtInCameras;
    for (ThreeDCameraSpec userCamera : m_userCameras) {
        userCamera.userDefined = true;
        const int existing = indexOfCameraKey(m_cameras, cameraKey(userCamera));
        if (existing >= 0)
            m_cameras[existing] = userCamera;
        else
            m_cameras.append(userCamera);
    }
}
