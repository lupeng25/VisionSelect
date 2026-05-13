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
    layout->addWidget(pageTitle(QString::fromUtf8("PDF \346\212\245\345\221\212")));

    QHBoxLayout *buttons = new QHBoxLayout;
    QPushButton *exportButton = new QPushButton(QString::fromUtf8("\345\257\274\345\207\272 PDF"));
    QPushButton *exportBomButton = new QPushButton(QString::fromUtf8("导出 BOM CSV"));
    QPushButton *recalculateButton = new QPushButton(QString::fromUtf8("\351\207\215\346\226\260\350\256\241\347\256\227"));
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
    Q_UNUSED(request);

    if (!m_reportPreview)
        return;
    QString text;
    text += QString::fromUtf8("PDF \345\260\206\345\214\205\345\220\253\357\274\232\n");
    text += QString::fromUtf8("- \351\234\200\346\261\202\350\276\223\345\205\245\343\200\201\347\233\256\346\240\207 FOV\343\200\201\347\233\256\346\240\207\347\211\251\346\226\271\345\203\217\347\264\240\n");
    text += QString::fromUtf8("- \346\231\256\351\200\232\351\225\234\345\244\264\344\270\216\350\277\234\345\277\203\351\225\234\345\244\264\347\232\204\345\205\263\351\224\256\345\205\254\345\274\217\n");
    text += QString::fromUtf8("- Top 推荐方案、方案对比、BOM、带宽/存储/曝光/DOF/畸变风险\n\n");
    if (!results.isEmpty()) {
        const SelectionResult &top = results.first();
        text += QString::fromUtf8("\345\275\223\345\211\215\351\246\226\351\200\211\357\274\232") + top.schemeTitle
            + QString::fromUtf8("\n\347\233\270\346\234\272\357\274\232") + productLabel(top.camera.manufacturer, top.camera.model)
            + QString::fromUtf8("\n\351\225\234\345\244\264\357\274\232") + productLabel(top.lens.manufacturer, top.lens.model)
            + QString::fromUtf8("\n\345\205\211\346\272\220\357\274\232") + productLabel(top.light.manufacturer, top.light.model)
            + QString::fromUtf8("\n\345\276\227\345\210\206\357\274\232") + QString::number(top.score.score, 'f', 1)
            + QString::fromUtf8("\n带宽/存储：") + QStringLiteral("%1 MB/s, %2 GB/h")
                .arg(top.bandwidthRequiredMBps, 0, 'f', 1)
                .arg(top.storagePerHourGB, 0, 'f', 0)
            + QString::fromUtf8("\nBOM：相机、镜头、光源 3 项")
            + QStringLiteral("\n");
    }
    m_reportPreview->setPlainText(text);
}
