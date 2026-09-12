#include "report/BomCsvWriter.h"
#include "core/Localization.h"
#include <QIODevice>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

namespace {
QString cell(QString value) { value.replace('"', "\"\""); return '"' + value + '"'; }
QString number(double value) { return value > 0.0 ? QString::number(value, 'g', 12) : CoreI18n::localizedText("未知", "Unknown"); }
}
bool BomCsvWriter::writeToDevice(QIODevice *device, const SelectionRequest &request,
    const QVector<SelectionResult> &results, QString *error) const
{
    if (error) error->clear();
    if (!device || !device->isWritable()) { if (error) *error = CoreI18n::localizedText("输出设备不可写。", "Output device is not writable."); return false; }
    const QString requestJson = QString::fromUtf8(QJsonDocument(QJsonObject{
        {"object_width_mm",request.objectWidthMm},{"object_height_mm",request.objectHeightMm},{"placement_margin_mm",request.placementMarginMm},
        {"min_feature_um",request.minFeatureUm},{"tolerance_um",request.measurementToleranceUm},{"working_distance_mm",request.workingDistanceMm},
        {"height_variation_mm",request.heightVariationMm},{"fps",request.requiredFps},{"motion_speed_mm_s",request.motionSpeedMmS},
        {"motion_mode",int(request.motionMode)},{"detection_type",int(request.detectionType)},{"surface_type",int(request.surfaceType)},
        {"reflective",request.reflective},{"prefer_mono",request.preferMono},{"allow_telecentric",request.allowTelecentric}
    }).toJson(QJsonDocument::Compact));
    QString output = QStringLiteral("schema_version,scheme_id,rank,status,failed_checks,unknown_checks,category,manufacturer,model,key_specs,risks,request_json,project_notes,source_url,source_date,bandwidth_source,pixel_format\n");
    for (int i=0; i<results.size(); ++i) {
        const auto r = localizedResult(results[i]);
        QStringList failures, unknowns;
        for (size_t j=0;j<r.checks.states.size();++j) {
            if (r.checks.states[j] == CandidateCheckState::Failed) failures << candidateCheckKey(static_cast<CandidateCheck>(j));
            if (r.checks.states[j] == CandidateCheckState::Unknown) unknowns << candidateCheckKey(static_cast<CandidateCheck>(j));
        }
        const QString status = !r.hardConstraintsPassed || r.checks.failed() ? "failed" : r.checks.unknown() ? "pending" : "screened";
        const auto row = [&](const QString &category, const QString &manufacturer, const QString &model, const QString &specs) {
            QStringList values = {"2", QStringLiteral("scheme-%1").arg(i+1),QString::number(i+1),status,failures.join(';'),unknowns.join(';'),
                category,manufacturer,model,specs,r.score.risks.join(';'),requestJson,request.projectNotes,
                category == "camera" ? r.camera.sourceUrl : QString(), category == "camera" ? r.camera.sourceDate : QString(),
                category == "camera" ? r.camera.bandwidthSource : QString(), category == "camera" ? r.camera.transportPixelFormat() : QString()};
            for (auto &value:values) value=cell(value);
            output += values.join(',') + '\n';
        };
        row("camera",r.camera.manufacturer,r.camera.model,QStringLiteral("%1 x %2 px; %3 um/px; %4 MB/s").arg(r.camera.resolutionX).arg(r.camera.resolutionY).arg(number(r.objectPixelSizeUm),number(r.bandwidthRequiredMBps)));
        row("lens",r.lens.manufacturer,r.lens.model,QStringLiteral("FOV %1 x %2 mm; DOF %3 mm; WD %4 mm; %5").arg(number(r.effectiveFovWidthMm),number(r.effectiveFovHeightMm),number(r.estimatedDofMm),number(request.workingDistanceMm),r.lens.lensMount));
        row("light",r.light.manufacturer,r.light.model,QStringLiteral("%1; %2 x %3 mm; %4").arg(r.light.typeLabel(),number(r.light.activeWidthMm),number(r.light.activeHeightMm),r.light.mode));
    }
    const QByteArray bytes = output.toUtf8();
    if (device->write(bytes) != bytes.size()) { if (error) *error = device->errorString(); return false; }
    return true;
}
bool BomCsvWriter::write(const QString &path, const SelectionRequest &request,
    const QVector<SelectionResult> &results, QString *error) const
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) { if (error) *error=file.errorString(); return false; }
    if (!writeToDevice(&file,request,results,error)) return false;
    if (!file.commit()) { if (error) *error=file.errorString(); return false; }
    return true;
}
