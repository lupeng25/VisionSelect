#ifndef THREEDCALCULATION_H
#define THREEDCALCULATION_H

#include "three_d/ThreeDCameraTypes.h"

#include <QString>
#include <QStringList>

enum class ThreeDTriggerMode
{
    FreeRun,
    ExternalTrigger,
    Encoder
};

struct ThreeDMotionSamplingInput
{
    double scanDistanceMm = 300.0;
    double profileIntervalMm = 0.05;
    double targetAxisSpeedMmS = -1.0;
    double axisTravelMm = 10.0;
    int pulseCount = 10000;
    int refinementPoints = 50;
    double samplingRateHz = 1000.0;
    double safetyFactor = 0.8;
    double overrideXPixelPitchMm = -1.0;
    ThreeDTriggerMode triggerMode = ThreeDTriggerMode::FreeRun;
    double exposureTimeUs = 100.0;
    double readoutMarginUs = 3.0;
    double encoderPulseFrequencyHz = -1.0;
    int encoderPulsesPerProfile = 1;
};

struct ThreeDMotionSamplingResult
{
    double profileCount = 0.0;
    double pulseIntervalMm = 0.0;
    double yPixelPitchMm = 0.0;
    double xPixelPitchMm = -1.0;
    double maxAxisSpeedMmS = 0.0;
    double xyPitchRatio = -1.0;
    double cameraSamplingRateLimitHz = -1.0;
    double encoderRateLimitHz = -1.0;
    double effectiveProfileRateHz = -1.0;
    double requiredProfileRateHz = -1.0;
    double profilePeriodUs = -1.0;
    double maxExposureTimeUs = -1.0;
    double encoderProfileRateHz = -1.0;
    double encoderProfileIntervalMm = -1.0;
    double encoderAxisSpeedMmS = -1.0;
    bool valid = true;
    bool xPixelPitchKnown = false;
    bool usesManualXPixelPitch = false;
    bool samplingRateKnown = false;
    bool samplingRateWithinCameraLimit = true;
    bool encoderRateKnown = false;
    bool encoderRateWithinCameraLimit = true;
    bool exposureWithinProfilePeriod = true;
    bool exposureWithinCameraRange = true;
    bool effectiveRateMeetsTarget = true;
    QStringList reasons;
    QStringList risks;
};

class ThreeDCalculation
{
public:
    static ThreeDMotionSamplingResult estimateMotionSampling(const ThreeDMotionSamplingInput &input,
                                                             const ThreeDCameraSpec *camera = nullptr);
    static double xPixelPitchMm(const ThreeDCameraSpec &camera,
                                double overrideXPixelPitchMm = -1.0,
                                bool *usesManualOverride = nullptr);
    static double samplingRateLimitHz(const ThreeDCameraSpec &camera);
};

#endif
