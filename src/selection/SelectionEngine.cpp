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
}

QVector<SelectionResult> SelectionEngine::select(const SelectionRequest &request,
                                                const QVector<CameraSpec> &cameras,
                                                const QVector<LensSpec> &lenses,
                                                const QVector<LightSpec> &lights,
                                                int limit) const
{
    QVector<SelectionResult> results;
    for (const CameraSpec &camera : cameras) {
        for (const LensSpec &lens : lenses) {
            if (!request.allowTelecentric && lens.isTelecentric())
                continue;
            QStringList lightReasons;
            const LightSpec light = chooseLight(request, lens, lights, &lightReasons);
            SelectionResult result = evaluatePair(request, camera, lens, light);
            result.score.reasons.append(lightReasons);
            results.append(result);
        }
    }

    std::sort(results.begin(), results.end(), [](const SelectionResult &a, const SelectionResult &b) {
        return a.score.score > b.score.score;
    });

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
        toleranceFactor = 1.0;
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
    const double bitsPerFrame = camera.resolutionX * camera.resolutionY * camera.bitDepth;
    return bitsPerFrame * fps / 8.0 / 1000000.0;
}

SelectionResult SelectionEngine::evaluatePair(const SelectionRequest &request,
                                             const CameraSpec &camera,
                                             const LensSpec &lens,
                                             const LightSpec &light) const
{
    SelectionResult result;
    result.camera = camera;
    result.lens = lens;
    result.light = light;
    result.requiredFovWidthMm = requiredFovWidth(request);
    result.requiredFovHeightMm = requiredFovHeight(request);
    result.score.score = 50.0;
    result.schemeTitle = lens.isTelecentric()
        ? QString::fromUtf8("\350\277\234\345\277\203\351\225\234\345\244\264\346\226\271\346\241\210")
        : QString::fromUtf8("\346\231\256\351\200\232\351\225\234\345\244\264\346\226\271\346\241\210");

    scoreCamera(request, camera, &result);
    if (lens.isTelecentric())
        scoreTelecentricLens(request, camera, lens, &result);
    else
        scoreFixedFocalLens(request, camera, lens, &result);

    QStringList lightReasons;
    result.score.score += scoreLight(request, lens, light, &lightReasons);
    result.score.reasons.append(lightReasons);

    if (result.score.score < 0.0)
        result.score.score = 0.0;
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
        const double score = scoreLight(request, lens, light, &localReasons);
        if (score > bestScore) {
            bestScore = score;
            best = &light;
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

    if (light.activeWidthMm < requiredFovWidth(request) || light.activeHeightMm < requiredFovHeight(request)) {
        score -= 16.0;
        if (reasons)
            reasons->append(QString::fromUtf8("\345\205\211\346\272\220\346\234\211\346\225\210\347\205\247\346\230\216\351\235\242\347\247\257\345\260\217\344\272\216\351\234\200\346\261\202\350\247\206\351\207\216\357\274\214\351\234\200\350\246\201\347\241\256\350\256\244\345\256\211\350\243\205\344\270\216\344\272\256\345\272\246"));
    }

    return score;
}

void SelectionEngine::scoreCamera(const SelectionRequest &request,
                                  const CameraSpec &camera,
                                  SelectionResult *result) const
{
    const double fps = qMax(1.0, request.requiredFps);
    result->bandwidthRequiredMBps = bandwidthRequiredMBps(camera, fps);

    if (request.preferMono && camera.isMono()) {
        result->score.score += 5.0;
        result->score.reasons.append(QString::fromUtf8("\351\273\221\347\231\275\347\233\270\346\234\272\351\200\232\345\270\270\346\234\211\346\233\264\351\253\230\347\201\265\346\225\217\345\272\246\345\222\214\350\276\271\347\274\230\347\250\263\345\256\232\346\200\247"));
    } else if (request.preferMono && !camera.isMono()) {
        result->score.score -= 8.0;
        result->score.risks.append(QString::fromUtf8("\351\234\200\346\261\202\345\201\217\345\220\221\351\273\221\347\231\275\346\265\213\351\207\217\357\274\214\344\275\206\350\257\245\347\233\270\346\234\272\344\270\272\345\275\251\350\211\262\345\236\213\345\217\267"));
    }

    if (camera.maxFps >= request.requiredFps) {
        result->score.score += 8.0;
        result->score.reasons.append(QString::fromUtf8("\347\233\270\346\234\272\345\270\247\347\216\207\346\273\241\350\266\263\350\212\202\346\213\215\351\234\200\346\261\202"));
    } else {
        result->score.score -= 25.0;
        result->score.risks.append(QString::fromUtf8("\347\233\270\346\234\272\346\234\200\345\244\247\345\270\247\347\216\207\344\275\216\344\272\216\351\234\200\346\261\202\350\212\202\346\213\215"));
    }

    if (result->bandwidthRequiredMBps <= camera.bandwidthMBps) {
        result->score.score += 6.0;
    } else {
        result->score.score -= 22.0;
        result->score.risks.append(QString::fromUtf8("\346\214\211\345\210\206\350\276\250\347\216\207/bit depth/fps \344\274\260\347\256\227\347\232\204\346\225\260\346\215\256\345\270\246\345\256\275\350\266\205\350\277\207\346\216\245\345\217\243\344\275\231\351\207\217"));
    }

    if (request.motionSpeedMmS > 20.0) {
        if (camera.isGlobalShutter()) {
            result->score.score += 14.0;
            result->score.reasons.append(QString::fromUtf8("\351\253\230\351\200\237\350\277\220\345\212\250\347\233\256\346\240\207\344\274\230\345\205\210\345\205\250\345\261\200\345\277\253\351\227\250"));
        } else {
            result->score.score -= 26.0;
            result->score.risks.append(QString::fromUtf8("\351\253\230\351\200\237\350\277\220\345\212\250\345\234\272\346\231\257\344\275\277\347\224\250\345\215\267\345\270\230\345\277\253\351\227\250\345\255\230\345\234\250\345\275\242\345\217\230\351\243\216\351\231\251"));
        }
    }
}

void SelectionEngine::scoreFixedFocalLens(const SelectionRequest &request,
                                          const CameraSpec &camera,
                                          const LensSpec &lens,
                                          SelectionResult *result) const
{
    const double fovW = camera.sensorWidthMm() * qMax(1.0, request.workingDistanceMm - lens.focalLengthMm) / lens.focalLengthMm;
    const double fovH = camera.sensorHeightMm() * qMax(1.0, request.workingDistanceMm - lens.focalLengthMm) / lens.focalLengthMm;
    result->effectiveFovWidthMm = fovW;
    result->effectiveFovHeightMm = fovH;
    result->magnification = camera.sensorWidthMm() / qMax(0.001, fovW);
    result->estimatedFocalLengthMm = request.workingDistanceMm * camera.sensorWidthMm()
        / (result->requiredFovWidthMm + camera.sensorWidthMm());
    result->objectPixelSizeUm = qMax(fovW * 1000.0 / camera.resolutionX,
                                     fovH * 1000.0 / camera.resolutionY);
    result->formulaSummary = QString::fromUtf8("\346\231\256\351\200\232\351\225\234\345\244\264\357\274\232M = SensorSize / FOV\357\274\214f \342\211\210 WD \303\227 SensorSize / (FOV + SensorSize)");

    const double targetPixel = targetObjectPixelUm(request);
    if (fovW >= result->requiredFovWidthMm && fovH >= result->requiredFovHeightMm) {
        result->score.score += 10.0;
        result->score.reasons.append(QString::fromUtf8("\346\231\256\351\200\232\351\225\234\345\244\264\350\247\206\351\207\216\350\246\206\347\233\226\345\267\245\344\273\266\345\222\214\345\256\232\344\275\215\344\275\231\351\207\217"));
    } else {
        result->score.score -= 40.0;
        result->score.risks.append(QString::fromUtf8("\346\231\256\351\200\232\351\225\234\345\244\264\345\234\250\345\275\223\345\211\215\345\267\245\344\275\234\350\267\235\347\246\273\344\270\213\350\247\206\351\207\216\344\270\215\350\266\263"));
    }

    if (result->objectPixelSizeUm <= targetPixel) {
        result->score.score += 18.0;
        result->score.reasons.append(QString::fromUtf8("\347\211\251\346\226\271\345\203\217\347\264\240\347\262\276\345\272\246 %1 um \346\273\241\350\266\263\347\233\256\346\240\207 %2 um")
            .arg(um(result->objectPixelSizeUm), um(targetPixel)));
    } else {
        const double penalty = clamp((result->objectPixelSizeUm / targetPixel - 1.0) * 14.0, 8.0, 40.0);
        result->score.score -= penalty;
        result->score.risks.append(QString::fromUtf8("\347\211\251\346\226\271\345\203\217\347\264\240\347\262\276\345\272\246 %1 um \347\262\227\344\272\216\347\233\256\346\240\207 %2 um")
            .arg(um(result->objectPixelSizeUm), um(targetPixel)));
    }

    if (lens.imageCircleMm >= camera.sensorDiagonalMm()) {
        result->score.score += 8.0;
    } else {
        result->score.score -= 35.0;
        result->score.risks.append(QString::fromUtf8("\351\225\234\345\244\264\345\203\217\345\234\206 %1 mm \345\260\217\344\272\216\347\233\270\346\234\272\344\274\240\346\204\237\345\231\250\345\257\271\350\247\222\347\272\277 %2 mm")
            .arg(lens.imageCircleMm)
            .arg(camera.sensorDiagonalMm()));
    }

    if (mountsCompatible(camera.lensMount, lens.lensMount)) {
        result->score.score += 5.0;
    } else {
        result->score.score -= 25.0;
        result->score.risks.append(QString::fromUtf8("\347\233\270\346\234\272\346\216\245\345\217\243 %1 \344\270\216\351\225\234\345\244\264\346\216\245\345\217\243 %2 \344\270\215\345\214\271\351\205\215")
            .arg(camera.lensMount, lens.lensMount));
    }

    if (request.workingDistanceMm >= lens.minWorkingDistanceMm) {
        result->score.score += 5.0;
    } else {
        result->score.score -= 20.0;
        result->score.risks.append(QString::fromUtf8("\345\267\245\344\275\234\350\267\235\347\246\273\345\260\217\344\272\216\351\225\234\345\244\264\346\234\200\345\260\217\345\267\245\344\275\234\350\267\235\347\246\273"));
    }

    if (lens.megapixelRating > 0.0 && lens.megapixelRating < camera.megapixels()) {
        result->score.score -= 12.0;
        result->score.risks.append(QString::fromUtf8("\351\225\234\345\244\264\346\240\207\347\247\260 MP \344\275\216\344\272\216\347\233\270\346\234\272\345\203\217\347\264\240\357\274\214\345\217\257\350\203\275\346\227\240\346\263\225\345\226\202\346\273\241\344\274\240\346\204\237\345\231\250"));
    } else {
        result->score.score += 4.0;
    }

    if (lens.recommendedMinPixelUm > 0.0 && lens.recommendedMinPixelUm > camera.pixelSizeUm) {
        result->score.score -= 10.0;
        result->score.risks.append(QString::fromUtf8("\351\225\234\345\244\264\346\216\250\350\215\220\346\234\200\345\260\217\345\203\217\345\205\203\345\244\247\344\272\216\347\233\270\346\234\272\345\203\217\345\205\203\357\274\214\351\234\200\347\241\256\350\256\244 MTF"));
    }

    if (measurementNeedsTelecentric(request)) {
        result->score.score -= 10.0;
        result->score.risks.append(QString::fromUtf8("\351\253\230\347\262\276\345\272\246\346\265\213\351\207\217\346\210\226\351\253\230\345\272\246\346\263\242\345\212\250\345\234\272\346\231\257\357\274\214\346\231\256\351\200\232\351\225\234\345\244\264\345\255\230\345\234\250\351\200\217\350\247\206\350\257\257\345\267\256"));
    } else {
        result->score.score += 10.0;
        result->score.reasons.append(QString::fromUtf8("\344\275\216/\344\270\255\347\262\276\345\272\246\346\210\226\345\244\247\350\247\206\351\207\216\344\273\273\345\212\241\344\275\277\347\224\250\346\231\256\351\200\232\351\225\234\345\244\264\346\210\220\346\234\254\345\222\214\345\256\211\350\243\205\346\233\264\345\217\213\345\245\275"));
    }

    if (result->requiredFovWidthMm > 150.0 || result->requiredFovHeightMm > 150.0) {
        result->score.score += 8.0;
        result->score.reasons.append(QString::fromUtf8("\345\244\247\350\247\206\351\207\216\344\273\273\345\212\241\344\274\230\345\205\210\346\231\256\351\200\232\345\267\245\344\270\232\351\225\234\345\244\264"));
    }

    const double focalMismatch = percentDifference(lens.focalLengthMm, result->estimatedFocalLengthMm);
    result->score.score += clamp(12.0 - focalMismatch * 24.0, -16.0, 12.0);
}

void SelectionEngine::scoreTelecentricLens(const SelectionRequest &request,
                                           const CameraSpec &camera,
                                           const LensSpec &lens,
                                           SelectionResult *result) const
{
    result->effectiveFovWidthMm = camera.sensorWidthMm() / lens.pmag;
    result->effectiveFovHeightMm = camera.sensorHeightMm() / lens.pmag;
    result->magnification = lens.pmag;
    result->objectPixelSizeUm = camera.pixelSizeUm / lens.pmag;
    result->estimatedFocalLengthMm = 0.0;
    result->formulaSummary = QString::fromUtf8("\350\277\234\345\277\203\351\225\234\345\244\264\357\274\232FOV = SensorSize / PMAG\357\274\214ObjectPixel = PixelSize / PMAG");

    const double targetPixel = targetObjectPixelUm(request);
    if (result->effectiveFovWidthMm >= result->requiredFovWidthMm
        && result->effectiveFovHeightMm >= result->requiredFovHeightMm) {
        result->score.score += 16.0;
        result->score.reasons.append(QString::fromUtf8("\350\277\234\345\277\203 FOV %1 x %2 mm \350\246\206\347\233\226\351\234\200\346\261\202")
            .arg(mm(result->effectiveFovWidthMm), mm(result->effectiveFovHeightMm)));
    } else {
        result->score.score -= 45.0;
        result->score.risks.append(QString::fromUtf8("\350\277\234\345\277\203\351\225\234\345\244\264\345\233\272\345\256\232\345\200\215\347\216\207\344\270\213 FOV \344\270\215\350\266\263\357\274\214\351\234\200\346\233\264\344\275\216\345\200\215\347\216\207\346\210\226\346\233\264\345\244\247\351\235\266\351\235\242"));
    }

    if (result->objectPixelSizeUm <= targetPixel) {
        result->score.score += 20.0;
        result->score.reasons.append(QString::fromUtf8("\350\277\234\345\277\203\347\211\251\346\226\271\345\203\217\347\264\240 %1 um \346\273\241\350\266\263\347\233\256\346\240\207 %2 um")
            .arg(um(result->objectPixelSizeUm), um(targetPixel)));
    } else {
        result->score.score -= clamp((result->objectPixelSizeUm / targetPixel - 1.0) * 14.0, 8.0, 38.0);
        result->score.risks.append(QString::fromUtf8("\350\277\234\345\277\203\347\211\251\346\226\271\345\203\217\347\264\240 %1 um \347\262\227\344\272\216\347\233\256\346\240\207 %2 um")
            .arg(um(result->objectPixelSizeUm), um(targetPixel)));
    }

    const double allowedDiag = lens.maxSensorDiagonalMm > 0.0 ? lens.maxSensorDiagonalMm : lens.imageCircleMm;
    if (allowedDiag >= camera.sensorDiagonalMm() && lens.imageCircleMm >= camera.sensorDiagonalMm()) {
        result->score.score += 10.0;
    } else {
        result->score.score -= 40.0;
        result->score.risks.append(QString::fromUtf8("\350\277\234\345\277\203\351\225\234\345\244\264\345\203\217\345\234\206/\346\234\200\345\244\247\351\235\266\351\235\242\345\260\217\344\272\216\347\233\270\346\234\272\344\274\240\346\204\237\345\231\250\357\274\214\344\274\232\344\272\247\347\224\237\346\232\227\350\247\222\346\210\226\350\276\271\347\274\230\351\200\200\345\214\226"));
    }

    if (mountsCompatible(camera.lensMount, lens.lensMount)) {
        result->score.score += 5.0;
    } else {
        result->score.score -= 25.0;
        result->score.risks.append(QString::fromUtf8("\347\233\270\346\234\272\346\216\245\345\217\243 %1 \344\270\216\350\277\234\345\277\203\351\225\234\345\244\264\346\216\245\345\217\243 %2 \344\270\215\345\214\271\351\205\215")
            .arg(camera.lensMount, lens.lensMount));
    }

    const double tolerance = lens.workingDistanceToleranceMm > 0.0 ? lens.workingDistanceToleranceMm : 5.0;
    if (qAbs(request.workingDistanceMm - lens.nominalWorkingDistanceMm) <= tolerance) {
        result->score.score += 12.0;
        result->score.reasons.append(QString::fromUtf8("\345\267\245\344\275\234\350\267\235\347\246\273\346\216\245\350\277\221\350\277\234\345\277\203\351\225\234\345\244\264\346\240\207\347\247\260 WD"));
    } else {
        result->score.score -= 24.0;
        result->score.risks.append(QString::fromUtf8("\350\277\234\345\277\203\351\225\234\345\244\264\351\200\232\345\270\270\345\217\252\345\234\250\346\240\207\347\247\260 WD \351\231\204\350\277\221\345\267\245\344\275\234\357\274\214\345\275\223\345\211\215 WD \345\201\217\345\267\256\350\276\203\345\244\247"));
    }

    if (lens.recommendedMinPixelUm > 0.0 && lens.recommendedMinPixelUm <= camera.pixelSizeUm) {
        result->score.score += 6.0;
    } else if (lens.recommendedMinPixelUm > camera.pixelSizeUm) {
        result->score.score -= 12.0;
        result->score.risks.append(QString::fromUtf8("\347\233\270\346\234\272\345\203\217\345\205\203\345\260\217\344\272\216\351\225\234\345\244\264\346\216\250\350\215\220\346\234\200\345\260\217\345\203\217\345\205\203\357\274\214\351\234\200\347\241\256\350\256\244\351\225\234\345\244\264\345\210\206\350\276\250\347\216\207/MTF"));
    }

    if (lens.dofMm > 0.0 && lens.dofMm >= request.heightVariationMm * 1.5) {
        result->score.score += 10.0;
        result->score.reasons.append(QString::fromUtf8("\350\277\234\345\277\203 DOF \350\246\206\347\233\226\351\253\230\345\272\246\346\263\242\345\212\250"));
    } else if (request.heightVariationMm > 0.0) {
        result->score.score -= 14.0;
        result->score.risks.append(QString::fromUtf8("\350\277\234\345\277\203 DOF \345\217\257\350\203\275\344\270\215\350\266\263\357\274\214\351\234\200\350\246\201\347\241\256\350\256\244\345\205\211\345\234\210\345\222\214\346\231\257\346\267\261"));
    }

    result->residualTelecentricErrorUm = request.heightVariationMm * qTan(qDegreesToRadians(lens.telecentricityDeg)) * 1000.0;
    if (request.measurementToleranceUm > 0.0
        && result->residualTelecentricErrorUm > request.measurementToleranceUm) {
        result->score.score -= 12.0;
        result->score.risks.append(QString::fromUtf8("\346\214\211\350\277\234\345\277\203\345\272\246\344\274\260\347\256\227\347\232\204\346\256\213\344\275\231\350\247\206\345\267\256\347\272\246 %1 um\357\274\214\351\253\230\344\272\216\345\205\201\350\256\270\350\257\257\345\267\256")
            .arg(um(result->residualTelecentricErrorUm)));
    } else {
        result->score.score += 5.0;
    }

    if (lens.lensType == LensType::BiTelecentric) {
        result->score.score += 10.0;
        result->score.reasons.append(QString::fromUtf8("\345\217\214\350\277\234\345\277\203\347\273\223\346\236\204\346\234\211\346\233\264\345\245\275\347\232\204\345\200\215\347\216\207\344\270\200\350\207\264\346\200\247"));
    }

    if (measurementNeedsTelecentric(request)) {
        result->score.score += 22.0;
        result->score.reasons.append(QString::fromUtf8("\351\253\230\347\262\276\345\272\246\346\265\213\351\207\217/\351\253\230\345\272\246\346\263\242\345\212\250\344\274\230\345\205\210\350\277\234\345\277\203\351\225\234\345\244\264\344\273\245\351\231\215\344\275\216\351\200\217\350\247\206\350\257\257\345\267\256"));
    }

    if (result->requiredFovWidthMm > 150.0 || result->requiredFovHeightMm > 150.0) {
        const bool lowPrecisionInspection = !measurementNeedsTelecentric(request)
            && request.detectionType == DetectionType::DefectInspection;
        result->score.score -= lowPrecisionInspection ? 45.0 : 18.0;
        result->score.risks.append(QString::fromUtf8("\345\244\247\350\247\206\351\207\216\350\277\234\345\277\203\351\225\234\345\244\264\351\200\232\345\270\270\344\275\223\347\247\257\343\200\201\351\207\215\351\207\217\345\222\214\346\210\220\346\234\254\350\276\203\351\253\230"));
    }
}

bool SelectionEngine::measurementNeedsTelecentric(const SelectionRequest &request)
{
    return request.detectionType == DetectionType::Measurement
        || request.measurementToleranceUm <= 20.0
        || request.heightVariationMm >= 1.0;
}
