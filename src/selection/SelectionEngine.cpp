#include "selection/SelectionEngine.h"

#include <algorithm>
#include <QtMath>

namespace {
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

QString mm(double value)
{
    return QString::number(value, 'f', 2);
}

QString um(double value)
{
    return QString::number(value, 'f', 2);
}

bool isStrobeLight(const LightSpec &light)
{
    const QString mode = light.mode.toLower();
    return mode.contains(QStringLiteral("strobe"))
        || mode.contains(QStringLiteral("trigger"))
        || mode.contains(QStringLiteral("pulse"))
        || mode.contains(QString::fromUtf8("\351\242\221\351\227\252"))
        || mode.contains(QString::fromUtf8("\350\247\246\345\217\221"));
}

void addHardFailure(SelectionResult *result, const QString &reason)
{
    if (!result)
        return;
    result->hardConstraintsPassed = false;
    if (!result->hardFailures.contains(reason))
        result->hardFailures.append(reason);
}

struct PairCandidate
{
    int cameraIndex;
    int lensIndex;
    double score;
    bool hardConstraintsPassed;
};

bool samePairCandidate(const PairCandidate &a, const PairCandidate &b)
{
    return a.cameraIndex == b.cameraIndex && a.lensIndex == b.lensIndex;
}

bool containsPairCandidate(const QVector<PairCandidate> &candidates, const PairCandidate &candidate)
{
    for (const PairCandidate &existing : candidates) {
        if (samePairCandidate(existing, candidate))
            return true;
    }
    return false;
}

bool betterCandidate(const SelectionResult &a, const SelectionResult &b)
{
    if (a.hardConstraintsPassed != b.hardConstraintsPassed)
        return a.hardConstraintsPassed;
    return a.score.score > b.score.score;
}

bool betterPairCandidate(const PairCandidate &a, const PairCandidate &b)
{
    if (a.hardConstraintsPassed != b.hardConstraintsPassed)
        return a.hardConstraintsPassed;
    return a.score > b.score;
}

int fixedFocalReserveCount(int limit)
{
    if (limit <= 1)
        return 0;
    return qMin(3, qMax(1, limit / 5));
}

bool isNearNominalWorkingDistance(double requestWdMm, double nominalWdMm)
{
    return qAbs(requestWdMm - nominalWdMm) <= 0.5;
}
}

#define ADD_DETAIL_REASON(reasonExpr) do { if (includeDetails) result->score.reasons.append(reasonExpr); } while (false)
#define ADD_DETAIL_RISK(riskExpr) do { if (includeDetails) result->score.risks.append(riskExpr); } while (false)
#define ADD_DETAIL_HARD_FAILURE(reasonExpr) do { if (result) { result->hardConstraintsPassed = false; if (includeDetails) addHardFailure(result, reasonExpr); } } while (false)

QVector<SelectionResult> SelectionEngine::select(const SelectionRequest &request,
                                                const QVector<CameraSpec> &cameras,
                                                const QVector<LensSpec> &lenses,
                                                const QVector<LightSpec> &lights,
                                                int limit) const
{
    QVector<LightSpec> bestLights;
    bestLights.reserve(lenses.size());
    for (const LensSpec &lens : lenses) {
        bestLights.append(chooseLight(request, lens, lights, nullptr));
    }

    QVector<PairCandidate> candidates;
    QVector<PairCandidate> fixedFocalCandidates;
    if (limit > 0)
        candidates.reserve(limit);
    const int typePoolLimit = limit > 0 ? qMax(limit, 12) : 0;
    const auto appendCandidate = [](QVector<PairCandidate> *pool, int poolLimit, const PairCandidate &candidate) {
        if (!pool)
            return;
        if (poolLimit <= 0) {
            pool->append(candidate);
            return;
        }
        if (pool->size() < poolLimit) {
            pool->append(candidate);
            return;
        }
        int worstIndex = 0;
        for (int i = 1; i < pool->size(); ++i) {
            if (betterPairCandidate(pool->at(worstIndex), pool->at(i)))
                worstIndex = i;
        }
        if (betterPairCandidate(candidate, pool->at(worstIndex)))
            (*pool)[worstIndex] = candidate;
    };

    for (int cameraIndex = 0; cameraIndex < cameras.size(); ++cameraIndex) {
        const CameraSpec &camera = cameras.at(cameraIndex);
        for (int lensIndex = 0; lensIndex < lenses.size(); ++lensIndex) {
            const LensSpec &lens = lenses.at(lensIndex);
            if (!request.allowTelecentric && lens.isTelecentric())
                continue;
            const LightSpec &light = bestLights.at(lensIndex);
            const SelectionResult quickResult = evaluatePair(request, camera, lens, light, false);
            PairCandidate candidate;
            candidate.cameraIndex = cameraIndex;
            candidate.lensIndex = lensIndex;
            candidate.score = quickResult.score.score;
            candidate.hardConstraintsPassed = quickResult.hardConstraintsPassed;
            appendCandidate(&candidates, limit, candidate);
            if (!lens.isTelecentric())
                appendCandidate(&fixedFocalCandidates, typePoolLimit, candidate);
        }
    }

    std::sort(candidates.begin(), candidates.end(), betterPairCandidate);
    if (limit > 0 && candidates.size() > limit)
        candidates.resize(limit);

    if (limit > 0 && !fixedFocalCandidates.isEmpty()) {
        std::sort(fixedFocalCandidates.begin(), fixedFocalCandidates.end(), betterPairCandidate);
        int fixedFocalCount = 0;
        for (const PairCandidate &candidate : candidates) {
            if (!lenses.at(candidate.lensIndex).isTelecentric())
                ++fixedFocalCount;
        }

        const int reserveCount = fixedFocalReserveCount(limit);
        for (const PairCandidate &fixedCandidate : fixedFocalCandidates) {
            if (fixedFocalCount >= reserveCount)
                break;
            if (!fixedCandidate.hardConstraintsPassed || containsPairCandidate(candidates, fixedCandidate))
                continue;

            if (candidates.size() < limit) {
                candidates.append(fixedCandidate);
                ++fixedFocalCount;
                continue;
            }

            int replacementIndex = -1;
            for (int i = 0; i < candidates.size(); ++i) {
                if (!lenses.at(candidates.at(i).lensIndex).isTelecentric())
                    continue;
                if (replacementIndex < 0 || betterPairCandidate(candidates.at(replacementIndex), candidates.at(i)))
                    replacementIndex = i;
            }
            if (replacementIndex < 0)
                break;

            candidates[replacementIndex] = fixedCandidate;
            ++fixedFocalCount;
        }

        if (fixedFocalCount == 0) {
            for (const PairCandidate &fixedCandidate : fixedFocalCandidates) {
                if (containsPairCandidate(candidates, fixedCandidate))
                    continue;

                if (candidates.size() < limit) {
                    candidates.append(fixedCandidate);
                    ++fixedFocalCount;
                    break;
                }

                int replacementIndex = -1;
                for (int i = 0; i < candidates.size(); ++i) {
                    if (candidates.at(i).hardConstraintsPassed)
                        continue;
                    if (!lenses.at(candidates.at(i).lensIndex).isTelecentric())
                        continue;
                    if (replacementIndex < 0 || betterPairCandidate(candidates.at(replacementIndex), candidates.at(i)))
                        replacementIndex = i;
                }
                if (replacementIndex < 0)
                    break;

                candidates[replacementIndex] = fixedCandidate;
                ++fixedFocalCount;
                break;
            }
        }
        std::sort(candidates.begin(), candidates.end(), betterPairCandidate);
    }

    QVector<SelectionResult> results;
    results.reserve(candidates.size());
    for (const PairCandidate &candidate : candidates) {
        const CameraSpec &camera = cameras.at(candidate.cameraIndex);
        const LensSpec &lens = lenses.at(candidate.lensIndex);
        const LightSpec &light = bestLights.at(candidate.lensIndex);
        results.append(evaluatePair(request, camera, lens, light, true));
    }

    std::sort(results.begin(), results.end(), betterCandidate);

    if (limit > 0 && results.size() > limit)
        results.resize(limit);
    return results;
}

double SelectionEngine::requiredFovWidth(const SelectionRequest &request)
{
    return request.objectWidthMm + request.placementMarginMm * 2.0;
}

double SelectionEngine::requiredFovHeight(const SelectionRequest &request)
{
    return request.objectHeightMm + request.placementMarginMm * 2.0;
}

double SelectionEngine::targetObjectPixelUm(const SelectionRequest &request)
{
    double featurePixels = 3.0;
    double toleranceFactor = 1.0;
    switch (request.detectionType) {
    case DetectionType::Measurement:
        featurePixels = 5.0;
        toleranceFactor = 1.0 / 5.0;
        break;
    case DetectionType::Positioning:
        featurePixels = 4.0;
        toleranceFactor = 1.5;
        break;
    case DetectionType::DefectInspection:
        featurePixels = 3.0;
        toleranceFactor = 2.0;
        break;
    case DetectionType::OcrCode:
        featurePixels = 4.0;
        toleranceFactor = 2.0;
        break;
    }

    double target = 999999.0;
    if (request.minFeatureUm > 0.0)
        target = qMin(target, request.minFeatureUm / featurePixels);
    if (request.measurementToleranceUm > 0.0)
        target = qMin(target, request.measurementToleranceUm * toleranceFactor);
    return qMax(0.5, target);
}

double SelectionEngine::bandwidthRequiredMBps(const CameraSpec &camera, double fps)
{
    return framePayloadMB(camera) * fps;
}

double SelectionEngine::framePayloadMB(const CameraSpec &camera)
{
    const double bitsPerFrame = camera.resolutionX * camera.resolutionY * camera.bitDepth;
    return bitsPerFrame / 8.0 / 1000000.0;
}

double SelectionEngine::storagePerHourGB(const CameraSpec &camera, double fps)
{
    return bandwidthRequiredMBps(camera, fps) * 3600.0 / 1024.0;
}

double SelectionEngine::interfaceCapacityMBps(const CameraSpec &camera)
{
    if (camera.bandwidthMBps > 0.0)
        return camera.bandwidthMBps;

    const QString interfaceName = camera.interfaceType.trimmed().toLower();
    if (interfaceName.contains(QStringLiteral("10gige")))
        return 1000.0;
    if (interfaceName.contains(QStringLiteral("5gige")))
        return 500.0;
    if (interfaceName.contains(QStringLiteral("gige")))
        return 110.0;
    if (interfaceName.contains(QStringLiteral("usb3")))
        return 380.0;
    if (interfaceName.contains(QStringLiteral("cameralink")))
        return 680.0;
    if (interfaceName.contains(QStringLiteral("coaxpress")) || interfaceName.contains(QStringLiteral("cxp")))
        return 1250.0;
    return 0.0;
}

double SelectionEngine::maxExposureUsForOnePixelBlur(const SelectionRequest &request)
{
    if (request.motionSpeedMmS <= 0.0)
        return 0.0;
    const double targetPixelMm = targetObjectPixelUm(request) / 1000.0;
    return targetPixelMm / request.motionSpeedMmS * 1000000.0;
}

double SelectionEngine::estimatedFixedLensDofMm(const CameraSpec &camera,
                                                const LensSpec &lens,
                                                double magnification)
{
    if (lens.dofMm > 0.0)
        return lens.dofMm;
    if (lens.fNumber <= 0.0 || camera.pixelSizeUm <= 0.0 || magnification <= 0.0)
        return 0.0;

    const double circleOfConfusionMm = camera.pixelSizeUm * 2.0 / 1000.0;
    return 2.0 * lens.fNumber * circleOfConfusionMm * (1.0 + magnification)
        / (magnification * magnification);
}

double SelectionEngine::distortionErrorUm(const LensSpec &lens, double fovWidthMm, double fovHeightMm)
{
    if (lens.distortionPercent <= 0.0)
        return 0.0;
    return qMax(fovWidthMm, fovHeightMm) * 1000.0 * lens.distortionPercent / 100.0;
}

double SelectionEngine::lightCoverageMarginPercent(const SelectionRequest &request, const LightSpec &light)
{
    const double requiredW = requiredFovWidth(request);
    const double requiredH = requiredFovHeight(request);
    if (requiredW <= 0.0 || requiredH <= 0.0 || light.activeWidthMm <= 0.0 || light.activeHeightMm <= 0.0)
        return -100.0;
    return (qMin(light.activeWidthMm / requiredW, light.activeHeightMm / requiredH) - 1.0) * 100.0;
}

SelectionResult SelectionEngine::evaluatePair(const SelectionRequest &request,
                                             const CameraSpec &camera,
                                             const LensSpec &lens,
                                             const LightSpec &light,
                                             bool includeDetails) const
{
    SelectionResult result;
    if (includeDetails) {
        result.camera = camera;
        result.lens = lens;
        result.light = light;
    }
    result.requiredFovWidthMm = requiredFovWidth(request);
    result.requiredFovHeightMm = requiredFovHeight(request);
    result.maxExposureUsForOnePixelBlur = maxExposureUsForOnePixelBlur(request);
    result.lightCoverageMarginPercent = lightCoverageMarginPercent(request, light);
    result.score.score = 50.0;
    if (includeDetails) {
        result.schemeTitle = lens.isTelecentric()
            ? QString::fromUtf8("\350\277\234\345\277\203\351\225\234\345\244\264\346\226\271\346\241\210")
            : QString::fromUtf8("\346\231\256\351\200\232\351\225\234\345\244\264\346\226\271\346\241\210");
    }

    scoreCamera(request, camera, &result, includeDetails);
    if (lens.isTelecentric())
        scoreTelecentricLens(request, camera, lens, &result, includeDetails);
    else
        scoreFixedFocalLens(request, camera, lens, &result, includeDetails);

    QStringList lightReasons;
    result.score.score += scoreLight(request, lens, light, includeDetails ? &lightReasons : nullptr);
    if (includeDetails)
        result.score.reasons.append(lightReasons);

    if (result.score.score < 0.0)
        result.score.score = 0.0;
    if (!result.hardConstraintsPassed)
        result.score.score = qMin(result.score.score, 20.0);
    return result;
}

LightSpec SelectionEngine::chooseLight(const SelectionRequest &request,
                                      const LensSpec &lens,
                                      const QVector<LightSpec> &lights,
                                      QStringList *reasons) const
{
    if (lights.isEmpty())
        return LightSpec();

    const LightSpec *best = &lights.first();
    double bestScore = -99999.0;
    QStringList bestReasons;
    for (const LightSpec &light : lights) {
        QStringList localReasons;
        const double score = scoreLight(request, lens, light, reasons ? &localReasons : nullptr);
        if (score > bestScore) {
            bestScore = score;
            best = &light;
            if (reasons)
                bestReasons = localReasons;
        }
    }
    if (reasons)
        *reasons = bestReasons;
    return *best;
}

double SelectionEngine::scoreLight(const SelectionRequest &request,
                                  const LensSpec &lens,
                                  const LightSpec &light,
                                  QStringList *reasons) const
{
    double score = 0.0;
    const bool reflective = request.reflective
        || request.surfaceType == SurfaceType::ReflectiveMetal
        || request.surfaceType == SurfaceType::GlassTransparent;

    if (lens.isTelecentric() && light.lightType == LightType::TelecentricBacklight) {
        score += 16.0;
        if (reasons)
            reasons->append(QString::fromUtf8("\350\277\234\345\277\203\351\225\234\345\244\264\346\220\255\351\205\215\350\277\234\345\277\203\345\271\263\350\241\214\350\203\214\345\205\211\345\217\257\346\217\220\351\253\230\350\275\256\345\273\223\346\265\213\351\207\217\344\270\200\350\207\264\346\200\247"));
    }
    if (lens.coaxialIllumination && light.lightType == LightType::Coaxial) {
        score += 12.0;
        if (reasons)
            reasons->append(QString::fromUtf8("\351\225\234\345\244\264\346\224\257\346\214\201\345\220\214\350\275\264\347\205\247\346\230\216\357\274\214\351\200\202\345\220\210\344\275\216\345\257\271\346\257\224\350\241\250\351\235\242\347\211\271\345\276\201"));
    }
    if (reflective && (light.lightType == LightType::Coaxial || light.lightType == LightType::Dome)) {
        score += 14.0;
        if (reasons)
            reasons->append(QString::fromUtf8("\345\217\215\345\205\211\350\241\250\351\235\242\344\274\230\345\205\210\345\220\214\350\275\264\345\205\211\346\210\226\347\251\271\351\241\266\345\205\211\344\273\245\351\231\215\344\275\216\347\234\251\345\205\211"));
    }
    if (request.detectionType == DetectionType::Measurement
        && (light.lightType == LightType::Backlight || light.lightType == LightType::TelecentricBacklight)) {
        score += 12.0;
        if (reasons)
            reasons->append(QString::fromUtf8("\345\260\272\345\257\270/\350\276\271\347\274\230\346\265\213\351\207\217\344\274\230\345\205\210\350\203\214\345\205\211\345\275\242\346\210\220\347\250\263\345\256\232\350\275\256\345\273\223"));
    }
    if (request.detectionType == DetectionType::DefectInspection
        && (light.lightType == LightType::Bar || light.lightType == LightType::DarkField)) {
        score += 10.0;
        if (reasons)
            reasons->append(QString::fromUtf8("\344\275\216\350\247\222\345\272\246\346\235\241\345\275\242/\346\232\227\345\234\272\345\205\211\351\200\202\345\220\210\345\210\222\347\227\225\345\222\214\347\273\206\345\260\217\347\274\272\351\231\267"));
    }
    if (request.detectionType == DetectionType::Positioning && light.lightType == LightType::Ring)
        score += 6.0;

    const double coverageMargin = lightCoverageMarginPercent(request, light);
    if (coverageMargin < 0.0) {
        score -= 24.0;
        if (reasons)
            reasons->append(QString::fromUtf8("光源有效照明面积小于需求 FOV，需要确认安装与亮度"));
    } else if (coverageMargin < 10.0) {
        score -= 8.0;
        if (reasons)
            reasons->append(QString::fromUtf8("光源覆盖余量低于 10%，边缘亮度可能不足"));
    } else {
        score += 6.0;
        if (reasons)
            reasons->append(QString::fromUtf8("光源覆盖需求 FOV，覆盖余量约 %1%").arg(coverageMargin, 0, 'f', 0));
    }

    if (request.motionSpeedMmS > 20.0) {
        if (isStrobeLight(light)) {
            score += 12.0;
            if (reasons)
                reasons->append(QString::fromUtf8("高速运动优先频闪光源以压低曝光时间"));
        } else {
            score -= 8.0;
            if (reasons)
                reasons->append(QString::fromUtf8("高速运动场景建议确认频闪能力和曝光时间"));
        }
    }

    if (reflective
        && light.lightType != LightType::Coaxial
        && light.lightType != LightType::Dome
        && light.lightType != LightType::TelecentricBacklight) {
        score -= 6.0;
        if (reasons)
            reasons->append(QString::fromUtf8("反光/透明表面使用当前光型可能需要额外控反光验证"));
    }

    return score;
}

void SelectionEngine::scoreCamera(const SelectionRequest &request,
                                  const CameraSpec &camera,
                                  SelectionResult *result,
                                  bool includeDetails) const
{
    const double fps = qMax(1.0, request.requiredFps);
    result->bandwidthRequiredMBps = bandwidthRequiredMBps(camera, fps);
    result->framePayloadMB = framePayloadMB(camera);
    result->storagePerHourGB = storagePerHourGB(camera, fps);
    result->interfaceCapacityMBps = interfaceCapacityMBps(camera);
    result->bandwidthUtilizationPercent = result->interfaceCapacityMBps > 0.0
        ? result->bandwidthRequiredMBps / result->interfaceCapacityMBps * 100.0
        : 0.0;

    if (request.preferMono && camera.isMono()) {
        result->score.score += 5.0;
        ADD_DETAIL_REASON(QString::fromUtf8("\351\273\221\347\231\275\347\233\270\346\234\272\351\200\232\345\270\270\346\234\211\346\233\264\351\253\230\347\201\265\346\225\217\345\272\246\345\222\214\350\276\271\347\274\230\347\250\263\345\256\232\346\200\247"));
    } else if (request.preferMono && !camera.isMono()) {
        result->score.score -= 8.0;
        ADD_DETAIL_RISK(QString::fromUtf8("\351\234\200\346\261\202\345\201\217\345\220\221\351\273\221\347\231\275\346\265\213\351\207\217\357\274\214\344\275\206\350\257\245\347\233\270\346\234\272\344\270\272\345\275\251\350\211\262\345\236\213\345\217\267"));
    }

    if (camera.maxFps >= request.requiredFps) {
        result->score.score += 8.0;
        ADD_DETAIL_REASON(QString::fromUtf8("\347\233\270\346\234\272\345\270\247\347\216\207\346\273\241\350\266\263\350\212\202\346\213\215\351\234\200\346\261\202"));
    } else {
        ADD_DETAIL_HARD_FAILURE( QString::fromUtf8("相机最大帧率低于需求"));
        result->score.score -= 25.0;
        ADD_DETAIL_RISK(QString::fromUtf8("\347\233\270\346\234\272\346\234\200\345\244\247\345\270\247\347\216\207\344\275\216\344\272\216\351\234\200\346\261\202\350\212\202\346\213\215"));
    }

    if (result->interfaceCapacityMBps <= 0.0) {
        result->score.score -= 6.0;
        ADD_DETAIL_RISK(QString::fromUtf8("缺少接口带宽数据，需按相机手册确认吞吐余量"));
    } else if (result->bandwidthUtilizationPercent <= 70.0) {
        result->score.score += 8.0;
        ADD_DETAIL_REASON(QString::fromUtf8("接口带宽利用率约 %1%，连续采集余量充足")
            .arg(result->bandwidthUtilizationPercent, 0, 'f', 0));
    } else if (result->bandwidthUtilizationPercent <= 90.0) {
        result->score.score += 3.0;
        ADD_DETAIL_REASON(QString::fromUtf8("接口带宽利用率约 %1%，建议保留触发和协议开销余量")
            .arg(result->bandwidthUtilizationPercent, 0, 'f', 0));
    } else if (result->bandwidthUtilizationPercent <= 100.0) {
        result->score.score -= 8.0;
        ADD_DETAIL_RISK(QString::fromUtf8("接口带宽利用率约 %1%，接近上限")
            .arg(result->bandwidthUtilizationPercent, 0, 'f', 0));
    } else {
        ADD_DETAIL_HARD_FAILURE( QString::fromUtf8("接口带宽不足"));
        result->score.score -= 24.0;
        ADD_DETAIL_RISK(QString::fromUtf8("按分辨率/bit depth/fps 估算的带宽 %1 MB/s 超过接口余量 %2 MB/s")
            .arg(result->bandwidthRequiredMBps, 0, 'f', 1)
            .arg(result->interfaceCapacityMBps, 0, 'f', 1));
    }

    if (result->storagePerHourGB > 300.0) {
        result->score.score -= 8.0;
        ADD_DETAIL_RISK(QString::fromUtf8("连续原始图像存储约 %1 GB/h，需确认硬盘、缓存和压缩策略")
            .arg(result->storagePerHourGB, 0, 'f', 0));
    } else if (result->storagePerHourGB > 0.0) {
        ADD_DETAIL_REASON(QString::fromUtf8("原始图像数据量约 %1 GB/h")
            .arg(result->storagePerHourGB, 0, 'f', 0));
    }

    if (request.motionSpeedMmS > 20.0) {
        if (camera.isGlobalShutter()) {
            result->score.score += 14.0;
            ADD_DETAIL_REASON(QString::fromUtf8("\351\253\230\351\200\237\350\277\220\345\212\250\347\233\256\346\240\207\344\274\230\345\205\210\345\205\250\345\261\200\345\277\253\351\227\250"));
        } else {
            result->score.score -= 26.0;
            ADD_DETAIL_RISK(QString::fromUtf8("\351\253\230\351\200\237\350\277\220\345\212\250\345\234\272\346\231\257\344\275\277\347\224\250\345\215\267\345\270\230\345\277\253\351\227\250\345\255\230\345\234\250\345\275\242\345\217\230\351\243\216\351\231\251"));
        }
    }
}

void SelectionEngine::scoreFixedFocalLens(const SelectionRequest &request,
                                          const CameraSpec &camera,
                                          const LensSpec &lens,
                                          SelectionResult *result,
                                          bool includeDetails) const
{
    if (lens.focalLengthMm <= 0.0) {
        if (includeDetails)
            result->formulaSummary = QString::fromUtf8("普通镜头：焦距无效，无法估算 FOV");
        result->score.score -= 60.0;
        ADD_DETAIL_HARD_FAILURE( QString::fromUtf8("普通镜头焦距无效"));
        ADD_DETAIL_RISK(QString::fromUtf8("普通镜头焦距必须大于 0"));
        return;
    }

    const double fovW = camera.sensorWidthMm() * qMax(1.0, request.workingDistanceMm - lens.focalLengthMm) / lens.focalLengthMm;
    const double fovH = camera.sensorHeightMm() * qMax(1.0, request.workingDistanceMm - lens.focalLengthMm) / lens.focalLengthMm;
    result->effectiveFovWidthMm = fovW;
    result->effectiveFovHeightMm = fovH;
    result->magnification = camera.sensorWidthMm() / qMax(0.001, fovW);
    result->estimatedFocalLengthMm = request.workingDistanceMm * camera.sensorWidthMm()
        / (result->requiredFovWidthMm + camera.sensorWidthMm());
    result->objectPixelSizeUm = qMax(fovW * 1000.0 / camera.resolutionX,
                                     fovH * 1000.0 / camera.resolutionY);
    result->estimatedDofMm = estimatedFixedLensDofMm(camera, lens, result->magnification);
    result->distortionErrorUm = distortionErrorUm(lens, fovW, fovH);
    if (includeDetails)
        result->formulaSummary = QString::fromUtf8("\346\231\256\351\200\232\351\225\234\345\244\264\357\274\232M = SensorSize / FOV\357\274\214f \342\211\210 WD \303\227 SensorSize / (FOV + SensorSize)");

    const double targetPixel = targetObjectPixelUm(request);
    if (fovW >= result->requiredFovWidthMm && fovH >= result->requiredFovHeightMm) {
        result->score.score += 10.0;
        ADD_DETAIL_REASON(QString::fromUtf8("\346\231\256\351\200\232\351\225\234\345\244\264\350\247\206\351\207\216\350\246\206\347\233\226\345\267\245\344\273\266\345\222\214\345\256\232\344\275\215\344\275\231\351\207\217"));
    } else {
        ADD_DETAIL_HARD_FAILURE( QString::fromUtf8("普通镜头 FOV 不覆盖需求"));
        result->score.score -= 40.0;
        ADD_DETAIL_RISK(QString::fromUtf8("\346\231\256\351\200\232\351\225\234\345\244\264\345\234\250\345\275\223\345\211\215\345\267\245\344\275\234\350\267\235\347\246\273\344\270\213\350\247\206\351\207\216\344\270\215\350\266\263"));
    }

    if (result->objectPixelSizeUm <= targetPixel) {
        result->score.score += 18.0;
        ADD_DETAIL_REASON(QString::fromUtf8("\347\211\251\346\226\271\345\203\217\347\264\240\347\262\276\345\272\246 %1 um \346\273\241\350\266\263\347\233\256\346\240\207 %2 um")
            .arg(um(result->objectPixelSizeUm), um(targetPixel)));
    } else {
        ADD_DETAIL_HARD_FAILURE( QString::fromUtf8("物方像素粗于目标"));
        const double penalty = clamp((result->objectPixelSizeUm / targetPixel - 1.0) * 14.0, 8.0, 40.0);
        result->score.score -= penalty;
        ADD_DETAIL_RISK(QString::fromUtf8("\347\211\251\346\226\271\345\203\217\347\264\240\347\262\276\345\272\246 %1 um \347\262\227\344\272\216\347\233\256\346\240\207 %2 um")
            .arg(um(result->objectPixelSizeUm), um(targetPixel)));
    }

    if (lens.imageCircleMm >= camera.sensorDiagonalMm()) {
        result->score.score += 8.0;
    } else {
        ADD_DETAIL_HARD_FAILURE( QString::fromUtf8("镜头像圈小于相机传感器"));
        result->score.score -= 35.0;
        ADD_DETAIL_RISK(QString::fromUtf8("\351\225\234\345\244\264\345\203\217\345\234\206 %1 mm \345\260\217\344\272\216\347\233\270\346\234\272\344\274\240\346\204\237\345\231\250\345\257\271\350\247\222\347\272\277 %2 mm")
            .arg(lens.imageCircleMm)
            .arg(camera.sensorDiagonalMm()));
    }

    if (mountsCompatible(camera.lensMount, lens.lensMount)) {
        result->score.score += 5.0;
    } else {
        ADD_DETAIL_HARD_FAILURE( QString::fromUtf8("相机与镜头接口不匹配"));
        result->score.score -= 25.0;
        ADD_DETAIL_RISK(QString::fromUtf8("\347\233\270\346\234\272\346\216\245\345\217\243 %1 \344\270\216\351\225\234\345\244\264\346\216\245\345\217\243 %2 \344\270\215\345\214\271\351\205\215")
            .arg(camera.lensMount, lens.lensMount));
    }

    if (request.workingDistanceMm >= lens.minWorkingDistanceMm) {
        result->score.score += 5.0;
    } else {
        ADD_DETAIL_HARD_FAILURE( QString::fromUtf8("工作距离小于镜头最小工作距离"));
        result->score.score -= 20.0;
        ADD_DETAIL_RISK(QString::fromUtf8("\345\267\245\344\275\234\350\267\235\347\246\273\345\260\217\344\272\216\351\225\234\345\244\264\346\234\200\345\260\217\345\267\245\344\275\234\350\267\235\347\246\273"));
    }

    if (request.heightVariationMm > 0.0) {
        if (result->estimatedDofMm > 0.0 && result->estimatedDofMm >= request.heightVariationMm * 1.5) {
            result->score.score += 8.0;
            ADD_DETAIL_REASON(QString::fromUtf8("估算 DOF %1 mm 覆盖高度波动").arg(mm(result->estimatedDofMm)));
        } else if (result->estimatedDofMm > 0.0) {
            result->score.score -= 16.0;
            ADD_DETAIL_RISK(QString::fromUtf8("估算 DOF %1 mm 可能不足以覆盖高度波动").arg(mm(result->estimatedDofMm)));
        } else {
            result->score.score -= 6.0;
            ADD_DETAIL_RISK(QString::fromUtf8("缺少 F/# 或 DOF 数据，普通镜头景深需要确认"));
        }
    }

    if (request.detectionType == DetectionType::Measurement && result->distortionErrorUm > request.measurementToleranceUm) {
        result->score.score -= 18.0;
        ADD_DETAIL_RISK(QString::fromUtf8("按未标定 FOV 边缘粗估畸变误差约 %1 um，高于允许误差，需按厂商畸变曲线和标定板复核").arg(um(result->distortionErrorUm)));
    } else if (result->distortionErrorUm > 0.0) {
        ADD_DETAIL_REASON(QString::fromUtf8("按未标定 FOV 边缘粗估畸变误差约 %1 um，最终测量需标定复核").arg(um(result->distortionErrorUm)));
    }

    if (lens.megapixelRating > 0.0) {
        result->lensMegapixelUtilizationPercent = camera.megapixels() / lens.megapixelRating * 100.0;
        if (lens.megapixelRating < camera.megapixels()) {
            result->score.score -= 12.0;
            ADD_DETAIL_RISK(QString::fromUtf8("\351\225\234\345\244\264\346\240\207\347\247\260 MP \344\275\216\344\272\216\347\233\270\346\234\272\345\203\217\347\264\240\357\274\214\345\217\257\350\203\275\346\227\240\346\263\225\345\226\202\346\273\241\344\274\240\346\204\237\345\231\250"));
        } else if (result->lensMegapixelUtilizationPercent > 85.0) {
            result->score.score -= 3.0;
            ADD_DETAIL_RISK(QString::fromUtf8("镜头 MP 余量较小，利用率约 %1%")
                .arg(result->lensMegapixelUtilizationPercent, 0, 'f', 0));
        } else {
            result->score.score += 5.0;
            ADD_DETAIL_REASON(QString::fromUtf8("镜头标称 MP 覆盖相机，利用率约 %1%，仍需按 MTF 曲线复核边缘对比度")
                .arg(result->lensMegapixelUtilizationPercent, 0, 'f', 0));
        }
    } else {
        result->score.score -= 4.0;
        ADD_DETAIL_RISK(QString::fromUtf8("镜头缺少标称 MP 数据，需按厂家 MTF 曲线确认分辨率余量"));
    }

    if (lens.recommendedMinPixelUm > 0.0 && lens.recommendedMinPixelUm > camera.pixelSizeUm) {
        result->score.score -= 10.0;
        ADD_DETAIL_RISK(QString::fromUtf8("\351\225\234\345\244\264\346\216\250\350\215\220\346\234\200\345\260\217\345\203\217\345\205\203\345\244\247\344\272\216\347\233\270\346\234\272\345\203\217\345\205\203\357\274\214\351\234\200\347\241\256\350\256\244 MTF"));
    }

    if (measurementNeedsTelecentric(request)) {
        result->score.score -= 24.0;
        ADD_DETAIL_RISK(QString::fromUtf8("\351\253\230\347\262\276\345\272\246\346\265\213\351\207\217\346\210\226\351\253\230\345\272\246\346\263\242\345\212\250\345\234\272\346\231\257\357\274\214\346\231\256\351\200\232\351\225\234\345\244\264\345\255\230\345\234\250\351\200\217\350\247\206\350\257\257\345\267\256"));
    } else {
        result->score.score += 10.0;
        ADD_DETAIL_REASON(QString::fromUtf8("\344\275\216/\344\270\255\347\262\276\345\272\246\346\210\226\345\244\247\350\247\206\351\207\216\344\273\273\345\212\241\344\275\277\347\224\250\346\231\256\351\200\232\351\225\234\345\244\264\346\210\220\346\234\254\345\222\214\345\256\211\350\243\205\346\233\264\345\217\213\345\245\275"));
    }

    if (result->requiredFovWidthMm > 150.0 || result->requiredFovHeightMm > 150.0) {
        result->score.score += 8.0;
        ADD_DETAIL_REASON(QString::fromUtf8("\345\244\247\350\247\206\351\207\216\344\273\273\345\212\241\344\274\230\345\205\210\346\231\256\351\200\232\345\267\245\344\270\232\351\225\234\345\244\264"));
    }

    const double focalMismatch = percentDifference(lens.focalLengthMm, result->estimatedFocalLengthMm);
    result->score.score += clamp(12.0 - focalMismatch * 24.0, -16.0, 12.0);
}

void SelectionEngine::scoreTelecentricLens(const SelectionRequest &request,
                                           const CameraSpec &camera,
                                           const LensSpec &lens,
                                           SelectionResult *result,
                                           bool includeDetails) const
{
    if (lens.pmag <= 0.0) {
        if (includeDetails)
            result->formulaSummary = QString::fromUtf8("远心镜头：PMAG 无效，无法估算 FOV");
        result->score.score -= 60.0;
        ADD_DETAIL_HARD_FAILURE( QString::fromUtf8("远心倍率 PMAG 无效"));
        ADD_DETAIL_RISK(QString::fromUtf8("远心倍率 PMAG 必须大于 0"));
        return;
    }

    result->effectiveFovWidthMm = camera.sensorWidthMm() / lens.pmag;
    result->effectiveFovHeightMm = camera.sensorHeightMm() / lens.pmag;
    result->magnification = lens.pmag;
    result->objectPixelSizeUm = camera.pixelSizeUm / lens.pmag;
    result->estimatedFocalLengthMm = 0.0;
    result->estimatedDofMm = lens.dofMm;
    result->distortionErrorUm = distortionErrorUm(lens, result->effectiveFovWidthMm, result->effectiveFovHeightMm);
    if (includeDetails)
        result->formulaSummary = QString::fromUtf8("\350\277\234\345\277\203\351\225\234\345\244\264\357\274\232FOV = SensorSize / PMAG\357\274\214ObjectPixel = PixelSize / PMAG");

    const double targetPixel = targetObjectPixelUm(request);
    if (result->effectiveFovWidthMm >= result->requiredFovWidthMm
        && result->effectiveFovHeightMm >= result->requiredFovHeightMm) {
        result->score.score += 16.0;
        ADD_DETAIL_REASON(QString::fromUtf8("\350\277\234\345\277\203 FOV %1 x %2 mm \350\246\206\347\233\226\351\234\200\346\261\202")
            .arg(mm(result->effectiveFovWidthMm), mm(result->effectiveFovHeightMm)));
    } else {
        ADD_DETAIL_HARD_FAILURE( QString::fromUtf8("远心镜头 FOV 不覆盖需求"));
        result->score.score -= 45.0;
        ADD_DETAIL_RISK(QString::fromUtf8("\350\277\234\345\277\203\351\225\234\345\244\264\345\233\272\345\256\232\345\200\215\347\216\207\344\270\213 FOV \344\270\215\350\266\263\357\274\214\351\234\200\346\233\264\344\275\216\345\200\215\347\216\207\346\210\226\346\233\264\345\244\247\351\235\266\351\235\242"));
    }

    if (result->objectPixelSizeUm <= targetPixel) {
        result->score.score += 20.0;
        ADD_DETAIL_REASON(QString::fromUtf8("\350\277\234\345\277\203\347\211\251\346\226\271\345\203\217\347\264\240 %1 um \346\273\241\350\266\263\347\233\256\346\240\207 %2 um")
            .arg(um(result->objectPixelSizeUm), um(targetPixel)));
    } else {
        ADD_DETAIL_HARD_FAILURE( QString::fromUtf8("远心物方像素粗于目标"));
        result->score.score -= clamp((result->objectPixelSizeUm / targetPixel - 1.0) * 14.0, 8.0, 38.0);
        ADD_DETAIL_RISK(QString::fromUtf8("\350\277\234\345\277\203\347\211\251\346\226\271\345\203\217\347\264\240 %1 um \347\262\227\344\272\216\347\233\256\346\240\207 %2 um")
            .arg(um(result->objectPixelSizeUm), um(targetPixel)));
    }

    const double allowedDiag = lens.maxSensorDiagonalMm > 0.0 ? lens.maxSensorDiagonalMm : lens.imageCircleMm;
    if (allowedDiag >= camera.sensorDiagonalMm() && lens.imageCircleMm >= camera.sensorDiagonalMm()) {
        result->score.score += 10.0;
    } else {
        ADD_DETAIL_HARD_FAILURE( QString::fromUtf8("远心镜头像圈/靶面小于相机传感器"));
        result->score.score -= 40.0;
        ADD_DETAIL_RISK(QString::fromUtf8("\350\277\234\345\277\203\351\225\234\345\244\264\345\203\217\345\234\206/\346\234\200\345\244\247\351\235\266\351\235\242\345\260\217\344\272\216\347\233\270\346\234\272\344\274\240\346\204\237\345\231\250\357\274\214\344\274\232\344\272\247\347\224\237\346\232\227\350\247\222\346\210\226\350\276\271\347\274\230\351\200\200\345\214\226"));
    }

    if (mountsCompatible(camera.lensMount, lens.lensMount)) {
        result->score.score += 5.0;
    } else {
        ADD_DETAIL_HARD_FAILURE( QString::fromUtf8("相机与远心镜头接口不匹配"));
        result->score.score -= 25.0;
        ADD_DETAIL_RISK(QString::fromUtf8("\347\233\270\346\234\272\346\216\245\345\217\243 %1 \344\270\216\350\277\234\345\277\203\351\225\234\345\244\264\346\216\245\345\217\243 %2 \344\270\215\345\214\271\351\205\215")
            .arg(camera.lensMount, lens.lensMount));
    }

    if (lens.nominalWorkingDistanceMm <= 0.0) {
        ADD_DETAIL_HARD_FAILURE( QString::fromUtf8("远心镜头缺少标称 WD"));
        result->score.score -= 24.0;
        ADD_DETAIL_RISK(QString::fromUtf8("远心镜头必须按厂商标称工作距离安装，当前资料缺少 nominal WD"));
    } else if (lens.workingDistanceToleranceMm > 0.0) {
        if (qAbs(request.workingDistanceMm - lens.nominalWorkingDistanceMm) <= lens.workingDistanceToleranceMm) {
            result->score.score += 12.0;
            ADD_DETAIL_REASON(QString::fromUtf8("工作距离落在远心镜头标称 WD 容差内"));
        } else {
            ADD_DETAIL_HARD_FAILURE( QString::fromUtf8("工作距离偏离远心镜头标称 WD 容差"));
            result->score.score -= 24.0;
            ADD_DETAIL_RISK(QString::fromUtf8("远心镜头通常只在标称 WD/远心范围内工作，当前 WD 偏差超过资料容差"));
        }
    } else if (isNearNominalWorkingDistance(request.workingDistanceMm, lens.nominalWorkingDistanceMm)) {
        result->score.score -= 4.0;
        ADD_DETAIL_RISK(QString::fromUtf8("远心镜头缺少 WD 容差数据，仅按接近标称 WD 粗略保留，需查 datasheet 复核"));
    } else {
        ADD_DETAIL_HARD_FAILURE( QString::fromUtf8("工作距离偏离远心镜头标称 WD 且缺少 WD 容差"));
        result->score.score -= 24.0;
        ADD_DETAIL_RISK(QString::fromUtf8("远心镜头缺少 WD 容差数据，当前 WD 又不接近标称 WD，不能直接判定可安装"));
    }

    if (lens.megapixelRating > 0.0) {
        result->lensMegapixelUtilizationPercent = camera.megapixels() / lens.megapixelRating * 100.0;
        if (lens.megapixelRating < camera.megapixels()) {
            result->score.score -= 12.0;
            ADD_DETAIL_RISK(QString::fromUtf8("镜头标称 MP 低于相机像素，需确认远心镜头分辨率/MTF"));
        } else {
            result->score.score += 5.0;
            ADD_DETAIL_REASON(QString::fromUtf8("镜头标称 MP 覆盖相机，利用率约 %1%，仍需按 MTF 曲线复核边缘对比度")
                .arg(result->lensMegapixelUtilizationPercent, 0, 'f', 0));
        }
    }

    if (lens.recommendedMinPixelUm > 0.0 && lens.recommendedMinPixelUm <= camera.pixelSizeUm) {
        result->score.score += 6.0;
        ADD_DETAIL_REASON(QString::fromUtf8("相机像元不小于镜头建议最小像元"));
    } else if (lens.recommendedMinPixelUm > camera.pixelSizeUm) {
        result->score.score -= 12.0;
        ADD_DETAIL_RISK(QString::fromUtf8("\347\233\270\346\234\272\345\203\217\345\205\203\345\260\217\344\272\216\351\225\234\345\244\264\346\216\250\350\215\220\346\234\200\345\260\217\345\203\217\345\205\203\357\274\214\351\234\200\347\241\256\350\256\244\351\225\234\345\244\264\345\210\206\350\276\250\347\216\207/MTF"));
    }

    if (request.heightVariationMm > 0.0) {
        if (lens.dofMm > 0.0 && lens.dofMm >= request.heightVariationMm * 1.5) {
            result->score.score += 10.0;
            ADD_DETAIL_REASON(QString::fromUtf8("远心 DOF 覆盖高度波动"));
        } else if (lens.dofMm > 0.0) {
            result->score.score -= 14.0;
            ADD_DETAIL_RISK(QString::fromUtf8("远心 DOF 可能不足，需要确认光圈和景深"));
        } else {
            result->score.score -= 10.0;
            ADD_DETAIL_RISK(QString::fromUtf8("远心镜头缺少 DOF 数据，无法确认是否覆盖高度波动"));
        }
    }

    if (request.detectionType == DetectionType::Measurement && result->distortionErrorUm > request.measurementToleranceUm) {
        result->score.score -= 12.0;
        ADD_DETAIL_RISK(QString::fromUtf8("按未标定 FOV 边缘粗估畸变误差约 %1 um，高于允许误差，需按厂商畸变曲线和标定板复核").arg(um(result->distortionErrorUm)));
    } else if (result->distortionErrorUm > 0.0) {
        ADD_DETAIL_REASON(QString::fromUtf8("按未标定 FOV 边缘粗估畸变误差约 %1 um，最终测量需标定复核").arg(um(result->distortionErrorUm)));
    }

    result->residualTelecentricErrorUm = request.heightVariationMm * qTan(qDegreesToRadians(lens.telecentricityDeg)) * 1000.0;
    if (request.measurementToleranceUm > 0.0
        && result->residualTelecentricErrorUm > request.measurementToleranceUm) {
        result->score.score -= 12.0;
        ADD_DETAIL_RISK(QString::fromUtf8("\346\214\211\350\277\234\345\277\203\345\272\246\344\274\260\347\256\227\347\232\204\346\256\213\344\275\231\350\247\206\345\267\256\347\272\246 %1 um\357\274\214\351\253\230\344\272\216\345\205\201\350\256\270\350\257\257\345\267\256")
            .arg(um(result->residualTelecentricErrorUm)));
    } else {
        result->score.score += 5.0;
    }

    if (lens.lensType == LensType::BiTelecentric) {
        result->score.score += 10.0;
        ADD_DETAIL_REASON(QString::fromUtf8("\345\217\214\350\277\234\345\277\203\347\273\223\346\236\204\346\234\211\346\233\264\345\245\275\347\232\204\345\200\215\347\216\207\344\270\200\350\207\264\346\200\247"));
    }

    if (measurementNeedsTelecentric(request)) {
        result->score.score += 14.0;
        ADD_DETAIL_REASON(QString::fromUtf8("\351\253\230\347\262\276\345\272\246\346\265\213\351\207\217/\351\253\230\345\272\246\346\263\242\345\212\250\344\274\230\345\205\210\350\277\234\345\277\203\351\225\234\345\244\264\344\273\245\351\231\215\344\275\216\351\200\217\350\247\206\350\257\257\345\267\256"));
    }

    if (result->requiredFovWidthMm > 150.0 || result->requiredFovHeightMm > 150.0) {
        const bool lowPrecisionInspection = !measurementNeedsTelecentric(request)
            && request.detectionType == DetectionType::DefectInspection;
        result->score.score -= lowPrecisionInspection ? 45.0 : 18.0;
        ADD_DETAIL_RISK(QString::fromUtf8("\345\244\247\350\247\206\351\207\216\350\277\234\345\277\203\351\225\234\345\244\264\351\200\232\345\270\270\344\275\223\347\247\257\343\200\201\351\207\215\351\207\217\345\222\214\346\210\220\346\234\254\350\276\203\351\253\230"));
    }
}

#undef ADD_DETAIL_REASON
#undef ADD_DETAIL_RISK
#undef ADD_DETAIL_HARD_FAILURE

bool SelectionEngine::measurementNeedsTelecentric(const SelectionRequest &request)
{
    return (request.detectionType == DetectionType::Measurement
            && request.measurementToleranceUm > 0.0
            && request.measurementToleranceUm <= 20.0)
        || request.heightVariationMm >= 1.0
        || targetObjectPixelUm(request) <= 5.0;
}
