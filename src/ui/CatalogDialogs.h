#ifndef CATALOGDIALOGS_H
#define CATALOGDIALOGS_H

#include "core/SelectionTypes.h"
#include "three_d/ThreeDCameraTypes.h"

class QWidget;

bool editCameraDialog(QWidget *parent, CameraSpec *camera, const QString &title);
bool editLensDialog(QWidget *parent, LensSpec *lens, const QString &title);
bool editLightDialog(QWidget *parent, LightSpec *light, const QString &title);
bool editThreeDCameraDialog(QWidget *parent, ThreeDCameraSpec *camera, const QString &title);

#endif
