#ifndef THREEDCAMERAREPOSITORY_H
#define THREEDCAMERAREPOSITORY_H

#include "three_d/ThreeDCameraTypes.h"

class ThreeDCameraRepository
{
public:
    void setStorageDirectory(const QString &directory);
    QString storageDirectory() const;

    bool loadFromResource(const QString &resourcePath = QStringLiteral(":/data/three_d_cameras.json"), QString *error = nullptr);
    bool addCamera(const ThreeDCameraSpec &camera, QString *error = nullptr);
    bool updateCamera(int index, const ThreeDCameraSpec &camera, QString *error = nullptr);
    bool removeCamera(int index, QString *error = nullptr);
    const QVector<ThreeDCameraSpec> &cameras() const { return m_cameras; }
    QStringList manufacturers() const;
    QStringList interfaces() const;
    QStringList ipRatings() const;
    QStringList materialScenarios() const;

private:
    QVector<ThreeDCameraSpec> m_builtInCameras;
    QVector<ThreeDCameraSpec> m_userCameras;
    QVector<ThreeDCameraSpec> m_cameras;
    QString m_storageDirectory;

    QString effectiveStorageDirectory() const;
    QString userCameraStoragePath() const;
    bool loadUserCameras(QString *error);
    bool saveUserCameras(QString *error) const;
    void rebuildMergedCameras();
};

#endif
