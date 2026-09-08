#include "ui/FovDiagram.h"
#include "ui/UiHelpers.h"

#include <QPainter>
#include <cmath>

FovDiagram::FovDiagram(QWidget *parent) : QWidget(parent)
{
    setObjectName(QStringLiteral("FovDiagram"));
    setMinimumHeight(154);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}
void FovDiagram::setFields(QSizeF target, QSizeF actual, bool circular)
{
    m_target = target;
    m_actual = actual;
    m_circular = circular;
    update();
}
void FovDiagram::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const QRectF area = QRectF(rect()).adjusted(18, 12, -18, -35);
    const auto usable = [](QSizeF size) { return size.width() > 0.0 && size.height() > 0.0
        && std::isfinite(size.width()) && std::isfinite(size.height()); };
    if (!usable(m_actual)) {
        painter.setPen(palette().color(QPalette::PlaceholderText));
        painter.drawText(area, Qt::AlignCenter, UiHelpers::localizedText("计算后显示视场覆盖", "FOV coverage appears after calculation"));
        return;
    }
    const double maxW = qMax(m_actual.width(), usable(m_target) ? m_target.width() : 0.0);
    const double maxH = qMax(m_actual.height(), usable(m_target) ? m_target.height() : 0.0);
    const double scale = qMin(area.width() / maxW, area.height() / maxH);
    const auto centered = [&](QSizeF size) {
        QSizeF scaled(size.width() * scale, size.height() * scale);
        return QRectF(area.center() - QPointF(scaled.width() / 2, scaled.height() / 2), scaled);
    };
    QColor accent = palette().color(QPalette::Highlight);
    QColor fill = accent;
    fill.setAlpha(28);
    painter.setBrush(fill);
    painter.setPen(QPen(accent, 1.5));
    painter.drawRoundedRect(centered(m_actual), 3, 3);
    if (usable(m_target)) {
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(palette().color(QPalette::Text), 1.5, Qt::DashLine));
        if (m_circular) painter.drawEllipse(centered(m_target));
        else painter.drawRect(centered(m_target));
    }
    painter.setPen(palette().color(QPalette::Text));
    painter.drawText(QRectF(4, height() - 27, width() - 8, 24), Qt::AlignCenter,
        UiHelpers::localizedText("实线：当前视场    虚线：需求区域", "Solid: actual FOV    Dashed: required area"));
}
