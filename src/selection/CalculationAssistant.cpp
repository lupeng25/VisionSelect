#include "selection/CalculationAssistant.h"

#include "selection/SelectionEngine.h"

#include <algorithm>
#include <QtMath>

namespace {
int ceilToInt(double value)
{
    return qMax(1, static_cast<int>(qCeil(value)));
}

double objectPixelUm(const SelectionRequest &request, const CameraSpec &camera)
{
    const double fovW = SelectionEngine::requiredFovWidth(request);
    const double fovH = SelectionEngine::requiredFovHeight(request);
    return qMax(fovW * 1000.0 / qMax(1, camera.resolutionX),
                fovH * 1000.0 / qMax(1, camera.resolutionY));
}

double clamp(double value, double low, double high)
{
    return qMax(low, qMin(high, value));
}

double percentDifference(double actual, double target)
{
    if (target <= 0.0)
        return 1.0;
    return qAbs(actual - target) / target;
}

QString mm(double value, int decimals = 1)
{
    return QStringLiteral("%1 mm").arg(value, 0, 'f', decimals);
}

QString um(double value, int decimals = 2)
{
    return QStringLiteral("%1 um").arg(value, 0, 'f', decimals);
}

double cameraScore(const SelectionRequest &request,
                   const RequirementEstimate &requirement,
                   const CameraCalculationEstimate &estimate)
{
    double score = 0.0;
    const double pixelRatio = estimate.objectPixelSizeUm / qMax(0.001, requirement.targetObjectPixelUm);
    score += estimate.meetsSampling ? 100.0 : -50.0 * pixelRatio;
    score += estimate.meetsFps ? 25.0 : -35.0;
    score += estimate.telecentricFeasible ? 10.0 : -4.0;

    if (estimate.interfaceCapacityMBps <= 0.0) {
        score -= 12.0;
    } else if (!estimate.meetsBandwidth) {
        const double overloadRatio = estimate.bandwidthRequiredMBps / qMax(1.0, estimate.interfaceCapacityMBps);
        score -= qMin(90.0, 45.0 + overloadRatio * 12.0);
    } else if (estimate.bandwidthUtilizationPercent > 90.0) {
        score -= 14.0;
    } else if (estimate.bandwidthUtilizationPercent > 70.0) {
        score -= 4.0;
    } else {
        score += 12.0;
    }

    if (request.preferMono)
        score += estimate.camera.isMono() ? 8.0 : -8.0;
    if (request.motionSpeedMmS > 20.0)
        score += estimate.camera.isGlobalShutter() ? 14.0 : -20.0;

    const double mpExcess = estimate.camera.megapixels() - requirement.requiredMegapixels;
    if (mpExcess > 0.0)
        score -= qMin(18.0, mpExcess * 0.6);

    return score;
}

void appendCommonLensJudgement(const SelectionRequest &request,
                               const RequirementEstimate &requirement,
                               const CameraSpec &camera,
                               LensCalculationEstimate *estimate)
{
    estimate->fovOk = estimate->effectiveFovWidthMm >= requirement.requiredFovWidthMm
        && estimate->effectiveFovHeightMm >= requirement.requiredFovHeightMm;
    estimate->samplingOk = estimate->objectPixelSizeUm <= requirement.targetObjectPixelUm;
    const double allowedDiagonal = estimate->lens.maxSensorDiagonalMm > 0.0
        ? estimate->lens.maxSensorDiagonalMm
        : estimate->lens.imageCircleMm;
    estimate->imageCircleOk = allowedDiagonal >= camera.sensorDiagonalMm()
        && estimate->lens.imageCircleMm >= camera.sensorDiagonalMm();
    estimate->mountOk = mountsCompatible(camera.lensMount, estimate->lens.lensMount);

    if (estimate->fovOk) {
        estimate->score += 18.0;
        estimate->reasons.append(QString::fromUtf8("\350\247\206\351\207\216\350\246\206\347\233\226\345\275\223\345\211\215\351\234\200\346\261\202"));
    } else {
        estimate->score -= 35.0;
        estimate->risks.append(QString::fromUtf8("\350\247\206\351\207\216\344\270\215\350\266\263\357\274\214\351\234\200\346\233\264\347\237\255\347\204\246\350\267\235/\346\233\264\344\275\216\345\200\215\347\216\207\346\210\226\346\233\264\345\244\247\351\235\266\351\235\242"));
    }

    if (estimate->samplingOk) {
        estimate->score += 18.0;
        estimate->reasons.append(QString::fromUtf8("\347\211\251\346\226\271\345\203\217\347\264\240 %1 \346\273\241\350\266\263\347\233\256\346\240\207 %2")
            .arg(um(estimate->objectPixelSizeUm), um(requirement.targetObjectPixelUm)));
    } else {
        const double ratio = estimate->objectPixelSizeUm / qMax(0.001, requirement.targetObjectPixelUm);
        estimate->score -= clamp((ratio - 1.0) * 18.0, 10.0, 40.0);
        estimate->risks.append(QString::fromUtf8("\347\211\251\346\226\271\345\203\217\347\264\240 %1 \347\262\227\344\272\216\347\233\256\346\240\207 %2")
            .arg(um(estimate->objectPixelSizeUm), um(requirement.targetObjectPixelUm)));
    }

    if (estimate->imageCircleOk) {
        estimate->score += 12.0;
    } else {
        estimate->score -= 30.0;
        estimate->risks.append(QString::fromUtf8("\351\225\234\345\244\264\345\203\217\345\234\206/\346\234\200\345\244\247\351\235\266\351\235\242\344\270\215\350\266\263\344\273\245\350\246\206\347\233\226\347\233\270\346\234\272\344\274\240\346\204\237\345\231\250"));
    }

    if (estimate->mountOk) {
        estimate->score += 8.0;
    } else {
        estimate->score -= 60.0;
        estimate->risks.append(QString::fromUtf8("\347\233\270\346\234\272\346\216\245\345\217\243 %1 \344\270\216\351\225\234\345\244\264\346\216\245\345\217\243 %2 \344\270\215\345\214\271\351\205\215")
            .arg(camera.lensMount, estimate->lens.lensMount));
    }

    if (request.detectionType == DetectionType::Measurement
        && request.measurementToleranceUm > 0.0
        && estimate->distortionErrorUm > request.measurementToleranceUm) {
        estimate->score -= 12.0;
        estimate->risks.append(QString::fromUtf8("按 FOV 边缘估算畸变误差约 %1，高于允许误差")
            .arg(um(estimate->distortionErrorUm)));
    } else if (estimate->distortionErrorUm > 0.0) {
        estimate->reasons.append(QString::fromUtf8("按 FOV 边缘估算畸变误差约 %1")
            .arg(um(estimate->distortionErrorUm)));
    }
}
}

RequirementEstimate CalculationAssistant::estimateRequirement(const SelectionRequest &request)
{
    RequirementEstimate estimate;
    estimate.requiredFovWidthMm = SelectionEngine::requiredFovWidth(request);
    estimate.requiredFovHeightMm = SelectionEngine::requiredFovHeight(request);
    estimate.targetObjectPixelUm = SelectionEngine::targetObjectPixelUm(request);
    estimate.requiredResolutionX = ceilToInt(estimate.requiredFovWidthMm * 1000.0 / estimate.targetObjectPixelUm);
    estimate.requiredResolutionY = ceilToInt(estimate.requiredFovHeightMm * 1000.0 / estimate.targetObjectPixelUm);
    estimate.requiredMegapixels = estimate.requiredResolutionX * estimate.requiredResolutionY / 1000000.0;
    estimate.requiredBandwidthMBps12Bit = estimate.requiredResolutionX * estimate.requiredResolutionY
        * 12.0 * qMax(1.0, request.requiredFps) / 8.0 / 1000000.0;
    estimate.hasMotionConstraint = request.motionSpeedMmS > 0.0;
    if (estimate.hasMotionConstraint)
        estimate.maxExposureUsForOnePixelBlur = SelectionEngine::maxExposureUsForOnePixelBlur(request);
    estimate.telecentricPreferred = telecentricPreferred(request);
    return estimate;
}

PureCalculationResult CalculationAssistant::estimatePure(const PureCalculationInput &input)
{
    PureCalculationResult result;
    result.requirement = estimateRequirement(input.request);

    CameraSpec camera = input.camera;
    LensSpec lens = input.lens;
    LightSpec light = input.light;
    if (input.telecentricMode)
        lens.lensType = LensType::ObjectTelecentric;
    else
        lens.lensType = LensType::FixedFocal;

    result.sensorWidthMm = camera.sensorWidthMm();
    result.sensorHeightMm = camera.sensorHeightMm();
    result.sensorDiagonalMm = camera.sensorDiagonalMm();
    result.framePayloadMB = SelectionEngine::framePayloadMB(camera);
    result.bandwidthRequiredMBps = SelectionEngine::bandwidthRequiredMBps(camera, qMax(1.0, input.request.requiredFps));
    result.interfaceCapacityMBps = SelectionEngine::interfaceCapacityMBps(camera);
    result.bandwidthUtilizationPercent = result.interfaceCapacityMBps > 0.0
        ? result.bandwidthRequiredMBps / result.interfaceCapacityMBps * 100.0
        : 0.0;
    result.storagePerHourGB = SelectionEngine::storagePerHourGB(camera, qMax(1.0, input.request.requiredFps));

    if (camera.resolutionX > 0 && camera.resolutionY > 0) {
        result.cameraObjectPixelSizeUm = qMax(result.requirement.requiredFovWidthMm * 1000.0 / camera.resolutionX,
                                              result.requirement.requiredFovHeightMm * 1000.0 / camera.resolutionY);
    }

    if (result.sensorWidthMm <= 0.0 || result.sensorHeightMm <= 0.0) {
        result.risks.append(QString::fromUtf8("相机分辨率或像元无效，无法计算传感器尺寸和镜头参数"));
    } else if (camera.resolutionX <= 0 || camera.resolutionY <= 0) {
        result.risks.append(QString::fromUtf8("相机分辨率无效，无法计算物方像素"));
    } else if (result.cameraObjectPixelSizeUm <= result.requirement.targetObjectPixelUm) {
        result.reasons.append(QString::fromUtf8("按需求 FOV 估算，相机采样满足目标物方像素"));
    } else {
        result.risks.append(QString::fromUtf8("按需求 FOV 估算，物方像素 %1 um 粗于目标 %2 um")
            .arg(result.cameraObjectPixelSizeUm, 0, 'f', 2)
            .arg(result.requirement.targetObjectPixelUm, 0, 'f', 2));
    }

    if (result.interfaceCapacityMBps <= 0.0) {
        result.risks.append(QString::fromUtf8("接口带宽未填写，无法判断吞吐余量"));
    } else if (result.bandwidthUtilizationPercent > 100.0) {
        result.risks.append(QString::fromUtf8("带宽利用率约 %1%，超过接口容量")
            .arg(result.bandwidthUtilizationPercent, 0, 'f', 0));
    } else if (result.bandwidthUtilizationPercent > 90.0) {
        result.risks.append(QString::fromUtf8("带宽利用率约 %1%，接近接口上限")
            .arg(result.bandwidthUtilizationPercent, 0, 'f', 0));
    } else {
        result.reasons.append(QString::fromUtf8("接口带宽利用率约 %1%")
            .arg(result.bandwidthUtilizationPercent, 0, 'f', 0));
    }

    if (input.request.motionSpeedMmS > 20.0 && !camera.isGlobalShutter()) {
        result.risks.append(QString::fromUtf8("高速运动建议使用全局快门相机"));
    } else if (input.request.motionSpeedMmS > 20.0) {
        result.reasons.append(QString::fromUtf8("高速运动场景已选择全局快门"));
    }

    result.targetFixedFocalLengthMm = estimatedFixedFocalLengthMm(input.request, camera);
    if (input.telecentricMode) {
        if (lens.pmag <= 0.0) {
            result.risks.append(QString::fromUtf8("远心倍率 PMAG 必须大于 0"));
        } else {
            result.effectiveFovWidthMm = result.sensorWidthMm / lens.pmag;
            result.effectiveFovHeightMm = result.sensorHeightMm / lens.pmag;
            result.lensObjectPixelSizeUm = camera.pixelSizeUm / lens.pmag;
            result.magnification = lens.pmag;
            result.estimatedDofMm = lens.dofMm;
            result.distortionErrorUm = SelectionEngine::distortionErrorUm(lens, result.effectiveFovWidthMm, result.effectiveFovHeightMm);
            result.residualTelecentricErrorUm = input.request.heightVariationMm
                * qTan(qDegreesToRadians(lens.telecentricityDeg)) * 1000.0;
            result.lensFormulaSummary = QString::fromUtf8("远心：FOV = SensorSize / PMAG，ObjectPixel = PixelSize / PMAG");

            if (result.effectiveFovWidthMm >= result.requirement.requiredFovWidthMm
                && result.effectiveFovHeightMm >= result.requirement.requiredFovHeightMm) {
                result.reasons.append(QString::fromUtf8("远心 FOV 覆盖需求"));
            } else {
                result.risks.append(QString::fromUtf8("远心 FOV 不足，需要更低倍率或更大靶面"));
            }

            if (result.lensObjectPixelSizeUm <= result.requirement.targetObjectPixelUm) {
                result.reasons.append(QString::fromUtf8("远心物方像素满足目标"));
            } else {
                result.risks.append(QString::fromUtf8("远心物方像素 %1 um 粗于目标 %2 um")
                    .arg(result.lensObjectPixelSizeUm, 0, 'f', 2)
                    .arg(result.requirement.targetObjectPixelUm, 0, 'f', 2));
            }

            if (lens.nominalWorkingDistanceMm <= 0.0) {
                result.risks.append(QString::fromUtf8("远心镜头缺少标称 WD，需查 datasheet 复核安装距离"));
            } else if (lens.workingDistanceToleranceMm > 0.0) {
                if (qAbs(input.request.workingDistanceMm - lens.nominalWorkingDistanceMm) > lens.workingDistanceToleranceMm)
                    result.risks.append(QString::fromUtf8("当前 WD 超出远心镜头标称 WD 容差，需确认安装距离"));
            } else if (qAbs(input.request.workingDistanceMm - lens.nominalWorkingDistanceMm) <= 0.5) {
                result.risks.append(QString::fromUtf8("远心镜头缺少 WD 容差数据，仅按接近标称 WD 粗略保留，需查 datasheet 复核"));
            } else {
                result.risks.append(QString::fromUtf8("当前 WD 偏离远心镜头标称 WD，且镜头缺少 WD 容差数据"));
            }

            if (input.request.heightVariationMm > 0.0) {
                if (lens.dofMm > 0.0 && lens.dofMm >= input.request.heightVariationMm * 1.5) {
                    result.reasons.append(QString::fromUtf8("远心 DOF 覆盖高度波动"));
                } else if (lens.dofMm > 0.0) {
                    result.risks.append(QString::fromUtf8("远心 DOF 可能不足以覆盖高度波动"));
                } else {
                    result.risks.append(QString::fromUtf8("远心镜头缺少 DOF 数据，无法确认是否覆盖高度波动"));
                }
            }

            if (input.request.measurementToleranceUm > 0.0
                && result.residualTelecentricErrorUm > input.request.measurementToleranceUm) {
                result.risks.append(QString::fromUtf8("残余远心误差约 %1 um，高于允许误差")
                    .arg(result.residualTelecentricErrorUm, 0, 'f', 2));
            }
        }
    } else {
        if (lens.focalLengthMm <= 0.0) {
            result.risks.append(QString::fromUtf8("普通镜头焦距必须大于 0"));
        } else {
            result.effectiveFovWidthMm = result.sensorWidthMm
                * qMax(1.0, input.request.workingDistanceMm - lens.focalLengthMm)
                / lens.focalLengthMm;
            result.effectiveFovHeightMm = result.sensorHeightMm
                * qMax(1.0, input.request.workingDistanceMm - lens.focalLengthMm)
                / lens.focalLengthMm;
            result.lensObjectPixelSizeUm = qMax(result.effectiveFovWidthMm * 1000.0 / qMax(1, camera.resolutionX),
                                                result.effectiveFovHeightMm * 1000.0 / qMax(1, camera.resolutionY));
            result.magnification = result.sensorWidthMm / qMax(0.001, result.effectiveFovWidthMm);
            result.estimatedDofMm = SelectionEngine::estimatedFixedLensDofMm(camera, lens, result.magnification);
            result.distortionErrorUm = SelectionEngine::distortionErrorUm(lens, result.effectiveFovWidthMm, result.effectiveFovHeightMm);
            result.lensFormulaSummary = QString::fromUtf8("普通镜头：M = SensorSize / FOV，f ≈ WD x SensorSize / (FOV + SensorSize)");

            if (result.effectiveFovWidthMm >= result.requirement.requiredFovWidthMm
                && result.effectiveFovHeightMm >= result.requirement.requiredFovHeightMm) {
                result.reasons.append(QString::fromUtf8("普通镜头 FOV 覆盖需求"));
            } else {
                result.risks.append(QString::fromUtf8("普通镜头在当前 WD 下 FOV 不足"));
            }

            if (input.request.workingDistanceMm < lens.minWorkingDistanceMm) {
                result.risks.append(QString::fromUtf8("当前 WD 小于普通镜头最小工作距离"));
            }

            if (input.request.heightVariationMm > 0.0) {
                if (result.estimatedDofMm > 0.0 && result.estimatedDofMm >= input.request.heightVariationMm * 1.5) {
                    result.reasons.append(QString::fromUtf8("估算 DOF 覆盖高度波动"));
                } else if (result.estimatedDofMm > 0.0) {
                    result.risks.append(QString::fromUtf8("估算 DOF %1 mm 可能不足")
                        .arg(result.estimatedDofMm, 0, 'f', 2));
                } else {
                    result.risks.append(QString::fromUtf8("缺少 F/# 或倍率数据，无法估算 DOF"));
                }
            }
        }
    }

    if (lens.imageCircleMm > 0.0 && result.sensorDiagonalMm > lens.imageCircleMm) {
        result.risks.append(QString::fromUtf8("镜头像面小于传感器对角线，存在暗角风险"));
    }
    if (lens.megapixelRating > 0.0 && camera.megapixels() > lens.megapixelRating) {
        result.risks.append(QString::fromUtf8("镜头标称 MP 低于相机像素，需确认 MTF"));
    }
    if (input.request.detectionType == DetectionType::Measurement
        && input.request.measurementToleranceUm > 0.0
        && result.distortionErrorUm > input.request.measurementToleranceUm) {
        result.risks.append(QString::fromUtf8("畸变边缘误差约 %1 um，高于允许误差")
            .arg(result.distortionErrorUm, 0, 'f', 2));
    }

    result.suggestedLightWidthMm = result.requirement.requiredFovWidthMm * 1.1;
    result.suggestedLightHeightMm = result.requirement.requiredFovHeightMm * 1.1;
    result.lightCoverageMarginPercent = SelectionEngine::lightCoverageMarginPercent(input.request, light);
    result.reasons.append(QString::fromUtf8("建议有效照明面积不小于 %1 x %2 mm")
        .arg(result.suggestedLightWidthMm, 0, 'f', 1)
        .arg(result.suggestedLightHeightMm, 0, 'f', 1));
    if (light.activeWidthMm > 0.0 && light.activeHeightMm > 0.0) {
        if (result.lightCoverageMarginPercent < 0.0) {
            result.risks.append(QString::fromUtf8("当前光源有效面积小于需求 FOV"));
        } else if (result.lightCoverageMarginPercent < 10.0) {
            result.risks.append(QString::fromUtf8("当前光源覆盖余量低于 10%"));
        } else {
            result.reasons.append(QString::fromUtf8("当前光源覆盖余量约 %1%")
                .arg(result.lightCoverageMarginPercent, 0, 'f', 0));
        }
    }

    const QString mode = light.mode.toLower();
    const bool strobe = mode.contains(QStringLiteral("strobe"))
        || mode.contains(QStringLiteral("pulse"))
        || mode.contains(QStringLiteral("trigger"))
        || mode.contains(QString::fromUtf8("频闪"))
        || mode.contains(QString::fromUtf8("触发"));
    if (input.request.motionSpeedMmS > 20.0 && !strobe)
        result.risks.append(QString::fromUtf8("高速运动建议使用频闪/触发光源以压低曝光时间"));
    else if (input.request.motionSpeedMmS > 20.0)
        result.reasons.append(QString::fromUtf8("高速运动已选择频闪/触发光源"));

    const bool reflective = input.request.reflective
        || input.request.surfaceType == SurfaceType::ReflectiveMetal
        || input.request.surfaceType == SurfaceType::GlassTransparent;
    if (reflective
        && light.lightType != LightType::Coaxial
        && light.lightType != LightType::Dome
        && !(input.request.detectionType == DetectionType::DefectInspection && light.isDarkFieldLike()))
        result.risks.append(QString::fromUtf8("反光/玻璃表面建议优先同轴光或穹顶光"));
    if (input.telecentricMode && input.request.detectionType == DetectionType::Measurement
        && light.lightType != LightType::TelecentricBacklight)
        result.risks.append(QString::fromUtf8("远心测量建议优先远心平行背光"));

    return result;
}

QVector<CameraCalculationEstimate> CalculationAssistant::estimateCameras(const SelectionRequest &request,
                                                                         const QVector<CameraSpec> &cameras,
                                                                         int limit)
{
    const RequirementEstimate requirement = estimateRequirement(request);
    QVector<CameraCalculationEstimate> estimates;
    estimates.reserve(cameras.size());

    const double fovW = requirement.requiredFovWidthMm;
    const double fovH = requirement.requiredFovHeightMm;
    for (const CameraSpec &camera : cameras) {
        if (camera.resolutionX <= 0 || camera.resolutionY <= 0 || camera.pixelSizeUm <= 0.0)
            continue;

        CameraCalculationEstimate estimate;
        estimate.camera = camera;
        estimate.objectPixelSizeUm = objectPixelUm(request, camera);
        estimate.meetsSampling = estimate.objectPixelSizeUm <= requirement.targetObjectPixelUm;
        estimate.meetsFps = camera.maxFps >= request.requiredFps;
        estimate.globalShutterRecommended = request.motionSpeedMmS > 20.0 && !camera.isGlobalShutter();
        estimate.fixedFocalLengthMm = estimatedFixedFocalLengthMm(request, camera);
        estimate.sensorDiagonalMm = camera.sensorDiagonalMm();
        estimate.telecentricPmagMin = camera.pixelSizeUm / qMax(0.001, requirement.targetObjectPixelUm);
        estimate.telecentricPmagMax = qMin(camera.sensorWidthMm() / qMax(0.001, fovW),
                                           camera.sensorHeightMm() / qMax(0.001, fovH));
        estimate.telecentricFeasible = estimate.telecentricPmagMax > 0.0
            && estimate.telecentricPmagMin <= estimate.telecentricPmagMax;
        estimate.bandwidthRequiredMBps = SelectionEngine::bandwidthRequiredMBps(camera, qMax(1.0, request.requiredFps));
        estimate.interfaceCapacityMBps = SelectionEngine::interfaceCapacityMBps(camera);
        estimate.bandwidthUtilizationPercent = estimate.interfaceCapacityMBps > 0.0
            ? estimate.bandwidthRequiredMBps / estimate.interfaceCapacityMBps * 100.0
            : 0.0;
        estimate.meetsBandwidth = estimate.interfaceCapacityMBps > 0.0
            && estimate.bandwidthRequiredMBps <= estimate.interfaceCapacityMBps;
        estimate.score = cameraScore(request, requirement, estimate);
        estimates.append(estimate);
    }

    std::sort(estimates.begin(), estimates.end(), [](const CameraCalculationEstimate &a, const CameraCalculationEstimate &b) {
        return a.score > b.score;
    });
    if (limit > 0 && estimates.size() > limit)
        estimates.resize(limit);
    return estimates;
}

QVector<LensCalculationEstimate> CalculationAssistant::estimateLenses(const SelectionRequest &request,
                                                                      const CameraSpec &camera,
                                                                      const QVector<LensSpec> &lenses,
                                                                      int limit)
{
    const RequirementEstimate requirement = estimateRequirement(request);
    QVector<LensCalculationEstimate> estimates;
    estimates.reserve(lenses.size());

    const double sensorW = camera.sensorWidthMm();
    const double sensorH = camera.sensorHeightMm();
    const double targetFocal = estimatedFixedFocalLengthMm(request, camera);
    for (const LensSpec &lens : lenses) {
        if (!request.allowTelecentric && lens.isTelecentric())
            continue;
        if (sensorW <= 0.0 || sensorH <= 0.0 || camera.resolutionX <= 0 || camera.resolutionY <= 0)
            continue;

        LensCalculationEstimate estimate;
        estimate.lens = lens;
        estimate.score = 50.0;
        estimate.estimatedFocalLengthMm = targetFocal;

        if (lens.isTelecentric()) {
            if (lens.pmag <= 0.0)
                continue;
            estimate.effectiveFovWidthMm = sensorW / lens.pmag;
            estimate.effectiveFovHeightMm = sensorH / lens.pmag;
            estimate.objectPixelSizeUm = camera.pixelSizeUm / lens.pmag;
            estimate.magnification = lens.pmag;
            estimate.estimatedDofMm = lens.dofMm;
            estimate.distortionErrorUm = SelectionEngine::distortionErrorUm(lens, estimate.effectiveFovWidthMm, estimate.effectiveFovHeightMm);
            estimate.formulaSummary = QString::fromUtf8("FOV = SensorSize / PMAG\357\274\214ObjectPixel = PixelSize / PMAG");
            const bool hasNominalWd = lens.nominalWorkingDistanceMm > 0.0;
            const bool hasWdTolerance = lens.workingDistanceToleranceMm > 0.0;
            estimate.workingDistanceOk = hasNominalWd
                && hasWdTolerance
                && qAbs(request.workingDistanceMm - lens.nominalWorkingDistanceMm) <= lens.workingDistanceToleranceMm;
            estimate.dofOk = request.heightVariationMm <= 0.0
                || (lens.dofMm > 0.0 && lens.dofMm >= request.heightVariationMm * 1.5);
            estimate.residualTelecentricErrorUm = request.heightVariationMm
                * qTan(qDegreesToRadians(lens.telecentricityDeg)) * 1000.0;

            if (telecentricPreferred(request)) {
                estimate.score += 18.0;
                estimate.reasons.append(QString::fromUtf8("\345\275\223\345\211\215\347\262\276\345\272\246/\351\253\230\345\272\246\346\263\242\345\212\250\351\234\200\346\261\202\344\274\230\345\205\210\350\277\234\345\277\203\351\225\234\345\244\264"));
            }

            if (estimate.workingDistanceOk) {
                estimate.score += 10.0;
            } else if (!hasNominalWd) {
                estimate.score -= 22.0;
                estimate.risks.append(QString::fromUtf8("远心镜头缺少标称 WD，需查 datasheet 复核"));
            } else if (!hasWdTolerance && qAbs(request.workingDistanceMm - lens.nominalWorkingDistanceMm) <= 0.5) {
                estimate.score -= 6.0;
                estimate.risks.append(QString::fromUtf8("远心镜头缺少 WD 容差数据，仅按接近标称 WD 粗略保留"));
            } else if (!hasWdTolerance) {
                estimate.score -= 22.0;
                estimate.risks.append(QString::fromUtf8("当前 WD 偏离远心镜头标称 WD，且缺少 WD 容差数据"));
            } else {
                estimate.score -= 22.0;
                estimate.risks.append(QString::fromUtf8("\346\240\207\347\247\260 WD %1 \344\270\216\345\275\223\345\211\215\345\267\245\344\275\234\350\267\235\347\246\273\345\201\217\345\267\256\350\276\203\345\244\247")
                    .arg(mm(lens.nominalWorkingDistanceMm)));
            }

            if (estimate.dofOk) {
                estimate.score += 8.0;
            } else if (lens.dofMm <= 0.0 && request.heightVariationMm > 0.0) {
                estimate.score -= 10.0;
                estimate.risks.append(QString::fromUtf8("远心镜头缺少 DOF 数据，无法确认是否覆盖高度波动"));
            } else {
                estimate.score -= 14.0;
                estimate.risks.append(QString::fromUtf8("DOF \345\217\257\350\203\275\344\270\215\350\266\263\344\273\245\350\246\206\347\233\226\351\253\230\345\272\246\346\263\242\345\212\250"));
            }

            if (request.measurementToleranceUm > 0.0
                && estimate.residualTelecentricErrorUm > request.measurementToleranceUm) {
                estimate.score -= 10.0;
                estimate.risks.append(QString::fromUtf8("\350\277\234\345\277\203\345\272\246\346\256\213\344\275\231\350\247\206\345\267\256\347\272\246 %1\357\274\214\350\266\205\350\277\207\345\205\201\350\256\270\350\257\257\345\267\256")
                    .arg(um(estimate.residualTelecentricErrorUm)));
            }
        } else {
            if (lens.focalLengthMm <= 0.0)
                continue;
            estimate.effectiveFovWidthMm = sensorW * qMax(1.0, request.workingDistanceMm - lens.focalLengthMm) / lens.focalLengthMm;
            estimate.effectiveFovHeightMm = sensorH * qMax(1.0, request.workingDistanceMm - lens.focalLengthMm) / lens.focalLengthMm;
            estimate.objectPixelSizeUm = qMax(estimate.effectiveFovWidthMm * 1000.0 / camera.resolutionX,
                                             estimate.effectiveFovHeightMm * 1000.0 / camera.resolutionY);
            estimate.magnification = sensorW / qMax(0.001, estimate.effectiveFovWidthMm);
            estimate.estimatedDofMm = SelectionEngine::estimatedFixedLensDofMm(camera, lens, estimate.magnification);
            estimate.distortionErrorUm = SelectionEngine::distortionErrorUm(lens, estimate.effectiveFovWidthMm, estimate.effectiveFovHeightMm);
            estimate.formulaSummary = QString::fromUtf8("M = SensorSize / FOV\357\274\214f \342\211\210 WD x SensorSize / (FOV + SensorSize)");
            estimate.workingDistanceOk = request.workingDistanceMm >= lens.minWorkingDistanceMm;
            estimate.dofOk = request.heightVariationMm <= 0.0
                || (estimate.estimatedDofMm > 0.0 && estimate.estimatedDofMm >= request.heightVariationMm * 1.5);

            if (estimate.workingDistanceOk) {
                estimate.score += 8.0;
            } else {
                estimate.score -= 20.0;
                estimate.risks.append(QString::fromUtf8("\345\275\223\345\211\215 WD \344\275\216\344\272\216\351\225\234\345\244\264\346\234\200\345\260\217\345\267\245\344\275\234\350\267\235\347\246\273"));
            }

            const double focalMismatch = percentDifference(lens.focalLengthMm, targetFocal);
            estimate.score += clamp(14.0 - focalMismatch * 28.0, -18.0, 14.0);
            if (focalMismatch <= 0.25) {
                estimate.reasons.append(QString::fromUtf8("\347\204\246\350\267\235 %1 \346\216\245\350\277\221\347\262\227\347\256\227\347\233\256\346\240\207 %2")
                    .arg(mm(lens.focalLengthMm), mm(targetFocal)));
            } else {
                estimate.risks.append(QString::fromUtf8("\347\204\246\350\267\235 %1 \344\270\216\347\262\227\347\256\227\347\233\256\346\240\207 %2 \345\267\256\345\274\202\350\276\203\345\244\247")
                    .arg(mm(lens.focalLengthMm), mm(targetFocal)));
            }

            if (telecentricPreferred(request)) {
                estimate.score -= 10.0;
                estimate.risks.append(QString::fromUtf8("\351\253\230\347\262\276\345\272\246/\351\253\230\345\272\246\346\263\242\345\212\250\345\234\272\346\231\257\346\231\256\351\200\232\351\225\234\345\244\264\345\255\230\345\234\250\351\200\217\350\247\206\350\257\257\345\267\256"));
            }

            if (request.heightVariationMm > 0.0) {
                if (estimate.dofOk) {
                    estimate.score += 8.0;
                    estimate.reasons.append(QString::fromUtf8("估算 DOF %1 覆盖高度波动")
                        .arg(mm(estimate.estimatedDofMm, 2)));
                } else if (estimate.estimatedDofMm > 0.0) {
                    estimate.score -= 14.0;
                    estimate.risks.append(QString::fromUtf8("估算 DOF %1 可能不足")
                        .arg(mm(estimate.estimatedDofMm, 2)));
                } else {
                    estimate.score -= 6.0;
                    estimate.risks.append(QString::fromUtf8("缺少 F/# 或 DOF 数据，景深需要确认"));
                }
            }
        }

        appendCommonLensJudgement(request, requirement, camera, &estimate);

        if (lens.megapixelRating > 0.0 && lens.megapixelRating < camera.megapixels()) {
            estimate.score -= 10.0;
            estimate.risks.append(QString::fromUtf8("\351\225\234\345\244\264 MP \346\240\207\347\247\260\344\275\216\344\272\216\347\233\270\346\234\272\345\203\217\347\264\240\357\274\214\351\234\200\347\241\256\350\256\244 MTF"));
        }
        if (lens.recommendedMinPixelUm > 0.0 && lens.recommendedMinPixelUm > camera.pixelSizeUm) {
            estimate.score -= 8.0;
            estimate.risks.append(QString::fromUtf8("\351\225\234\345\244\264\346\216\250\350\215\220\346\234\200\345\260\217\345\203\217\345\205\203\345\244\247\344\272\216\347\233\270\346\234\272\345\203\217\345\205\203"));
        }

        estimates.append(estimate);
    }

    std::sort(estimates.begin(), estimates.end(), [](const LensCalculationEstimate &a, const LensCalculationEstimate &b) {
        return a.score > b.score;
    });
    if (limit > 0 && estimates.size() > limit)
        estimates.resize(limit);
    return estimates;
}

double CalculationAssistant::estimatedFixedFocalLengthMm(const SelectionRequest &request, const CameraSpec &camera)
{
    const double fovW = SelectionEngine::requiredFovWidth(request);
    const double fovH = SelectionEngine::requiredFovHeight(request);
    const double sensorW = camera.sensorWidthMm();
    const double sensorH = camera.sensorHeightMm();
    if (fovW <= 0.0 || fovH <= 0.0 || sensorW <= 0.0 || sensorH <= 0.0)
        return 0.0;

    const bool widthLimited = (fovW / fovH) >= (sensorW / sensorH);
    const double fov = widthLimited ? fovW : fovH;
    const double sensor = widthLimited ? sensorW : sensorH;
    return request.workingDistanceMm * sensor / (fov + sensor);
}

bool CalculationAssistant::telecentricPreferred(const SelectionRequest &request)
{
    if (request.detectionType != DetectionType::Measurement)
        return false;

    return (request.measurementToleranceUm > 0.0
            && request.measurementToleranceUm <= 20.0)
        || request.heightVariationMm >= 1.0
        || SelectionEngine::targetObjectPixelUm(request) <= 5.0;
}
