use crate::model::{Hardware, Kind};
use serde_json::json;
use std::borrow::Cow;

const SOURCE: &str =
    "https://lmi3d.com/wp-content/uploads/2025/05/DATASHEET_Gocator_2300_US_WEB.pdf";

/// 仅纠正已核实的内置旧记录；原文件、数据库记录及方案快照保持原样。
pub fn apply(item: &mut Hardware) -> bool {
    if item.kind != Kind::ThreeD
        || item.origin != "内置资料"
        || item.manufacturer != "LMI"
        || item.text("sourceUrl") != SOURCE
    {
        return false;
    }
    // 原厂数据表第 2 页：型号、CD、MR、近端 FOV、远端 FOV，单位均为 mm。
    let (cd, range, near, far) = match item.model.as_str() {
        "Gocator 2320" => (40., 25., 18., 26.),
        "Gocator 2330" => (90., 80., 47., 85.),
        "Gocator 2340" => (190., 210., 96., 194.),
        "Gocator 2350" => (300., 400., 158., 365.),
        "Gocator 2370" => (400., 500., 308., 687.),
        "Gocator 2375" => (650., 1350., 324., 1010.),
        "Gocator 2380" => (350., 800., 390., 1260.),
        _ => return false,
    };
    // 同时匹配旧错误形状，避免覆盖用户修改过的几何规格或后续更新的资料。
    if item.specs.contains_key("workingDistanceMinMm")
        || item.specs.contains_key("workingDistanceMaxMm")
        || ![
            ("referenceDistanceMm", cd),
            ("zMeasurementRangeMm", range),
            ("xFovNearMm", near),
            ("xFovFarMm", far),
            ("xFovReferenceMm", (near + far) / 2.),
        ]
        .iter()
        .all(|(key, value)| item.number(key) == Some(*value))
    {
        return false;
    }
    let original = json!({
        "referenceDistanceMm":item.specs.remove("referenceDistanceMm"),
        "xFovReferenceMm":item.specs.remove("xFovReferenceMm"),
    });
    item.specs.insert("workingDistanceMinMm".into(), json!(cd));
    item.specs
        .insert("workingDistanceMaxMm".into(), json!(cd + range));
    item.specs.insert(
        "geometryCorrection".into(),
        json!({"id":"lmi-2300-cd-fov-20260926","original":original}),
    );
    item.specs.insert(
        "geometryCorrectionNote".into(),
        json!("已按原厂规格表校正：CD 对应近端视场，远端距离为 CD + MR；撤销无依据的参考截面均值。原始资料保留。"),
    );
    item.specs
        .insert("geometryCorrectionSource".into(), json!(SOURCE));
    true
}

pub fn corrected(item: &Hardware) -> Cow<'_, Hardware> {
    let mut corrected = item.clone();
    if apply(&mut corrected) {
        Cow::Owned(corrected)
    } else {
        Cow::Borrowed(item)
    }
}
