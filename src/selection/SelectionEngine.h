#ifndef SELECTIONENGINE_H
#define SELECTIONENGINE_H

#include "core/SelectionTypes.h"

class SelectionEngine
{
public:
    QVector<SelectionResult> select(const SelectionRequest &request,
                                    const QVector<CameraSpec> &cameras,
                                    const QVector<LensSpec> &lenses,
                                    const QVector<LightSpec> &lights,
                                    int limit = 12) const;

    static double requiredFovWidth(const SelectionRequest &request);
    static double requiredFovHeight(const SelectionRequest &request);
    static double targetObjectPixelUm(const SelectionRequest &request);
    static double bandwidthRequiredMBps(const CameraSpec &camera, double fps);
    static double framePayloadMB(const CameraSpec &camera);
    static double storagePerHourGB(const CameraSpec &camera, double fps);
    static double interfaceCapacityMBps(const CameraSpec &camera);
    static double maxExposureUsForOnePixelBlur(const SelectionRequest &request);
    static double estimatedFixedLensDofMm(const CameraSpec &camera,
                                          const LensSpec &lens,
                                          double magnification);
    static double distortionErrorUm(const LensSpec &lens,
                                    double fovWidthMm,
                                    double fovHeightMm);
    static double lightCoverageMarginPercent(const SelectionRequest &request,
                                             const LightSpec &light);

private:
    SelectionResult evaluatePair(const SelectionRequest &request,
                                 const CameraSpec &camera,
                                 const LensSpec &lens,
                                 const LightSpec &light,
                                 bool includeDetails = true) const;

    LightSpec chooseLight(const SelectionRequest &request,
                          const LensSpec &lens,
                          const QVector<LightSpec> &lights,
                          QStringList *reasons) const;

    double scoreLight(const SelectionRequest &request,
                      const LensSpec &lens,
                      const LightSpec &light,
                      QStringList *reasons) const;

    void scoreCamera(const SelectionRequest &request,
                     const CameraSpec &camera,
                     SelectionResult *result,
                     bool includeDetails) const;

    void scoreFixedFocalLens(const SelectionRequest &request,
                             const CameraSpec &camera,
                             const LensSpec &lens,
                             SelectionResult *result,
                             bool includeDetails) const;

    void scoreTelecentricLens(const SelectionRequest &request,
                              const CameraSpec &camera,
                              const LensSpec &lens,
                              SelectionResult *result,
                              bool includeDetails) const;

    static bool measurementNeedsTelecentric(const SelectionRequest &request);
};

#endif
