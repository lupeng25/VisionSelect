#ifndef THREEDCALCULATION_H
#define THREEDCALCULATION_H

#include "three_d/ThreeDCameraTypes.h"

#include <QStringList>

struct ThreeDMotionSamplingInput
{
    double scanDistanceMm = 300.0;
    double profileIntervalMm = 0.05;
    double axisTravelMm = 10.0;
    int pulseCount = 10000;
    int refinementPoints = 50;
    double samplingRateHz = 1000.0;
    double safetyFactor = 0.8;
    double overrideXPixelPitchMm = -1.0;
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
    bool valid = true;
    bool xPixelPitchKnown = false;
    bool usesManualXPixelPitch = false;
    bool samplingRateKnown = false;
    bool samplingRateWithinCameraLimit = true;
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
