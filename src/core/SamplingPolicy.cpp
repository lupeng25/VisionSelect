#include "core/SamplingPolicy.h"

#include <algorithm>
#include <cmath>
#include <limits>

double SamplingPolicy::featurePixels(DetectionType type)
{
    switch (type) {
    case DetectionType::Measurement: return 5.0;
    case DetectionType::Positioning: return 4.0;
    case DetectionType::OcrCode: return 4.0;
    case DetectionType::DefectInspection: return 3.0;
    }
    return 3.0;
}

double SamplingPolicy::targetObjectPixelUm(const SelectionRequest &request)
{
    double target = std::numeric_limits<double>::infinity();
    if (std::isfinite(request.minFeatureUm) && request.minFeatureUm > 0.0)
        target = request.minFeatureUm / featurePixels(request.detectionType);
    if (request.detectionType == DetectionType::Measurement
        && std::isfinite(request.measurementToleranceUm) && request.measurementToleranceUm > 0.0)
        target = std::min(target, request.measurementToleranceUm / 5.0);
    // 不把严格要求提升为更宽松的下限；无约束时由调用方显示待补参数。
    return std::isfinite(target) ? target : 0.0;
}
