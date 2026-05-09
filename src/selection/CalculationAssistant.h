#ifndef CALCULATIONASSISTANT_H
#define CALCULATIONASSISTANT_H

#include "core/SelectionTypes.h"

#include <QVector>

struct RequirementEstimate {
    double requiredFovWidthMm = 0.0;
    double requiredFovHeightMm = 0.0;
    double targetObjectPixelUm = 0.0;
    int requiredResolutionX = 0;
    int requiredResolutionY = 0;
    double requiredMegapixels = 0.0;
    double requiredBandwidthMBps12Bit = 0.0;
    bool hasMotionConstraint = false;
    double maxExposureUsForOnePixelBlur = 0.0;
    bool telecentricPreferred = false;
};

struct CameraCalculationEstimate {
    CameraSpec camera;
    double objectPixelSizeUm = 0.0;
    bool meetsSampling = false;
    bool meetsFps = false;
    bool globalShutterRecommended = false;
    double fixedFocalLengthMm = 0.0;
    double sensorDiagonalMm = 0.0;
    double telecentricPmagMin = 0.0;
    double telecentricPmagMax = 0.0;
    bool telecentricFeasible = false;
    double bandwidthRequiredMBps = 0.0;
    double score = 0.0;
};

struct LensCalculationEstimate {
    LensSpec lens;
    double score = 0.0;
    double estimatedFocalLengthMm = 0.0;
    double effectiveFovWidthMm = 0.0;
    double effectiveFovHeightMm = 0.0;
    double objectPixelSizeUm = 0.0;
    double magnification = 0.0;
    double residualTelecentricErrorUm = 0.0;
    bool fovOk = false;
    bool samplingOk = false;
    bool imageCircleOk = false;
    bool mountOk = false;
    bool workingDistanceOk = false;
    bool dofOk = false;
    QString formulaSummary;
    QStringList reasons;
    QStringList risks;
};

class CalculationAssistant
{
public:
    static RequirementEstimate estimateRequirement(const SelectionRequest &request);
    static QVector<CameraCalculationEstimate> estimateCameras(const SelectionRequest &request,
                                                              const QVector<CameraSpec> &cameras,
                                                              int limit = 12);
    static QVector<LensCalculationEstimate> estimateLenses(const SelectionRequest &request,
                                                           const CameraSpec &camera,
                                                           const QVector<LensSpec> &lenses,
                                                           int limit = 12);
    static double estimatedFixedFocalLengthMm(const SelectionRequest &request, const CameraSpec &camera);
    static bool telecentricPreferred(const SelectionRequest &request);
};

#endif
