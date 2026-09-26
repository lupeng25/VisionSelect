use crate::{
    Result,
    model::{Hardware, Kind},
};
use serde::{Deserialize, Serialize};
use serde_json::Value;
use std::collections::BTreeMap;

#[derive(Clone, Debug, Default, Deserialize)]
#[serde(default, deny_unknown_fields)]
pub struct Range {
    pub min: Option<f64>,
    pub max: Option<f64>,
}

#[derive(Clone, Debug, Default, Deserialize)]
#[serde(default, deny_unknown_fields)]
pub struct Filters {
    pub choices: BTreeMap<String, Option<String>>,
    pub ranges: BTreeMap<String, Range>,
}

#[derive(Clone, Copy, Debug, PartialEq, Serialize)]
#[serde(rename_all = "snake_case")]
pub enum Control {
    Choice,
    Range,
}

#[derive(Debug, Serialize)]
pub struct Choice {
    pub value: Option<String>,
    pub label: String,
    pub count: usize,
}

#[derive(Debug, Serialize)]
pub struct Definition {
    pub key: &'static str,
    pub label: &'static str,
    pub unit: &'static str,
    pub control: Control,
    pub primary: bool,
    pub options: Vec<Choice>,
    pub min: Option<f64>,
    pub max: Option<f64>,
    pub known_count: usize,
    #[serde(skip)]
    choices: BTreeMap<String, (String, usize)>,
    #[serde(skip)]
    missing: usize,
}

impl Definition {
    fn new(
        key: &'static str,
        label: &'static str,
        unit: &'static str,
        control: Control,
        primary: bool,
    ) -> Self {
        Self {
            key,
            label,
            unit,
            control,
            primary,
            options: vec![],
            min: None,
            max: None,
            known_count: 0,
            choices: BTreeMap::new(),
            missing: 0,
        }
    }
    pub fn observe(&mut self, item: &Hardware) {
        match self.control {
            Control::Range => {
                if let Some(value) = numeric(item, self.key) {
                    self.min = Some(self.min.map_or(value, |v| v.min(value)));
                    self.max = Some(self.max.map_or(value, |v| v.max(value)));
                    self.known_count += 1;
                }
            }
            Control::Choice => {
                let values = choices(item, self.key);
                if values.is_empty() {
                    self.missing += 1;
                } else {
                    self.known_count += 1;
                }
                for (key, label) in values {
                    self.choices.entry(key).or_insert((label, 0)).1 += 1;
                }
            }
        }
    }
    pub fn finish(&mut self) {
        self.options = std::mem::take(&mut self.choices)
            .into_iter()
            .map(|(value, (label, count))| Choice {
                value: Some(value),
                label,
                count,
            })
            .collect();
        if self.missing > 0 {
            self.options.push(Choice {
                value: None,
                label: "未公开".into(),
                count: self.missing,
            });
        }
    }
}

pub fn definitions(kind: Kind) -> Vec<Definition> {
    use Control::*;
    let fields = match kind {
        Kind::Camera => vec![
            ("interface", "相机接口", "", Choice, true),
            ("color_mode", "色彩类型", "", Choice, true),
            ("megapixels", "有效像素", "MP", Range, true),
            ("max_fps", "最高帧率", "fps", Range, true),
            ("shutter_type", "快门类型", "", Choice, false),
            ("sensor_format", "传感器规格", "", Choice, false),
            ("lens_mount", "镜头接口", "", Choice, false),
            ("resolution_x", "水平有效像素", "px", Range, false),
            ("resolution_y", "垂直有效像素", "px", Range, false),
            ("pixel_size_um", "像元尺寸", "μm", Range, false),
            ("bit_depth", "传感器位深", "bit", Range, false),
        ],
        Kind::Lens => vec![
            ("lens_type", "镜头类型", "", Choice, true),
            ("lens_mount", "镜头接口", "", Choice, true),
            ("focal_length_mm", "焦距", "mm", Range, true),
            ("pmag", "远心倍率", "×", Range, true),
            ("image_circle_mm", "像圈直径", "mm", Range, false),
            ("min_wd_mm", "最小工作距离", "mm", Range, false),
            ("nominal_wd_mm", "标称工作距离", "mm", Range, false),
            ("f_number", "光圈 F 值", "", Range, false),
            ("coaxial_illumination", "同轴照明", "", Choice, false),
        ],
        Kind::Light => vec![
            ("light_type", "光源形态", "", Choice, true),
            ("color", "光源颜色", "", Choice, true),
            ("active_width_mm", "发光宽度", "mm", Range, true),
            ("active_height_mm", "发光高度", "mm", Range, true),
            ("mode", "工作模式", "", Choice, false),
            ("wavelength_nm", "波长", "nm", Range, false),
        ],
        Kind::ThreeD => vec![
            ("technology", "技术路线", "", Choice, true),
            ("interfaces", "设备接口", "", Choice, true),
            ("scanRateMaxHz", "最高扫描频率", "Hz", Range, true),
            ("zMeasurementRangeMm", "Z 量程", "mm", Range, true),
            ("profileDataIntervalUm", "轮廓采样间隔", "μm", Range, false),
            ("zRepeatabilityUm", "Z 重复精度", "μm", Range, false),
            ("xFovReferenceMm", "参考 X 视场", "mm", Range, false),
            ("workingDistanceMinMm", "最小工作距离", "mm", Range, false),
            ("workingDistanceMaxMm", "最大工作距离", "mm", Range, false),
            ("supportsEncoder", "编码器支持", "", Choice, false),
        ],
    };
    fields
        .into_iter()
        .map(|(key, label, unit, control, primary)| {
            Definition::new(key, label, unit, control, primary)
        })
        .collect()
}

impl Filters {
    pub fn parse(payload: &Value, schema: &[Definition]) -> Result<Self> {
        let mut filters: Self = if payload.is_null() {
            Self::default()
        } else {
            serde_json::from_value(payload.clone()).map_err(|e| format!("筛选条件格式无效：{e}"))?
        };
        for (key, value) in &mut filters.choices {
            let definition = schema
                .iter()
                .find(|d| d.key == key && d.control == Control::Choice)
                .ok_or_else(|| format!("当前分类不支持枚举筛选：{key}"))?;
            if let Some(value) = value {
                if value.trim().is_empty() || value.len() > 2048 {
                    return Err(format!("{}筛选值无效", definition.label));
                }
                *value = canonical(key, value).0;
            }
        }
        for (key, range) in &filters.ranges {
            let definition = schema
                .iter()
                .find(|d| d.key == key && d.control == Control::Range)
                .ok_or_else(|| format!("当前分类不支持数值筛选：{key}"))?;
            if [range.min, range.max]
                .into_iter()
                .flatten()
                .any(|n| !n.is_finite() || !(0.0..=1e12).contains(&n))
            {
                return Err(format!("{}范围必须为有效的非负数", definition.label));
            }
            if range.min.zip(range.max).is_some_and(|(min, max)| min > max) {
                return Err(format!("{}下限不能大于上限", definition.label));
            }
        }
        Ok(filters)
    }
    pub fn matches(&self, item: &Hardware) -> bool {
        self.choices.iter().all(|(key, expected)| {
            let values = choices(item, key);
            match expected {
                Some(expected) => values.contains_key(expected),
                None => values.is_empty(),
            }
        }) && self.ranges.iter().all(|(key, range)| {
            if range.min.is_none() && range.max.is_none() {
                return true;
            }
            numeric(item, key).is_some_and(|n| {
                range.min.is_none_or(|min| n >= min) && range.max.is_none_or(|max| n <= max)
            })
        })
    }
}

fn numeric(item: &Hardware, key: &str) -> Option<f64> {
    if key == "megapixels" {
        return numeric(item, "resolution_x")
            .zip(numeric(item, "resolution_y"))
            .map(|(x, y)| x * y / 1e6)
            .filter(|v| v.is_finite());
    }
    item.specs
        .get(key)
        .and_then(|v| v.as_f64().or_else(|| v.as_str()?.trim().parse().ok()))
        .filter(|v| v.is_finite() && *v > 0.)
}

fn choices(item: &Hardware, key: &str) -> BTreeMap<String, String> {
    let Some(value) = item.specs.get(key) else {
        return BTreeMap::new();
    };
    if ["supportsEncoder", "coaxial_illumination"].contains(&key) {
        let flag = match value {
            Value::Bool(b) => Some(*b),
            Value::Number(n) if n.as_i64() == Some(0) => Some(false),
            Value::Number(n) if n.as_i64() == Some(1) => Some(true),
            Value::String(s) => match s.trim().to_lowercase().as_str() {
                "true" | "1" | "yes" | "是" => Some(true),
                "false" | "0" | "no" | "否" => Some(false),
                _ => None,
            },
            _ => None,
        };
        return flag
            .map(|b| BTreeMap::from([(b.to_string(), if b { "支持" } else { "不支持" }.into())]))
            .unwrap_or_default();
    }
    let raw: Vec<&str> = match value {
        Value::String(s) => vec![s],
        Value::Array(values) => values.iter().filter_map(Value::as_str).collect(),
        _ => vec![],
    };
    let mut values = BTreeMap::new();
    for raw in raw {
        // 光源颜色可列出多个可选版本；接口数组则保留完整名称，不能拆开 I/O 或以太网规格中的斜杠。
        let tokens: Vec<_> = if key == "color" {
            raw.split(['/', ',', ';', '、', '，']).collect()
        } else {
            vec![raw]
        };
        for token in tokens {
            let (value, label) = canonical(key, token);
            if ![
                "",
                "unknown",
                "n/a",
                "na",
                "未公开",
                "未标注",
                "未知",
                "-",
                "--",
            ]
            .contains(&value.as_str())
            {
                values.insert(value, label);
            }
        }
    }
    values
}

fn canonical(key: &str, raw: &str) -> (String, String) {
    let value = raw
        .split_whitespace()
        .collect::<Vec<_>>()
        .join(" ")
        .to_lowercase();
    let alias = match (key, value.as_str()) {
        ("interface", "usb3" | "usb3.0" | "usb 3.0") => Some(("usb3", "USB3")),
        ("interface", "gige") => Some(("gige", "GigE")),
        ("color_mode", "mono" | "monochrome" | "黑白") => Some(("mono", "黑白")),
        ("color_mode", "color" | "colour" | "彩色") => Some(("color", "彩色")),
        ("color_mode", "near ir" | "near-infrared" | "nir") => Some(("nir", "近红外")),
        ("shutter_type", "global" | "全局") => Some(("global", "全局快门")),
        ("shutter_type", "rolling" | "卷帘") => Some(("rolling", "卷帘快门")),
        ("shutter_type", "rolling/globalresetrealease" | "rolling/globalresetrelease") => {
            Some(("rolling_global_reset", "卷帘 / 全局复位释放"))
        }
        ("lens_mount", "c" | "c-mount" | "c mount" | "c口") => Some(("c", "C")),
        ("lens_mount", "cs" | "cs-mount" | "cs mount" | "cs口") => Some(("cs", "CS")),
        ("lens_type", "fixed" | "fixedfocal" | "定焦") => Some(("fixed_focal", "定焦")),
        ("lens_type", "objecttelecentric") => Some(("object_telecentric", "物方远心")),
        ("lens_type", "bitelecentric") => Some(("bi_telecentric", "双远心")),
        ("lens_type", "telecentric") => Some(("telecentric", "远心（未细分）")),
        ("light_type", "bar") => Some(("bar", "条形光源")),
        ("light_type", "ring") => Some(("ring", "环形光源")),
        ("light_type", "coaxial") => Some(("coaxial", "同轴光源")),
        ("light_type", "backlight") => Some(("backlight", "背光")),
        ("light_type", "dome") => Some(("dome", "穹顶光源")),
        ("light_type", "telecentricbacklight") => Some(("telecentricbacklight", "远心背光")),
        ("color", "red" | "红色") => Some(("red", "红色")),
        ("color", "green" | "绿色") => Some(("green", "绿色")),
        ("color", "blue" | "蓝色") => Some(("blue", "蓝色")),
        ("color", "white" | "白色") => Some(("white", "白色")),
        ("color", "ir" | "红外") => Some(("ir", "红外")),
        ("color", "uv" | "紫外") => Some(("uv", "紫外")),
        ("mode", "continuous") => Some(("continuous", "连续")),
        ("mode", "strobe") => Some(("strobe", "频闪")),
        _ => None,
    };
    alias
        .map(|(value, label)| (value.into(), label.into()))
        .unwrap_or_else(|| (value, raw.trim().into()))
}
