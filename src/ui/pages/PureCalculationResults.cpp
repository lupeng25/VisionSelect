#include "ui/pages/PureCalculationPage.h"
#include "ui/FovDiagram.h"
#include "ui/ParameterNumberField.h"
#include "ui/ParameterUi.h"

#include <QCheckBox>
#include <QComboBox>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QStyle>
#include <QTableWidget>

using namespace UiHelpers;
using namespace ParameterUi;

void PureCalculationPage::metricText(int index, const QString &label, const QString &text)
{
    m_metricLabels[index]->setText(label);
    m_metricValues[index]->setText(text);
    m_report += label + QStringLiteral(": ") + text + QLatin1Char('\n');
}
void PureCalculationPage::metric(int index, const QString &label, Number value, const QString &unit)
{
    metricText(index, label, valueText(value, unit));
}
void PureCalculationPage::showRows(const QStringList &headers, const QVector<QStringList> &rows)
{
    const QSignalBlocker block(m_results);
    m_results->clear();
    m_results->setColumnCount(headers.size());
    m_results->setHorizontalHeaderLabels(headers);
    m_results->setRowCount(rows.size());
    m_results->setSortingEnabled(false);
    m_results->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_report += QLatin1Char('\n') + headers.join(QStringLiteral(" | ")) + QLatin1Char('\n');
    for (int row = 0; row < rows.size(); ++row) {
        for (int col = 0; col < headers.size(); ++col) {
            auto *cell = item(rows.at(row).value(col));
            cell->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
            m_results->setItem(row, col, cell);
        }
        m_report += rows.at(row).join(QStringLiteral(" | ")) + QLatin1Char('\n');
    }
    m_results->resizeRowsToContents();
    int height = m_results->horizontalHeader()->height() + 6;
    for (int row = 0; row < rows.size(); ++row) height += m_results->rowHeight(row);
    m_results->setFixedHeight(qBound(80, height, 540));
    m_results->setVisible(!rows.isEmpty());
}

void PureCalculationPage::refresh()
{
    if (m_loading) return;
    m_lastCameraSignature = cameraSignature();
    updateVisibility();
    m_cameraSource->setText(localizedText("共用相机参数 · ", "Shared camera · ") + sourceText("camera"));
    m_taskSource->setText(sourceText(task()));
    m_report = taskLabels().value(taskKeys.indexOf(task())) + QLatin1Char('\n')
        + sourceText("camera") + QLatin1Char('\n') + sourceText(task()) + QLatin1Char('\n');
    m_comparisonValues.clear();
    m_comparisonKind.clear();
    m_diagram->hide();
    m_applyCheck->setEnabled(false);
    CalculationStatus status = CalculationStatus::Unknown;
    QString note;
    QVector<QStringList> rows;
    QStringList headers;
    if (task() == QLatin1String("optics")) {
        const auto input = opticsInput();
        const auto result = Parameters::optics(input);
        status = result.status;
        note = result.note + QLatin1Char('\n') + localizedText("点选下表焦距可校核该参数；比较值不代表已选产品。", "Select a focal length below to check it. Values are not selected products.");
        if (input.solve == Parameters::OpticsSolve::Distance)
            metric(0, localizedText("所需距离（估算）", "Required distance (estimate)"), result.distanceMm, "mm");
        else metric(0, localizedText("焦距（估算）", "Focal length (estimate)"), result.focalLengthMm, "mm");
        metricText(1, localizedText("当前视场 X × Y", "Actual FOV X × Y"),
                   valueText(result.fovWidthMm) + QStringLiteral(" × ") + valueText(result.fovHeightMm, "mm"));
        metric(2, localizedText("实际像素当量", "Actual pixel scale"), result.objectPixelUm, "μm/px");
        m_diagram->show();
        m_diagram->setFields(sizeOf(input.targetFovWidthMm, input.targetFovHeightMm), sizeOf(result.fovWidthMm, result.fovHeightMm), choice("optics.target") == QLatin1String("circle"));
        headers = {localizedText("焦距 mm", "Focal mm"), localizedText("视场 X × Y mm", "FOV X × Y mm"), localizedText("像素当量 μm/px", "Pixel scale μm/px"), localizedText("覆盖 / 采样", "Coverage / sampling")};
        if (result.status == CalculationStatus::Passed) {
            for (double focal : {12.0, 16.0, 20.0, 25.0, 35.0, 50.0}) {
                auto candidate = input;
                candidate.solve = Parameters::OpticsSolve::FieldOfView;
                candidate.focalLengthMm = focal;
                candidate.distanceMm = result.distanceMm;
                const auto value = Parameters::optics(candidate);
                const auto sampling = checkUpperBound(value.objectPixelUm, number("optics.targetPixel"));
                rows.append({valueText(focal), valueText(value.fovWidthMm) + " × " + valueText(value.fovHeightMm), valueText(value.objectPixelUm),
                             value.status == CalculationStatus::Passed
                                ? Parameters::statusText(value.coverage) + " / " + Parameters::statusText(sampling)
                                : Parameters::statusText(value.status)});
                m_comparisonValues.append(focal);
            }
            m_comparisonKind = "focal";
            m_applyCheck->setEnabled(true);
        }
    } else if (task() == QLatin1String("sampling")) {
        Parameters::SamplingInput input;
        input.solve = choice("sampling.solve") == QLatin1String("calibration") ? Parameters::SamplingSolve::Calibration
            : (choice("sampling.solve") == QLatin1String("actual") ? Parameters::SamplingSolve::Actual : Parameters::SamplingSolve::Requirement);
        input.sensor = sensor();
        input.fovWidthMm = number("sampling.width"); input.fovHeightMm = number("sampling.height");
        input.featureUm = number("sampling.feature"); input.pixelsPerFeature = number("sampling.featurePixels");
        if (m_flags.value("sampling.measurement")->isChecked()) {
            input.measurementBudget = true;
            input.toleranceUm = number("sampling.tolerance"); input.pixelsPerTolerance = number("sampling.tolerancePixels");
        }
        input.calibrationLengthMm = number("sampling.length"); input.calibrationPixels = number("sampling.pixelDistance");
        const auto result = Parameters::sampling(input);
        status = result.status; note = result.note;
        headers = {localizedText("指标", "Metric"), localizedText("X 方向", "X axis"), localizedText("Y 方向", "Y axis")};
        if (input.solve == Parameters::SamplingSolve::Calibration) {
            metric(0, localizedText("实测像素当量", "Measured pixel scale"), result.calibratedPixelUm, "μm/px");
            metricText(1, localizedText("标定方向", "Calibration axis"), m_choices.value("sampling.axis")->currentText());
            metricText(2, localizedText("适用区域", "Applicable region"), m_texts.value("sampling.region")->text().isEmpty()
                ? localizedText("待记录", "Not recorded") : m_texts.value("sampling.region")->text());
        } else if (input.solve == Parameters::SamplingSolve::Actual) {
            metric(0, localizedText("实际像素当量 X", "Actual pixel scale X"), result.objectPixelXUm, "μm/px");
            metric(1, localizedText("实际像素当量 Y", "Actual pixel scale Y"), result.objectPixelYUm, "μm/px");
            metric(2, localizedText("最小特征", "Smallest feature"), input.featureUm, "μm");
            rows.append({localizedText("特征覆盖像素", "Pixels per feature"), valueText(result.featurePixelsX), valueText(result.featurePixelsY)});
            rows.append({localizedText("像素密度 px/mm", "Pixel density px/mm"),
                valueText(usable(result.objectPixelXUm) ? Number(1000.0 / *result.objectPixelXUm) : Number()),
                valueText(usable(result.objectPixelYUm) ? Number(1000.0 / *result.objectPixelYUm) : Number())});
            m_applyCheck->setEnabled(status == CalculationStatus::Passed);
        } else {
            metricText(0, localizedText("最低分辨率", "Minimum resolution"), valueText(result.requiredResolutionX) + " × " + valueText(result.requiredResolutionY));
            metric(1, localizedText("目标像素当量", "Target pixel scale"), result.targetObjectPixelUm, "μm/px");
            metric(2, localizedText("每特征采样", "Sampling per feature"), input.pixelsPerFeature, "px");
            rows.append({localizedText("所需像素数", "Required pixel count"), valueText(result.requiredResolutionX), valueText(result.requiredResolutionY)});
            rows.append({localizedText("需求视场 mm", "Required FOV mm"), valueText(input.fovWidthMm), valueText(input.fovHeightMm)});
            m_applyCheck->setEnabled(status == CalculationStatus::Passed);
        }
    } else if (task() == QLatin1String("check")) {
        const auto input = systemInput();
        const auto result = Parameters::checkSystem(input);
        status = result.status;
        metricText(0, localizedText("当前视场 X × Y", "Actual FOV X × Y"), valueText(result.actualFovWidthMm) + " × " + valueText(result.actualFovHeightMm, "mm"));
        metric(1, localizedText("实际像素当量 X", "Actual pixel scale X"), result.objectPixelXUm, "μm/px");
        metric(2, localizedText("实际像素当量 Y", "Actual pixel scale Y"), result.objectPixelYUm, "μm/px");
        m_diagram->show();
        m_diagram->setFields(sizeOf(input.targetFovWidthMm, input.targetFovHeightMm), sizeOf(result.actualFovWidthMm, result.actualFovHeightMm));
        headers = {localizedText("校核项", "Check"), localizedText("要求 / 限值", "Requirement / limit"), localizedText("当前", "Actual"), localizedText("状态", "Status")};
        for (const auto &check : result.checks)
            rows.append({Parameters::checkTitle(check.key), valueText(check.target, check.unit), valueText(check.actual, check.unit), Parameters::statusText(check.status)});
        showRows(headers, rows);
        for (int i = 0; i < result.checks.size(); ++i) {
            const QString detail = Parameters::checkTitle(result.checks.at(i).key) + QStringLiteral("：") + result.checks.at(i).detail;
            for (int c = 0; c < m_results->columnCount(); ++c) {
                m_results->item(i, c)->setToolTip(detail);
                m_results->item(i, c)->setData(Qt::UserRole, detail);
            }
            m_results->item(i, 3)->setData(Qt::UserRole + 1, static_cast<int>(result.checks.at(i).status));
            m_report += detail + QLatin1Char('\n');
        }
        m_comparisonKind = "check";
        note = localizedText("选中一行查看计算依据。未知项不会计为通过；已填写条件满足也不代表系统精度已保证。",
                             "Select a row to inspect its basis. Unknowns do not pass; meeting entered conditions does not guarantee system accuracy.");
    } else if (task() == QLatin1String("tele")) {
        const auto input = telecentricInput();
        const auto result = Parameters::telecentric(input);
        status = result.status; note = result.note;
        if (input.solve == Parameters::TelecentricSolve::Range)
            metricText(0, localizedText("倍率可行区间", "Feasible magnification"), status == CalculationStatus::Failed
                ? localizedText("无交集", "No overlap") : valueText(result.minMagnification) + " – " + valueText(result.maxMagnification, "×"));
        else metric(0, localizedText("倍率", "Magnification"), result.magnification, "×");
        metricText(1, localizedText("当前视场 X × Y", "Actual FOV X × Y"), valueText(result.fovWidthMm) + " × " + valueText(result.fovHeightMm, "mm"));
        metric(2, localizedText("像素当量", "Pixel scale"), result.objectPixelUm, "μm/px");
        m_diagram->show();
        m_diagram->setFields(sizeOf(input.targetFovWidthMm, input.targetFovHeightMm), sizeOf(result.fovWidthMm, result.fovHeightMm));
        headers = {localizedText("倍率", "Magnification"), localizedText("视场 X × Y mm", "FOV X × Y mm"), localizedText("μm/px", "μm/px"), localizedText("覆盖 / 采样", "Coverage / sampling")};
        if (usable(input.sensor.resolutionX) && usable(input.sensor.resolutionY) && usable(input.sensor.pixelSizeUm)) {
            for (double mag : {0.05, 0.1, 0.2, 0.3, 0.5, 1.0}) {
                auto candidate = input;
                candidate.solve = Parameters::TelecentricSolve::FieldOfView; candidate.magnification = mag;
                const auto value = Parameters::telecentric(candidate);
                const auto x = checkUpperBound(input.targetFovWidthMm, value.fovWidthMm);
                const auto y = checkUpperBound(input.targetFovHeightMm, value.fovHeightMm);
                const auto coverage = x == CalculationStatus::Failed || y == CalculationStatus::Failed ? CalculationStatus::Failed
                    : (x == CalculationStatus::Unknown || y == CalculationStatus::Unknown ? CalculationStatus::Unknown : CalculationStatus::Passed);
                rows.append({valueText(mag), valueText(value.fovWidthMm) + " × " + valueText(value.fovHeightMm), valueText(value.objectPixelUm),
                             Parameters::statusText(coverage) + " / " + Parameters::statusText(checkUpperBound(value.objectPixelUm, input.targetObjectPixelUm))});
                m_comparisonValues.append(mag);
            }
            m_comparisonKind = "mag";
        }
        if (result.minMagnification && result.maxMagnification)
            note += QLatin1Char('\n') + localizedText("采样下限：", "Sampling minimum: ") + valueText(result.minMagnification, "×")
                + localizedText("；视场上限：", "; coverage maximum: ") + valueText(result.maxMagnification, "×");
        m_applyCheck->setEnabled(status == CalculationStatus::Passed);
    } else if (task() == QLatin1String("motion")) {
        Parameters::ExposureInput input;
        input.solve = choice("motion.solve") == QLatin1String("blur") ? Parameters::ExposureSolve::Blur
            : (choice("motion.solve") == QLatin1String("speed") ? Parameters::ExposureSolve::Speed : Parameters::ExposureSolve::Exposure);
        input.objectPixelUm = number("motion.pixel");
        if (choice("motion.source") != QLatin1String("manual")) {
            const auto actual = Parameters::checkSystem(systemInput());
            input.objectPixelUm = choice("motion.source") == QLatin1String("checkX") ? actual.objectPixelXUm : actual.objectPixelYUm;
        }
        input.speedMmS = number("motion.speed"); input.exposureUs = number("motion.exposure"); input.blurPixels = number("motion.blur");
        const auto result = Parameters::exposure(input);
        status = result.status; note = result.note;
        if (input.solve == Parameters::ExposureSolve::Exposure) metric(0, localizedText("最大曝光", "Maximum exposure"), result.exposureUs, "μs");
        else if (input.solve == Parameters::ExposureSolve::Blur) metric(0, localizedText("预计拖影", "Predicted blur"), result.blurPixels, "px");
        else metric(0, localizedText("允许速度", "Allowed speed"), result.speedMmS, "mm/s");
        metric(1, localizedText("采用的像素当量", "Pixel scale used"), input.objectPixelUm, "μm/px");
        metricText(2, localizedText("像素当量来源", "Pixel scale source"), m_choices.value("motion.source")->currentText());
        headers = {localizedText("说明", "Note"), localizedText("当前条件", "Current condition")};
        rows.append({localizedText("快门与照明", "Shutter and lighting"), localizedText("请单独确认快门形变、亮度及触发时序", "Verify shutter deformation, lighting and trigger timing separately")});
    } else {
        Parameters::TransferInput input;
        input.width = number("camera.nx"); input.height = number("camera.ny");
        input.roiWidth = number("data.roiWidth"); input.roiHeight = number("data.roiHeight");
        input.fps = number("data.fps"); input.cameraCount = number("data.count"); input.overheadPercent = number("data.overhead");
        input.capacityMBps = number("data.capacity"); input.hours = number("data.hours");
        input.pixelFormat = choice("data.format"); input.storageFormat = choice("data.storage");
        const auto result = Parameters::transfer(input);
        status = result.capacityStatus == CalculationStatus::Failed ? CalculationStatus::Failed : result.status;
        note = result.note;
        metric(0, localizedText("每台单帧载荷", "Frame payload per camera"), result.frameBytes ? Number(*result.frameBytes / 1e6) : Number(), "MB");
        metric(1, localizedText("共享链路需求", "Shared link demand"), result.transportMBps, "MB/s");
        metric(2, localizedText("保存数据总量", "Total recording size"), result.storageGB, "GB");
        headers = {localizedText("指标", "Metric"), localizedText("结果", "Result"), localizedText("口径", "Basis")};
        rows.append({localizedText("单帧字节", "Bytes per frame"), valueText(result.frameBytes), input.pixelFormat});
        rows.append({localizedText("总原始载荷", "Total raw payload"), valueText(result.payloadMBps, "MB/s"), localizedText("不含协议预留", "Excludes overhead")});
        rows.append({localizedText("容量利用率", "Capacity utilization"), valueText(result.utilizationPercent, "%"), Parameters::statusText(result.capacityStatus)});
        rows.append({localizedText("保存容量", "Recording size"), valueText(result.storageGB, "GB"), QStringLiteral("1 GB = 10⁹ byte")});
        rows.append({localizedText("保存容量", "Recording size"), valueText(result.storageGiB, "GiB"), QStringLiteral("1 GiB = 2³⁰ byte")});
        if (!input.hours) note += QLatin1Char('\n') + localizedText("填写保存时长后计算总容量。", "Enter recording duration to calculate total storage.");
    }
    if (m_comparisonKind != QLatin1String("check")) showRows(headers, rows);
    m_resultNote->setText(note);
    const QString statusLabel = task() == QLatin1String("check")
        ? (status == CalculationStatus::Passed ? localizedText("已填写校核项满足", "Entered checks meet requirements") : Parameters::statusText(status))
        : (status == CalculationStatus::Passed ? localizedText("已计算 · 依据见下方", "Calculated · see assumptions below") : Parameters::statusText(status));
    m_resultStatus->setText(statusLabel);
    m_resultStatus->setProperty("calculationStatus", static_cast<int>(status));
    setWidgetState(m_resultStatus, stateProperty(status));
    m_resultStatus->style()->unpolish(m_resultStatus);
    m_resultStatus->style()->polish(m_resultStatus);
    m_report += QLatin1Char('\n') + statusLabel + QLatin1Char('\n') + note + QLatin1Char('\n')
        + localizedText("\n输入记录（包含当前任务保留值）：\n", "\nInput record (including retained task values):\n");
    for (auto it = m_fields.cbegin(); it != m_fields.cend(); ++it)
        if (it.key().startsWith(task() + '.') || it.key().startsWith("camera."))
            m_report += it.value()->title() + ": " + it.value()->displayText() + QLatin1Char('\n');
    for (auto it = m_choices.cbegin(); it != m_choices.cend(); ++it)
        if (it.key().startsWith(task() + '.')) m_report += it.value()->accessibleName() + ": " + it.value()->currentText() + QLatin1Char('\n');
    for (auto it = m_texts.cbegin(); it != m_texts.cend(); ++it)
        if (it.key().startsWith(task() + '.')) m_report += it.value()->accessibleName() + ": " + it.value()->text() + QLatin1Char('\n');
    const auto candidate = candidateRequest();
    m_matchLens->setEnabled(m_catalog && usable(number("camera.nx")) && usable(number("camera.ny")) && usable(number("camera.pixel"))
                           && candidate.objectWidthMm > 0.0 && candidate.objectHeightMm > 0.0
                           && (candidate.allowTelecentric || candidate.workingDistanceMm > 0.0));
}

void PureCalculationPage::resetDefaults()
{
    clearCurrentTask();
    m_loading = true;
    // 公共相机已有数据时保留它；只为尚未填写的上下文加载示例。
    if (!number("camera.nx") && !number("camera.ny") && !number("camera.pixel")) {
        setNumber("camera.nx", 2448); setNumber("camera.ny", 2048); setNumber("camera.pixel", 3.45);
        setSource("camera", "example");
    }
    if (task() == QLatin1String("optics")) {
        setNumber("optics.width", 120); setNumber("optics.height", 80); setNumber("optics.distance", 300);
        setNumber("optics.focal", 20); setNumber("optics.margin", 0); setNumber("optics.diameter", 80); setNumber("optics.targetPixel", 50);
    } else if (task() == QLatin1String("sampling")) {
        setNumber("sampling.width", 120); setNumber("sampling.height", 80); setNumber("sampling.feature", 200);
        setNumber("sampling.featurePixels", 4); setNumber("sampling.tolerance", 250); setNumber("sampling.tolerancePixels", 5);
        setNumber("sampling.length", 10); setNumber("sampling.pixelDistance", 200);
        m_texts.value("sampling.region")->setText(localizedText("视场中心", "Image center"));
    } else if (task() == QLatin1String("check")) {
        setNumber("check.width", 120); setNumber("check.height", 80); setNumber("check.targetPixel", 50);
        setNumber("check.distance", 300); setNumber("check.focal", 20); setNumber("check.mag", 0.05);
        setNumber("check.fps", 30); setNumber("check.maxFps", 60); setNumber("check.capacity", 380);
        setChoice("check.format", "Mono8"); setNumber("check.imageCircle", 12); setNumber("check.heightRange", 0);
        setNumber("check.minDistance", 100); setNumber("check.nominalDistance", 300); setNumber("check.distanceTolerance", 5);
        m_texts.value("check.cameraMount")->setText("C"); m_texts.value("check.lensMount")->setText("C");
        setNumber("check.actualWidth", 126.684); setNumber("check.actualHeight", 105.984);
        setNumber("check.speed", 500); setNumber("check.exposure", 50); setNumber("check.blur", 0.5);
    } else if (task() == QLatin1String("tele")) {
        setNumber("tele.width", 120); setNumber("tele.height", 80); setNumber("tele.targetPixel", 50); setNumber("tele.mag", 0.05);
    } else if (task() == QLatin1String("motion")) {
        setNumber("motion.pixel", 50); setNumber("motion.speed", 500); setNumber("motion.blur", 0.5); setNumber("motion.exposure", 50);
    } else {
        setChoice("data.format", "Mono8"); setNumber("data.fps", 30); setNumber("data.count", 1);
        setNumber("data.capacity", 380); setNumber("data.overhead", 0); setNumber("data.hours", 1);
    }
    setSource(task(), "example");
    m_loading = false;
    refresh();
}

void PureCalculationPage::applyToCheck()
{
    if (!m_applyCheck->isEnabled()) return;
    const QString origin = task();
    m_loading = true;
    if (origin == QLatin1String("optics") || origin == QLatin1String("tele"))
        clearLensSpecifications();
    if (origin == QLatin1String("optics")) {
        const auto input = opticsInput(); const auto result = Parameters::optics(input);
        setChoice("check.lens", "fixed"); setChoice("check.geometry", "estimate"); setChoice("check.model", choice("optics.model"));
        setNumber("check.focal", result.focalLengthMm); setNumber("check.distance", result.distanceMm);
        setNumber("check.width", input.targetFovWidthMm); setNumber("check.height", input.targetFovHeightMm);
        setNumber("check.targetPixel", number("optics.targetPixel"));
    } else if (origin == QLatin1String("tele")) {
        const auto input = telecentricInput(); const auto result = Parameters::telecentric(input);
        setChoice("check.lens", "tele"); setChoice("check.geometry", "estimate");
        setNumber("check.mag", result.magnification); setNumber("check.width", input.targetFovWidthMm);
        setNumber("check.height", input.targetFovHeightMm); setNumber("check.targetPixel", input.targetObjectPixelUm);
    } else {
        setNumber("check.width", number("sampling.width")); setNumber("check.height", number("sampling.height"));
        if (choice("sampling.solve") == QLatin1String("actual")) {
            setChoice("check.geometry", "measured");
            setNumber("check.actualWidth", number("sampling.width")); setNumber("check.actualHeight", number("sampling.height"));
        } else {
            Parameters::SamplingInput input;
            input.fovWidthMm = number("sampling.width"); input.fovHeightMm = number("sampling.height");
            input.featureUm = number("sampling.feature"); input.pixelsPerFeature = number("sampling.featurePixels");
            if (m_flags.value("sampling.measurement")->isChecked()) {
                input.measurementBudget = true;
                input.toleranceUm = number("sampling.tolerance"); input.pixelsPerTolerance = number("sampling.tolerancePixels");
            }
            setNumber("check.targetPixel", Parameters::sampling(input).targetObjectPixelUm);
        }
    }
    setSource("check", "derived", origin);
    m_loading = false;
    setTask("check");
    refresh();
}
