#include "selection/ParameterCalculator.h"

#include "core/PixelFormat.h"
#include "core/SelectionTypes.h"
#include "i18n/LanguageManager.h"

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <limits>
#include <QtMath>

namespace {
using namespace Parameters;
QString L(const char *zh, const char *en)
{
    return QString::fromUtf8(LanguageManager::instance().currentLanguage() == QLatin1String("en_US") ? en : zh);
}
bool positive(Number value) { return value && std::isfinite(*value) && *value > 0.0; }
bool nonnegative(Number value) { return value && std::isfinite(*value) && *value >= 0.0; }
bool dimension(Number value)
{
    return positive(value) && *value <= 200000.0 && std::floor(*value) == *value;
}
CalculationStatus required(std::initializer_list<Number> values)
{
    bool missing = false;
    for (const auto &value : values) {
        if (value && !positive(value))
            return CalculationStatus::Invalid;
        missing |= !value.has_value();
    }
    return missing ? CalculationStatus::Unknown : CalculationStatus::Passed;
}
CalculationStatus sensorStatus(const Sensor &sensor)
{
    const auto status = required({sensor.resolutionX, sensor.resolutionY, sensor.pixelSizeUm});
    if (status != CalculationStatus::Passed)
        return status;
    return dimension(sensor.resolutionX) && dimension(sensor.resolutionY)
        ? CalculationStatus::Passed : CalculationStatus::Invalid;
}
QString missingNote(CalculationStatus status)
{
    return status == CalculationStatus::Invalid
        ? L("输入无效，请检查数值范围。", "Invalid input; check the value ranges.")
        : L("填写当前求解所需参数后显示结果。", "Enter the required inputs to calculate.");
}
Number objectPixel(Number fovMm, Number resolution)
{
    if (!positive(fovMm) || !dimension(resolution)) return {};
    return *fovMm * 1000.0 / *resolution;
}
CalculationStatus combined(CalculationStatus a, CalculationStatus b)
{
    if (a == CalculationStatus::Failed || b == CalculationStatus::Failed) return CalculationStatus::Failed;
    if (a == CalculationStatus::Invalid || b == CalculationStatus::Invalid) return CalculationStatus::Invalid;
    if (a == CalculationStatus::Unknown || b == CalculationStatus::Unknown) return CalculationStatus::Unknown;
    return CalculationStatus::Passed;
}
}

QString Parameters::statusText(CalculationStatus status)
{
    switch (status) {
    case CalculationStatus::Passed: return L("满足", "Meets requirement");
    case CalculationStatus::Failed: return L("不满足", "Does not meet");
    case CalculationStatus::Unknown: return L("待补参数", "Missing data");
    case CalculationStatus::NotApplicable: return L("不适用", "Not applicable");
    case CalculationStatus::Invalid: return L("输入无效", "Invalid input");
    }
    return {};
}

Parameters::OpticsResult Parameters::optics(const OpticsInput &input)
{
    OpticsResult result;
    result.status = sensorStatus(input.sensor);
    if (result.status != CalculationStatus::Passed) {
        result.note = missingNote(result.status);
        return result;
    }
    result.status = input.solve == OpticsSolve::FieldOfView
        ? required({input.focalLengthMm, input.distanceMm})
        : required({input.targetFovWidthMm, input.targetFovHeightMm,
                    input.solve == OpticsSolve::FocalLength ? input.distanceMm : input.focalLengthMm});
    if (result.status != CalculationStatus::Passed) {
        result.note = missingNote(result.status);
        return result;
    }
    const double sx = *input.sensor.resolutionX * *input.sensor.pixelSizeUm / 1000.0;
    const double sy = *input.sensor.resolutionY * *input.sensor.pixelSizeUm / 1000.0;
    const double offset = input.model == OpticsModel::ThinLens ? 1.0 : 0.0;
    double focal = input.focalLengthMm.value_or(0.0);
    double distance = input.distanceMm.value_or(0.0);
    if (input.solve != OpticsSolve::FieldOfView) {
        const double ratio = std::max(*input.targetFovWidthMm / sx, *input.targetFovHeightMm / sy);
        if (input.solve == OpticsSolve::FocalLength) focal = distance / (ratio + offset);
        else distance = focal * (ratio + offset);
    }
    const double ratio = distance / focal - offset;
    if (!std::isfinite(ratio) || ratio <= 0.0) {
        result.status = CalculationStatus::Invalid;
        result.note = L("薄透镜物距必须大于焦距；机械工作距离不能直接视为主平面物距。",
                        "Thin-lens object distance must exceed focal length; mechanical WD is not principal-plane distance.");
        return result;
    }
    result.focalLengthMm = focal;
    result.distanceMm = distance;
    result.fovWidthMm = sx * ratio;
    result.fovHeightMm = sy * ratio;
    result.magnification = 1.0 / ratio;
    result.objectPixelUm = *input.sensor.pixelSizeUm * ratio;
    result.coverage = combined(checkUpperBound(input.targetFovWidthMm, result.fovWidthMm),
                               checkUpperBound(input.targetFovHeightMm, result.fovHeightMm));
    result.note = input.model == OpticsModel::Paraxial
        ? L("近轴粗算：FOV ≈ 靶面 × 距离 / 焦距。忽略主平面与畸变，安装尺寸需查规格或实测。",
            "Paraxial estimate: FOV ≈ sensor × distance / focal length. Principal planes and distortion are ignored; verify mechanical dimensions.")
        : L("薄透镜：FOV = 靶面 × (物距 − 焦距) / 焦距。物距从物方主平面起算，不是镜头前端工作距离。",
            "Thin lens: FOV = sensor × (object distance − focal length) / focal length. Distance is measured from the object principal plane, not the lens front.");
    return result;
}

Parameters::SamplingResult Parameters::sampling(const SamplingInput &input)
{
    SamplingResult result;
    if (input.solve == SamplingSolve::Calibration) {
        result.status = required({input.calibrationLengthMm, input.calibrationPixels});
        if (result.status == CalculationStatus::Passed)
            result.calibratedPixelUm = *input.calibrationLengthMm * 1000.0 / *input.calibrationPixels;
        result.note = L("标定像素当量 = 实测长度 / 图像像素距离。仅代表记录方向和位置，不代表全视场精度。",
                        "Pixel scale = measured length / image pixel distance. It applies only to the recorded direction and region.");
        return result;
    }
    result.status = required({input.fovWidthMm, input.fovHeightMm});
    if (result.status != CalculationStatus::Passed) {
        result.note = missingNote(result.status);
        return result;
    }
    if (input.solve == SamplingSolve::Actual) {
        result.status = required({input.sensor.resolutionX, input.sensor.resolutionY});
        if (result.status == CalculationStatus::Passed
            && (!dimension(input.sensor.resolutionX) || !dimension(input.sensor.resolutionY)))
            result.status = CalculationStatus::Invalid;
        if (result.status == CalculationStatus::Passed) {
            result.objectPixelXUm = objectPixel(input.fovWidthMm, input.sensor.resolutionX);
            result.objectPixelYUm = objectPixel(input.fovHeightMm, input.sensor.resolutionY);
            if (positive(input.featureUm)) {
                result.featurePixelsX = *input.featureUm / *result.objectPixelXUm;
                result.featurePixelsY = *input.featureUm / *result.objectPixelYUm;
            }
        }
    } else {
        const bool featureBudget = input.featureUm.has_value() || input.pixelsPerFeature.has_value();
        const bool measurementBudget = input.measurementBudget || input.toleranceUm.has_value();
        result.status = featureBudget ? required({input.featureUm, input.pixelsPerFeature})
                                      : (measurementBudget ? CalculationStatus::Passed : CalculationStatus::Unknown);
        if (measurementBudget)
            result.status = combined(result.status, required({input.toleranceUm, input.pixelsPerTolerance}));
        if (result.status == CalculationStatus::Passed) {
            double target = featureBudget ? *input.featureUm / *input.pixelsPerFeature : std::numeric_limits<double>::infinity();
            if (measurementBudget) target = std::min(target, *input.toleranceUm / *input.pixelsPerTolerance);
            result.targetObjectPixelUm = target;
            const double nx = std::ceil(*input.fovWidthMm * 1000.0 / target);
            const double ny = std::ceil(*input.fovHeightMm * 1000.0 / target);
            if (!std::isfinite(nx) || !std::isfinite(ny) || nx > std::numeric_limits<int>::max() || ny > std::numeric_limits<int>::max()) {
                result.status = CalculationStatus::Invalid;
                result.note = L("所需分辨率超出计算范围，保留原始目标，不放宽采样要求。",
                                "Required resolution exceeds the supported range; the sampling target has not been relaxed.");
                return result;
            }
            result.requiredResolutionX = nx;
            result.requiredResolutionY = ny;
        }
    }
    result.note = L("像素当量描述采样间隔；光学分辨能力与最终测量误差还需独立验证。",
                    "Pixel scale describes sampling pitch. Optical resolution and final measurement error require separate validation.");
    return result;
}

Parameters::TelecentricResult Parameters::telecentric(const TelecentricInput &input)
{
    TelecentricResult result;
    result.status = sensorStatus(input.sensor);
    if (result.status == CalculationStatus::Passed)
        result.status = input.solve == TelecentricSolve::FieldOfView ? required({input.magnification})
            : required({input.targetFovWidthMm, input.targetFovHeightMm});
    if (input.solve == TelecentricSolve::Range)
        result.status = combined(result.status, required({input.targetObjectPixelUm}));
    if (result.status != CalculationStatus::Passed) {
        result.note = missingNote(result.status);
        return result;
    }
    const double sx = *input.sensor.resolutionX * *input.sensor.pixelSizeUm / 1000.0;
    const double sy = *input.sensor.resolutionY * *input.sensor.pixelSizeUm / 1000.0;
    double mag = input.magnification.value_or(0.0);
    if (input.solve != TelecentricSolve::FieldOfView) {
        mag = std::min(sx / *input.targetFovWidthMm, sy / *input.targetFovHeightMm);
        result.maxMagnification = mag;
        if (input.solve == TelecentricSolve::Range) {
            result.minMagnification = *input.sensor.pixelSizeUm / *input.targetObjectPixelUm;
            if (checkUpperBound(result.minMagnification, result.maxMagnification) == CalculationStatus::Failed) {
                result.status = CalculationStatus::Failed;
                result.note = L("倍率区间无交集：采样需要更高倍率，视场需要更低倍率。请调整视场、相机或采样目标。",
                                "No feasible magnification: sampling requires more magnification than coverage allows. Adjust FOV, camera, or sampling target.");
                return result;
            }
        }
    }
    result.magnification = mag;
    result.fovWidthMm = sx / mag;
    result.fovHeightMm = sy / mag;
    result.objectPixelUm = *input.sensor.pixelSizeUm / mag;
    result.note = L("远心视场由固定倍率决定；工作距离是镜头安装约束，不能用改变距离任意调整视场。",
                    "Telecentric FOV follows fixed magnification. Working distance is an installation constraint, not an independent FOV adjustment.");
    return result;
}

Parameters::ExposureResult Parameters::exposure(const ExposureInput &input)
{
    ExposureResult result;
    if (input.solve != ExposureSolve::Speed && nonnegative(input.speedMmS) && *input.speedMmS == 0.0) {
        result.status = CalculationStatus::NotApplicable;
        result.blurPixels = input.solve == ExposureSolve::Blur ? Number(0.0) : Number();
        result.note = L("速度为零，无运动拖影约束；曝光仍受亮度、饱和与节拍限制。",
                        "Zero speed: no motion-blur limit. Brightness, saturation and cycle time still constrain exposure.");
        return result;
    }
    result.status = input.solve == ExposureSolve::Exposure ? required({input.objectPixelUm, input.speedMmS, input.blurPixels})
        : (input.solve == ExposureSolve::Blur ? required({input.objectPixelUm, input.speedMmS, input.exposureUs})
                                              : required({input.objectPixelUm, input.exposureUs, input.blurPixels}));
    if (result.status != CalculationStatus::Passed) {
        result.note = missingNote(result.status);
        return result;
    }
    if (input.solve == ExposureSolve::Exposure)
        result.exposureUs = *input.blurPixels * *input.objectPixelUm * 1000.0 / *input.speedMmS;
    else if (input.solve == ExposureSolve::Blur)
        result.blurPixels = *input.speedMmS * *input.exposureUs / (*input.objectPixelUm * 1000.0);
    else result.speedMmS = *input.blurPixels * *input.objectPixelUm * 1000.0 / *input.exposureUs;
    result.note = L("沿所选轴匀速运动，拖影 = 速度 × 曝光 / 像素当量。该条件不判断光照是否足够。",
                    "Constant motion along the selected axis: blur = speed × exposure / pixel scale. This does not assess available light.");
    return result;
}

Parameters::TransferResult Parameters::transfer(const TransferInput &input)
{
    TransferResult result;
    const Number width = input.roiWidth ? input.roiWidth : input.width;
    const Number height = input.roiHeight ? input.roiHeight : input.height;
    result.status = required({width, height});
    if (result.status == CalculationStatus::Passed
        && (!dimension(width) || !dimension(height)
            || (input.roiWidth && (!dimension(input.width) || *width > *input.width))
            || (input.roiHeight && (!dimension(input.height) || *height > *input.height))))
        result.status = CalculationStatus::Invalid;
    if (result.status != CalculationStatus::Passed) {
        result.note = missingNote(result.status);
        return result;
    }
    result.frameBytes = PixelFormat::frameBytes(static_cast<int>(*width), static_cast<int>(*height), input.pixelFormat);
    if (!result.frameBytes) {
        result.status = input.pixelFormat.isEmpty() ? CalculationStatus::Unknown : CalculationStatus::Invalid;
        result.note = L("请选择明确的传输像素格式；传感器位深不能直接代表传输数据量。",
                        "Select an explicit transport pixel format. Sensor bit depth alone does not determine payload.");
        return result;
    }
    result.status = required({input.fps, input.cameraCount});
    if (result.status == CalculationStatus::Passed && std::floor(*input.cameraCount) != *input.cameraCount)
        result.status = CalculationStatus::Invalid;
    if (input.overheadPercent && !nonnegative(input.overheadPercent)) result.status = CalculationStatus::Invalid;
    if (!input.overheadPercent && result.status == CalculationStatus::Passed) result.status = CalculationStatus::Unknown;
    if (result.status != CalculationStatus::Passed) {
        result.note = missingNote(result.status);
        return result;
    }
    result.payloadMBps = *result.frameBytes * *input.fps * *input.cameraCount / 1e6;
    result.transportMBps = *result.payloadMBps * (1.0 + *input.overheadPercent / 100.0);
    if (positive(input.capacityMBps)) {
        result.utilizationPercent = *result.transportMBps / *input.capacityMBps * 100.0;
        result.capacityStatus = checkUpperBound(result.transportMBps, input.capacityMBps);
    } else if (input.capacityMBps) {
        result.capacityStatus = CalculationStatus::Invalid;
        result.status = CalculationStatus::Invalid;
    }
    const auto savedFrame = input.storageFormat.isEmpty() ? result.frameBytes
        : PixelFormat::frameBytes(static_cast<int>(*width), static_cast<int>(*height), input.storageFormat);
    if (savedFrame && nonnegative(input.hours)) {
        const double bytes = *savedFrame * *input.fps * *input.cameraCount * *input.hours * 3600.0;
        result.storageGB = bytes / 1e9;
        result.storageGiB = bytes / 1073741824.0;
    } else if (!savedFrame || (input.hours && !nonnegative(input.hours))) result.status = CalculationStatus::Invalid;
    result.note = L("载荷与协议预留分开；链路容量按总共享容量填写。存储不含文件头、压缩及主机额外对齐。",
                    "Payload and overhead are separate; enter total shared link capacity. Storage excludes headers, compression and extra host alignment.");
    return result;
}

QString Parameters::checkTitle(const QString &key)
{
    if (key == QLatin1String("fovX")) return L("视场 X", "FOV X");
    if (key == QLatin1String("fovY")) return L("视场 Y", "FOV Y");
    if (key == QLatin1String("samplingX")) return L("像素当量 X", "Pixel scale X");
    if (key == QLatin1String("samplingY")) return L("像素当量 Y", "Pixel scale Y");
    if (key == QLatin1String("fps")) return L("标称帧率", "Nominal frame rate");
    if (key == QLatin1String("imageCircle")) return L("像圈覆盖", "Image circle");
    if (key == QLatin1String("mount")) return L("镜头接口", "Lens mount");
    if (key == QLatin1String("distance")) return L("安装距离", "Working distance");
    if (key == QLatin1String("dof")) return L("景深", "Depth of field");
    if (key == QLatin1String("telecentricity")) return L("残余远心误差", "Residual telecentric error");
    if (key == QLatin1String("motion")) return L("运动拖影", "Motion blur");
    if (key == QLatin1String("bandwidth")) return L("传输吞吐", "Transport throughput");
    return key;
}

Parameters::SystemResult Parameters::checkSystem(const SystemInput &input)
{
    SystemResult result;
    QString geometryNote;
    CalculationStatus geometryStatus = CalculationStatus::Unknown;
    if (input.measuredFov) {
        geometryStatus = required({input.measuredFovWidthMm, input.measuredFovHeightMm});
        if (positive(input.measuredFovWidthMm)) result.actualFovWidthMm = input.measuredFovWidthMm;
        if (positive(input.measuredFovHeightMm)) result.actualFovHeightMm = input.measuredFovHeightMm;
        geometryNote = L("使用实测视场；仅用于当前安装和成像区域。", "Measured FOV; valid for the current installation and imaging area.");
    } else if (input.telecentric) {
        TelecentricInput ti;
        ti.sensor = input.sensor;
        ti.solve = TelecentricSolve::FieldOfView;
        ti.magnification = input.magnification;
        const auto tr = telecentric(ti);
        geometryStatus = tr.status;
        result.actualFovWidthMm = tr.fovWidthMm;
        result.actualFovHeightMm = tr.fovHeightMm;
        geometryNote = tr.note;
    } else {
        OpticsInput oi;
        oi.sensor = input.sensor;
        oi.model = input.model;
        oi.solve = OpticsSolve::FieldOfView;
        oi.focalLengthMm = input.focalLengthMm;
        oi.distanceMm = input.distanceMm;
        const auto optical = optics(oi);
        geometryStatus = optical.status;
        result.actualFovWidthMm = optical.fovWidthMm;
        result.actualFovHeightMm = optical.fovHeightMm;
        geometryNote = optical.note;
    }
    result.objectPixelXUm = objectPixel(result.actualFovWidthMm, input.sensor.resolutionX);
    result.objectPixelYUm = objectPixel(result.actualFovHeightMm, input.sensor.resolutionY);
    const auto add = [&](const char *key, Number target, Number actual, const char *unit, bool upper, const QString &detail) {
        const auto state = upper ? checkUpperBound(actual, target) : checkUpperBound(target, actual);
        result.checks.append({QString::fromLatin1(key), target, actual, QString::fromUtf8(unit), state, detail});
    };
    add("fovX", input.targetFovWidthMm, result.actualFovWidthMm, "mm", false, geometryNote);
    add("fovY", input.targetFovHeightMm, result.actualFovHeightMm, "mm", false, geometryNote);
    add("samplingX", input.targetObjectPixelUm, result.objectPixelXUm, "μm/px", true,
        L("按当前实际视场校核，数值越小采样越细。", "Checked against actual FOV; smaller pitch means finer sampling."));
    add("samplingY", input.targetObjectPixelUm, result.objectPixelYUm, "μm/px", true,
        L("几何采样满足不等于测量精度保证。", "Geometric sampling does not guarantee measurement accuracy."));
    if (geometryStatus == CalculationStatus::Invalid) {
        for (auto &check : result.checks) check.status = CalculationStatus::Invalid;
    }
    for (auto &check : result.checks) {
        if (check.target && !positive(check.target)) check.status = CalculationStatus::Invalid;
    }
    add("fps", input.fps, positive(input.maxFps) ? input.maxFps : Number(), "fps", false,
        L("仅校核标称上限；曝光、ROI 与读出可能进一步限制帧率。", "Nominal limit only; exposure, ROI and readout can further limit frame rate."));
    if (input.fps && !positive(input.fps)) result.checks.last().status = CalculationStatus::Invalid;
    Number diagonal;
    if (sensorStatus(input.sensor) == CalculationStatus::Passed)
        diagonal = std::hypot(*input.sensor.resolutionX, *input.sensor.resolutionY) * *input.sensor.pixelSizeUm / 1000.0;
    add("imageCircle", diagonal, positive(input.imageCircleMm) ? input.imageCircleMm : Number(), "mm", false,
        L("需镜头像圈规格，不能仅凭标称英寸判断。", "Requires image-circle specifications, not only nominal sensor format."));
    result.checks.append({QStringLiteral("mount"), {}, {}, {}, input.cameraMount.isEmpty() || input.lensMount.isEmpty()
        ? CalculationStatus::Unknown : (mountsCompatible(input.cameraMount, input.lensMount) ? CalculationStatus::Passed : CalculationStatus::Failed),
        input.cameraMount + QStringLiteral(" / ") + input.lensMount});
    if (input.telecentric) {
        Number delta;
        if (positive(input.distanceMm) && positive(input.nominalWorkingDistanceMm))
            delta = std::abs(*input.distanceMm - *input.nominalWorkingDistanceMm);
        add("distance", input.workingDistanceToleranceMm, delta, "mm", true,
            L("当前列显示距标称工作距离的偏差；要求列为允许偏差，需确认规格容差。",
              "Actual is the offset from nominal WD; target is the allowed offset. Confirm the specified tolerance."));
    } else add("distance", positive(input.minWorkingDistanceMm) ? input.minWorkingDistanceMm : Number(), input.distanceMm, "mm", false,
        input.model == OpticsModel::ThinLens
            ? L("光学物距与机械工作距离基准不同；此项需另行确认。", "Optical object distance and mechanical WD have different references; verify separately.")
            : L("按镜头最小工作距离校核。", "Checked against the minimum lens working distance."));
    if (!input.telecentric && input.model == OpticsModel::ThinLens)
        result.checks.last().status = CalculationStatus::Unknown;
    add("dof", input.heightVariationMm ? Number(*input.heightVariationMm * 1.5) : Number(), positive(input.dofMm) ? input.dofMm : Number(), "mm", false,
        L("要求景深 = 高度峰峰值 × 1.5；须确认当前倍率、光圈及评价标准。",
          "Required DOF = peak-to-peak height × 1.5; confirm magnification, aperture and acceptance criterion."));
    if (input.heightVariationMm && *input.heightVariationMm == 0.0) result.checks.last().status = CalculationStatus::NotApplicable;
    else if (!input.dofConditionsConfirmed) result.checks.last().status = CalculationStatus::Unknown;
    Number error;
    if (input.telecentric && nonnegative(input.telecentricityDeg) && nonnegative(input.heightVariationMm) && *input.telecentricityDeg < 90.0)
        error = *input.heightVariationMm * std::tan(qDegreesToRadians(*input.telecentricityDeg)) * 1000.0;
    add("telecentricity", input.measurementToleranceUm, error, "μm", true,
        L("高度变化 × tan(远心度)，不包含其他误差项。", "Height change × tan(telecentricity); excludes other error sources."));
    if (!input.telecentric || (input.heightVariationMm && *input.heightVariationMm == 0.0))
        result.checks.last().status = CalculationStatus::NotApplicable;
    ExposureInput ei;
    ei.solve = ExposureSolve::Blur;
    if (result.objectPixelXUm && result.objectPixelYUm)
        ei.objectPixelUm = std::min(*result.objectPixelXUm, *result.objectPixelYUm);
    ei.exposureUs = input.exposureUs;
    ei.speedMmS = input.speedMmS;
    const auto motion = exposure(ei);
    add("motion", input.allowedBlurPixels, motion.blurPixels, "px", true,
        L("使用两轴中更细的像素当量保守估算匀速拖影。", "Conservative constant-motion estimate using the finer of the two axis pixel scales."));
    if (!input.motionEnabled || motion.status == CalculationStatus::NotApplicable)
        result.checks.last().status = CalculationStatus::NotApplicable;
    else if (motion.status == CalculationStatus::Invalid) result.checks.last().status = CalculationStatus::Invalid;
    TransferInput ti;
    ti.width = input.sensor.resolutionX;
    ti.height = input.sensor.resolutionY;
    ti.pixelFormat = input.pixelFormat;
    ti.fps = input.fps;
    ti.capacityMBps = input.capacityMBps;
    const auto data = transfer(ti);
    add("bandwidth", input.capacityMBps, data.transportMBps, "MB/s", true,
        L("单相机载荷；有效容量应已扣除线路开销及预留。多相机请使用带宽任务。",
          "Single-camera payload; usable capacity must exclude overhead and reserves. Use the transfer task for multiple cameras."));
    if (data.status == CalculationStatus::Invalid || (input.capacityMBps && !positive(input.capacityMBps)))
        result.checks.last().status = CalculationStatus::Invalid;
    result.status = CalculationStatus::Passed;
    for (const auto &check : result.checks) result.status = combined(result.status, check.status);
    return result;
}
