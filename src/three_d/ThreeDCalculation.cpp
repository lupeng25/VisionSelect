#include "three_d/ThreeDCalculation.h"

#include "i18n/LanguageManager.h"

#include <QtMath>

namespace {
QString text(const char *zhUtf8, const char *enUtf8)
{
    return LanguageManager::instance().currentLanguage() == QLatin1String("en_US")
        ? QString::fromUtf8(enUtf8)
        : QString::fromUtf8(zhUtf8);
}

bool positive(double value)
{
    return value > 0.0;
}

void markInvalid(ThreeDMotionSamplingResult *result, const QString &risk)
{
    result->valid = false;
    result->risks.append(risk);
}
}

double ThreeDCalculation::samplingRateLimitHz(const ThreeDCameraSpec &camera)
{
    double limit = -1.0;
    if (threeDHasValue(camera.scanRateMaxHz))
        limit = qMax(limit, camera.scanRateMaxHz);
    if (threeDHasValue(camera.frameRateHz))
        limit = qMax(limit, camera.frameRateHz);
    return limit;
}

double ThreeDCalculation::xPixelPitchMm(const ThreeDCameraSpec &camera,
                                        double overrideXPixelPitchMm,
                                        bool *usesManualOverride)
{
    if (usesManualOverride)
        *usesManualOverride = false;
    if (positive(overrideXPixelPitchMm)) {
        if (usesManualOverride)
            *usesManualOverride = true;
        return overrideXPixelPitchMm;
    }
    if (threeDHasValue(camera.profileDataIntervalUm))
        return camera.profileDataIntervalUm / 1000.0;
    if (threeDHasValue(camera.xyResolutionUm))
        return camera.xyResolutionUm / 1000.0;
    return -1.0;
}

ThreeDMotionSamplingResult ThreeDCalculation::estimateMotionSampling(const ThreeDMotionSamplingInput &input,
                                                                     const ThreeDCameraSpec *camera)
{
    ThreeDMotionSamplingResult result;

    if (!positive(input.scanDistanceMm))
        markInvalid(&result, text("扫描距离必须大于 0。", "Scan distance must be greater than 0."));
    if (!positive(input.profileIntervalMm))
        markInvalid(&result, text("轮廓采样间隔必须大于 0。", "Profile interval must be greater than 0."));
    if (!positive(input.axisTravelMm))
        markInvalid(&result, text("轴移动距离必须大于 0。", "Axis travel must be greater than 0."));
    if (input.pulseCount <= 0)
        markInvalid(&result, text("脉冲数量必须大于 0。", "Pulse count must be greater than 0."));
    if (input.refinementPoints <= 0)
        markInvalid(&result, text("细化点数必须大于 0。", "Refinement points must be greater than 0."));
    if (!positive(input.samplingRateHz))
        markInvalid(&result, text("采样频率必须大于 0。", "Sampling rate must be greater than 0."));
    if (!positive(input.safetyFactor) || input.safetyFactor > 1.0)
        markInvalid(&result, text("安全系数建议在 0 到 1 之间。", "Safety factor should be between 0 and 1."));
    if (input.triggerMode == ThreeDTriggerMode::Encoder && input.encoderPulsesPerProfile <= 0)
        markInvalid(&result, text("每轮廓脉冲数必须大于 0。", "Pulses per profile must be greater than 0."));
    if (input.exposureTimeUs < 0.0)
        markInvalid(&result, text("曝光时间不能小于 0。", "Exposure time cannot be negative."));
    if (input.readoutMarginUs < 0.0)
        markInvalid(&result, text("读出/复位余量不能小于 0。", "Readout/reset margin cannot be negative."));

    if (positive(input.scanDistanceMm) && positive(input.profileIntervalMm))
        result.profileCount = input.scanDistanceMm / input.profileIntervalMm;
    if (positive(input.axisTravelMm) && input.pulseCount > 0)
        result.pulseIntervalMm = input.axisTravelMm / input.pulseCount;
    if (positive(result.pulseIntervalMm) && input.refinementPoints > 0)
        result.yPixelPitchMm = result.pulseIntervalMm * input.refinementPoints;

    if (input.triggerMode == ThreeDTriggerMode::Encoder
        && positive(input.encoderPulseFrequencyHz) && input.encoderPulsesPerProfile > 0) {
        result.encoderProfileRateHz = input.encoderPulseFrequencyHz / input.encoderPulsesPerProfile;
        result.effectiveProfileRateHz = result.encoderProfileRateHz;
        if (positive(result.pulseIntervalMm)) {
            result.encoderProfileIntervalMm = result.pulseIntervalMm * input.encoderPulsesPerProfile;
            result.encoderAxisSpeedMmS = input.encoderPulseFrequencyHz * result.pulseIntervalMm;
        }
    } else if (positive(input.samplingRateHz)) {
        result.effectiveProfileRateHz = input.samplingRateHz;
    }

    if (positive(input.targetAxisSpeedMmS) && positive(input.profileIntervalMm) && positive(input.safetyFactor))
        result.requiredProfileRateHz = input.targetAxisSpeedMmS / (input.profileIntervalMm * input.safetyFactor);
    if (positive(result.effectiveProfileRateHz) && positive(input.profileIntervalMm) && positive(input.safetyFactor))
        result.maxAxisSpeedMmS = result.effectiveProfileRateHz * input.profileIntervalMm * input.safetyFactor;
    if (positive(result.effectiveProfileRateHz)) {
        result.profilePeriodUs = 1000000.0 / result.effectiveProfileRateHz;
        result.maxExposureTimeUs = result.profilePeriodUs - input.readoutMarginUs;
        if (result.maxExposureTimeUs < 0.0)
            result.maxExposureTimeUs = 0.0;
    }

    if (camera) {
        result.xPixelPitchMm = xPixelPitchMm(*camera, input.overrideXPixelPitchMm, &result.usesManualXPixelPitch);
        result.cameraSamplingRateLimitHz = samplingRateLimitHz(*camera);
        result.samplingRateKnown = threeDHasValue(result.cameraSamplingRateLimitHz);
        result.encoderRateLimitHz = camera->encoderRateMaxHz;
        result.encoderRateKnown = threeDHasValue(result.encoderRateLimitHz);
    } else if (positive(input.overrideXPixelPitchMm)) {
        result.xPixelPitchMm = input.overrideXPixelPitchMm;
        result.usesManualXPixelPitch = true;
    }
    result.xPixelPitchKnown = threeDHasValue(result.xPixelPitchMm);

    if (result.xPixelPitchKnown && positive(result.yPixelPitchMm))
        result.xyPitchRatio = result.yPixelPitchMm / result.xPixelPitchMm;

    if (camera && !result.xPixelPitchKnown) {
        result.risks.append(text("当前型号未公开 X 轮廓数据间隔，需手动输入或向厂家确认。",
                                 "The selected model does not publish X profile data interval; enter it manually or confirm with the vendor."));
    } else if (!camera && !result.xPixelPitchKnown) {
        result.risks.append(text("未选择 3D 相机且未手动输入 X 像素当量，X/Y 点距比例需确认。",
                                 "No 3D camera is selected and no manual X pitch is entered; X/Y pitch ratio needs confirmation."));
    } else if (result.usesManualXPixelPitch) {
        result.reasons.append(text("X 像素当量使用手动输入值。",
                                   "X pixel pitch uses the manual value."));
    } else if (camera && threeDHasValue(camera->profileDataIntervalUm)) {
        result.reasons.append(text("X 像素当量来自相机公开的 X 轮廓数据间隔。",
                                   "X pixel pitch comes from the published X profile data interval."));
    } else if (camera && threeDHasValue(camera->xyResolutionUm)) {
        result.risks.append(text("相机未公开 X 轮廓数据间隔，已使用 XY 分辨率字段作为近似参考。",
                                 "The camera does not publish X profile data interval; XY resolution is used as an approximate reference."));
    }

    if (input.triggerMode == ThreeDTriggerMode::Encoder) {
        if (!positive(input.encoderPulseFrequencyHz)) {
            result.risks.append(text("编码器触发模式下未输入编码器脉冲频率，无法校验触发轮廓频率。",
                                     "Encoder trigger mode needs encoder pulse frequency to validate profile trigger rate."));
        } else {
            result.reasons.append(text("有效轮廓频率按编码器脉冲频率 / 每轮廓脉冲数计算。",
                                       "Effective profile rate is calculated from encoder pulse frequency divided by pulses per profile."));
        }
        if (positive(result.encoderProfileIntervalMm) && positive(input.profileIntervalMm)) {
            const double diff = qAbs(result.encoderProfileIntervalMm - input.profileIntervalMm);
            if (diff > qMax(0.001, input.profileIntervalMm * 0.1)) {
                result.risks.append(text("编码器每轮廓距离与目标轮廓采样间隔差异较大，需调整每轮廓脉冲数或编码器分辨率。",
                                         "Encoder distance per profile differs from the target profile interval; adjust pulses per profile or encoder resolution."));
            } else {
                result.reasons.append(text("编码器每轮廓距离接近目标轮廓采样间隔。",
                                           "Encoder distance per profile is close to the target profile interval."));
            }
        }
        if (camera && result.encoderRateKnown && positive(input.encoderPulseFrequencyHz)) {
            result.encoderRateWithinCameraLimit = input.encoderPulseFrequencyHz <= result.encoderRateLimitHz + 1e-9;
            if (result.encoderRateWithinCameraLimit) {
                result.reasons.append(text("编码器脉冲频率未超过相机公开的编码器输入上限。",
                                           "Encoder pulse frequency is within the camera's published encoder input limit."));
            } else {
                result.risks.append(text("编码器脉冲频率超过相机公开的编码器输入上限。",
                                         "Encoder pulse frequency exceeds the camera's published encoder input limit."));
            }
        } else if (camera && positive(input.encoderPulseFrequencyHz)) {
            result.risks.append(text("当前型号未公开编码器输入频率上限，编码器脉冲频率需向厂商确认。",
                                     "The selected model does not publish encoder input frequency limit; confirm encoder pulse frequency with the vendor."));
        }
    } else if (input.triggerMode == ThreeDTriggerMode::ExternalTrigger) {
        result.reasons.append(text("有效轮廓频率按外部线触发/触发输入频率计算。",
                                   "Effective profile rate is based on the external line trigger rate."));
        if (camera && camera->supportsExternalTrigger == 0) {
            result.risks.append(text("当前型号标记为不支持外部触发。",
                                     "The selected model is marked as not supporting external trigger."));
        } else if (camera && camera->supportsExternalTrigger < 0) {
            result.risks.append(text("当前型号未公开外部触发能力，需确认 I/O 或 GenICam 触发支持。",
                                     "The selected model does not publish external trigger capability; confirm I/O or GenICam trigger support."));
        }
    } else {
        result.reasons.append(text("有效轮廓频率按自由运行采样频率计算。",
                                   "Effective profile rate is based on free-running sampling rate."));
    }

    if (positive(input.targetAxisSpeedMmS) && positive(result.requiredProfileRateHz) && positive(result.effectiveProfileRateHz)) {
        result.effectiveRateMeetsTarget = result.effectiveProfileRateHz + 1e-9 >= result.requiredProfileRateHz;
        if (result.effectiveRateMeetsTarget) {
            result.reasons.append(text("有效轮廓频率满足目标轴速度和采样间隔要求。",
                                       "Effective profile rate meets the target axis speed and profile interval requirement."));
        } else {
            result.risks.append(text("有效轮廓频率不足以覆盖目标轴速度下的轮廓采样间隔。",
                                     "Effective profile rate is insufficient for the target axis speed and profile interval."));
        }
    }

    if (threeDHasValue(input.exposureTimeUs) && positive(result.profilePeriodUs)) {
        result.exposureWithinProfilePeriod = input.exposureTimeUs <= result.maxExposureTimeUs + 1e-9;
        if (result.exposureWithinProfilePeriod) {
            result.reasons.append(text("曝光时间未超过当前有效轮廓周期扣除读出/复位余量后的上限。",
                                       "Exposure time is within the effective profile period after readout/reset margin."));
        } else {
            result.risks.append(text("曝光时间超过当前轮廓周期允许值，会限制轮廓频率或导致触发丢失。",
                                     "Exposure time exceeds the current profile period limit and may reduce profile rate or lose triggers."));
        }
    }
    if (camera && threeDHasValue(input.exposureTimeUs)) {
        if (threeDHasValue(camera->exposureTimeMinUs) && input.exposureTimeUs + 1e-9 < camera->exposureTimeMinUs) {
            result.exposureWithinCameraRange = false;
            result.risks.append(text("曝光时间低于相机公开的最小曝光时间。",
                                     "Exposure time is below the camera's published minimum exposure time."));
        }
        if (threeDHasValue(camera->exposureTimeMaxUs) && input.exposureTimeUs - 1e-9 > camera->exposureTimeMaxUs) {
            result.exposureWithinCameraRange = false;
            result.risks.append(text("曝光时间超过相机公开的最大曝光时间。",
                                     "Exposure time exceeds the camera's published maximum exposure time."));
        }
    }

    if (camera) {
        if (result.samplingRateKnown && positive(result.effectiveProfileRateHz)) {
            result.samplingRateWithinCameraLimit = result.effectiveProfileRateHz <= result.cameraSamplingRateLimitHz + 1e-9;
            if (result.samplingRateWithinCameraLimit) {
                result.reasons.append(text("采样频率未超过相机公开速度上限。",
                                           "Sampling rate is within the camera's published speed limit."));
            } else {
                result.risks.append(text("采样频率超过相机公开速度上限。",
                                         "Sampling rate exceeds the camera's published speed limit."));
            }
        } else {
            result.risks.append(text("当前型号未公开最大扫描频率或帧率，速度上限需人工确认。",
                                     "The selected model does not publish max scan rate or frame rate; speed limit needs confirmation."));
        }
    }

    if (result.xPixelPitchKnown && positive(result.yPixelPitchMm)) {
        if (result.xyPitchRatio > 5.0) {
            result.risks.append(text("Y 向点距明显大于 X 向点距，运动方向可能采样不足。",
                                     "Y pitch is much larger than X pitch; sampling along motion direction may be insufficient."));
        } else if (result.xyPitchRatio < 0.2) {
            result.risks.append(text("Y 向点距明显小于 X 向点距，可能产生不必要的数据量。",
                                     "Y pitch is much smaller than X pitch; data volume may be unnecessarily high."));
        } else {
            result.reasons.append(text("X/Y 点距比例处于常规范围。",
                                       "X/Y pitch ratio is in a typical range."));
        }
    }

    if (result.profileCount > 100000.0) {
        result.risks.append(text("采集轮廓数较高，需要确认控制器缓存、传输和处理能力。",
                                 "Profile count is high; confirm controller buffer, transfer, and processing capacity."));
    }

    if (result.reasons.isEmpty() && result.risks.isEmpty())
        result.reasons.append(text("已按当前参数完成运动采样计算。",
                                   "Motion sampling calculation completed from current parameters."));
    return result;
}
