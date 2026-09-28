use crate::model::{Hardware, Kind};
use serde_json::json;
use std::borrow::Cow;

const GOCATOR_SOURCE: &str =
    "https://lmi3d.com/wp-content/uploads/2025/05/DATASHEET_Gocator_2300_US_WEB.pdf";
const GOCATOR_LASER_SOURCE: &str = "https://am.lmi3d.com/manuals/gopxl/gopxl-1.1/LMILaserLineProfiler/Content/Specs/G2/Gocator2300Series/Gocator2300Series.htm?TocPath=Specifications%7CSensors%7CGocator+2300+Series%7C_____0";
const COOLENS_SOURCE: &str = "https://www.coolens.cn/upfile/2025070753032153.pdf";

/// 修正已核实的内置旧记录，并隔离同批次的未核实字段；保留原文件和方案快照。
pub fn apply(item: &mut Hardware) -> bool {
    if item.origin != "内置资料" {
        return false;
    }
    match item.kind {
        Kind::Camera => correct_camera(item),
        Kind::Lens => correct_lens(item),
        Kind::ThreeD => correct_gocator(item),
        Kind::Light => false,
    }
}

fn daheng_snr_source(model: &str, value: &str) -> Option<&'static str> {
    match (model, value) {
        ("MER3-515-131X2M", "39.8") => Some("https://en.daheng-imaging.com/show-975-3355-1.html"),
        ("MER3-515-131X2C", "38.728") => Some("https://en.daheng-imaging.com/show-975-3354-1.html"),
        ("MER3-1610-18G3M-P", "40.07") => {
            Some("https://en.daheng-imaging.com/show-957-3353-1.html")
        }
        ("MER2-041-302GC", "43") => Some("https://en.daheng-imaging.com/show-104-1891-1.html"),
        ("MARS-251-1938X2C-NF", "45.331") => {
            Some("https://www.daheng-imaging.com/uploadfile/2025/0310/20250310103759850.pdf")
        }
        _ => None,
    }
}

fn correct_camera(item: &mut Hardware) -> bool {
    if item.manufacturer != "Daheng Imaging" || !item.text("dynamic_range_source").is_empty() {
        return false;
    }
    // 同一来源批次的五款样本证实把信噪比误放入动态范围。
    // 其余旧数值保留但暂停当作动态范围使用，避免推断成已核实的信噪比。
    let Some(original) = item.specs.get("dynamic_range_db").cloned() else {
        return false;
    };
    let value = item.text("dynamic_range_db");
    if value.trim().is_empty() {
        return false;
    }
    item.specs.remove("dynamic_range_db");
    if let Some(source) = daheng_snr_source(&item.model, &value) {
        item.specs
            .insert("signal_noise_ratio_db".into(), original.clone());
        let condition = match item.model.as_str() {
            "MER3-515-131X2M" => Some("Mono10；官网另列 Mono12 为 39.797 dB"),
            "MER3-515-131X2C" => Some("Bayer RG10；官网另列 Bayer RG12 为 38.597 dB"),
            _ => None,
        };
        if let Some(condition) = condition {
            item.specs
                .insert("signal_noise_ratio_condition".into(), json!(condition));
        }
        item.specs.insert(
            "dataCorrectionNote".into(),
            json!("官网字段为信噪比，并非动态范围；已按原厂规格重新标注。"),
        );
        item.specs
            .insert("dataCorrectionSource".into(), json!(source));
    } else {
        item.specs
            .insert("unverifiedLegacyDb".into(), original.clone());
        item.specs.insert(
            "dataCorrectionNote".into(),
            json!("历史目录 dB 数值的原始字段尚未逐型号确认，暂不作为动态范围或信噪比使用。"),
        );
    }
    item.specs.insert(
        "dataCorrection".into(),
        json!({"id":"daheng-db-field-20260926","originalDynamicRangeDb":original}),
    );
    true
}

fn correct_lens(item: &mut Hardware) -> bool {
    if item.manufacturer != "COOLENS"
        || item.model != "APO-DTJCA25M-2C-110-G"
        || !item.text("telecentricitySource").is_empty()
        || item.text("telecentricity_deg") != "0.1"
        || item.number("pmag") != Some(2.)
        || item.number("nominal_wd_mm") != Some(110.)
    {
        return false;
    }
    item.specs
        .insert("telecentricity_deg".into(), json!("0.06"));
    item.specs.insert(
        "dataCorrectionNote".into(),
        json!("官网规格书给出物方远心度最大 0.06°；景深 0.17–0.44 mm 与像方最佳 F/# 8.4–22 为条件范围，未填入单值字段。"),
    );
    item.specs
        .insert("dataCorrectionSource".into(), json!(COOLENS_SOURCE));
    item.specs.insert(
        "dataCorrection".into(),
        json!({"id":"coolens-2c-telecentricity-20260926","originalTelecentricityDeg":"0.1"}),
    );
    true
}

fn correct_gocator(item: &mut Hardware) -> bool {
    if item.manufacturer != "LMI" || item.text("sourceUrl") != GOCATOR_SOURCE {
        return false;
    }
    if item.model == "Gocator 2390" {
        if item.text("status") != "未能官网确认"
            || !item.text("laserVariantSource").is_empty()
            || item.text("lightSource") != "蓝色激光"
            || item.number("wavelengthNm") != Some(405.)
        {
            return false;
        }
        let original = json!({
            "lightSource":item.specs.remove("lightSource"),
            "wavelengthNm":item.specs.remove("wavelengthNm"),
        });
        item.specs.insert(
            "laserCorrectionNote".into(),
            json!("当前 LMI 2300 系列规格表未列出此型号；原 405 nm 蓝光声明没有可核实依据，暂不用于选型。"),
        );
        item.specs
            .insert("laserCorrectionSource".into(), json!(GOCATOR_LASER_SOURCE));
        item.specs.insert(
            "laserCorrection".into(),
            json!({"id":"lmi-2390-unverified-laser-20260926","original":original}),
        );
        return true;
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
    let old_geometry = !item.specs.contains_key("workingDistanceMinMm")
        && !item.specs.contains_key("workingDistanceMaxMm")
        && [
            ("referenceDistanceMm", cd),
            ("zMeasurementRangeMm", range),
            ("xFovNearMm", near),
            ("xFovFarMm", far),
            ("xFovReferenceMm", (near + far) / 2.),
        ]
        .iter()
        .all(|(key, value)| item.number(key) == Some(*value));
    let corrected_geometry = item.specs.contains_key("geometryCorrection")
        && item.number("workingDistanceMinMm") == Some(cd)
        && item.number("workingDistanceMaxMm") == Some(cd + range);
    if !old_geometry && !corrected_geometry {
        return false;
    }
    let mut changed = false;
    if old_geometry {
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
            .insert("geometryCorrectionSource".into(), json!(GOCATOR_SOURCE));
        changed = true;
    }
    if item.text("laserVariantSource").is_empty()
        && item.text("lightSource") == "蓝色激光"
        && item.number("wavelengthNm") == Some(405.)
    {
        let original = json!({
            "lightSource":item.specs.insert("lightSource".into(), json!("红色激光（标准配置）")),
            "wavelengthNm":item.specs.insert("wavelengthNm".into(), json!(660)),
        });
        item.specs.insert(
            "laserCorrectionNote".into(),
            json!("LMI 官方表规定未另行注明的标准配置为 660 nm；2375 道路应用型另有 808 nm 近红外，定制配置须核对完整订货号。"),
        );
        item.specs
            .insert("laserCorrectionSource".into(), json!(GOCATOR_LASER_SOURCE));
        item.specs.insert(
            "laserCorrection".into(),
            json!({"id":"lmi-2300-standard-laser-20260926","original":original}),
        );
        changed = true;
    }
    changed
}

pub fn corrected(item: &Hardware) -> Cow<'_, Hardware> {
    let mut corrected = item.clone();
    if apply(&mut corrected) {
        Cow::Owned(corrected)
    } else {
        Cow::Borrowed(item)
    }
}
