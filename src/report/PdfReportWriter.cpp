#include "report/PdfReportWriter.h"

#include "ui/UiHelpers.h"

#include <QDateTime>
#include <QFont>
#include <QPainter>
#include <QPdfWriter>
#include <QTextOption>
#include <QtGlobal>

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

QString lt(const char *zhUtf8, const char *enUtf8)
{
    return UiHelpers::localizedText(zhUtf8, enUtf8);
}

QString value(double number, const QString &unit)
{
    return QStringLiteral("%1 %2").arg(QString::number(number, 'f', 2), unit);
}

QString bomSpecForCamera(const CameraSpec &camera, const SelectionResult &result)
{
    return QStringLiteral("%1 x %2, %3 um, %4, %5 fps, bandwidth %6 MB/s")
        .arg(camera.resolutionX)
        .arg(camera.resolutionY)
        .arg(camera.pixelSizeUm, 0, 'f', 2)
        .arg(camera.interfaceType)
        .arg(camera.maxFps, 0, 'f', 1)
        .arg(result.interfaceCapacityMBps, 0, 'f', 1);
}

QString bomSpecForLens(const LensSpec &lens, const SelectionResult &result)
{
    if (lens.isTelecentric()) {
        return QStringLiteral("%1, PMAG %2x, WD %3 mm, DOF %4 mm")
            .arg(lens.typeLabel())
            .arg(result.magnification, 0, 'f', 3)
            .arg(lens.nominalWorkingDistanceMm, 0, 'f', 1)
            .arg(result.estimatedDofMm, 0, 'f', 2);
    }
    return QStringLiteral("%1, f %2 mm, min WD %3 mm, DOF %4 mm")
        .arg(lens.typeLabel())
        .arg(lens.focalLengthMm, 0, 'f', 1)
        .arg(lens.minWorkingDistanceMm, 0, 'f', 1)
        .arg(result.estimatedDofMm, 0, 'f', 2);
}

QString bomSpecForLight(const LightSpec &light, const SelectionResult &result)
{
    return QStringLiteral("%1, %2, %3, %4 x %5 mm, margin %6%")
        .arg(light.typeLabel())
        .arg(light.color)
        .arg(light.mode)
        .arg(light.activeWidthMm, 0, 'f', 0)
        .arg(light.activeHeightMm, 0, 'f', 0)
        .arg(result.lightCoverageMarginPercent, 0, 'f', 0);
}
}

bool PdfReportWriter::write(const QString &filePath,
                            const SelectionRequest &request,
                            const QVector<SelectionResult> &results,
                            QString *errorMessage) const
{
    if (filePath.isEmpty()) {
        if (errorMessage)
            *errorMessage = lt("PDF 输出路径为空", "PDF output path is empty");
        return false;
    }

    QPdfWriter writer(filePath);
    writer.setPageSize(QPagedPaintDevice::A4);
    writer.setResolution(96);
    writer.setTitle(lt("工业机器视觉选型报告", "Industrial Machine Vision Selection Report"));

    QPainter painter(&writer);
    if (!painter.isActive()) {
        if (errorMessage)
            *errorMessage = lt("无法创建 PDF：%1", "Unable to create PDF: %1").arg(filePath);
        return false;
    }

    PdfPage page(&writer, &painter);
    page.title(lt("工业机器视觉选型报告", "Industrial Machine Vision Selection Report"));
    page.paragraph(lt("生成时间：%1。本报告用于方案初筛，最终型号请结合厂家数据表、安装空间、MTF/DOF 和现场打光实测确认。",
                      "Generated at %1. This report is for preliminary selection; final models should be verified with vendor datasheets, installation space, MTF/DOF, and on-site lighting tests.")
        .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm"))));

    page.section(lt("需求输入", "Requirements"));
    page.keyValue(lt("检测类型", "Inspection type"), detectionTypeLabel(request.detectionType));
    page.keyValue(lt("工件尺寸", "Part size"), QStringLiteral("%1 x %2 mm")
                  .arg(request.objectWidthMm, 0, 'f', 2)
                  .arg(request.objectHeightMm, 0, 'f', 2));
    page.keyValue(lt("定位余量", "Placement margin"), value(request.placementMarginMm, QStringLiteral("mm")));
    page.keyValue(lt("最小特征", "Minimum feature"), value(request.minFeatureUm, QStringLiteral("um")));
    page.keyValue(lt("允许误差", "Allowed error"), value(request.measurementToleranceUm, QStringLiteral("um")));
    page.keyValue(lt("工作距离", "Working distance"), value(request.workingDistanceMm, QStringLiteral("mm")));
    page.keyValue(lt("高度波动", "Height variation"), value(request.heightVariationMm, QStringLiteral("mm")));
    page.keyValue(lt("运动速度/帧率", "Motion speed / frame rate"), QStringLiteral("%1 mm/s, %2 fps")
                  .arg(request.motionSpeedMmS, 0, 'f', 1)
                  .arg(request.requiredFps, 0, 'f', 1));
    page.keyValue(lt("表面特性", "Surface"), surfaceTypeLabel(request.surfaceType));

    page.section(lt("关键公式和判断", "Key Formulas and Checks"));
    page.paragraph(lt("普通工业镜头按 M = SensorSize / FOV 和 f ~= WD x SensorSize / (FOV + SensorSize) 初筛，并校验像圈、接口、工作距离、畸变和镜头 MP/像元能力。",
                      "Fixed-focal lenses are estimated with M = SensorSize / FOV and f ~= WD x SensorSize / (FOV + SensorSize), then checked against image circle, mount, working distance, distortion, and lens MP/pixel capability."));
    page.paragraph(lt("远心镜头按固定倍率模型计算：FOV = SensorSize / PMAG，ObjectPixel = PixelSize / PMAG；同时校验标称 WD、WD 容差、像圈/最大靶面、远心度、畸变、DOF 和同轴/远心光源适配。",
                      "Telecentric lenses use fixed magnification: FOV = SensorSize / PMAG and ObjectPixel = PixelSize / PMAG; nominal WD, WD tolerance, image circle/max sensor, telecentricity, distortion, DOF, and coaxial/telecentric lighting are also checked."));
    page.paragraph(lt("接口和存储按原始图像估算：单帧数据 = 分辨率 x bit depth / 8，吞吐 = 单帧数据 x fps，存储 = 吞吐 x 3600；带宽利用率超过 90% 时提示接口余量风险。",
                      "Interface and storage are estimated from raw images: frame payload = resolution x bit depth / 8, throughput = frame payload x fps, storage = throughput x 3600; bandwidth utilization over 90% is flagged as interface margin risk."));

    page.section(lt("推荐方案 Top 方案", "Top Recommended Plans"));
    const QVector<int> widths = {58, 58, 105, 110, 90, 82, 72, 95};
    page.tableHeader({lt("类型", "Type"), lt("得分", "Score"), lt("相机", "Camera"),
                      lt("镜头", "Lens"), lt("光源", "Light"), QStringLiteral("FOV"),
                      lt("像素", "Pixel"), lt("风险", "Risk")},
                     widths);

    const int count = qMin(8, results.size());
    for (int i = 0; i < count; ++i) {
        const SelectionResult &r = results.at(i);
        page.tableRow({
            r.isTelecentric() ? lt("远心", "Tele.") : lt("普通", "Fixed"),
            QString::number(r.score.score, 'f', 1),
            UiHelpers::productLabel(r.camera.manufacturer, r.camera.model),
            UiHelpers::productLabel(r.lens.manufacturer, r.lens.model),
            UiHelpers::productLabel(r.light.manufacturer, r.light.model),
            QStringLiteral("%1x%2").arg(r.effectiveFovWidthMm, 0, 'f', 1).arg(r.effectiveFovHeightMm, 0, 'f', 1),
            QStringLiteral("%1um").arg(r.objectPixelSizeUm, 0, 'f', 1),
            UiHelpers::riskSummary(r).left(42)
        }, widths);
    }

    page.section(lt("方案对比", "Plan Comparison"));
    const QVector<int> compareWidths = {38, 46, 92, 100, 88, 62, 84, 165};
    page.tableHeader({lt("序号", "No."), lt("得分", "Score"), lt("相机", "Camera"),
                      lt("镜头", "Lens"), lt("光源", "Light"), lt("像素", "Pixel"),
                      lt("带宽/存储", "BW/Storage"), lt("关键风险", "Key Risk")},
                     compareWidths);
    const int compareCount = qMin(5, results.size());
    for (int i = 0; i < compareCount; ++i) {
        const SelectionResult &r = results.at(i);
        page.tableRow({
            QStringLiteral("#%1").arg(i + 1),
            QString::number(r.score.score, 'f', 1),
            UiHelpers::productLabel(r.camera.manufacturer, r.camera.model).left(18),
            UiHelpers::productLabel(r.lens.manufacturer, r.lens.model).left(20),
            UiHelpers::productLabel(r.light.manufacturer, r.light.model).left(16),
            QStringLiteral("%1um").arg(r.objectPixelSizeUm, 0, 'f', 1),
            QStringLiteral("%1%/%2GBh")
                .arg(r.bandwidthUtilizationPercent, 0, 'f', 0)
                .arg(r.storagePerHourGB, 0, 'f', 0),
            UiHelpers::riskSummary(r).left(46)
        }, compareWidths);
    }

    if (!results.isEmpty()) {
        const SelectionResult &top = results.first();
        page.section(lt("首选方案说明", "Top Plan Details"));
        page.keyValue(lt("方案", "Plan"), top.schemeTitle + lt("：", ": ")
                      + UiHelpers::productLabel(top.camera.manufacturer, top.camera.model) + QStringLiteral(" + ")
                      + UiHelpers::productLabel(top.lens.manufacturer, top.lens.model) + QStringLiteral(" + ")
                      + UiHelpers::productLabel(top.light.manufacturer, top.light.model));
        page.keyValue(lt("适配状态", "Compatibility"), UiHelpers::compatibilityText(top));
        page.keyValue(lt("计算视野", "Calculated FOV"), QStringLiteral("%1 x %2 mm")
                      .arg(top.effectiveFovWidthMm, 0, 'f', 2)
                      .arg(top.effectiveFovHeightMm, 0, 'f', 2));
        page.keyValue(lt("物方像素", "Object pixel"), value(top.objectPixelSizeUm, QStringLiteral("um/px")));
        page.keyValue(lt("倍率/焦距", "Magnification / focal"), top.isTelecentric()
                      ? QStringLiteral("PMAG %1x").arg(top.magnification, 0, 'f', 3)
                      : value(top.lens.focalLengthMm, QStringLiteral("mm")));
        page.keyValue(lt("曝光/景深/畸变", "Exposure / DOF / distortion"), QStringLiteral("%1, DOF %2 mm, distortion %3 um")
                      .arg(UiHelpers::exposureText(top.maxExposureUsForOnePixelBlur))
                      .arg(top.estimatedDofMm, 0, 'f', 2)
                      .arg(top.distortionErrorUm, 0, 'f', 2));
        page.keyValue(lt("接口/存储", "Interface / storage"), QStringLiteral("%1 MB/s, %2% of %3 MB/s, %4 GB/h")
                      .arg(top.bandwidthRequiredMBps, 0, 'f', 1)
                      .arg(top.bandwidthUtilizationPercent, 0, 'f', 0)
                      .arg(top.interfaceCapacityMBps, 0, 'f', 1)
                      .arg(top.storagePerHourGB, 0, 'f', 0));
        page.keyValue(lt("镜头/光源余量", "Lens / light margins"), QStringLiteral("lens MP %1%, light margin %2%")
                      .arg(top.lensMegapixelUtilizationPercent, 0, 'f', 0)
                      .arg(top.lightCoverageMarginPercent, 0, 'f', 0));
        page.paragraph(top.score.reasons.join(lt("；", "; ")));
        if (!top.score.risks.isEmpty() || !top.hardFailures.isEmpty())
            page.paragraph(lt("风险提示：", "Risk notes: ") + UiHelpers::riskSummary(top));

        page.section(QStringLiteral("BOM"));
        const QVector<int> bomWidths = {58, 110, 130, 260, 112};
        page.tableHeader({lt("类别", "Category"), lt("厂家", "Manufacturer"), lt("型号", "Model"),
                          lt("关键规格", "Key Specs"), lt("备注", "Notes")},
                         bomWidths);
        page.tableRow({lt("相机", "Camera"), top.camera.manufacturer, top.camera.model,
                       bomSpecForCamera(top.camera, top).left(56),
                       QStringLiteral("%1 x %2 mm").arg(top.effectiveFovWidthMm, 0, 'f', 1).arg(top.effectiveFovHeightMm, 0, 'f', 1)}, bomWidths);
        page.tableRow({lt("镜头", "Lens"), top.lens.manufacturer, top.lens.model,
                       bomSpecForLens(top.lens, top).left(56),
                       QStringLiteral("distortion %1 um").arg(top.distortionErrorUm, 0, 'f', 1)}, bomWidths);
        page.tableRow({lt("光源", "Light"), top.light.manufacturer, top.light.model,
                       bomSpecForLight(top.light, top).left(56),
                       UiHelpers::riskSummary(top).left(26)}, bomWidths);
    }

    page.section(lt("资料依据", "Reference Basis"));
    page.paragraph(lt("远心镜头固定倍率与无视差测量参考 Opto Engineering、Edmund Optics、STEMMER Imaging；相机分辨率、快门、帧率、接口参考 Basler；镜头分辨率/像元匹配参考 Teledyne FLIR；光源策略参考 Cognex。",
                      "Telecentric fixed magnification and parallax-free measurement references: Opto Engineering, Edmund Optics, STEMMER Imaging. Camera resolution, shutter, frame rate, and interface references: Basler. Lens resolution / pixel matching references: Teledyne FLIR. Lighting strategy references: Cognex."));

    painter.end();
    return true;
}
