#ifndef THREEDCAMERATYPES_H
#define THREEDCAMERATYPES_H

#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVector>

enum class ThreeDTechnology
{
    LineLaserProfile,
    PointLaserProfile,
    StructuredLightSnapshot,
    StereoStructuredLight,
    LineConfocal,
    InterferometryWhiteLight,
    Other
};

enum class ThreeDMatchStatus
{
    Match,
    MissingData,
    NoMatch
};

struct ThreeDCameraSpec
{
    QString manufacturer;
    QString series;
    QString model;
    ThreeDTechnology technology = ThreeDTechnology::Other;
    QString technologyLabel;
    QString status;
    QString sourceUrl;
    QString sourceDate;

    double referenceDistanceMm = -1.0;
    double workingDistanceMinMm = -1.0;
    double workingDistanceMaxMm = -1.0;
    double zMeasurementRangeMm = -1.0;
    double xFovNearMm = -1.0;
    double xFovReferenceMm = -1.0;
    double xFovFarMm = -1.0;
    double yFovNearMm = -1.0;
    double yFovReferenceMm = -1.0;
    double yFovFarMm = -1.0;

    double zRepeatabilityUm = -1.0;
    double xRepeatabilityUm = -1.0;
    double zResolutionUm = -1.0;
    double zLinearityPercentOfRange = -1.0;
    double measurementAccuracyUm = -1.0;
    double xyResolutionUm = -1.0;
    double profileDataIntervalUm = -1.0;
    QString accuracyCondition;

    int profilePoints = -1;
    double scanRateMinHz = -1.0;
    double scanRateMaxHz = -1.0;
    double acquisitionTimeMs = -1.0;
    double frameRateHz = -1.0;
    int requiresExternalMotion = -1;
    int supportsEncoder = -1;

    QString lightSource;
    double wavelengthNm = -1.0;
    QString laserClass;
    QString structure;
    QString ipRating;
    double weightG = -1.0;
    QString dimensions;
    QString temperature;
    QString power;

    QStringList interfaces;
    QStringList outputTypes;
    QStringList materialScenarios;
    QStringList notes;
    QJsonObject rawSpecs;
};

struct ThreeDCameraRequirement
{
    QString manufacturer;
    QString technologyLabel;
    double targetXCoverageMm = -1.0;
    double targetYCoverageMm = -1.0;
    double zMeasurementRangeMm = -1.0;
    double workingDistanceMm = -1.0;
    double maxZRepeatabilityUm = -1.0;
    double minSpeedHz = -1.0;
    bool requireNoExternalMotion = false;
    bool requireEncoder = false;
    QString interfaceText;
    QString ipRating;
    QString materialScenario;
};

struct ThreeDCameraMatch
{
    ThreeDCameraSpec spec;
    ThreeDMatchStatus status = ThreeDMatchStatus::Match;
    QStringList missingFields;
    QStringList rejectionReasons;
};

bool threeDHasValue(double value);
QString threeDTechnologyLabel(ThreeDTechnology technology);
ThreeDTechnology threeDTechnologyFromLabel(const QString &label);
QStringList threeDTechnologyLabels();
QString threeDMatchStatusLabel(ThreeDMatchStatus status);
int threeDMatchStatusOrder(ThreeDMatchStatus status);

#endif
