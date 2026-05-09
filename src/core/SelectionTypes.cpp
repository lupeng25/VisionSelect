#include "core/SelectionTypes.h"

#include <QtMath>

double CameraSpec::sensorWidthMm() const
{
    return resolutionX * pixelSizeUm / 1000.0;
}

double CameraSpec::sensorHeightMm() const
{
    return resolutionY * pixelSizeUm / 1000.0;
}

double CameraSpec::sensorDiagonalMm() const
{
    return qSqrt(qPow(sensorWidthMm(), 2.0) + qPow(sensorHeightMm(), 2.0));
}

double CameraSpec::megapixels() const
{
    return resolutionX * resolutionY / 1000000.0;
}

bool CameraSpec::isMono() const
{
    return colorMode.compare(QStringLiteral("Mono"), Qt::CaseInsensitive) == 0
        || colorMode.contains(QString::fromUtf8("\351\273\221\347\231\275"), Qt::CaseInsensitive);
}

bool CameraSpec::isGlobalShutter() const
{
    return shutterType.compare(QStringLiteral("Global"), Qt::CaseInsensitive) == 0
        || shutterType.contains(QString::fromUtf8("\345\205\250\345\261\200"), Qt::CaseInsensitive);
}

bool LensSpec::isTelecentric() const
{
    return lensType == LensType::ObjectTelecentric || lensType == LensType::BiTelecentric;
}

QString LensSpec::typeLabel() const
{
    return lensTypeLabel(lensType);
}

QString LightSpec::typeLabel() const
{
    return lightTypeLabel(lightType);
}

bool SelectionResult::isTelecentric() const
{
    return lens.isTelecentric();
}

QString detectionTypeLabel(DetectionType type)
{
    switch (type) {
    case DetectionType::Measurement: return QString::fromUtf8("\345\260\272\345\257\270\346\265\213\351\207\217");
    case DetectionType::Positioning: return QString::fromUtf8("\345\256\232\344\275\215");
    case DetectionType::DefectInspection: return QString::fromUtf8("\347\274\272\351\231\267\346\243\200\346\265\213");
    case DetectionType::OcrCode: return QString::fromUtf8("OCR/\350\257\273\347\240\201");
    }
    return QString::fromUtf8("\346\234\252\347\237\245");
}

QString surfaceTypeLabel(SurfaceType type)
{
    switch (type) {
    case SurfaceType::Matte: return QString::fromUtf8("\345\223\221\345\205\211");
    case SurfaceType::ReflectiveMetal: return QString::fromUtf8("\345\217\215\345\205\211\351\207\221\345\261\236");
    case SurfaceType::GlassTransparent: return QString::fromUtf8("\347\216\273\347\222\203/\351\200\217\346\230\216\344\273\266");
    case SurfaceType::PCB: return QString::fromUtf8("PCB/\347\224\265\345\255\220\344\273\266");
    case SurfaceType::Plastic: return QString::fromUtf8("\345\241\221\346\226\231");
    case SurfaceType::Mixed: return QString::fromUtf8("\346\267\267\345\220\210\346\235\220\350\264\250");
    }
    return QString::fromUtf8("\346\234\252\347\237\245");
}

QString lensTypeLabel(LensType type)
{
    switch (type) {
    case LensType::FixedFocal: return QString::fromUtf8("\346\231\256\351\200\232\345\267\245\344\270\232\351\225\234\345\244\264");
    case LensType::ObjectTelecentric: return QString::fromUtf8("\347\211\251\346\226\271\350\277\234\345\277\203\351\225\234\345\244\264");
    case LensType::BiTelecentric: return QString::fromUtf8("\345\217\214\350\277\234\345\277\203\351\225\234\345\244\264");
    }
    return QString::fromUtf8("\346\234\252\347\237\245\351\225\234\345\244\264");
}

QString lightTypeLabel(LightType type)
{
    switch (type) {
    case LightType::Backlight: return QString::fromUtf8("\350\203\214\345\205\211");
    case LightType::Ring: return QString::fromUtf8("\347\216\257\345\275\242\345\205\211");
    case LightType::Bar: return QString::fromUtf8("\346\235\241\345\275\242\345\205\211");
    case LightType::Coaxial: return QString::fromUtf8("\345\220\214\350\275\264\345\205\211");
    case LightType::Dome: return QString::fromUtf8("\347\251\271\351\241\266\345\205\211");
    case LightType::TelecentricBacklight: return QString::fromUtf8("\350\277\234\345\277\203\345\271\263\350\241\214\350\203\214\345\205\211");
    case LightType::DarkField: return QString::fromUtf8("\346\232\227\345\234\272\345\205\211");
    }
    return QString::fromUtf8("\346\234\252\347\237\245\345\205\211\346\272\220");
}

QString detectionTypeKey(DetectionType type)
{
    switch (type) {
    case DetectionType::Measurement: return QStringLiteral("measurement");
    case DetectionType::Positioning: return QStringLiteral("positioning");
    case DetectionType::DefectInspection: return QStringLiteral("defect");
    case DetectionType::OcrCode: return QStringLiteral("ocr");
    }
    return QStringLiteral("unknown");
}

QString surfaceTypeKey(SurfaceType type)
{
    switch (type) {
    case SurfaceType::Matte: return QStringLiteral("matte");
    case SurfaceType::ReflectiveMetal: return QStringLiteral("reflective");
    case SurfaceType::GlassTransparent: return QStringLiteral("glass");
    case SurfaceType::PCB: return QStringLiteral("pcb");
    case SurfaceType::Plastic: return QStringLiteral("plastic");
    case SurfaceType::Mixed: return QStringLiteral("mixed");
    }
    return QStringLiteral("unknown");
}

DetectionType detectionTypeFromIndex(int index)
{
    switch (index) {
    case 0: return DetectionType::Measurement;
    case 1: return DetectionType::Positioning;
    case 2: return DetectionType::DefectInspection;
    case 3: return DetectionType::OcrCode;
    default: return DetectionType::Measurement;
    }
}

SurfaceType surfaceTypeFromIndex(int index)
{
    switch (index) {
    case 0: return SurfaceType::Matte;
    case 1: return SurfaceType::ReflectiveMetal;
    case 2: return SurfaceType::GlassTransparent;
    case 3: return SurfaceType::PCB;
    case 4: return SurfaceType::Plastic;
    case 5: return SurfaceType::Mixed;
    default: return SurfaceType::Mixed;
    }
}

LensType lensTypeFromString(const QString &value)
{
    const QString v = value.trimmed().toLower();
    if (v == QStringLiteral("objecttelecentric") || v.contains(QStringLiteral("object")))
        return LensType::ObjectTelecentric;
    if (v == QStringLiteral("bitelecentric") || v.contains(QStringLiteral("bi")))
        return LensType::BiTelecentric;
    return LensType::FixedFocal;
}

LightType lightTypeFromString(const QString &value)
{
    const QString v = value.trimmed().toLower();
    if (v.contains(QStringLiteral("telecentric"))) return LightType::TelecentricBacklight;
    if (v.contains(QStringLiteral("back"))) return LightType::Backlight;
    if (v.contains(QStringLiteral("coax"))) return LightType::Coaxial;
    if (v.contains(QStringLiteral("dome"))) return LightType::Dome;
    if (v.contains(QStringLiteral("bar"))) return LightType::Bar;
    if (v.contains(QStringLiteral("dark"))) return LightType::DarkField;
    return LightType::Ring;
}

QString boolLabel(bool value)
{
    return value ? QString::fromUtf8("\346\230\257") : QString::fromUtf8("\345\220\246");
}

bool parseBool(const QString &value)
{
    const QString v = value.trimmed().toLower();
    return v == QStringLiteral("true")
        || v == QStringLiteral("1")
        || v == QStringLiteral("yes")
        || v == QStringLiteral("y")
        || v == QString::fromUtf8("\346\230\257");
}

bool mountsCompatible(const QString &cameraMount, const QString &lensMount)
{
    const QString camera = cameraMount.trimmed().toUpper();
    const QString lens = lensMount.trimmed().toUpper();
    if (camera.isEmpty() || lens.isEmpty())
        return true;
    if (camera == lens)
        return true;
    if (camera == QStringLiteral("M42") && lens == QStringLiteral("C"))
        return true;
    return false;
}
