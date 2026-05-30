#ifndef THREEDCAMERAREPOSITORY_H
#define THREEDCAMERAREPOSITORY_H

#include "three_d/ThreeDCameraTypes.h"

class ThreeDCameraRepository
{
public:
    bool loadFromResource(const QString &resourcePath = QStringLiteral(":/data/three_d_cameras.json"), QString *error = nullptr);
    const QVector<ThreeDCameraSpec> &cameras() const { return m_cameras; }
    QStringList manufacturers() const;
    QStringList interfaces() const;
    QStringList ipRatings() const;
    QStringList materialScenarios() const;

private:
    QVector<ThreeDCameraSpec> m_cameras;
};

#endif
