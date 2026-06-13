#include "core/SelectionTypes.h"

#include "i18n/LanguageManager.h"

#include <QtMath>

namespace {
QString selectionText(const char *zhUtf8, const char *enUtf8)
{
    return LanguageManager::instance().currentLanguage() == QLatin1String("en_US")
        ? QString::fromUtf8(enUtf8)
        : QString::fromUtf8(zhUtf8);
}

QString normalizedMount(const QString &value)
{
    QString v = value.trimmed().toUpper();
    v.replace(QStringLiteral("*"), QStringLiteral("X"));
    v.replace(QStringLiteral(" "), QString());
    v.replace(QStringLiteral("X P"), QStringLiteral("X"));
    v.replace(QStringLiteral("XP"), QStringLiteral("X"));
    const int detailSeparator = v.indexOf(QLatin1Char(','));
    if (detailSeparator >= 0)
        v = v.left(detailSeparator);
    if (v.isEmpty()
        || v == QStringLiteral("NONE")
        || v == QStringLiteral("N/A")
        || v == QStringLiteral("NA")
        || v == QStringLiteral("UNKNOWN")) {
        return QString();
    }

    if (v == QStringLiteral("C-MOUNT") || v == QStringLiteral("CMOUNT"))
        return QStringLiteral("C");
    if (v == QStringLiteral("CS-MOUNT") || v == QStringLiteral("CSMOUNT"))
        return QStringLiteral("CS");
    if (v == QStringLiteral("F-MOUNT") || v == QStringLiteral("FMOUNT") || v.startsWith(QStringLiteral("F-MOUN")))
        return QStringLiteral("F");
    return v;
}
}

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
    case DetectionType::Measurement: return selectionText("尺寸测量", "Measurement");
    case DetectionType::Positioning: return selectionText("定位", "Positioning");
    case DetectionType::DefectInspection: return selectionText("缺陷检测", "Defect Inspection");
    case DetectionType::OcrCode: return selectionText("OCR/读码", "OCR / Code Reading");
    }
    return selectionText("未知", "Unknown");
}

QString surfaceTypeLabel(SurfaceType type)
{
    switch (type) {
    case SurfaceType::Matte: return selectionText("哑光", "Matte");
    case SurfaceType::ReflectiveMetal: return selectionText("反光金属", "Reflective Metal");
    case SurfaceType::GlassTransparent: return selectionText("玻璃/透明件", "Glass / Transparent");
    case SurfaceType::PCB: return selectionText("PCB/电子件", "PCB / Electronics");
    case SurfaceType::Plastic: return selectionText("塑料", "Plastic");
    case SurfaceType::Mixed: return selectionText("混合材质", "Mixed Materials");
    }
    return selectionText("未知", "Unknown");
}

QString lensTypeLabel(LensType type)
{
    switch (type) {
    case LensType::FixedFocal: return selectionText("普通工业镜头", "Fixed-focal Industrial Lens");
    case LensType::ObjectTelecentric: return selectionText("物方远心镜头", "Object-side Telecentric Lens");
    case LensType::BiTelecentric: return selectionText("双远心镜头", "Bi-telecentric Lens");
    }
    return selectionText("未知镜头", "Unknown Lens");
}

QString lightTypeLabel(LightType type)
{
    switch (type) {
    case LightType::Backlight: return selectionText("背光", "Backlight");
    case LightType::Ring: return selectionText("环形光", "Ring Light");
    case LightType::Bar: return selectionText("条形光", "Bar Light");
    case LightType::Coaxial: return selectionText("同轴光", "Coaxial Light");
    case LightType::Dome: return selectionText("穹顶光", "Dome Light");
    case LightType::TelecentricBacklight: return selectionText("远心平行背光", "Telecentric Backlight");
    case LightType::DarkField: return selectionText("暗场光", "Dark-field Light");
    }
    return selectionText("未知光源", "Unknown Light");
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
    if (v.contains(QStringLiteral("non-telecentric"))
        || v.contains(QStringLiteral("non telecentric"))
        || v.contains(QStringLiteral("not telecentric"))) {
        return LensType::FixedFocal;
    }
    if (v == QStringLiteral("bitelecentric")
        || v.contains(QStringLiteral("bi-telecentric"))
        || v.contains(QStringLiteral("bi telecentric"))
        || v.contains(QStringLiteral("double telecentric"))
        || v.contains(QString::fromUtf8("\345\217\214\350\277\234\345\277\203"))) {
        return LensType::BiTelecentric;
    }
    if (v == QStringLiteral("objecttelecentric")
        || v == QStringLiteral("telecentric")
        || v.contains(QStringLiteral("object-side telecentric"))
        || v.contains(QStringLiteral("object side telecentric"))
        || v.contains(QStringLiteral("object telecentric"))
        || v.contains(QStringLiteral("telecentric"))
        || v.contains(QString::fromUtf8("\347\211\251\346\226\271\350\277\234\345\277\203"))
        || v.contains(QString::fromUtf8("\350\277\234\345\277\203"))) {
        return LensType::ObjectTelecentric;
    }
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
    return value ? selectionText("是", "Yes") : selectionText("否", "No");
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
    const QString camera = normalizedMount(cameraMount);
    const QString lens = normalizedMount(lensMount);
    if (camera.isEmpty() || lens.isEmpty())
        return false;
    if (camera == lens)
        return true;
    return false;
}
