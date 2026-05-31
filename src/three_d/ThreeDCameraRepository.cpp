#include "three_d/ThreeDCameraRepository.h"

#include "i18n/LanguageManager.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>

namespace {
QString text(const char *zhUtf8, const char *enUtf8)
{
    return LanguageManager::instance().currentLanguage() == QLatin1String("en_US")
        ? QString::fromUtf8(enUtf8)
        : QString::fromUtf8(zhUtf8);
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
        const QJsonObject object = array.at(i).toObject();
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
        spec.requiresExternalMotion = tristateValue(object, "requiresExternalMotion");
        spec.supportsEncoder = tristateValue(object, "supportsEncoder");
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

        if (spec.manufacturer.isEmpty() || spec.series.isEmpty() || spec.model.isEmpty()
            || spec.technologyLabel.isEmpty() || spec.status.isEmpty() || spec.sourceUrl.isEmpty()) {
            setError(error, text("第 %1 条 3D 相机数据缺少品牌、系列、型号、技术路线、状态或来源。",
                                 "3D camera record %1 is missing brand, series, model, technology, status, or source.").arg(i + 1));
            return false;
        }
        loaded.append(spec);
    }

    m_cameras = loaded;
    setError(error, QString());
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
