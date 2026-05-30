#include "three_d/ThreeDCameraTypes.h"

#include <QtGlobal>

bool threeDHasValue(double value)
{
    return value >= 0.0;
}

QString threeDTechnologyLabel(ThreeDTechnology technology)
{
    switch (technology) {
    case ThreeDTechnology::LineLaserProfile:
        return QString::fromUtf8("线激光轮廓");
    case ThreeDTechnology::PointLaserProfile:
        return QString::fromUtf8("点激光/位移轮廓");
    case ThreeDTechnology::StructuredLightSnapshot:
        return QString::fromUtf8("结构光快照");
    case ThreeDTechnology::StereoStructuredLight:
        return QString::fromUtf8("双目结构光/RGB-D");
    case ThreeDTechnology::LineConfocal:
        return QString::fromUtf8("线共焦");
    case ThreeDTechnology::InterferometryWhiteLight:
        return QString::fromUtf8("干涉/白光类测量");
    case ThreeDTechnology::Other:
        return QString::fromUtf8("其它官方标注 3D 技术");
    }
    return QString::fromUtf8("其它官方标注 3D 技术");
}

ThreeDTechnology threeDTechnologyFromLabel(const QString &label)
{
    const QString normalized = label.trimmed();
    if (normalized == threeDTechnologyLabel(ThreeDTechnology::LineLaserProfile))
        return ThreeDTechnology::LineLaserProfile;
    if (normalized == threeDTechnologyLabel(ThreeDTechnology::PointLaserProfile))
        return ThreeDTechnology::PointLaserProfile;
    if (normalized == threeDTechnologyLabel(ThreeDTechnology::StructuredLightSnapshot))
        return ThreeDTechnology::StructuredLightSnapshot;
    if (normalized == threeDTechnologyLabel(ThreeDTechnology::StereoStructuredLight))
        return ThreeDTechnology::StereoStructuredLight;
    if (normalized == threeDTechnologyLabel(ThreeDTechnology::LineConfocal))
        return ThreeDTechnology::LineConfocal;
    if (normalized == threeDTechnologyLabel(ThreeDTechnology::InterferometryWhiteLight))
        return ThreeDTechnology::InterferometryWhiteLight;
    return ThreeDTechnology::Other;
}

QStringList threeDTechnologyLabels()
{
    return QStringList()
        << threeDTechnologyLabel(ThreeDTechnology::LineLaserProfile)
        << threeDTechnologyLabel(ThreeDTechnology::PointLaserProfile)
        << threeDTechnologyLabel(ThreeDTechnology::StructuredLightSnapshot)
        << threeDTechnologyLabel(ThreeDTechnology::StereoStructuredLight)
        << threeDTechnologyLabel(ThreeDTechnology::LineConfocal)
        << threeDTechnologyLabel(ThreeDTechnology::InterferometryWhiteLight)
        << threeDTechnologyLabel(ThreeDTechnology::Other);
}

QString threeDMatchStatusLabel(ThreeDMatchStatus status)
{
    switch (status) {
    case ThreeDMatchStatus::Match:
        return QString::fromUtf8("满足需求");
    case ThreeDMatchStatus::MissingData:
        return QString::fromUtf8("参数缺失待确认");
    case ThreeDMatchStatus::NoMatch:
        return QString::fromUtf8("不满足需求");
    }
    return QString::fromUtf8("参数缺失待确认");
}

int threeDMatchStatusOrder(ThreeDMatchStatus status)
{
    switch (status) {
    case ThreeDMatchStatus::Match:
        return 0;
    case ThreeDMatchStatus::MissingData:
        return 1;
    case ThreeDMatchStatus::NoMatch:
        return 2;
    }
    return 3;
}
