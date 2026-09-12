#include "selection/CandidateValidator.h"
#include "selection/SelectionEngine.h"
#include "core/PixelFormat.h"
#include <cmath>

namespace {
using State = CandidateCheckState;
State lowerBound(double actual, double minimum)
{
    if (!std::isfinite(actual) || !std::isfinite(minimum) || actual <= 0.0 || minimum <= 0.0)
        return State::Unknown;
    return actual + 1e-10 * qMax(actual, minimum) >= minimum ? State::Passed : State::Failed;
}
State combined(State a, State b)
{
    return static_cast<State>(qMax(static_cast<int>(a), static_cast<int>(b)));
}
}

CandidateChecks CandidateValidator::lens(const SelectionRequest &request, const CameraSpec &camera,
    const LensSpec &lens, double fovWidth, double fovHeight, double objectPixel, double dof)
{
    CandidateChecks checks;
    checks[CandidateCheck::FieldOfView] = combined(lowerBound(fovWidth, SelectionEngine::requiredFovWidth(request)),
        lowerBound(fovHeight, SelectionEngine::requiredFovHeight(request)));
    checks[CandidateCheck::Sampling] = lowerBound(SelectionEngine::targetObjectPixelUm(request), objectPixel);
    checks[CandidateCheck::ImageCircle] = lowerBound(lens.imageCircleMm, camera.sensorDiagonalMm());
    if (lens.maxSensorDiagonalMm > 0.0)
        checks[CandidateCheck::ImageCircle] = combined(checks[CandidateCheck::ImageCircle],
            lowerBound(lens.maxSensorDiagonalMm, camera.sensorDiagonalMm()));
    checks[CandidateCheck::Mount] = camera.lensMount.trimmed().isEmpty() || lens.lensMount.trimmed().isEmpty()
        ? State::Unknown : (mountsCompatible(camera.lensMount, lens.lensMount) ? State::Passed : State::Failed);

    State wd = State::Unknown;
    if (lens.isTelecentric()) {
        if (lens.pmag <= 0.0 || !std::isfinite(lens.pmag))
            checks[CandidateCheck::FieldOfView] = checks[CandidateCheck::Sampling] = State::Failed;
        if (lens.nominalWorkingDistanceMm > 0.0 && request.workingDistanceMm > 0.0) {
            const double difference = qAbs(request.workingDistanceMm - lens.nominalWorkingDistanceMm);
            wd = lens.workingDistanceToleranceMm > 0.0
                ? (difference <= lens.workingDistanceToleranceMm ? State::Passed : State::Failed)
                : (difference <= 0.5 ? State::Unknown : State::Failed);
        }
    } else {
        if (lens.focalLengthMm <= 0.0 || request.workingDistanceMm <= lens.focalLengthMm) {
            wd = State::Failed;
            checks[CandidateCheck::FieldOfView] = checks[CandidateCheck::Sampling] = State::Failed;
        } else {
            wd = lowerBound(request.workingDistanceMm, lens.minWorkingDistanceMm);
        }
    }
    checks[CandidateCheck::WorkingDistance] = wd;
    if (request.heightVariationMm > 0.0)
        checks[CandidateCheck::DepthOfField] = lens.dofConditionsConfirmed
            ? lowerBound(dof, request.heightVariationMm * 1.5) : State::Unknown;
    return checks;
}

void CandidateValidator::camera(const SelectionRequest &request, const CameraSpec &camera,
    double bandwidth, double capacity, CandidateChecks *checks)
{
    (*checks)[CandidateCheck::FrameRate] = request.requiredFps > 0.0
        ? lowerBound(camera.maxFps, request.requiredFps) : State::NotApplicable;
    (*checks)[CandidateCheck::Bandwidth] = camera.bandwidthMBps > 0.0 && camera.bandwidthSource == QLatin1String("specified")
        && PixelFormat::layout(camera.transportPixelFormat())
        ? lowerBound(capacity, bandwidth) : State::Unknown;
    (*checks)[CandidateCheck::PixelFormat] = PixelFormat::layout(camera.transportPixelFormat()) ? State::Passed : State::Unknown;
    if (request.hasContinuousMotion())
        (*checks)[CandidateCheck::GlobalShutter] = camera.shutterType.trimmed().isEmpty()
            ? State::Unknown : (camera.isGlobalShutter() ? State::Passed : State::Failed);
}
