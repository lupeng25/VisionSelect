#ifndef PIXELFORMAT_H
#define PIXELFORMAT_H

#include <QString>
#include <optional>

namespace PixelFormat {
struct Layout {
    int storageBitsPerPixel = 0;
    bool packed = false;
};

// 只接受明确支持的传输格式；普通 Mono/Color 类型不等同于像素格式。
std::optional<Layout> layout(const QString &name);
std::optional<double> frameBytes(int width, int height, const QString &name);
}

#endif
