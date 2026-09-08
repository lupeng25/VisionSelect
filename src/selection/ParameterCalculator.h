#ifndef PARAMETERCALCULATOR_H
#define PARAMETERCALCULATOR_H

#include "selection/CalculationStatus.h"

#include <QString>
#include <QVector>

namespace Parameters {
using Number = std::optional<double>;

struct Sensor {
    Number resolutionX;
    Number resolutionY;
    Number pixelSizeUm;
};

enum class OpticsModel { Paraxial, ThinLens };
enum class OpticsSolve { FocalLength, FieldOfView, Distance };
struct OpticsInput {
    Sensor sensor;
    OpticsModel model = OpticsModel::Paraxial;
    OpticsSolve solve = OpticsSolve::FocalLength;
    Number focalLengthMm;
    Number distanceMm;
    Number targetFovWidthMm;
    Number targetFovHeightMm;
};
struct OpticsResult {
    CalculationStatus status = CalculationStatus::Unknown;
    Number focalLengthMm;
    Number distanceMm;
    Number fovWidthMm;
    Number fovHeightMm;
    Number magnification;
    Number objectPixelUm;
    CalculationStatus coverage = CalculationStatus::Unknown;
    QString note;
};

enum class SamplingSolve { Requirement, Actual, Calibration };
struct SamplingInput {
    SamplingSolve solve = SamplingSolve::Requirement;
    Sensor sensor;
    Number fovWidthMm;
    Number fovHeightMm;
    Number featureUm;
    Number pixelsPerFeature;
    bool measurementBudget = false;
    Number toleranceUm;
    Number pixelsPerTolerance;
    Number calibrationLengthMm;
    Number calibrationPixels;
};
struct SamplingResult {
    CalculationStatus status = CalculationStatus::Unknown;
    Number targetObjectPixelUm;
    Number requiredResolutionX;
    Number requiredResolutionY;
    Number objectPixelXUm;
    Number objectPixelYUm;
    Number featurePixelsX;
    Number featurePixelsY;
    Number calibratedPixelUm;
    QString note;
};

enum class TelecentricSolve { FieldOfView, Magnification, Range };
struct TelecentricInput {
    Sensor sensor;
    TelecentricSolve solve = TelecentricSolve::Range;
    Number magnification;
    Number targetFovWidthMm;
    Number targetFovHeightMm;
    Number targetObjectPixelUm;
};
struct TelecentricResult {
    CalculationStatus status = CalculationStatus::Unknown;
    Number minMagnification;
    Number maxMagnification;
    Number magnification;
    Number fovWidthMm;
    Number fovHeightMm;
    Number objectPixelUm;
    QString note;
};

enum class ExposureSolve { Exposure, Blur, Speed };
struct ExposureInput {
    ExposureSolve solve = ExposureSolve::Exposure;
    Number objectPixelUm;
    Number speedMmS;
    Number exposureUs;
    Number blurPixels;
};
struct ExposureResult {
    CalculationStatus status = CalculationStatus::Unknown;
    Number exposureUs;
    Number blurPixels;
    Number speedMmS;
    QString note;
};

struct TransferInput {
    Number width;
    Number height;
    Number roiWidth;
    Number roiHeight;
    Number fps;
    Number cameraCount = 1.0;
    Number overheadPercent = 0.0;
    Number capacityMBps;
    Number hours = 1.0;
    QString pixelFormat;
    // 空字符串表示直接保存传输载荷；非空表示主机转换后的保存格式。
    QString storageFormat;
};
struct TransferResult {
    CalculationStatus status = CalculationStatus::Unknown;
    Number frameBytes;
    Number payloadMBps;
    Number transportMBps;
    Number utilizationPercent;
    Number storageGB;
    Number storageGiB;
    CalculationStatus capacityStatus = CalculationStatus::Unknown;
    QString note;
};

struct CheckItem {
    QString key;
    Number target;
    Number actual;
    QString unit;
    CalculationStatus status = CalculationStatus::Unknown;
    QString detail;
};
struct SystemInput {
    Sensor sensor;
    OpticsModel model = OpticsModel::Paraxial;
    bool telecentric = false;
    Number focalLengthMm;
    Number distanceMm;
    Number magnification;
    bool measuredFov = false;
    Number measuredFovWidthMm;
    Number measuredFovHeightMm;
    Number targetFovWidthMm;
    Number targetFovHeightMm;
    Number targetObjectPixelUm;
    Number fps;
    Number maxFps;
    Number capacityMBps;
    QString pixelFormat;
    Number imageCircleMm;
    QString cameraMount;
    QString lensMount;
    Number minWorkingDistanceMm;
    Number nominalWorkingDistanceMm;
    Number workingDistanceToleranceMm;
    Number heightVariationMm;
    Number dofMm;
    bool dofConditionsConfirmed = false;
    Number telecentricityDeg;
    Number measurementToleranceUm;
    bool motionEnabled = false;
    Number speedMmS;
    Number exposureUs;
    Number allowedBlurPixels;
};
struct SystemResult {
    CalculationStatus status = CalculationStatus::Unknown;
    Number actualFovWidthMm;
    Number actualFovHeightMm;
    Number objectPixelXUm;
    Number objectPixelYUm;
    QVector<CheckItem> checks;
};

OpticsResult optics(const OpticsInput &input);
SamplingResult sampling(const SamplingInput &input);
TelecentricResult telecentric(const TelecentricInput &input);
ExposureResult exposure(const ExposureInput &input);
TransferResult transfer(const TransferInput &input);
SystemResult checkSystem(const SystemInput &input);
QString statusText(CalculationStatus status);
QString checkTitle(const QString &key);
}

#endif
