use serde::{Deserialize, Serialize};
use serde_json::{Map, Value};

#[derive(Debug, Clone, Copy, Serialize, Deserialize, PartialEq, Eq, Hash)]
#[serde(rename_all = "snake_case")]
pub enum Kind {
    Camera,
    Lens,
    Light,
    ThreeD,
}

impl Kind {
    pub const ALL: [Self; 4] = [Self::Camera, Self::Lens, Self::Light, Self::ThreeD];
    pub fn key(self) -> &'static str {
        match self {
            Self::Camera => "camera",
            Self::Lens => "lens",
            Self::Light => "light",
            Self::ThreeD => "three_d",
        }
    }
    pub fn title(self) -> &'static str {
        match self {
            Self::Camera => "面阵相机",
            Self::Lens => "成像镜头",
            Self::Light => "照明光源",
            Self::ThreeD => "3D 相机",
        }
    }
    pub fn index(self) -> usize {
        match self {
            Self::Camera => 0,
            Self::Lens => 1,
            Self::Light => 2,
            Self::ThreeD => 3,
        }
    }
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct Hardware {
    pub id: String,
    pub kind: Kind,
    pub manufacturer: String,
    pub model: String,
    pub specs: Map<String, Value>,
    pub origin: String,
}
impl Hardware {
    pub fn text(&self, key: &str) -> String {
        match self.specs.get(key) {
            Some(Value::String(v)) => v.clone(),
            Some(Value::Array(values)) => values
                .iter()
                .filter_map(Value::as_str)
                .collect::<Vec<_>>()
                .join(" / "),
            Some(v) if !v.is_null() => v.to_string(),
            _ => String::new(),
        }
    }
    pub fn number(&self, key: &str) -> Option<f64> {
        self.specs
            .get(key)
            .and_then(|v| v.as_f64().or_else(|| v.as_str()?.parse().ok()))
            .filter(|n| n.is_finite() && *n > 0.0)
    }
    pub fn flag(&self, key: &str) -> Option<bool> {
        match self.specs.get(key)? {
            Value::Bool(v) => Some(*v),
            Value::Number(v) if v.as_i64() == Some(0) => Some(false),
            Value::Number(v) if v.as_i64() == Some(1) => Some(true),
            Value::String(v) if v == "true" || v == "1" => Some(true),
            Value::String(v) if v == "false" || v == "0" => Some(false),
            _ => None,
        }
    }
    pub fn telecentric(&self) -> bool {
        self.text("lens_type")
            .to_lowercase()
            .contains("telecentric")
    }
    pub fn validate(&self) -> crate::Result<()> {
        if self.model.trim().is_empty()
            || self.model.len() > 1000
            || self.manufacturer.len() > 1000
            || self.id.len() > 2200
        {
            return Err("设备型号或厂家字段无效".into());
        }
        if self.specs.len() > 200 {
            return Err("设备规格字段数量超限".into());
        }
        Ok(())
    }
}

#[derive(Debug, Clone, Copy, Serialize, Deserialize, PartialEq, Eq)]
#[serde(rename_all = "snake_case")]
pub enum Mode {
    Imaging,
    Scanning,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct Parameters {
    pub width: f64,
    pub height: f64,
    pub margin: f64,
    pub pixel: f64,
    pub distance: f64,
    pub fps: f64,
    pub speed: f64,
    pub exposure: f64,
    pub blur: f64,
    pub variation: f64,
    pub dof_confirmed: bool,
    pub measured: bool,
    pub measured_width: f64,
    pub measured_height: f64,
    pub pixel_format: String,
    pub scan_width: f64,
    pub z_range: f64,
    pub z_repeat: f64,
    pub scan_distance: Option<f64>,
    pub scan_length: f64,
    pub interval: f64,
    pub scan_speed: f64,
    pub rate: f64,
    pub safety: f64,
    pub scan_exposure: f64,
    pub readout: f64,
    pub trigger: String,
    pub travel: f64,
    pub pulses: u32,
    pub per_profile: u32,
    pub pulse_rate: f64,
}
impl Default for Parameters {
    fn default() -> Self {
        Self {
            width: 20.,
            height: 20.,
            margin: 2.,
            pixel: 10.,
            distance: 110.,
            fps: 20.,
            speed: 0.,
            exposure: 100.,
            blur: 1.,
            variation: 2.,
            dof_confirmed: false,
            measured: false,
            measured_width: 24.,
            measured_height: 24.,
            pixel_format: String::new(),
            scan_width: 40.,
            z_range: 10.,
            z_repeat: 10.,
            scan_distance: None,
            scan_length: 300.,
            interval: 0.05,
            scan_speed: 40.,
            rate: 1000.,
            safety: 0.8,
            scan_exposure: 100.,
            readout: 3.,
            trigger: "free".into(),
            travel: 10.,
            pulses: 10000,
            per_profile: 50,
            pulse_rate: 50000.,
        }
    }
}
impl Parameters {
    pub fn validate(&self) -> crate::Result<()> {
        let values = [
            ("工件宽度", self.width),
            ("工件高度", self.height),
            ("像素当量", self.pixel),
            ("工作距离", self.distance),
            ("帧率", self.fps),
            ("曝光", self.exposure),
            ("拖影", self.blur),
            ("实测宽度", self.measured_width),
            ("实测高度", self.measured_height),
            ("扫描覆盖", self.scan_width),
            ("Z 量程", self.z_range),
            ("重复精度", self.z_repeat),
            ("扫描长度", self.scan_length),
            ("轮廓间距", self.interval),
            ("扫描速度", self.scan_speed),
            ("采样频率", self.rate),
            ("三维曝光", self.scan_exposure),
            ("编码器距离", self.travel),
            ("脉冲频率", self.pulse_rate),
        ];
        for (name, value) in values {
            if !value.is_finite() || value <= 0. || value > 1e8 {
                return Err(format!("{name}必须是大于 0 且不超过 100000000 的有限数值"));
            }
        }
        for value in [self.margin, self.speed, self.variation, self.readout] {
            if !value.is_finite() || !(0.0..=1e8).contains(&value) {
                return Err("余量、速度、高度变化和读出时间必须是有效非负数".into());
            }
        }
        if !self.safety.is_finite() || self.safety <= 0. || self.safety > 1. {
            return Err("采样裕量系数必须在 0 到 1 之间".into());
        }
        if self.pulses == 0 || self.per_profile == 0 {
            return Err("编码器脉冲数量必须大于 0".into());
        }
        if self
            .scan_distance
            .is_some_and(|distance| !distance.is_finite() || distance <= 0. || distance > 1e8)
        {
            return Err("三维工作距离无效".into());
        }
        if !["free", "external", "encoder"].contains(&self.trigger.as_str()) {
            return Err("未知触发方式".into());
        }
        if self.pixel_format.len() > 64 {
            return Err("像素格式过长".into());
        }
        Ok(())
    }
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct Reference {
    pub name: String,
    pub mode: Mode,
    pub parameters: Parameters,
    pub hardware: [Option<Hardware>; 4],
}
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct Project {
    pub version: u32,
    pub id: String,
    pub name: String,
    pub mode: Mode,
    pub parameters: Parameters,
    pub hardware: [Option<Hardware>; 4],
    pub reference: Option<Reference>,
}
impl Default for Project {
    fn default() -> Self {
        Self {
            version: 1,
            id: uuid::Uuid::new_v4().to_string(),
            name: "未命名方案".into(),
            mode: Mode::Imaging,
            parameters: Parameters::default(),
            hardware: [None, None, None, None],
            reference: None,
        }
    }
}
impl Project {
    pub fn validate(&self) -> crate::Result<()> {
        if self.version != 1 {
            return Err("不支持的方案版本".into());
        }
        if uuid::Uuid::parse_str(&self.id).is_err() {
            return Err("方案标识无效".into());
        }
        if self.name.trim().is_empty() || self.name.chars().count() > 100 {
            return Err("方案名称必须为 1–100 个字符".into());
        }
        self.parameters.validate()?;
        validate_hardware(&self.hardware)?;
        if let Some(reference) = &self.reference {
            reference.parameters.validate()?;
            validate_hardware(&reference.hardware)?;
        }
        Ok(())
    }
}
fn validate_hardware(items: &[Option<Hardware>; 4]) -> crate::Result<()> {
    for (index, item) in items.iter().enumerate() {
        if let Some(item) = item {
            item.validate()?;
            if item.kind.index() != index {
                return Err("方案中的设备分类不一致".into());
            }
        }
    }
    Ok(())
}

#[derive(Debug, Clone, Copy, Serialize, Deserialize, PartialEq, Eq)]
#[serde(rename_all = "snake_case")]
pub enum Status {
    Passed,
    Failed,
    Unknown,
    NotApplicable,
}
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Check {
    pub key: String,
    pub title: String,
    pub status: Status,
    pub detail: String,
    pub actual: Option<f64>,
    pub target: Option<f64>,
    pub unit: String,
}
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Metric {
    pub key: String,
    pub label: String,
    pub value: Option<f64>,
    pub unit: String,
}
#[derive(Debug, Clone, Serialize, Deserialize, Default)]
pub struct Evaluation {
    pub checks: Vec<Check>,
    pub metrics: Vec<Metric>,
    pub fov_x: Option<f64>,
    pub fov_y: Option<f64>,
    pub passed: usize,
    pub failed: usize,
    pub unknown: usize,
    pub model_note: String,
}
