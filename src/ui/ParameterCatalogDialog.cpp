#include "ui/ParameterCatalogDialog.h"

#include "catalog/CatalogRepository.h"
#include "ui/ParameterUi.h"

#include <QDialog>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <algorithm>

using namespace UiHelpers;
using namespace ParameterUi;

namespace {
struct Picker {
    QDialog dialog;
    QLineEdit *search;
    QLabel *summary;
    QTableWidget *table;
    QPushButton *previous;
    QPushButton *next;
    QDialogButtonBox *buttons;
    explicit Picker(QWidget *parent, const QString &title) : dialog(parent) {
        if (parent && parent->testAttribute(Qt::WA_DontShowOnScreen))
            dialog.setAttribute(Qt::WA_DontShowOnScreen);
        dialog.setObjectName(QStringLiteral("ParameterCatalogPicker"));
        dialog.setWindowTitle(title);
        dialog.resize(900, 560);
        auto *layout = new QVBoxLayout(&dialog);
        search = new QLineEdit;
        search->setObjectName(QStringLiteral("ParameterCatalogSearch"));
        search->setAccessibleName(localizedText("搜索型号或制造商", "Search model or manufacturer"));
        search->setPlaceholderText(search->accessibleName());
        layout->addWidget(search);
        summary = new QLabel;
        summary->setWordWrap(true);
        layout->addWidget(summary);
        table = new QTableWidget;
        table->setObjectName(QStringLiteral("ParameterCatalogResults"));
        setupTable(table);
        table->setSelectionMode(QAbstractItemView::SingleSelection);
        table->setSelectionBehavior(QAbstractItemView::SelectRows);
        table->setSortingEnabled(false);
        table->verticalHeader()->hide();
        layout->addWidget(table, 1);
        auto *navigation = new QHBoxLayout;
        previous = actionButton(localizedText("上一页", "Previous"), QString(), true);
        next = actionButton(localizedText("下一页", "Next"), QString(), true);
        navigation->addWidget(previous); navigation->addWidget(next); navigation->addStretch();
        buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        buttons->button(QDialogButtonBox::Ok)->setText(localizedText("导入参数", "Import parameters"));
        buttons->button(QDialogButtonBox::Cancel)->setText(localizedText("取消", "Cancel"));
        buttons->button(QDialogButtonBox::Ok)->setEnabled(false);
        navigation->addWidget(buttons);
        layout->addLayout(navigation);
        QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        QObject::connect(table, &QTableWidget::itemSelectionChanged, &dialog, [this]() {
            buttons->button(QDialogButtonBox::Ok)->setEnabled(table->currentRow() >= 0);
        });
    }
    void fill(const QStringList &headers, const QVector<QStringList> &rows) {
        table->clear();
        table->setColumnCount(headers.size()); table->setHorizontalHeaderLabels(headers); table->setRowCount(rows.size());
        table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        for (int row = 0; row < rows.size(); ++row)
            for (int col = 0; col < headers.size(); ++col) table->setItem(row, col, item(rows.at(row).value(col)));
        table->resizeRowsToContents();
        buttons->button(QDialogButtonBox::Ok)->setEnabled(false);
        table->setCurrentCell(-1, -1);
    }
};
Parameters::Number known(double value) { return value > 0.0 ? Parameters::Number(value) : Parameters::Number(); }
}

std::optional<CameraSpec> ParameterCatalogDialog::camera(QWidget *parent, const CatalogRepository &catalog)
{
    Picker picker(parent, localizedText("从产品库导入相机", "Import camera from catalog"));
    int offset = 0;
    QVector<CameraSpec> cameras;
    const auto load = [&]() {
        CatalogQuery query;
        query.search = picker.search->text(); query.offset = offset; query.limit = 100;
        QString error;
        const auto page = catalog.queryCameras(query, &error);
        cameras = page.items;
        QVector<QStringList> rows;
        for (const auto &camera : cameras)
            rows.append({productLabel(camera.manufacturer, camera.model),
                         QStringLiteral("%1 × %2").arg(camera.resolutionX).arg(camera.resolutionY), valueText(known(camera.pixelSizeUm), "μm"),
                         valueText(known(camera.maxFps), "fps"), camera.lensMount});
        picker.fill({localizedText("相机", "Camera"), localizedText("分辨率", "Resolution"), localizedText("像元", "Pixel pitch"), localizedText("最大帧率", "Maximum FPS"), localizedText("镜头接口", "Lens mount")}, rows);
        picker.summary->setText(error.isEmpty()
            ? localizedText("共 %1 项，当前 %2–%3。导入会更新共享相机参数与方案校核中的相机规格；不推测传输格式。",
                            "%1 products, showing %2–%3. Import updates the shared camera and system-check camera specifications; pixel format is not inferred.")
                .arg(page.totalCount).arg(cameras.isEmpty() ? 0 : offset + 1).arg(offset + cameras.size()) : error);
        picker.previous->setEnabled(offset > 0); picker.next->setEnabled(offset + cameras.size() < page.totalCount);
    };
    QTimer timer;
    timer.setSingleShot(true); timer.setInterval(180);
    QObject::connect(picker.search, &QLineEdit::textChanged, &picker.dialog, [&]() { offset = 0; timer.start(); });
    QObject::connect(&timer, &QTimer::timeout, &picker.dialog, load);
    QObject::connect(picker.previous, &QPushButton::clicked, &picker.dialog, [&]() { offset = qMax(0, offset - 100); load(); });
    QObject::connect(picker.next, &QPushButton::clicked, &picker.dialog, [&]() { offset += 100; load(); });
    load();
    if (picker.dialog.exec() != QDialog::Accepted) return {};
    const int row = picker.table->currentRow();
    return row >= 0 && row < cameras.size() ? std::optional<CameraSpec>(cameras.at(row)) : std::nullopt;
}

std::optional<LensSpec> ParameterCatalogDialog::lens(QWidget *parent, const CatalogRepository &catalog,
                                                    const Parameters::SystemInput &context, bool matched)
{
    Picker picker(parent, matched ? localizedText("当前条件的镜头候选", "Lens candidates for current conditions")
                                 : localizedText("从产品库导入镜头", "Import an existing lens"));
    struct Candidate { LensSpec lens; Parameters::SystemResult check; int failed = 0; int unknown = 0; double excess = 0.0; };
    QVector<Candidate> candidates;
    QString queryError;
    if (matched) {
        SelectionRequest request;
        request.objectWidthMm = context.targetFovWidthMm.value_or(0.0); request.objectHeightMm = context.targetFovHeightMm.value_or(0.0);
        request.placementMarginMm = 0.0; request.workingDistanceMm = context.distanceMm.value_or(0.0);
        request.allowTelecentric = context.telecentric;
        if (!context.telecentric && context.model == Parameters::OpticsModel::ThinLens)
            request.workingDistanceMm = 0.0;
        const auto lenses = catalog.selectionCandidateLenses(request, 500, &queryError);
        for (const auto &lens : lenses) {
            if (lens.isTelecentric() != context.telecentric) continue;
            auto input = context;
            input.measuredFov = false;
            input.focalLengthMm = known(lens.focalLengthMm); input.magnification = known(lens.pmag);
            input.imageCircleMm = known(lens.imageCircleMm); input.lensMount = lens.lensMount;
            input.minWorkingDistanceMm = known(lens.minWorkingDistanceMm); input.nominalWorkingDistanceMm = known(lens.nominalWorkingDistanceMm);
            input.workingDistanceToleranceMm = known(lens.workingDistanceToleranceMm);
            Candidate candidate;
            candidate.lens = lens; candidate.check = Parameters::checkSystem(input);
            for (const auto &check : candidate.check.checks) {
                if (!QStringList{"fovX", "fovY", "samplingX", "samplingY", "imageCircle", "mount", "distance"}.contains(check.key)) continue;
                if (check.status == CalculationStatus::Failed || check.status == CalculationStatus::Invalid) ++candidate.failed;
                if (check.status == CalculationStatus::Unknown) ++candidate.unknown;
            }
            if (usable(candidate.check.actualFovWidthMm) && usable(context.targetFovWidthMm))
                candidate.excess = std::abs(*candidate.check.actualFovWidthMm / *context.targetFovWidthMm - 1.0);
            candidates.append(candidate);
        }
        std::stable_sort(candidates.begin(), candidates.end(), [](const Candidate &a, const Candidate &b) {
            if (a.failed != b.failed) return a.failed < b.failed;
            if (a.unknown != b.unknown) return a.unknown < b.unknown;
            return a.excess < b.excess;
        });
    }
    int offset = 0;
    QVector<LensSpec> visible;
    const auto load = [&]() {
        visible.clear();
        QVector<QStringList> rows;
        int total = 0;
        QString error = queryError;
        if (matched) {
            for (const auto &candidate : candidates) {
                const QString label = productLabel(candidate.lens.manufacturer, candidate.lens.model);
                if (!label.contains(picker.search->text(), Qt::CaseInsensitive)) continue;
                const int index = total++;
                if (index < offset || index >= offset + 100) continue;
                visible.append(candidate.lens);
                rows.append({label, candidate.lens.isTelecentric() ? valueText(candidate.lens.pmag, "×") : valueText(candidate.lens.focalLengthMm, "mm"),
                             valueText(candidate.check.actualFovWidthMm) + " × " + valueText(candidate.check.actualFovHeightMm, "mm"),
                             candidate.failed > 0 ? localizedText("存在不满足项", "Has failing checks")
                                : (candidate.unknown > 0 ? localizedText("几何候选 · 待补规格", "Geometric candidate · missing specs") : localizedText("已检查条件满足", "Entered conditions met"))});
            }
            picker.summary->setText(error.isEmpty()
                ? localizedText("从最多 500 项预筛结果中比较，共 %1 项。按不满足项、缺项及视场余量排序；不代表全库穷举或最终精度保证。",
                                "%1 candidates from at most 500 preselected lenses. Ordered by failures, missing data and FOV excess; not an exhaustive catalog search or accuracy guarantee.").arg(total) : error);
        } else {
            CatalogQuery query; query.search = picker.search->text(); query.offset = offset; query.limit = 100;
            const auto page = catalog.queryLenses(query, &error);
            total = page.totalCount; visible = page.items;
            for (const auto &lens : visible)
                rows.append({productLabel(lens.manufacturer, lens.model), lens.isTelecentric() ? valueText(lens.pmag, "×") : valueText(lens.focalLengthMm, "mm"),
                             valueText(known(lens.imageCircleMm), "mm"), lens.lensMount});
            picker.summary->setText(error.isEmpty() ? localizedText("共 %1 项。导入当前镜头规格；缺失数据保持未填写。", "%1 lenses. Import current lens specifications; missing values stay empty.").arg(total) : error);
        }
        picker.fill({localizedText("镜头", "Lens"), localizedText("焦距 / 倍率", "Focal / magnification"),
                     matched ? localizedText("估算视场", "Estimated FOV") : localizedText("像圈", "Image circle"),
                     matched ? localizedText("校核提示", "Check note") : localizedText("接口", "Mount")}, rows);
        picker.previous->setEnabled(offset > 0); picker.next->setEnabled(offset + visible.size() < total);
    };
    QTimer timer; timer.setSingleShot(true); timer.setInterval(180);
    QObject::connect(picker.search, &QLineEdit::textChanged, &picker.dialog, [&]() { offset = 0; timer.start(); });
    QObject::connect(&timer, &QTimer::timeout, &picker.dialog, load);
    QObject::connect(picker.previous, &QPushButton::clicked, &picker.dialog, [&]() { offset = qMax(0, offset - 100); load(); });
    QObject::connect(picker.next, &QPushButton::clicked, &picker.dialog, [&]() { offset += 100; load(); });
    load();
    if (picker.dialog.exec() != QDialog::Accepted) return {};
    const int row = picker.table->currentRow();
    return row >= 0 && row < visible.size() ? std::optional<LensSpec>(visible.at(row)) : std::nullopt;
}
