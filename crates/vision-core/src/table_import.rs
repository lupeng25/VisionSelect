use crate::{
    Result, catalog,
    model::{Hardware, Kind},
};
use base64::Engine;
use calamine::{Reader, open_workbook_auto_from_rs};
use serde::Serialize;
use serde_json::{Value, json};
use std::{collections::HashSet, io::Cursor};

#[derive(Clone, Serialize)]
pub struct Field {
    pub key: &'static str,
    pub label: &'static str,
    pub numeric: bool,
}
fn field(key: &'static str, label: &'static str, numeric: bool) -> Field {
    Field {
        key,
        label,
        numeric,
    }
}
pub fn fields(kind: Kind) -> Vec<Field> {
    let mut result = vec![
        field("model", "型号", false),
        field("manufacturer", "厂家", false),
    ];
    let extra = match kind {
        Kind::Camera => vec![
            ("resolution_x", "水平像素 / px", true),
            ("resolution_y", "垂直像素 / px", true),
            ("pixel_size_um", "像元尺寸 / μm", true),
            ("sensor_format", "传感器规格", false),
            ("color_mode", "色彩模式", false),
            ("pixel_format", "传输像素格式", false),
            ("shutter_type", "快门类型", false),
            ("max_fps", "标称帧率 / fps", true),
            ("interface", "数据接口", false),
            ("lens_mount", "镜头接口", false),
            ("bandwidth_mbps", "数据带宽 / MB/s", true),
            ("bandwidth_source", "带宽来源", false),
            ("bit_depth", "位深 / bit", true),
            ("dynamic_range_db", "动态范围 / dB", true),
            ("signal_noise_ratio_db", "信噪比 / dB", true),
        ],
        Kind::Lens => vec![
            ("lens_type", "镜头类型", false),
            ("lens_mount", "镜头接口", false),
            ("focal_length_mm", "焦距 / mm", true),
            ("min_wd_mm", "最小工作距离 / mm", true),
            ("image_circle_mm", "像圈 / mm", true),
            ("pmag", "倍率", true),
            ("nominal_wd_mm", "标称工作距离 / mm", true),
            ("wd_tolerance_mm", "距离容差 / mm", true),
            ("max_sensor_diagonal_mm", "最大靶面对角线 / mm", true),
            ("dof_mm", "景深 / mm", true),
            ("f_number", "光圈 F 值", true),
            ("megapixel_rating", "标称分辨率 / MP", true),
            ("distortion_percent", "畸变 / %", true),
            ("telecentricity_deg", "远心度 / °", true),
        ],
        Kind::Light => vec![
            ("light_type", "光源类型", false),
            ("color", "颜色", false),
            ("wavelength_nm", "波长 / nm", true),
            ("mode", "照明模式", false),
            ("active_width_mm", "发光面宽 / mm", true),
            ("active_height_mm", "发光面高 / mm", true),
            ("best_for", "适用场景", false),
        ],
        Kind::ThreeD => vec![
            ("technology", "技术路线", false),
            ("referenceDistanceMm", "参考距离 / mm", true),
            ("workingDistanceMinMm", "最近工作距离 / mm", true),
            ("workingDistanceMaxMm", "最远工作距离 / mm", true),
            ("xFovNearMm", "近端 X 视场 / mm", true),
            ("xFovReferenceMm", "参考截面 X 视场 / mm", true),
            ("xFovFarMm", "远端 X 视场 / mm", true),
            ("zMeasurementRangeMm", "Z 量程 / mm", true),
            ("zRepeatabilityUm", "Z 重复精度 / μm", true),
            ("profileDataIntervalUm", "轮廓数据间隔 / μm", true),
            ("scanRateMaxHz", "最高轮廓频率 / Hz", true),
            ("profilePoints", "轮廓点数", true),
            ("supportsEncoder", "支持编码器", false),
            ("supportsExternalTrigger", "支持外部触发", false),
        ],
    };
    result.extend(extra.into_iter().map(|(k, l, n)| field(k, l, n)));
    result.extend([
        field("source_url", "来源链接", false),
        field("notes", "备注", false),
    ]);
    result
}
fn normalized(value: &str) -> String {
    value
        .to_lowercase()
        .replace(['μ', 'µ'], "u")
        .chars()
        .filter(|c| !c.is_whitespace() && !"_-/()（）".contains(*c))
        .collect()
}
fn guess(header: &str, options: &[Field]) -> Option<String> {
    let normalized_header = normalized(header);
    if let Some(f) = options.iter().find(|f| {
        normalized(f.key) == normalized_header || normalized(f.label) == normalized_header
    }) {
        return Some(f.key.into());
    }
    let alias = match header.trim().to_lowercase().as_str() {
        "产品型号" | "设备型号" | "产品名称" => "model",
        "品牌" | "制造商" | "生产厂家" | "厂商" | "brand" => "manufacturer",
        "像元尺寸" | "像素尺寸" => "pixel_size_um",
        "水平像素" | "宽度像素" | "分辨率宽" => "resolution_x",
        "垂直像素" | "高度像素" | "分辨率高" => "resolution_y",
        "帧率" | "最大帧率" => "max_fps",
        "焦距" => "focal_length_mm",
        "光学倍率" | "远心倍率" => "pmag",
        "工作距离" => "nominal_wd_mm",
        "景深" => "dof_mm",
        "接口" => "interface",
        "镜头卡口" => "lens_mount",
        "发光面宽" => "active_width_mm",
        "发光面高" => "active_height_mm",
        "波长" => "wavelength_nm",
        "来源" => "source_url",
        "说明" => "notes",
        _ => return None,
    };
    options.iter().any(|f| f.key == alias).then(|| alias.into())
}

pub struct Table {
    pub headers: Vec<String>,
    pub rows: Vec<(usize, Vec<String>)>,
    pub sheets: Vec<String>,
    pub sheet: String,
}
pub fn read(payload: &Value) -> Result<Table> {
    let content = payload["content"].as_str().ok_or("缺少表格内容")?;
    if content.len() > 48 * 1024 * 1024 {
        return Err("文件超过大小限制".into());
    }
    let header_row = match payload.get("header_row") {
        None => 1,
        Some(value) => value.as_u64().ok_or("表头行必须是整数")? as usize,
    };
    if !(1..=1000).contains(&header_row) {
        return Err("表头行应在 1–1000 之间".into());
    }
    let mut sheets = Vec::new();
    let mut sheet = String::new();
    let rows: Vec<(usize, Vec<String>)> = match payload["format"].as_str() {
        Some("csv") => {
            let delimiter = match payload["delimiter"].as_str().unwrap_or(",") {
                "," => b',',
                ";" => b';',
                "\t" => b'\t',
                _ => return Err("不支持的分隔符".into()),
            };
            let mut reader = csv::ReaderBuilder::new()
                .has_headers(false)
                .flexible(true)
                .delimiter(delimiter)
                .trim(csv::Trim::All)
                .from_reader(content.trim_start_matches('\u{feff}').as_bytes());
            let mut rows = Vec::new();
            for row in reader.records() {
                let record = row.map_err(|e| format!("CSV 格式错误：{e}"))?;
                rows.push((
                    record
                        .position()
                        .map(|p| p.line() as usize)
                        .unwrap_or(rows.len() + 1),
                    record.iter().map(str::to_owned).collect(),
                ));
                if rows.len() > 101_000 {
                    return Err("表格超过十万条数据限制".into());
                }
            }
            rows
        }
        Some("excel") => {
            let bytes = base64::engine::general_purpose::STANDARD
                .decode(content)
                .map_err(|_| "Excel 文件编码无效")?;
            if bytes.len() > 32 * 1024 * 1024 {
                return Err("Excel 文件超过 32 MB 限制".into());
            }
            let mut book = open_workbook_auto_from_rs(Cursor::new(bytes))
                .map_err(|e| format!("无法读取 Excel：{e}"))?;
            sheets = book.sheet_names();
            sheet = payload["sheet"]
                .as_str()
                .filter(|s| !s.is_empty())
                .map(str::to_owned)
                .or_else(|| sheets.first().cloned())
                .ok_or("工作簿没有工作表")?;
            let range = book
                .worksheet_range(&sheet)
                .map_err(|e| format!("无法读取工作表：{e}"))?;
            if range.height() > 101_000 || range.width() > 200 {
                return Err("工作表最多支持十万条数据、200 列".into());
            }
            let start = range.start().unwrap_or((0, 0)).0 as usize + 1;
            range
                .rows()
                .enumerate()
                .map(|(i, r)| {
                    (
                        start + i,
                        r.iter().map(|v| v.to_string().trim().to_owned()).collect(),
                    )
                })
                .collect()
        }
        _ => return Err("请选择 Excel 或 CSV 参数表".into()),
    };
    let headers = rows
        .iter()
        .find(|(n, _)| *n == header_row)
        .map(|(_, r)| r.clone())
        .unwrap_or_default();
    if headers.len() > 200 {
        return Err("表格最多支持 200 列".into());
    }
    let rows: Vec<_> = rows
        .into_iter()
        .filter(|(n, r)| *n > header_row && r.iter().any(|v| !v.trim().is_empty()))
        .collect();
    if rows.len() > 100_000 {
        return Err("单次最多导入十万条数据".into());
    }
    Ok(Table {
        headers,
        rows,
        sheets,
        sheet,
    })
}
pub fn inspect(payload: &Value) -> Result<Value> {
    let kind: Kind =
        serde_json::from_value(payload["kind"].clone()).map_err(|_| "请选择硬件分类")?;
    let table = read(payload)?;
    let options = fields(kind);
    let mapping: Vec<_> = table.headers.iter().map(|h| guess(h, &options)).collect();
    Ok(
        json!({"headers":table.headers,"sample":table.rows.iter().take(5).map(|(row,values)|json!({"row":row,"values":values})).collect::<Vec<_>>(),"total":table.rows.len(),"sheets":table.sheets,"sheet":table.sheet,"fields":options,"mapping":mapping}),
    )
}

pub fn parse(payload: &Value) -> Result<(Vec<(usize, Hardware)>, Vec<Value>)> {
    let kind: Kind =
        serde_json::from_value(payload["kind"].clone()).map_err(|_| "请选择硬件分类")?;
    let table = read(payload)?;
    if table.headers.is_empty() {
        return Err("表头行没有内容，请调整表头行号".into());
    }
    if table.rows.is_empty() {
        return Err("表头下没有设备数据".into());
    }
    let options = fields(kind);
    let mapping: Vec<Option<String>> =
        serde_json::from_value(payload["mapping"].clone()).map_err(|_| "请先完成字段对应")?;
    if mapping.len() != table.headers.len() {
        return Err("字段对应与表格列数不一致".into());
    }
    let mut mapped = HashSet::new();
    for key in mapping.iter().flatten() {
        if !options.iter().any(|f| f.key == key) {
            return Err(format!("未知目标字段：{key}"));
        }
        if !mapped.insert(key.as_str()) {
            return Err(format!("多列不能同时对应字段：{key}"));
        }
    }
    if !mapped.contains("model") || !mapped.contains("manufacturer") {
        return Err("必须对应型号和厂家两列".into());
    }
    let mut items = Vec::new();
    let mut errors = Vec::new();
    let mut seen = HashSet::new();
    for (row, cells) in table.rows {
        let convert = || -> Result<Hardware> {
            if cells.len() > mapping.len() && cells[mapping.len()..].iter().any(|s| !s.is_empty()) {
                return Err("存在没有表头的数据列".into());
            }
            let mut specs = serde_json::Map::new();
            for (index, key) in mapping.iter().enumerate() {
                let Some(key) = key else {
                    continue;
                };
                let text = cells.get(index).map(|s| s.trim()).unwrap_or("");
                if text.is_empty() {
                    continue;
                }
                let f = options.iter().find(|f| f.key == key).unwrap();
                if f.numeric {
                    let value: f64 = text.parse().map_err(|_| {
                        format!(
                            "{}：需要纯数值，单位以目标字段为准（当前：{}）",
                            f.label, text
                        )
                    })?;
                    let signed = key == "distortion_percent";
                    let zero = ["telecentricity_deg", "wd_tolerance_mm"].contains(&key.as_str());
                    if !value.is_finite()
                        || value.abs() > 1e12
                        || (!signed && (value < 0. || (!zero && value == 0.)))
                    {
                        return Err(format!("{}：数值超出有效范围", f.label));
                    }
                    if ["resolution_x", "resolution_y", "bit_depth", "profilePoints"]
                        .contains(&key.as_str())
                        && value.fract() != 0.
                    {
                        return Err(format!("{}：必须是整数", f.label));
                    }
                    specs.insert(key.clone(), json!(value));
                } else {
                    let text = match (key.as_str(), text) {
                        ("lens_type", "远心" | "远心镜头") => "Telecentric",
                        ("lens_type", "双远心") => "BiTelecentric",
                        ("lens_type", "定焦" | "定焦镜头") => "Fixed",
                        ("lens_type", "线扫" | "线扫镜头") => "LineScan",
                        ("shutter_type", "全局" | "全局快门") => "Global",
                        ("shutter_type", "滚动" | "滚动快门") => "Rolling",
                        ("bandwidth_source", "已核实" | "原厂规格") => "specified",
                        _ => text,
                    };
                    if ["supportsEncoder", "supportsExternalTrigger"].contains(&key.as_str()) {
                        let flag = match text.to_lowercase().as_str() {
                            "是" | "支持" | "true" | "1" => true,
                            "否" | "不支持" | "false" | "0" => false,
                            _ => return Err(format!("{}：请填写是/否", f.label)),
                        };
                        specs.insert(key.clone(), json!(flag));
                    } else {
                        specs.insert(key.clone(), json!(text));
                    }
                }
            }
            for key in ["model", "manufacturer"] {
                if !specs.contains_key(key) {
                    return Err(format!(
                        "{}不能为空",
                        if key == "model" { "型号" } else { "厂家" }
                    ));
                }
            }
            catalog::from_specs(kind, specs, "用户导入")
        };
        match convert() {
            Ok(item) if seen.insert(item.id.clone()) => items.push((row,item)),
            Ok(item) => errors.push(json!({"row":row,"message":format!("同文件型号重复：{} {}",item.manufacturer,item.model)})),
            Err(message) => errors.push(json!({"row":row,"message":message})),
        }
    }
    Ok((items, errors))
}
