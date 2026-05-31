#include "ui/pages/ReportPage.h"

#include "ui/UiHelpers.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTextEdit>
#include <QVBoxLayout>

using namespace UiHelpers;

ReportPage::ReportPage(QWidget *parent)
    : QWidget(parent)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(28, 24, 28, 24);
    layout->setSpacing(14);
    layout->addWidget(pageTitle(localizedText("PDF 报告", "PDF Report")));

    QHBoxLayout *buttons = new QHBoxLayout;
    QPushButton *exportButton = new QPushButton(localizedText("导出 PDF", "Export PDF"));
    QPushButton *exportBomButton = new QPushButton(localizedText("导出 BOM CSV", "Export BOM CSV"));
    QPushButton *recalculateButton = new QPushButton(localizedText("重新计算", "Recalculate"));
    exportBomButton->setObjectName(QStringLiteral("SecondaryButton"));
    recalculateButton->setObjectName(QStringLiteral("SecondaryButton"));
    connect(exportButton, &QPushButton::clicked, this, &ReportPage::exportPdfRequested);
    connect(exportBomButton, &QPushButton::clicked, this, &ReportPage::exportBomRequested);
    connect(recalculateButton, &QPushButton::clicked, this, &ReportPage::recalculateRequested);
    buttons->addWidget(exportButton);
    buttons->addWidget(exportBomButton);
    buttons->addWidget(recalculateButton);
    buttons->addStretch();
    layout->addLayout(buttons);

    m_reportPreview = new QTextEdit;
    m_reportPreview->setReadOnly(true);
    layout->addWidget(m_reportPreview, 1);
}

void ReportPage::setReportData(const SelectionRequest &request,
                               const QVector<SelectionResult> &results)
{
    m_request = request;
    m_results = results;
    refreshPreview();
}

void ReportPage::refreshPreview()
{
    if (!m_reportPreview)
        return;

    QString text;
    text += localizedText("PDF 将包含：\n", "The PDF will include:\n");
    text += localizedText("- 需求输入、目标 FOV、目标物方像素\n", "- Requirements, target FOV, and target object pixel size\n");
    text += localizedText("- 普通镜头与远心镜头的关键公式\n", "- Key formulas for fixed-focal and telecentric lenses\n");
    text += localizedText("- Top 推荐方案、方案对比、BOM、带宽/存储/曝光/DOF/畸变风险\n\n",
                          "- Top recommendations, plan comparison, BOM, bandwidth/storage/exposure/DOF/distortion risks\n\n");
    if (!m_results.isEmpty()) {
        const SelectionResult &top = m_results.first();
        text += localizedText("当前首选：", "Current top choice: ") + top.schemeTitle
            + localizedText("\n相机：", "\nCamera: ") + productLabel(top.camera.manufacturer, top.camera.model)
            + localizedText("\n镜头：", "\nLens: ") + productLabel(top.lens.manufacturer, top.lens.model)
            + localizedText("\n光源：", "\nLight: ") + productLabel(top.light.manufacturer, top.light.model)
            + localizedText("\n适配状态：", "\nCompatibility: ") + compatibilityText(top)
            + localizedText("\n得分：", "\nScore: ") + QString::number(top.score.score, 'f', 1)
            + localizedText("\n带宽/存储：", "\nBandwidth / storage: ") + QStringLiteral("%1 MB/s, %2 GB/h")
                .arg(top.bandwidthRequiredMBps, 0, 'f', 1)
                .arg(top.storagePerHourGB, 0, 'f', 0)
            + localizedText("\nBOM：相机、镜头、光源 3 项", "\nBOM: camera, lens, and light")
            + QStringLiteral("\n");
    }
    m_reportPreview->setPlainText(text);
}
