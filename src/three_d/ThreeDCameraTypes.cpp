#include "three_d/ThreeDCameraTypes.h"

#include "i18n/LanguageManager.h"

#include <QtGlobal>

namespace {
QString text(const char *zhUtf8, const char *enUtf8)
{
    return LanguageManager::instance().currentLanguage() == QLatin1String("en_US")
        ? QString::fromUtf8(enUtf8)
        : QString::fromUtf8(zhUtf8);
}

QString canonicalTechnologyLabel(ThreeDTechnology technology)
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

QString englishTechnologyLabel(ThreeDTechnology technology)
{
    switch (technology) {
    case ThreeDTechnology::LineLaserProfile:
        return QStringLiteral("Line laser profile");
    case ThreeDTechnology::PointLaserProfile:
        return QStringLiteral("Point laser / displacement profile");
    case ThreeDTechnology::StructuredLightSnapshot:
        return QStringLiteral("Structured light snapshot");
    case ThreeDTechnology::StereoStructuredLight:
        return QStringLiteral("Stereo structured light / RGB-D");
    case ThreeDTechnology::LineConfocal:
        return QStringLiteral("Line confocal");
    case ThreeDTechnology::InterferometryWhiteLight:
        return QStringLiteral("Interferometry / white-light measurement");
    case ThreeDTechnology::Other:
        return QStringLiteral("Other official 3D technology");
    }
    return QStringLiteral("Other official 3D technology");
}
}

bool threeDHasValue(double value)
{
    return value >= 0.0;
}

QString threeDTechnologyLabel(ThreeDTechnology technology)
{
    switch (technology) {
    case ThreeDTechnology::LineLaserProfile:
        return text("线激光轮廓", "Line laser profile");
    case ThreeDTechnology::PointLaserProfile:
        return text("点激光/位移轮廓", "Point laser / displacement profile");
    case ThreeDTechnology::StructuredLightSnapshot:
        return text("结构光快照", "Structured light snapshot");
    case ThreeDTechnology::StereoStructuredLight:
        return text("双目结构光/RGB-D", "Stereo structured light / RGB-D");
    case ThreeDTechnology::LineConfocal:
        return text("线共焦", "Line confocal");
    case ThreeDTechnology::InterferometryWhiteLight:
        return text("干涉/白光类测量", "Interferometry / white-light measurement");
    case ThreeDTechnology::Other:
        return text("其它官方标注 3D 技术", "Other official 3D technology");
    }
    return text("其它官方标注 3D 技术", "Other official 3D technology");
}

ThreeDTechnology threeDTechnologyFromLabel(const QString &label)
{
    const QString normalized = label.trimmed();
    const QVector<ThreeDTechnology> values = {
        ThreeDTechnology::LineLaserProfile,
        ThreeDTechnology::PointLaserProfile,
        ThreeDTechnology::StructuredLightSnapshot,
        ThreeDTechnology::StereoStructuredLight,
        ThreeDTechnology::LineConfocal,
        ThreeDTechnology::InterferometryWhiteLight,
        ThreeDTechnology::Other
    };
    for (ThreeDTechnology technology : values) {
        if (normalized == canonicalTechnologyLabel(technology)
            || normalized.compare(englishTechnologyLabel(technology), Qt::CaseInsensitive) == 0
            || normalized == threeDTechnologyLabel(technology)) {
            return technology;
        }
    }
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
        return text("满足需求", "Meets requirements");
    case ThreeDMatchStatus::MissingData:
        return text("参数缺失待确认", "Missing data");
    case ThreeDMatchStatus::NoMatch:
        return text("不满足需求", "Does not meet requirements");
    }
    return text("参数缺失待确认", "Missing data");
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
