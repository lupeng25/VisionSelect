#include "core/PixelFormat.h"

#include <QRegularExpression>
#include <cmath>

std::optional<PixelFormat::Layout> PixelFormat::layout(const QString &name)
{
    QString normalized = name.trimmed().toLower();
    normalized.remove(QLatin1Char(' '));
    normalized.remove(QLatin1Char('_'));
    if (normalized == QLatin1String("yuyv")) return Layout{16, false};
    // 大多数旧目录只有 Mono / Color 等类型，不必在每次组合评分中运行正则解析。
    if (normalized.isEmpty() || (!normalized.back().isDigit() && normalized.back() != QLatin1Char('p')))
        return std::nullopt;
    static const QRegularExpression mono(QStringLiteral("^(mono|bayer(?:rg|bg|gr|gb)?)(8|10|12|14|16)(p)?$"));
    const auto match = mono.match(normalized);
    if (match.hasMatch()) {
        const int bits = match.captured(2).toInt();
        const bool packed = !match.captured(3).isEmpty();
        return Layout{packed ? bits : ((bits + 7) / 8) * 8, packed};
    }
    static const QRegularExpression rgb(QStringLiteral("^(rgb|bgr|rgba|bgra)(8|10|12|16)$"));
    const auto color = rgb.match(normalized);
    if (color.hasMatch()) {
        const int channels = color.captured(1).size() == 4 ? 4 : 3;
        return Layout{channels * ((color.captured(2).toInt() + 7) / 8) * 8, false};
    }
    if (normalized == QLatin1String("yuv422") || normalized == QLatin1String("yuyv")
        || normalized == QLatin1String("ycbcr4228"))
        return Layout{16, false};
    return std::nullopt;
}

std::optional<double> PixelFormat::frameBytes(int width, int height, const QString &name)
{
    const auto format = layout(name);
    if (!format || width <= 0 || height <= 0)
        return std::nullopt;
    // 每行结束补足字节；设备额外 stride/padding 需由传输开销单独描述。
    const double rowBytes = std::ceil(static_cast<double>(width) * format->storageBitsPerPixel / 8.0);
    return rowBytes * static_cast<double>(height);
}
