#include "ui/pages/CalculationPage.h"

#include "ui/UiHelpers.h"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QStringList>
#include <QTableWidget>
#include <QTextEdit>
#include <QVBoxLayout>

using namespace UiHelpers;

namespace {
void selectRowBySourceIndex(QTableWidget *table, int sourceIndex)
{
    if (!table || sourceIndex < 0)
        return;
    for (int row = 0; row < table->rowCount(); ++row) {
        if (rowSourceIndex(table, row) == sourceIndex) {
            table->selectRow(row);
            return;
        }
    }
}
}

CalculationPage::CalculationPage(QWidget *parent)
    : QWidget(parent)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(28, 24, 28, 24);
    layout->setSpacing(14);

    layout->addWidget(pageTitle(QString::fromUtf8("产品计算助手")));

    QHBoxLayout *buttons = new QHBoxLayout;
    QPushButton *refreshButton = new QPushButton(QString::fromUtf8("\346\240\271\346\215\256\345\275\223\345\211\215\351\234\200\346\261\202\350\256\241\347\256\227"));
    QPushButton *inputButton = new QPushButton(QString::fromUtf8("\350\277\224\345\233\236\351\234\200\346\261\202\350\276\223\345\205\245"));
    inputButton->setObjectName(QStringLiteral("SecondaryButton"));
    connect(refreshButton, &QPushButton::clicked, this, &CalculationPage::recalculateRequested);
    connect(inputButton, &QPushButton::clicked, this, &CalculationPage::inputRequested);
    buttons->addWidget(refreshButton);
    buttons->addWidget(inputButton);
    buttons->addStretch();
    layout->addLayout(buttons);

    m_summaryLabel = new QLabel;
    m_summaryLabel->setObjectName(QStringLiteral("SectionTitle"));
    m_summaryLabel->setWordWrap(true);
    layout->addWidget(m_summaryLabel);

    QLabel *cameraTitle = new QLabel(QString::fromUtf8("\347\233\270\346\234\272\344\274\260\347\256\227"));
    cameraTitle->setObjectName(QStringLiteral("SectionTitle"));
    layout->addWidget(cameraTitle);

    m_cameraTable = new QTableWidget;
    setupTable(m_cameraTable);
    m_cameraTable->setColumnCount(10);
    m_cameraTable->setHorizontalHeaderLabels({
        QString::fromUtf8("\347\233\270\346\234\272"), QString::fromUtf8("\345\216\202\345\256\266"),
        QString::fromUtf8("\345\210\206\350\276\250\347\216\207"), QString::fromUtf8("\345\203\217\345\205\203"),
        QString::fromUtf8("\344\274\240\346\204\237\345\231\250"), QString::fromUtf8("\347\211\251\346\226\271\345\203\217\347\264\240"),
        QString::fromUtf8("\346\231\256\351\200\232\347\204\246\350\267\235"), QStringLiteral("PMAG"),
        QString::fromUtf8("\345\270\246\345\256\275"), QString::fromUtf8("\345\210\244\346\226\255")
    });
    m_cameraTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_cameraTable->horizontalHeader()->setSectionResizeMode(9, QHeaderView::Stretch);
    connect(m_cameraTable, &QTableWidget::cellClicked, this, [this](int row, int) {
        const int sourceRow = rowSourceIndex(m_cameraTable, row);
        m_selectedCameraEstimateRow = sourceRow >= 0 ? sourceRow : row;
        emit cameraSelectionChanged(m_selectedCameraEstimateRow);
    });
    layout->addWidget(m_cameraTable, 1);

    QLabel *lensTitle = new QLabel(QString::fromUtf8("\351\225\234\345\244\264\345\200\231\351\200\211"));
    lensTitle->setObjectName(QStringLiteral("SectionTitle"));
    layout->addWidget(lensTitle);

    m_lensTable = new QTableWidget;
    setupTable(m_lensTable);
    m_lensTable->setColumnCount(10);
    m_lensTable->setHorizontalHeaderLabels({
        QString::fromUtf8("\347\261\273\345\236\213"), QString::fromUtf8("\345\216\202\345\256\266"),
        QString::fromUtf8("\351\225\234\345\244\264"), QString::fromUtf8("\346\216\245\345\217\243"),
        QString::fromUtf8("\347\204\246\350\267\235/PMAG"), QStringLiteral("FOV"),
        QString::fromUtf8("\347\211\251\346\226\271\345\203\217\347\264\240"), QStringLiteral("WD/DOF"),
        QString::fromUtf8("\345\203\217\345\234\206"), QString::fromUtf8("\345\210\244\346\226\255")
    });
    m_lensTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_lensTable->horizontalHeader()->setSectionResizeMode(9, QHeaderView::Stretch);
    layout->addWidget(m_lensTable, 1);

    m_details = new QTextEdit;
    m_details->setReadOnly(true);
    m_details->setMinimumHeight(120);
    layout->addWidget(m_details);
}

void CalculationPage::setSummary(const QString &text)
{
    if (m_summaryLabel)
        m_summaryLabel->setText(text);
}

void CalculationPage::setCameraEstimates(const QVector<CameraEstimate> &estimates)
{
    if (!m_cameraTable)
        return;

    m_cameraTable->setSortingEnabled(false);
    m_cameraTable->setRowCount(estimates.size());
    for (int row = 0; row < estimates.size(); ++row) {
        const CameraEstimate &estimate = estimates.at(row);
        const CameraSpec &camera = estimate.camera;
        QStringList verdict;
        verdict.append(estimate.meetsSampling
            ? QString::fromUtf8("\345\203\217\347\264\240\346\273\241\350\266\263")
            : QString::fromUtf8("\345\203\217\347\264\240\344\270\215\350\266\263"));
        verdict.append(estimate.meetsFps
            ? QString::fromUtf8("\345\270\247\347\216\207\346\273\241\350\266\263")
            : QString::fromUtf8("\345\270\247\347\216\207\344\270\215\350\266\263"));
        if (estimate.globalShutterRecommended)
            verdict.append(QString::fromUtf8("\345\273\272\350\256\256\345\205\250\345\261\200\345\277\253\351\227\250"));

        m_cameraTable->setItem(row, 0, indexedItem(camera.model, row));
        m_cameraTable->setItem(row, 1, item(camera.manufacturer));
        m_cameraTable->setItem(row, 2, item(QStringLiteral("%1 x %2").arg(camera.resolutionX).arg(camera.resolutionY)));
        m_cameraTable->setItem(row, 3, item(QStringLiteral("%1 um").arg(camera.pixelSizeUm, 0, 'f', 2)));
        m_cameraTable->setItem(row, 4, item(QStringLiteral("%1 mm").arg(estimate.sensorDiagonalMm, 0, 'f', 2)));
        m_cameraTable->setItem(row, 5, item(QStringLiteral("%1 um/px").arg(estimate.objectPixelSizeUm, 0, 'f', 2)));
        m_cameraTable->setItem(row, 6, item(QStringLiteral("%1 mm").arg(estimate.fixedFocalLengthMm, 0, 'f', 1)));
        m_cameraTable->setItem(row, 7, item(estimate.telecentricFeasible
            ? QStringLiteral("%1 - %2x").arg(estimate.telecentricPmagMin, 0, 'f', 3).arg(estimate.telecentricPmagMax, 0, 'f', 3)
            : QString::fromUtf8("\344\270\215\345\273\272\350\256\256")));
        m_cameraTable->setItem(row, 8, item(QStringLiteral("%1 MB/s").arg(estimate.bandwidthRequiredMBps, 0, 'f', 1)));
        m_cameraTable->setItem(row, 9, item(verdict.join(QString::fromUtf8("\357\274\233"))));
    }
    m_cameraTable->setSortingEnabled(true);

    if (estimates.isEmpty()) {
        m_selectedCameraEstimateRow = -1;
    } else if (m_selectedCameraEstimateRow < 0 || m_selectedCameraEstimateRow >= estimates.size()) {
        m_selectedCameraEstimateRow = 0;
    }
    if (m_selectedCameraEstimateRow >= 0)
        selectRowBySourceIndex(m_cameraTable, m_selectedCameraEstimateRow);
}

void CalculationPage::setLensEstimates(const QVector<LensEstimate> &estimates)
{
    if (!m_lensTable)
        return;

    m_lensTable->setSortingEnabled(false);
    m_lensTable->setRowCount(estimates.size());
    for (int row = 0; row < estimates.size(); ++row) {
        const LensEstimate &estimate = estimates.at(row);
        const LensSpec &lens = estimate.lens;
        const QString focalOrPmag = lens.isTelecentric()
            ? QStringLiteral("%1x").arg(estimate.magnification, 0, 'f', 3)
            : QStringLiteral("%1 mm").arg(lens.focalLengthMm, 0, 'f', 1);
        const QString wdOrDof = lens.isTelecentric()
            ? QStringLiteral("WD %1 / DOF %2").arg(lens.nominalWorkingDistanceMm, 0, 'f', 0).arg(lens.dofMm, 0, 'f', 1)
            : QStringLiteral("min WD %1 / DOF %2").arg(lens.minWorkingDistanceMm, 0, 'f', 0).arg(estimate.estimatedDofMm, 0, 'f', 1);
        QStringList verdict;
        verdict.append(estimate.fovOk ? QString::fromUtf8("FOV \346\273\241\350\266\263") : QString::fromUtf8("FOV \344\270\215\350\266\263"));
        verdict.append(estimate.samplingOk ? QString::fromUtf8("\345\203\217\347\264\240\346\273\241\350\266\263") : QString::fromUtf8("\345\203\217\347\264\240\344\270\215\350\266\263"));
        if (!estimate.mountOk)
            verdict.append(QString::fromUtf8("\346\216\245\345\217\243\344\270\215\345\214\271\351\205\215"));
        if (!estimate.workingDistanceOk)
            verdict.append(QStringLiteral("WD"));
        if (!estimate.dofOk)
            verdict.append(QStringLiteral("DOF"));

        m_lensTable->setItem(row, 0, indexedItem(lens.typeLabel(), row));
        m_lensTable->setItem(row, 1, item(lens.manufacturer));
        m_lensTable->setItem(row, 2, item(lens.model));
        m_lensTable->setItem(row, 3, item(lens.lensMount));
        m_lensTable->setItem(row, 4, item(focalOrPmag));
        m_lensTable->setItem(row, 5, item(QStringLiteral("%1 x %2").arg(estimate.effectiveFovWidthMm, 0, 'f', 1).arg(estimate.effectiveFovHeightMm, 0, 'f', 1)));
        m_lensTable->setItem(row, 6, item(QStringLiteral("%1 um").arg(estimate.objectPixelSizeUm, 0, 'f', 2)));
        m_lensTable->setItem(row, 7, item(wdOrDof));
        m_lensTable->setItem(row, 8, item(QStringLiteral("%1 mm").arg(lens.imageCircleMm, 0, 'f', 1)));
        m_lensTable->setItem(row, 9, item(verdict.join(QString::fromUtf8("\357\274\233"))));
    }
    m_lensTable->setSortingEnabled(true);
}

void CalculationPage::setDetails(const QString &text)
{
    if (m_details)
        m_details->setPlainText(text);
}

int CalculationPage::selectedCameraEstimateRow() const
{
    if (m_cameraTable && m_cameraTable->currentRow() >= 0) {
        const int sourceRow = rowSourceIndex(m_cameraTable, m_cameraTable->currentRow());
        return sourceRow >= 0 ? sourceRow : m_cameraTable->currentRow();
    }
    return m_selectedCameraEstimateRow;
}
