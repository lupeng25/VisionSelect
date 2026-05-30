#ifndef THREEDCAMERAMATCHER_H
#define THREEDCAMERAMATCHER_H

#include "three_d/ThreeDCameraTypes.h"

class ThreeDCameraMatcher
{
public:
    QVector<ThreeDCameraMatch> match(const ThreeDCameraRequirement &requirement, const QVector<ThreeDCameraSpec> &cameras) const;

private:
    ThreeDCameraMatch matchOne(const ThreeDCameraRequirement &requirement, const ThreeDCameraSpec &camera) const;
};

#endif
