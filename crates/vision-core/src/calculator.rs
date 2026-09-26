use crate::{Result, engine::frame_bytes};
use serde::{Deserialize, Serialize};
use serde_json::{Value, json};

#[derive(Debug, Deserialize)]
#[serde(tag = "kind", rename_all = "snake_case")]
pub enum CalculationInput {
    Resolution(ResolutionInput),
    Optics(OpticsInput),
    Motion(MotionInput),
    Data(DataInput),
}

#[derive(Debug, Deserialize)]
pub struct Sensor {
    pub resolution_x: f64,
    pub resolution_y: f64,
    pub pixel_size_um: f64,
}

impl Sensor {
    fn dimensions(&self) -> Result<(f64, f64)> {
        pixels(self.resolution_x, self.resolution_y)?;
        positive(self.pixel_size_um, "像元尺寸")?;
        Ok((
            self.resolution_x * self.pixel_size_um / 1000.,
            self.resolution_y * self.pixel_size_um / 1000.,
        ))
    }
}

#[derive(Debug, Deserialize)]
pub struct ResolutionInput {
    #[serde(flatten)]
    pub sensor: Sensor,
    pub object_width: f64,
    pub object_height: f64,
    pub margin: f64,
    pub pixel_limit: f64,
}

#[derive(Clone, Copy, Debug, Deserialize, PartialEq)]
#[serde(rename_all = "snake_case")]
pub enum OpticalMethod {
    Fov,
    Focal,
    Distance,
    TelecentricFov,
    Magnification,
}

#[derive(Debug, Deserialize)]
pub struct OpticsInput {
    #[serde(flatten)]
    pub sensor: Sensor,
    pub method: OpticalMethod,
    pub focal_mm: Option<f64>,
    pub distance_mm: Option<f64>,
    pub fov_width: Option<f64>,
    pub fov_height: Option<f64>,
    pub magnification: Option<f64>,
}

#[derive(Debug, Deserialize)]
pub struct MotionInput {
    pub sampling_um: f64,
    pub speed: f64,
    pub exposure: f64,
    pub blur: f64,
    pub fps: f64,
}

#[derive(Debug, Deserialize)]
pub struct DataInput {
    pub resolution_x: f64,
    pub resolution_y: f64,
    pub pixel_format: String,
    pub fps: f64,
    pub duration_seconds: f64,
    pub bandwidth_mbps: f64,
    pub utilization_percent: f64,
}

#[derive(Debug, Serialize)]
pub struct CalculationMetric {
    pub key: String,
    pub label: String,
    pub value: Option<f64>,
    pub other: Option<f64>,
    pub unit: String,
    pub digits: u8,
}

#[derive(Debug, Serialize)]
pub struct Geometry {
    pub width: f64,
    pub height: f64,
    pub required_width: Option<f64>,
    pub required_height: Option<f64>,
}

#[derive(Debug, Serialize)]
pub struct CalculationResult {
    pub status: String,
    pub summary: String,
    pub metrics: Vec<CalculationMetric>,
    pub formulas: Vec<String>,
    pub notes: Vec<String>,
    pub geometry: Option<Geometry>,
    pub apply: Value,
    pub apply_description: String,
}

impl CalculationResult {
    fn new(status: &str, summary: &str) -> Self {
        Self {
            status: status.into(),
            summary: summary.into(),
            metrics: vec![],
            formulas: vec![],
            notes: vec![],
            geometry: None,
            apply: json!({}),
            apply_description: String::new(),
        }
    }
    fn metric(&mut self, key: &str, label: &str, value: Option<f64>, unit: &str, digits: u8) {
        self.metrics.push(CalculationMetric {
            key: key.into(),
            label: label.into(),
            value,
            other: None,
            unit: unit.into(),
            digits,
        });
    }
    fn pair(&mut self, key: &str, label: &str, x: f64, y: f64, unit: &str, digits: u8) {
        self.metric(key, label, Some(x), unit, digits);
        self.metrics.last_mut().unwrap().other = Some(y);
    }
}

fn range(value: f64, name: &str, min: f64, max: f64) -> Result<()> {
    if !value.is_finite() || !(min..=max).contains(&value) {
        return Err(format!(
            "{name}需在 {min} 到 {max} 之间，且必须为有限数值。"
        ));
    }
    Ok(())
}
fn positive(value: f64, name: &str) -> Result<()> {
    range(value, name, 0.000001, 1e8)
}
fn required(value: Option<f64>, name: &str) -> Result<f64> {
    let value = value.ok_or_else(|| format!("请填写{name}。"))?;
    positive(value, name)?;
    Ok(value)
}
fn pixels(x: f64, y: f64) -> Result<()> {
    for (value, name) in [(x, "水平有效像素"), (y, "垂直有效像素")] {
        range(value, name, 1., 1e6)?;
        if value.fract() != 0. {
            return Err(format!("{name}必须为整数。"));
        }
    }
    Ok(())
}
fn within(actual: f64, limit: f64) -> bool {
    actual <= limit + 1e-9 * actual.abs().max(limit.abs()).max(1.)
}

pub fn calculate(input: CalculationInput) -> Result<CalculationResult> {
    match input {
        CalculationInput::Resolution(p) => resolution(p),
        CalculationInput::Optics(p) => optics(p),
        CalculationInput::Motion(p) => motion(p),
        CalculationInput::Data(p) => data(p),
    }
}

fn resolution(p: ResolutionInput) -> Result<CalculationResult> {
    let (sx, sy) = p.sensor.dimensions()?;
    positive(p.object_width, "工件宽度")?;
    positive(p.object_height, "工件高度")?;
    range(p.margin, "单边余量", 0., 1e8)?;
    positive(p.pixel_limit, "像素当量上限")?;
    let (w, h) = (
        p.object_width + 2. * p.margin,
        p.object_height + 2. * p.margin,
    );
    let sampling = (w / p.sensor.resolution_x).max(h / p.sensor.resolution_y) * 1000.;
    let (fx, fy) = (
        p.sensor.resolution_x * sampling / 1000.,
        p.sensor.resolution_y * sampling / 1000.,
    );
    let (nx, ny) = (
        (w * 1000. / p.pixel_limit).ceil(),
        (h * 1000. / p.pixel_limit).ceil(),
    );
    let passed = within(sampling, p.pixel_limit);
    let mut r = CalculationResult::new(
        if passed { "passed" } else { "failed" },
        if passed {
            "当前分辨率满足两轴采样要求"
        } else {
            "当前分辨率不足，需增加像素或放宽采样要求"
        },
    );
    r.pair("minimum_resolution", "最低有效分辨率", nx, ny, "px", 0);
    r.metric(
        "sampling",
        "按当前比例覆盖时的像素当量",
        Some(sampling),
        "μm/px",
        3,
    );
    r.pair("required_fov", "所需最小视场", w, h, "mm", 3);
    r.pair("coverage_fov", "按当前比例覆盖的视场", fx, fy, "mm", 3);
    r.pair("sensor", "传感器有效尺寸", sx, sy, "mm", 3);
    r.metric("megapixels", "最低像素总量", Some(nx * ny / 1e6), "MP", 3);
    r.geometry = Some(Geometry {
        width: fx,
        height: fy,
        required_width: Some(w),
        required_height: Some(h),
    });
    r.formulas = vec![
        "所需视场 = 工件尺寸 + 2 × 单边余量".into(),
        "最低像素数 = 向上取整（视场 × 1000 ÷ 像素当量上限）".into(),
        "覆盖像素当量 = max（水平视场 ÷ 水平像素，垂直视场 ÷ 垂直像素）× 1000".into(),
    ];
    r.notes = vec![
        "按当前工件方向与方形像元计算，不自动旋转工件；两轴像素必须同时满足要求。".into(),
        "覆盖视场是保持相机纵横比的目标值，不是所选镜头的实测结果。采样密度不能替代测量精度。"
            .into(),
    ];
    r.apply = json!({"width":p.object_width,"height":p.object_height,"margin":p.margin,"pixel":p.pixel_limit});
    r.apply_description = "应用工件宽度、高度、单边余量和像素当量上限".into();
    Ok(r)
}

fn optics(p: OpticsInput) -> Result<CalculationResult> {
    use OpticalMethod::*;
    let (sx, sy) = p.sensor.dimensions()?;
    let telecentric = matches!(p.method, TelecentricFov | Magnification);
    let target = if matches!(p.method, Focal | Distance | Magnification) {
        Some((
            required(p.fov_width, "目标视场宽度")?,
            required(p.fov_height, "目标视场高度")?,
        ))
    } else {
        None
    };
    let maximum_m = target.map(|(w, h)| (sx / w).min(sy / h));
    let (m, distance, focal) = match p.method {
        Fov => {
            let f = required(p.focal_mm, "镜头焦距")?;
            let d = required(p.distance_mm, "工作距离")?;
            (f / d, Some(d), Some(f))
        }
        Focal => {
            let d = required(p.distance_mm, "工作距离")?;
            let m = maximum_m.unwrap();
            (m, Some(d), Some(d * m))
        }
        Distance => {
            let f = required(p.focal_mm, "镜头焦距")?;
            let m = maximum_m.unwrap();
            (m, Some(f / m), Some(f))
        }
        TelecentricFov => (required(p.magnification, "远心倍率")?, None, None),
        Magnification => (maximum_m.unwrap(), None, None),
    };
    if distance.zip(focal).is_some_and(|(d, f)| d <= f) {
        return Err("工作距离必须大于焦距；此条件下不能使用近轴估算。".into());
    }
    if distance.is_some_and(|d| d > 1e8) {
        return Err("反算工作距离超出可用范围，请检查单位与输入。".into());
    }
    let (fx, fy) = (sx / m, sy / m);
    let approximate = !telecentric && m > 0.1;
    let mut r = CalculationResult::new(
        if approximate { "unknown" } else { "neutral" },
        if approximate {
            "倍率大于 0.1，近轴结果仅供粗选"
        } else if telecentric {
            "按固定倍率计算，工作距离仍需查镜头规格"
        } else {
            "近轴估算完成，安装前请复核实际视场"
        },
    );
    match p.method {
        Focal => r.metric("focal", "覆盖两轴的焦距上限（估算）", focal, "mm", 3),
        Distance => r.metric(
            "distance",
            "覆盖两轴的最小工作距离（估算）",
            distance,
            "mm",
            3,
        ),
        Magnification => r.metric("magnification", "覆盖两轴的倍率上限", Some(m), "×", 4),
        _ => r.pair("fov", "计算视场", fx, fy, "mm", 3),
    }
    r.metric(
        "sampling",
        "计算像素当量",
        Some(fx * 1000. / p.sensor.resolution_x),
        "μm/px",
        3,
    );
    if matches!(p.method, Focal | Distance | Magnification) {
        r.pair("fov", "对应视场", fx, fy, "mm", 3);
    }
    r.pair("sensor", "传感器有效尺寸", sx, sy, "mm", 3);
    r.metric("image_circle", "最小像圈直径", Some(sx.hypot(sy)), "mm", 3);
    if p.method != Magnification {
        r.metric("magnification", "成像倍率", Some(m), "×", 4);
    }
    r.geometry = Some(Geometry {
        width: fx,
        height: fy,
        required_width: target.map(|t| t.0),
        required_height: target.map(|t| t.1),
    });
    r.formulas = if telecentric {
        vec![
            "视场 = 传感器有效尺寸 ÷ 倍率".into(),
            "倍率上限 = min（传感器宽 ÷ 目标视场宽，传感器高 ÷ 目标视场高）".into(),
        ]
    } else {
        vec![
            "视场 ≈ 传感器有效尺寸 × 工作距离 ÷ 焦距".into(),
            "焦距上限 ≈ 工作距离 × min（传感器宽 ÷ 视场宽，传感器高 ÷ 视场高）".into(),
            "最小工作距离 ≈ 焦距 × max（视场宽 ÷ 传感器宽，视场高 ÷ 传感器高）".into(),
        ]
    };
    r.notes = if telecentric {
        vec!["倍率由镜头定义；改变安装距离不能用来任意改变远心镜头视场。需确认标称工作距离、像圈与景深。".into()]
    } else {
        vec!["定焦公式与工作台一致，属于一阶近轴估算，未计入镜头主平面和畸变；高倍率或短工作距离请查厂家数据。".into()]
    };
    r.notes
        .push("此处计算不会改写硬件快照，也不会把估算视场标记为实测视场。".into());
    if let Some(d) = distance {
        r.apply = json!({"distance":d});
        r.apply_description = "仅应用工作距离；焦距或型号需在资料库中选择".into();
    }
    Ok(r)
}

fn motion(p: MotionInput) -> Result<CalculationResult> {
    positive(p.sampling_um, "物方像素当量")?;
    range(p.speed, "运动速度", 0., 1e8)?;
    positive(p.exposure, "曝光时间")?;
    positive(p.blur, "允许拖影")?;
    positive(p.fps, "目标帧率")?;
    let motion_limit = if p.speed > 0. {
        Some(p.blur * p.sampling_um * 1000. / p.speed)
    } else {
        None
    };
    let period = 1e6 / p.fps;
    let limit = motion_limit.map_or(period, |v| v.min(period));
    let smear = p.speed * p.exposure / (1000. * p.sampling_um);
    let passed = within(p.exposure, limit);
    let mut r = CalculationResult::new(
        if passed { "passed" } else { "failed" },
        if passed {
            "当前曝光满足拖影与帧周期要求"
        } else {
            "当前曝光超过拖影或帧周期允许值"
        },
    );
    r.metric("exposure_limit", "综合曝光上限", Some(limit), "μs", 3);
    r.metric("motion_blur", "当前曝光下的拖影", Some(smear), "px", 3);
    r.metric("motion_limit", "拖影约束曝光上限", motion_limit, "μs", 3);
    r.metric("frame_period", "目标帧周期", Some(period), "μs", 3);
    r.metric(
        "speed_limit",
        "当前曝光允许的最高速度",
        Some(p.blur * p.sampling_um * 1000. / p.exposure),
        "mm/s",
        3,
    );
    r.metric(
        "travel",
        "曝光期间移动距离",
        Some(p.speed * p.exposure / 1000.),
        "μm",
        3,
    );
    r.formulas = vec![
        "拖影 px = 速度 mm/s × 曝光 μs ÷（1000 × 像素当量 μm/px）".into(),
        "综合曝光上限 = min（拖影限制，1,000,000 ÷ 帧率）".into(),
    ];
    r.notes = vec![
        "按匀速、全局曝光估算；像素当量需使用实际成像值。两轴不同且方向未知时取较小值。".into(),
        "未计入读出、触发延迟和照明亮度；最终采集帧率需结合相机时序验证。".into(),
    ];
    if p.speed == 0. {
        r.notes.insert(
            0,
            "当前速度为 0，拖影约束无上限（显示为 —），仍需满足帧周期。".into(),
        );
    }
    r.apply = json!({"speed":p.speed,"exposure":p.exposure,"blur":p.blur,"fps":p.fps});
    r.apply_description = "应用运动速度、曝光时间、允许拖影和目标帧率".into();
    Ok(r)
}

fn data(p: DataInput) -> Result<CalculationResult> {
    pixels(p.resolution_x, p.resolution_y)?;
    positive(p.fps, "目标帧率")?;
    positive(p.duration_seconds, "记录时长")?;
    positive(p.bandwidth_mbps, "链路额定带宽")?;
    range(p.utilization_percent, "可用带宽比例", 1., 100.)?;
    let bytes = frame_bytes(p.resolution_x, p.resolution_y, &p.pixel_format)
        .ok_or("不支持的像素格式，请选择明确的传输格式。")?;
    let capacity = p.bandwidth_mbps * p.utilization_percent / 100.;
    let throughput = bytes * p.fps / 1e6;
    let frames = (p.fps * p.duration_seconds).ceil();
    let passed = within(throughput, capacity);
    let mut r = CalculationResult::new(
        if passed { "passed" } else { "failed" },
        if passed {
            "图像载荷在设定可用带宽内"
        } else {
            "图像载荷超过设定可用带宽"
        },
    );
    r.metric("payload", "图像传输载荷", Some(throughput), "MB/s", 3);
    r.metric(
        "storage",
        "原始图像存储量",
        Some(frames * bytes / 1e9),
        "GB",
        4,
    );
    r.metric("frame_size", "单帧图像大小", Some(bytes / 1e6), "MB", 6);
    r.metric("frame_count", "完整图像帧数", Some(frames), "帧", 0);
    r.metric(
        "usable_bandwidth",
        "设定可用带宽",
        Some(capacity),
        "MB/s",
        3,
    );
    r.metric(
        "max_fps",
        "带宽允许的理论帧率",
        Some(capacity * 1e6 / bytes),
        "fps",
        3,
    );
    r.formulas = vec![
        "单帧字节 = 向上取整（每行像素 × 传输位数 ÷ 8）× 行数".into(),
        "载荷 MB/s = 单帧字节 × 帧率 ÷ 1,000,000".into(),
        "存储 GB = 单帧字节 × 向上取整（帧率 × 秒数）÷ 1,000,000,000".into(),
    ];
    r.notes = vec![
        "Mono10 / Mono12 使用 16 位容器；Mono10p / Mono12p 为打包传输，按整行字节对齐估算。".into(),
        "MB、GB 均使用十进制单位；不含压缩、协议开销、额外行填充、Chunk 数据和文件元数据。".into(),
        "理论帧率仅由带宽推算，相机本身的曝光、读出和接口配置还会限制实际帧率。".into(),
    ];
    r.apply = json!({"fps":p.fps,"pixel_format":p.pixel_format});
    r.apply_description = "仅应用目标帧率和像素格式，手动分辨率不覆盖所选相机".into();
    Ok(r)
}
