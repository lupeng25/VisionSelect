#ifndef SELECTIONTYPES_H
#define SELECTIONTYPES_H

#include <QMetaType>
#include <QString>
#include <QStringList>
#include <QVector>

enum class DetectionType {
    Measurement,
    Positioning,
    DefectInspection,
    OcrCode
};

enum class SurfaceType {
    Matte,
    ReflectiveMetal,
    GlassTransparent,
    PCB,
    Plastic,
    Mixed
};

enum class LensType {
    FixedFocal,
    ObjectTelecentric,
    BiTelecentric
};

enum class LightType {
    Backlight,
    Ring,
    Bar,
    Coaxial,
    Dome,
    TelecentricBacklight,
    DarkField
};

struct SelectionRequest {
    double objectWidthMm = 20.0;
    double objectHeightMm = 20.0;
    double placementMarginMm = 2.0;
    double minFeatureUm = 50.0;
    double measurementToleranceUm = 10.0;
    double workingDistanceMm = 110.0;
    double heightVariationMm = 2.0;
    double motionSpeedMmS = 0.0;
    double requiredFps = 20.0;
    DetectionType detectionType = DetectionType::Measurement;
    SurfaceType surfaceType = SurfaceType::ReflectiveMetal;
    bool reflective = true;
    bool preferMono = true;
    bool allowTelecentric = true;
};

struct CameraSpec {
    QString model;
    int resolutionX = 0;
    int resolutionY = 0;
    double pixelSizeUm = 0.0;
    QString sensorFormat;
    QString colorMode;
    QString shutterType;
    double maxFps = 0.0;
    QString interfaceType;
    double bandwidthMBps = 0.0;
    double bitDepth = 8.0;
    double dynamicRangeDb = 0.0;
    QString lensMount;

    double sensorWidthMm() const;
    double sensorHeightMm() const;
    double sensorDiagonalMm() const;
    double megapixels() const;
    bool isMono() const;
    bool isGlobalShutter() const;
};

struct LensSpec {
    QString model;
    LensType lensType = LensType::FixedFocal;
    QString lensMount;
    double focalLengthMm = 0.0;
    double minWorkingDistanceMm = 0.0;
    double distortionPercent = 0.0;
    double imageCircleMm = 0.0;
    double megapixelRating = 0.0;
    double recommendedMinPixelUm = 0.0;

    double pmag = 0.0;
    double nominalWorkingDistanceMm = 0.0;
    double workingDistanceToleranceMm = 0.0;
    double maxSensorDiagonalMm = 0.0;
    double telecentricityDeg = 0.0;
    double dofMm = 0.0;
    double numericalAperture = 0.0;
    double fNumber = 0.0;
    bool coaxialIllumination = false;
    QString notes;

    bool isTelecentric() const;
    QString typeLabel() const;
};

struct LightSpec {
    QString model;
    LightType lightType = LightType::Ring;
    QString color;
    int wavelengthNm = 0;
    QString mode;
    double activeWidthMm = 0.0;
    double activeHeightMm = 0.0;
    QString bestFor;
    QString typeLabel() const;
};

struct CandidateScore {
    double score = 0.0;
    QStringList reasons;
    QStringList risks;
};

struct SelectionResult {
    CameraSpec camera;
    LensSpec lens;
    LightSpec light;
    double requiredFovWidthMm = 0.0;
    double requiredFovHeightMm = 0.0;
    double effectiveFovWidthMm = 0.0;
    double effectiveFovHeightMm = 0.0;
    double objectPixelSizeUm = 0.0;
    double magnification = 0.0;
    double estimatedFocalLengthMm = 0.0;
    double bandwidthRequiredMBps = 0.0;
    double residualTelecentricErrorUm = 0.0;
    CandidateScore score;
    QString schemeTitle;
    QString formulaSummary;

    bool isTelecentric() const;
};

QString detectionTypeLabel(DetectionType type);
QString surfaceTypeLabel(SurfaceType type);
QString lensTypeLabel(LensType type);
QString lightTypeLabel(LightType type);
QString detectionTypeKey(DetectionType type);
QString surfaceTypeKey(SurfaceType type);
DetectionType detectionTypeFromIndex(int index);
SurfaceType surfaceTypeFromIndex(int index);
LensType lensTypeFromString(const QString &value);
LightType lightTypeFromString(const QString &value);
QString boolLabel(bool value);
bool parseBool(const QString &value);
bool mountsCompatible(const QString &cameraMount, const QString &lensMount);

Q_DECLARE_METATYPE(SelectionResult)

#endif
