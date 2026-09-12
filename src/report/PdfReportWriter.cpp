#include "report/PdfReportWriter.h"
#include "core/PixelFormat.h"
#include "ui/UiHelpers.h"
#include <QDateTime>
#include <QPdfWriter>
#include <QSaveFile>
#include <QTextDocument>
#include <QTextOption>

namespace {
QString L(const char *zh,const char *en) { return UiHelpers::localizedText(zh,en); }
QString escape(const QString &s) { return s.toHtmlEscaped().replace('\n',QStringLiteral("<br/>")); }
QString number(double value,const QString &unit={}) { return value>0 ? QString::number(value,'g',8)+" "+unit : L("未知","Unknown"); }
QString paragraph(const QString &s) { return "<p>"+escape(s)+"</p>"; }
QString row(const QStringList &cells,bool header=false) {
    QString html="<tr>";
    for(const auto &s:cells) html+=(header?"<th>":"<td>")+escape(s.isEmpty()?L("未知","Unknown"):s)+(header?"</th>":"</td>");
    return html+"</tr>";
}
QString startTable(const QStringList &headers) { return "<table border='1' cellspacing='0' cellpadding='6' width='100%'><thead>"+row(headers,true)+"</thead><tbody>"; }
}

bool PdfReportWriter::write(const QString &path,const SelectionRequest &request,
    const QVector<SelectionResult> &results,QString *error) const
{
    if(error) error->clear();
    QSaveFile file(path);
    if(!file.open(QIODevice::WriteOnly)) { if(error) *error=file.errorString(); return false; }
    QString html="<html><head><style>body{font-size:9pt;color:#223047;}h1{font-size:18pt;}h2{font-size:12pt;color:#334699;}th{background-color:#edf0fa;}td,th{border-color:#c8cfdc;}</style></head><body>";
    html+="<h1>"+L("工业机器视觉选型报告","Industrial Machine Vision Selection Report")+"</h1>";
    html+=paragraph(L("生成时间：","Generated: ")+QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm"));
    html+=paragraph(L("本报告用于初筛。状态与未知项是方案的一部分；相对排序不代表精度保证。最终需确认安装、厂家规格及实际打光。",
        "This report supports preliminary selection. Status and unknown checks are part of each plan; ranking does not guarantee accuracy. Verify installation, vendor specifications and actual lighting."));
    html+="<h2>"+L("需求快照","Requirement snapshot")+"</h2>";
    html+=startTable({L("参数","Parameter"),L("数值","Value")});
    html+=row({L("工件尺寸 / 定位余量","Part size / margin"),QStringLiteral("%1 x %2 mm / %3 mm").arg(request.objectWidthMm).arg(request.objectHeightMm).arg(request.placementMarginMm)});
    html+=row({L("特征 / 测量容差","Feature / tolerance"),QStringLiteral("%1 / %2 um").arg(request.minFeatureUm).arg(request.measurementToleranceUm)});
    html+=row({L("工作距离 / 高度峰峰值","Working distance / peak-to-peak height"),QStringLiteral("%1 / %2 mm").arg(request.workingDistanceMm).arg(request.heightVariationMm)});
    html+=row({L("帧率 / 运动速度","Frame rate / motion speed"),QStringLiteral("%1 fps / %2 mm/s").arg(request.requiredFps).arg(request.motionSpeedMmS)});
    html+=row({L("检测 / 材质 / 运动模式","Inspection / surface / motion"),detectionTypeLabel(request.detectionType)+" / "+surfaceTypeLabel(request.surfaceType)+" / "+motionModeLabel(request.motionMode)});
    html+="</tbody></table>";
    if(!request.projectNotes.isEmpty()) html+=paragraph(request.projectNotes);
    html+=paragraph(L("景深余量系数为 1.5；景深条件未确认时保留待确认。传输载荷按明确像素格式计算，缺失格式或经验带宽不作为已确认规格。",
        "DOF safety factor is 1.5; unconfirmed DOF conditions remain pending. Payload uses the explicit pixel format; missing formats and estimated bandwidth are unconfirmed specifications."));
    for(int i=0;i<results.size();++i) {
        const auto r=localizedResult(results[i]);
        html+="<h2 style='page-break-before:always'>"+escape(QStringLiteral("#%1 · %2 · %3").arg(i+1).arg(r.schemeTitle,UiHelpers::compatibilityText(r)))+"</h2>";
        html+=startTable({L("类别","Category"),L("厂家","Manufacturer"),L("完整型号","Full model")});
        html+=row({L("相机","Camera"),r.camera.manufacturer,r.camera.model});
        html+=row({L("镜头","Lens"),r.lens.manufacturer,r.lens.model});
        html+=row({L("光源","Light"),r.light.manufacturer,r.light.model});html+="</tbody></table>";
        html+=paragraph(QStringLiteral("FOV: %1 x %2; %3; DOF: %4; %5").arg(number(r.effectiveFovWidthMm,"mm"),number(r.effectiveFovHeightMm,"mm"),number(r.objectPixelSizeUm,"um/px"),number(r.estimatedDofMm,"mm"),r.formulaSummary));
        const QString bandwidthSource = r.camera.bandwidthMBps <= 0 ? L("容量未知","Unknown capacity")
            : r.camera.bandwidthSource == "specified" ? L("容量已确认","Capacity confirmed")
            : r.camera.bandwidthSource == "estimated" ? L("容量为估算值，待确认","Estimated capacity; unconfirmed") : L("容量未知","Unknown capacity");
        html+=paragraph(L("带宽需求 / 容量：","Bandwidth required / capacity: ")+number(r.bandwidthRequiredMBps,"MB/s")+" / "+number(r.interfaceCapacityMBps,"MB/s")+"; "+bandwidthSource);
        html+=paragraph(L("传输格式：","Pixel format: ")+ (PixelFormat::layout(r.camera.transportPixelFormat())?r.camera.transportPixelFormat():L("待确认","Unconfirmed")));
        html+=startTable({L("校核项","Check"),L("状态","Status")});
        for(size_t j=0;j<r.checks.states.size();++j) if(r.checks.states[j]!=CandidateCheckState::NotApplicable)
            html+=row({candidateCheckLabel(static_cast<CandidateCheck>(j)),candidateCheckStateLabel(r.checks.states[j])});
        html+="</tbody></table>";
        html+=paragraph(L("风险与确认项：","Risks and confirmation items: ")+UiHelpers::riskSummary(r));
        html+=paragraph(L("计算依据：","Calculation reasons: ")+r.score.reasons.join("; "));
        html+=paragraph(L("相机资料来源：","Camera source: ")+(r.camera.sourceUrl.isEmpty()?L("未公开，请确认厂家数据表","Unpublished; verify the vendor datasheet"):r.camera.sourceUrl)+" "+r.camera.sourceDate);
        if(r.catalogCameras>0) html+=paragraph(L("本批比较范围：","Compared in this batch: ")+QStringLiteral("cameras %1/%2, lenses %3/%4").arg(r.searchedCameras).arg(r.catalogCameras).arg(r.searchedLenses).arg(r.catalogLenses));
    }
    html+="</body></html>";
    {
        QPdfWriter writer(&file);writer.setPageSize(QPageSize(QPageSize::A4));writer.setResolution(96);
        writer.setPageMargins(QMarginsF(15,15,15,15));writer.setTitle(L("工业机器视觉选型报告","Industrial Machine Vision Selection Report"));
        QTextDocument document;document.setDefaultFont(QFont("Microsoft YaHei",9));
        QTextOption option;option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);document.setDefaultTextOption(option);
        document.setHtml(html);document.print(&writer);
    }
    if(file.error()!=QFileDevice::NoError || file.size()==0) { if(error) *error=file.errorString(); return false; }
    if(!file.commit()) { if(error) *error=file.errorString(); return false; }
    return true;
}
