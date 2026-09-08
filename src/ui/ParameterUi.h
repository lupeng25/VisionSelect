#ifndef PARAMETERUI_H
#define PARAMETERUI_H

#include "selection/ParameterCalculator.h"
#include "ui/UiHelpers.h"
#include <QSizeF>
#include <cmath>

namespace ParameterUi {
using Parameters::Number;
using UiHelpers::localizedText;
inline const QStringList taskKeys = {"optics", "sampling", "check", "tele", "motion", "data"};
inline QStringList taskLabels() {
    return {localizedText("视场与焦距", "FOV / focal length"), localizedText("分辨率与采样", "Resolution / sampling"),
            localizedText("方案校核", "System check"), localizedText("远心倍率", "Telecentric"),
            localizedText("运动与曝光", "Motion / exposure"), localizedText("带宽与存储", "Transfer / storage")};
}
inline QStringList formats() {
    return {"", "Mono8", "Mono10", "Mono10p", "Mono12", "Mono12p", "Mono16", "BayerRG8", "BayerRG12", "BayerRG12p", "RGB8"};
}
inline QStringList formatLabels() {
    auto values = formats();
    values[0] = localizedText("未确认像素格式", "Pixel format unconfirmed");
    for (auto &value : values) {
        if (value.endsWith(QLatin1String("12")) || value.endsWith(QLatin1String("10")))
            value += localizedText(" · 16 位容器", " · 16-bit container");
        else if (value.endsWith(QLatin1Char('p'))) value += localizedText(" · 打包", " · packed");
    }
    return values;
}
inline QString valueText(Number value, const QString &unit = QString()) {
    if (!value || !std::isfinite(*value)) return QStringLiteral("—");
    return QString::number(*value, 'g', 8) + (unit.isEmpty() ? QString() : QLatin1Char(' ') + unit);
}
inline bool usable(Number value) { return value && std::isfinite(*value) && *value > 0.0; }
inline QSizeF sizeOf(Number width, Number height) {
    return usable(width) && usable(height) ? QSizeF(*width, *height) : QSizeF();
}
inline QString stateProperty(CalculationStatus status) {
    if (status == CalculationStatus::Failed || status == CalculationStatus::Invalid) return QStringLiteral("danger");
    if (status == CalculationStatus::Unknown) return QStringLiteral("warning");
    return QStringLiteral("info");
}
}

#endif
