#include "three_d/ThreeDCameraMatcher.h"

#include <QCollator>
#include <QtMath>

#include <algorithm>

namespace {
int brandOrder(const QString &brand)
{
    if (brand == QStringLiteral("LMI"))
        return 0;
    if (brand == QString::fromUtf8("深视智能"))
        return 1;
    if (brand == QString::fromUtf8("基恩士"))
        return 2;
    if (brand == QString::fromUtf8("海康机器人"))
        return 3;
    return 10;
}

bool containsText(const QStringList &values, const QString &needle)
{
    for (const QString &value : values) {
        if (value.contains(needle, Qt::CaseInsensitive) || needle.contains(value, Qt::CaseInsensitive))
            return true;
    }
    return false;
}

double maxKnown(double a, double b, double c)
{
    double result = -1.0;
    if (threeDHasValue(a))
        result = qMax(result, a);
    if (threeDHasValue(b))
        result = qMax(result, b);
    if (threeDHasValue(c))
        result = qMax(result, c);
    return result;
}

double maxKnown(double a, double b)
{
    double result = -1.0;
    if (threeDHasValue(a))
        result = qMax(result, a);
    if (threeDHasValue(b))
        result = qMax(result, b);
    return result;
}

void addMissing(QStringList *missing, const QString &field)
{
    if (!missing->contains(field))
        missing->append(field);
}

void checkMinimum(double actual, double required, const QString &field, const QString &unit,
    QStringList *missing, QStringList *rejections)
{
    if (!threeDHasValue(required))
        return;
    if (!threeDHasValue(actual)) {
        addMissing(missing, field);
        return;
    }
    if (actual + 1e-9 < required) {
        rejections->append(QString::fromUtf8("%1不足：需要 ≥ %2%3，当前 %4%3")
            .arg(field)
            .arg(required, 0, 'f', 2)
            .arg(unit)
            .arg(actual, 0, 'f', 2));
    }
}

void checkMaximum(double actual, double required, const QString &field, const QString &unit,
    QStringList *missing, QStringList *rejections)
{
    if (!threeDHasValue(required))
        return;
    if (!threeDHasValue(actual)) {
        addMissing(missing, field);
        return;
    }
    if (actual - 1e-9 > required) {
        rejections->append(QString::fromUtf8("%1超限：需要 ≤ %2%3，当前 %4%3")
            .arg(field)
            .arg(required, 0, 'f', 2)
            .arg(unit)
            .arg(actual, 0, 'f', 2));
    }
}
}

QVector<ThreeDCameraMatch> ThreeDCameraMatcher::match(const ThreeDCameraRequirement &requirement, const QVector<ThreeDCameraSpec> &cameras) const
{
    QVector<ThreeDCameraMatch> matches;
    matches.reserve(cameras.size());
    for (const ThreeDCameraSpec &camera : cameras)
        matches.append(matchOne(requirement, camera));

    QCollator collator;
    collator.setNumericMode(true);
    std::sort(matches.begin(), matches.end(), [&collator](const ThreeDCameraMatch &left, const ThreeDCameraMatch &right) {
        const int statusLeft = threeDMatchStatusOrder(left.status);
        const int statusRight = threeDMatchStatusOrder(right.status);
        if (statusLeft != statusRight)
            return statusLeft < statusRight;

        const int brandLeft = brandOrder(left.spec.manufacturer);
        const int brandRight = brandOrder(right.spec.manufacturer);
        if (brandLeft != brandRight)
            return brandLeft < brandRight;

        const int technologyCompare = collator.compare(left.spec.technologyLabel, right.spec.technologyLabel);
        if (technologyCompare != 0)
            return technologyCompare < 0;
        const int seriesCompare = collator.compare(left.spec.series, right.spec.series);
        if (seriesCompare != 0)
            return seriesCompare < 0;
        return collator.compare(left.spec.model, right.spec.model) < 0;
    });
    return matches;
}

ThreeDCameraMatch ThreeDCameraMatcher::matchOne(const ThreeDCameraRequirement &requirement, const ThreeDCameraSpec &camera) const
{
    ThreeDCameraMatch result;
    result.spec = camera;

    if (!requirement.manufacturer.isEmpty() && requirement.manufacturer != camera.manufacturer)
        result.rejectionReasons.append(QString::fromUtf8("品牌不匹配：需要 %1").arg(requirement.manufacturer));
    if (!requirement.technologyLabel.isEmpty() && requirement.technologyLabel != camera.technologyLabel)
        result.rejectionReasons.append(QString::fromUtf8("技术路线不匹配：需要 %1").arg(requirement.technologyLabel));

    checkMinimum(maxKnown(camera.xFovNearMm, camera.xFovReferenceMm, camera.xFovFarMm),
        requirement.targetXCoverageMm, QString::fromUtf8("X 覆盖"), QStringLiteral(" mm"),
        &result.missingFields, &result.rejectionReasons);
    checkMinimum(maxKnown(camera.yFovNearMm, camera.yFovReferenceMm, camera.yFovFarMm),
        requirement.targetYCoverageMm, QString::fromUtf8("Y 覆盖"), QStringLiteral(" mm"),
        &result.missingFields, &result.rejectionReasons);
    checkMinimum(camera.zMeasurementRangeMm, requirement.zMeasurementRangeMm,
        QString::fromUtf8("Z 量程"), QStringLiteral(" mm"), &result.missingFields, &result.rejectionReasons);
    checkMaximum(camera.zRepeatabilityUm, requirement.maxZRepeatabilityUm,
        QString::fromUtf8("Z 重复精度"), QStringLiteral(" um"), &result.missingFields, &result.rejectionReasons);
    checkMinimum(maxKnown(camera.scanRateMaxHz, camera.frameRateHz), requirement.minSpeedHz,
        QString::fromUtf8("速度"), QStringLiteral(" Hz"), &result.missingFields, &result.rejectionReasons);

    if (threeDHasValue(requirement.workingDistanceMm)) {
        if (threeDHasValue(camera.workingDistanceMinMm) && threeDHasValue(camera.workingDistanceMaxMm)) {
            if (requirement.workingDistanceMm < camera.workingDistanceMinMm || requirement.workingDistanceMm > camera.workingDistanceMaxMm) {
                result.rejectionReasons.append(QString::fromUtf8("工作距离不在公开范围内：需要 %1 mm，范围 %2-%3 mm")
                    .arg(requirement.workingDistanceMm, 0, 'f', 1)
                    .arg(camera.workingDistanceMinMm, 0, 'f', 1)
                    .arg(camera.workingDistanceMaxMm, 0, 'f', 1));
            }
        } else if (threeDHasValue(camera.referenceDistanceMm)) {
            const double tolerance = qMax(5.0, camera.referenceDistanceMm * 0.15);
            if (qAbs(requirement.workingDistanceMm - camera.referenceDistanceMm) > tolerance) {
                result.rejectionReasons.append(QString::fromUtf8("工作/参考距离偏离较大：需要 %1 mm，官方参考距离 %2 mm")
                    .arg(requirement.workingDistanceMm, 0, 'f', 1)
                    .arg(camera.referenceDistanceMm, 0, 'f', 1));
            }
        } else {
            addMissing(&result.missingFields, QString::fromUtf8("工作/参考距离"));
        }
    }

    if (requirement.requireNoExternalMotion) {
        if (camera.requiresExternalMotion < 0) {
            addMissing(&result.missingFields, QString::fromUtf8("是否需要外部运动"));
        } else if (camera.requiresExternalMotion > 0) {
            result.rejectionReasons.append(QString::fromUtf8("需要外部运动平台或产线运动，不能满足无需运动平台"));
        }
    }

    if (requirement.requireEncoder) {
        if (camera.supportsEncoder < 0) {
            addMissing(&result.missingFields, QString::fromUtf8("编码器接口"));
        } else if (camera.supportsEncoder == 0) {
            result.rejectionReasons.append(QString::fromUtf8("未公开或不支持编码器接口"));
        }
    }

    if (!requirement.interfaceText.isEmpty()) {
        if (camera.interfaces.isEmpty())
            addMissing(&result.missingFields, QString::fromUtf8("接口"));
        else if (!containsText(camera.interfaces, requirement.interfaceText))
            result.rejectionReasons.append(QString::fromUtf8("接口不匹配：需要 %1").arg(requirement.interfaceText));
    }

    if (!requirement.ipRating.isEmpty()) {
        if (camera.ipRating.isEmpty())
            addMissing(&result.missingFields, QString::fromUtf8("IP 等级"));
        else if (!camera.ipRating.contains(requirement.ipRating, Qt::CaseInsensitive))
            result.rejectionReasons.append(QString::fromUtf8("IP 等级不匹配：需要 %1，当前 %2").arg(requirement.ipRating, camera.ipRating));
    }

    if (!requirement.materialScenario.isEmpty()) {
        if (camera.materialScenarios.isEmpty())
            addMissing(&result.missingFields, QString::fromUtf8("材质场景"));
        else if (!containsText(camera.materialScenarios, requirement.materialScenario))
            result.rejectionReasons.append(QString::fromUtf8("材质场景不匹配：需要 %1").arg(requirement.materialScenario));
    }

    if (!result.rejectionReasons.isEmpty())
        result.status = ThreeDMatchStatus::NoMatch;
    else if (!result.missingFields.isEmpty())
        result.status = ThreeDMatchStatus::MissingData;
    else
        result.status = ThreeDMatchStatus::Match;
    return result;
}
