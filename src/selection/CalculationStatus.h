#ifndef CALCULATIONSTATUS_H
#define CALCULATIONSTATUS_H

#include <algorithm>
#include <cmath>
#include <optional>

enum class CalculationStatus { Passed, Failed, Unknown, NotApplicable, Invalid };

inline CalculationStatus checkUpperBound(std::optional<double> actual, std::optional<double> maximum)
{
    if ((actual && (!std::isfinite(*actual) || *actual < 0.0))
        || (maximum && (!std::isfinite(*maximum) || *maximum < 0.0)))
        return CalculationStatus::Invalid;
    if (!actual || !maximum)
        return CalculationStatus::Unknown;
    const double tolerance = 1e-10 * std::max(std::abs(*actual), std::abs(*maximum));
    return *actual <= *maximum + tolerance ? CalculationStatus::Passed : CalculationStatus::Failed;
}

#endif
