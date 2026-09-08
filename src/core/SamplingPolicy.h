#ifndef SAMPLINGPOLICY_H
#define SAMPLINGPOLICY_H

#include "core/SelectionTypes.h"

// 需求估算、目录预筛和最终校核共用的经验采样策略。
namespace SamplingPolicy {
double featurePixels(DetectionType type);
double targetObjectPixelUm(const SelectionRequest &request);
}

#endif
