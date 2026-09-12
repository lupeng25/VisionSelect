#ifndef CANDIDATEVALIDATOR_H
#define CANDIDATEVALIDATOR_H

#include "core/SelectionTypes.h"

namespace CandidateValidator {
CandidateChecks lens(const SelectionRequest &request, const CameraSpec &camera, const LensSpec &lens,
                     double fovWidth, double fovHeight, double objectPixel, double dof);
void camera(const SelectionRequest &request, const CameraSpec &camera, double bandwidth,
            double capacity, CandidateChecks *checks);
}

#endif
