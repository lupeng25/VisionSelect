#include "report/PdfReportWriter.h"

#include <QDateTime>
#include <QFont>
#include <QPainter>
#include <QPdfWriter>
#include <QTextOption>

namespace {
class PdfPage
{
public:
    PdfPage(QPdfWriter *writer, QPainter *painter)
        : m_writer(writer), m_painter(painter)
    {
        m_margin = 48;
        m_y = m_margin;
        m_width = writer->width() - m_margin * 2;
    }

    void title(const QString &text)
    {
        QFont font(QStringLiteral("Microsoft YaHei"), 18, QFont::Bold);
        m_painter->setFont(font);
        m_painter->setPen(QColor(15, 23, 42));
        drawText(text, 32);
        m_y += 8;
    }

    void section(const QString &text)
    {
        ensure(38);
        QFont font(QStringLiteral("Microsoft YaHei"), 12, QFont::Bold);
        m_painter->setFont(font);
        m_painter->setPen(QColor(30, 138, 138));
        drawText(text, 24);
        m_y += 4;
    }

    void paragraph(const QString &text)
    {
        ensure(54);
        QFont font(QStringLiteral("Microsoft YaHei"), 9);
        m_painter->setFont(font);
        m_painter->setPen(QColor(31, 41, 55));
        QRect rect(m_margin, m_y, m_width, 70);
        QTextOption option;
        option.setWrapMode(QTextOption::WordWrap);
        m_painter->drawText(rect, text, option);
        m_y += qMax(28, text.size() / 35 * 16 + 22);
    }

    void keyValue(const QString &key, const QString &value)
    {
        ensure(26);
        QFont keyFont(QStringLiteral("Microsoft YaHei"), 9, QFont::Bold);
        m_painter->setFont(keyFont);
        m_painter->setPen(QColor(51, 65, 85));
        m_painter->drawText(m_margin, m_y, 155, 22, Qt::AlignLeft | Qt::AlignVCenter, key);

        QFont valueFont(QStringLiteral("Microsoft YaHei"), 9);
        m_painter->setFont(valueFont);
        m_painter->setPen(QColor(15, 23, 42));
        m_painter->drawText(m_margin + 160, m_y, m_width - 160, 22, Qt::AlignLeft | Qt::AlignVCenter, value);
        m_y += 24;
    }

    void tableHeader(const QStringList &headers, const QVector<int> &widths)
    {
        ensure(32);
        int x = m_margin;
        m_painter->fillRect(m_margin, m_y, m_width, 26, QColor(237, 242, 248));
        QFont font(QStringLiteral("Microsoft YaHei"), 8, QFont::Bold);
        m_painter->setFont(font);
        m_painter->setPen(QColor(51, 65, 85));
        for (int i = 0; i < headers.size(); ++i) {
            m_painter->drawText(x + 4, m_y, widths.value(i, 80) - 8, 26,
                                Qt::AlignLeft | Qt::AlignVCenter, headers.at(i));
            x += widths.value(i, 80);
        }
        m_y += 26;
    }

    void tableRow(const QStringList &values, const QVector<int> &widths)
    {
        ensure(34);
        int x = m_margin;
        QFont font(QStringLiteral("Microsoft YaHei"), 8);
        m_painter->setFont(font);
        m_painter->setPen(QColor(31, 41, 55));
        m_painter->drawLine(m_margin, m_y, m_margin + m_width, m_y);
        for (int i = 0; i < values.size(); ++i) {
            m_painter->drawText(x + 4, m_y + 2, widths.value(i, 80) - 8, 30,
                                Qt::AlignLeft | Qt::AlignVCenter, values.at(i));
            x += widths.value(i, 80);
        }
        m_y += 32;
    }

private:
    QPdfWriter *m_writer;
    QPainter *m_painter;
    int m_margin;
    int m_y;
    int m_width;

    void drawText(const QString &text, int height)
    {
        m_painter->drawText(m_margin, m_y, m_width, height,
                            Qt::AlignLeft | Qt::AlignVCenter, text);
        m_y += height;
    }

    void ensure(int height)
    {
        if (m_y + height < m_writer->height() - m_margin)
            return;
        m_writer->newPage();
        m_y = m_margin;
    }
};

QString value(double number, const QString &unit)
{
    return QStringLiteral("%1 %2").arg(QString::number(number, 'f', 2), unit);
}
}

bool PdfReportWriter::write(const QString &filePath,
                            const SelectionRequest &request,
                            const QVector<SelectionResult> &results,
                            QString *errorMessage) const
{
    if (filePath.isEmpty()) {
        if (errorMessage)
            *errorMessage = QString::fromUtf8("PDF \350\276\223\345\207\272\350\267\257\345\276\204\344\270\272\347\251\272");
        return false;
    }

    QPdfWriter writer(filePath);
    writer.setPageSize(QPagedPaintDevice::A4);
    writer.setResolution(96);
    writer.setTitle(QString::fromUtf8("\345\267\245\344\270\232\346\234\272\345\231\250\350\247\206\350\247\211\351\200\211\345\236\213\346\212\245\345\221\212"));

    QPainter painter(&writer);
    if (!painter.isActive()) {
        if (errorMessage)
            *errorMessage = QString::fromUtf8("\346\227\240\346\263\225\345\210\233\345\273\272 PDF\357\274\232%1").arg(filePath);
        return false;
    }

    PdfPage page(&writer, &painter);
    page.title(QString::fromUtf8("\345\267\245\344\270\232\346\234\272\345\231\250\350\247\206\350\247\211\351\200\211\345\236\213\346\212\245\345\221\212"));
    page.paragraph(QString::fromUtf8("\347\224\237\346\210\220\346\227\266\351\227\264\357\274\232")
        + QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm"))
        + QString::fromUtf8("\343\200\202\346\234\254\346\212\245\345\221\212\347\224\250\344\272\216\346\226\271\346\241\210\345\210\235\347\255\233\357\274\214\346\234\200\347\273\210\345\236\213\345\217\267\350\257\267\347\273\223\345\220\210\345\216\202\345\256\266\346\225\260\346\215\256\350\241\250\343\200\201\345\256\211\350\243\205\347\251\272\351\227\264\343\200\201MTF/DOF \345\222\214\347\216\260\345\234\272\346\211\223\345\205\211\345\256\236\346\265\213\347\241\256\350\256\244\343\200\202"));

    page.section(QString::fromUtf8("\351\234\200\346\261\202\350\276\223\345\205\245"));
    page.keyValue(QString::fromUtf8("\346\243\200\346\265\213\347\261\273\345\236\213"), detectionTypeLabel(request.detectionType));
    page.keyValue(QString::fromUtf8("\345\267\245\344\273\266\345\260\272\345\257\270"), QStringLiteral("%1 x %2 mm")
                  .arg(request.objectWidthMm, 0, 'f', 2)
                  .arg(request.objectHeightMm, 0, 'f', 2));
    page.keyValue(QString::fromUtf8("\345\256\232\344\275\215\344\275\231\351\207\217"), value(request.placementMarginMm, QStringLiteral("mm")));
    page.keyValue(QString::fromUtf8("\346\234\200\345\260\217\347\211\271\345\276\201"), value(request.minFeatureUm, QStringLiteral("um")));
    page.keyValue(QString::fromUtf8("\345\205\201\350\256\270\350\257\257\345\267\256"), value(request.measurementToleranceUm, QStringLiteral("um")));
    page.keyValue(QString::fromUtf8("\345\267\245\344\275\234\350\267\235\347\246\273"), value(request.workingDistanceMm, QStringLiteral("mm")));
    page.keyValue(QString::fromUtf8("\351\253\230\345\272\246\346\263\242\345\212\250"), value(request.heightVariationMm, QStringLiteral("mm")));
    page.keyValue(QString::fromUtf8("\350\277\220\345\212\250\351\200\237\345\272\246/\345\270\247\347\216\207"), QStringLiteral("%1 mm/s, %2 fps")
                  .arg(request.motionSpeedMmS, 0, 'f', 1)
                  .arg(request.requiredFps, 0, 'f', 1));
    page.keyValue(QString::fromUtf8("\350\241\250\351\235\242\347\211\271\346\200\247"), surfaceTypeLabel(request.surfaceType));

    page.section(QString::fromUtf8("\345\205\263\351\224\256\345\205\254\345\274\217\345\222\214\345\210\244\346\226\255"));
    page.paragraph(QString::fromUtf8("\346\231\256\351\200\232\345\267\245\344\270\232\351\225\234\345\244\264\346\214\211 M = SensorSize / FOV \345\222\214 f \342\211\210 WD \303\227 SensorSize / (FOV + SensorSize) \345\210\235\347\255\233\357\274\214\345\271\266\346\240\241\351\252\214\345\203\217\345\234\206\343\200\201\346\216\245\345\217\243\343\200\201\345\267\245\344\275\234\350\267\235\347\246\273\343\200\201\347\225\270\345\217\230\345\222\214\351\225\234\345\244\264 MP/\345\203\217\345\205\203\350\203\275\345\212\233\343\200\202"));
    page.paragraph(QString::fromUtf8("\350\277\234\345\277\203\351\225\234\345\244\264\346\214\211\345\233\272\345\256\232\345\200\215\347\216\207\346\250\241\345\236\213\350\256\241\347\256\227\357\274\232FOV = SensorSize / PMAG\357\274\214ObjectPixel = PixelSize / PMAG\357\274\233\345\220\214\346\227\266\346\240\241\351\252\214\346\240\207\347\247\260 WD\343\200\201WD \345\256\271\345\267\256\343\200\201\345\203\217\345\234\206/\346\234\200\345\244\247\351\235\266\351\235\242\343\200\201\350\277\234\345\277\203\345\272\246\343\200\201\347\225\270\345\217\230\343\200\201DOF \345\222\214\345\220\214\350\275\264/\350\277\234\345\277\203\345\205\211\346\272\220\351\200\202\351\205\215\343\200\202"));

    page.section(QString::fromUtf8("\346\216\250\350\215\220\346\226\271\346\241\210 Top \346\226\271\346\241\210"));
    const QVector<int> widths = {52, 58, 105, 110, 90, 82, 72, 95};
    page.tableHeader({QString::fromUtf8("\347\261\273\345\236\213"), QString::fromUtf8("\345\276\227\345\210\206"), QString::fromUtf8("\347\233\270\346\234\272"),
                      QString::fromUtf8("\351\225\234\345\244\264"), QString::fromUtf8("\345\205\211\346\272\220"), QStringLiteral("FOV"),
                      QString::fromUtf8("\345\203\217\347\264\240"), QString::fromUtf8("\351\243\216\351\231\251")},
                     widths);

    const int count = qMin(8, results.size());
    for (int i = 0; i < count; ++i) {
        const SelectionResult &r = results.at(i);
        page.tableRow({
            r.isTelecentric() ? QString::fromUtf8("\350\277\234\345\277\203") : QString::fromUtf8("\346\231\256\351\200\232"),
            QString::number(r.score.score, 'f', 1),
            r.camera.model,
            r.lens.model,
            r.light.model,
            QStringLiteral("%1x%2").arg(r.effectiveFovWidthMm, 0, 'f', 1).arg(r.effectiveFovHeightMm, 0, 'f', 1),
            QStringLiteral("%1um").arg(r.objectPixelSizeUm, 0, 'f', 1),
            r.score.risks.isEmpty() ? QString::fromUtf8("\346\227\240\344\270\273\350\246\201\351\243\216\351\231\251") : r.score.risks.join(QString::fromUtf8("\357\274\233")).left(42)
        }, widths);
    }

    if (!results.isEmpty()) {
        const SelectionResult &top = results.first();
        page.section(QString::fromUtf8("\351\246\226\351\200\211\346\226\271\346\241\210\350\257\264\346\230\216"));
        page.keyValue(QString::fromUtf8("\346\226\271\346\241\210"), top.schemeTitle + QString::fromUtf8("\357\274\232")
                      + top.camera.model + QStringLiteral(" + ")
                      + top.lens.model + QStringLiteral(" + ")
                      + top.light.model);
        page.keyValue(QString::fromUtf8("\350\256\241\347\256\227\350\247\206\351\207\216"), QStringLiteral("%1 x %2 mm")
                      .arg(top.effectiveFovWidthMm, 0, 'f', 2)
                      .arg(top.effectiveFovHeightMm, 0, 'f', 2));
        page.keyValue(QString::fromUtf8("\347\211\251\346\226\271\345\203\217\347\264\240"), value(top.objectPixelSizeUm, QStringLiteral("um/px")));
        page.keyValue(QString::fromUtf8("\345\200\215\347\216\207/\347\204\246\350\267\235"), top.isTelecentric()
                      ? QStringLiteral("PMAG %1x").arg(top.magnification, 0, 'f', 3)
                      : value(top.lens.focalLengthMm, QStringLiteral("mm")));
        page.paragraph(top.score.reasons.join(QString::fromUtf8("\357\274\233")));
        if (!top.score.risks.isEmpty())
            page.paragraph(QString::fromUtf8("\351\243\216\351\231\251\346\217\220\347\244\272\357\274\232") + top.score.risks.join(QString::fromUtf8("\357\274\233")));
    }

    page.section(QString::fromUtf8("\350\265\204\346\226\231\344\276\235\346\215\256"));
    page.paragraph(QString::fromUtf8("\350\277\234\345\277\203\351\225\234\345\244\264\345\233\272\345\256\232\345\200\215\347\216\207\344\270\216\346\227\240\350\247\206\345\267\256\346\265\213\351\207\217\345\217\202\350\200\203\357\274\232Opto Engineering\343\200\201Edmund Optics\343\200\201STEMMER Imaging\357\274\233\347\233\270\346\234\272\345\210\206\350\276\250\347\216\207\343\200\201\345\277\253\351\227\250\343\200\201\345\270\247\347\216\207\343\200\201\346\216\245\345\217\243\345\217\202\350\200\203 Basler\357\274\233\351\225\234\345\244\264\345\210\206\350\276\250\347\216\207/\345\203\217\345\205\203\345\214\271\351\205\215\345\217\202\350\200\203 Teledyne FLIR\357\274\233\345\205\211\346\272\220\347\255\226\347\225\245\345\217\202\350\200\203 Cognex\343\200\202"));

    painter.end();
    return true;
}
