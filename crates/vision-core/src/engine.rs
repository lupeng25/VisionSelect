use crate::{Result, model::*};

fn number(item: Option<&Hardware>, key: &str) -> Option<f64> {
    item.and_then(|h| h.number(key))
}
fn text(item: Option<&Hardware>, key: &str) -> String {
    item.map(|h| h.text(key)).unwrap_or_default()
}
// 与 Check 字段逐项对应，调用处保留单位和判断依据，便于工程复核。
#[allow(clippy::too_many_arguments)]
fn push(
    e: &mut Evaluation,
    key: &str,
    title: &str,
    status: Status,
    actual: Option<f64>,
    target: Option<f64>,
    unit: &str,
    detail: &str,
) {
    e.checks.push(Check {
        key: key.into(),
        title: title.into(),
        status,
        actual,
        target,
        unit: unit.into(),
        detail: detail.into(),
    });
}
#[allow(clippy::too_many_arguments)]
fn bound(
    e: &mut Evaluation,
    key: &str,
    title: &str,
    actual: Option<f64>,
    target: Option<f64>,
    upper: bool,
    unit: &str,
    detail: &str,
) {
    let status = match (actual, target) {
        (Some(a), Some(t)) if a.is_finite() && t.is_finite() => {
            let tolerance = 1e-9 * a.abs().max(t.abs()).max(1.);
            if (upper && a <= t + tolerance) || (!upper && a + tolerance >= t) {
                Status::Passed
            } else {
                Status::Failed
            }
        }
        _ => Status::Unknown,
    };
    push(e, key, title, status, actual, target, unit, detail);
}
fn metric(e: &mut Evaluation, key: &str, label: &str, value: Option<f64>, unit: &str) {
    e.metrics.push(Metric {
        key: key.into(),
        label: label.into(),
        value: value.filter(|n| n.is_finite()),
        unit: unit.into(),
    });
}
fn mount(value: &str) -> String {
    value
        .trim()
        .to_uppercase()
        .replace("-MOUNT", "")
        .replace(" MOUNT", "")
        .replace('口', "")
}
pub fn frame_bytes(width: f64, height: f64, format: &str) -> Option<f64> {
    if width <= 0. || height <= 0. || width.fract() != 0. || height.fract() != 0. {
        return None;
    }
    let bits = match format.trim().to_ascii_lowercase().as_str() {
        "mono8" | "bayerrg8" | "bayerbg8" | "bayergb8" | "bayergr8" => 8.,
        "mono10p" | "bayerrg10p" | "bayerbg10p" => 10.,
        "mono12p" | "bayerrg12p" | "bayerbg12p" => 12.,
        "mono10" | "mono12" | "mono16" | "bayerrg10" | "bayerrg12" => 16.,
        "rgb8" | "bgr8" | "rgb8packed" | "bgr8packed" => 24.,
        _ => return None,
    };
    Some((width * bits / 8.).ceil() * height)
}
pub fn evaluate(project: &Project) -> Result<Evaluation> {
    project.validate()?;
    let mut e = Evaluation::default();
    match project.mode {
        Mode::Imaging => imaging(project, &mut e),
        Mode::Scanning => scanning(project, &mut e),
    }
    for check in &e.checks {
        match check.status {
            Status::Passed => e.passed += 1,
            Status::Failed => e.failed += 1,
            Status::Unknown => e.unknown += 1,
            Status::NotApplicable => {}
        }
    }
    Ok(e)
}
fn imaging(project: &Project, e: &mut Evaluation) {
    let p = &project.parameters;
    // 旧方案快照仍保留原文；计算只使用核对后的内置设备字段。
    let camera = project.hardware[0]
        .as_ref()
        .map(crate::catalog_corrections::corrected);
    let lens = project.hardware[1]
        .as_ref()
        .map(crate::catalog_corrections::corrected);
    let c = camera.as_deref();
    let l = lens.as_deref();
    let light = project.hardware[2].as_ref();
    for kind in [Kind::Camera, Kind::Lens, Kind::Light] {
        if project.hardware[kind.index()].is_none() {
            push(
                e,
                &format!("missing_{}", kind.key()),
                kind.title(),
                Status::Unknown,
                None,
                None,
                "",
                "从硬件资料库选择设备后参与校核。",
            );
        }
    }
    let rx = number(c, "resolution_x");
    let ry = number(c, "resolution_y");
    let pitch = number(c, "pixel_size_um");
    let sensor_x = rx.zip(pitch).map(|(r, s)| r * s / 1000.);
    let sensor_y = ry.zip(pitch).map(|(r, s)| r * s / 1000.);
    let tele = l.is_some_and(Hardware::telecentric);
    let lens_type = text(l, "lens_type");
    let line_scan_lens = lens_type.eq_ignore_ascii_case("LineScan");
    if line_scan_lens {
        push(
            e,
            "lens_application",
            "镜头适用相机类型",
            Status::Unknown,
            None,
            None,
            "",
            "线扫镜头的面阵适用性需核对厂家规格、像圈和安装条件；仅凭类别不能判定不兼容，实测视场也不代表完整成像质量已确认。",
        );
    }
    let magnification = if line_scan_lens {
        None
    } else if tele {
        number(l, "pmag")
    } else {
        number(l, "focal_length_mm").map(|f| f / p.distance)
    };
    if p.measured {
        e.fov_x = Some(p.measured_width);
        e.fov_y = Some(p.measured_height);
        e.model_note = "使用当前安装条件下的实测视场；不外推到其他距离和成像区域。".into();
    } else {
        e.fov_x = sensor_x.zip(magnification).map(|(s, m)| s / m);
        e.fov_y = sensor_y.zip(magnification).map(|(s, m)| s / m);
        if !tele && number(l, "focal_length_mm").is_some_and(|f| p.distance <= f) {
            e.fov_x = None;
            e.fov_y = None;
        }
        e.model_note = if tele {
            "远心模型：视场 = 传感器尺寸 ÷ 目录倍率。"
        } else {
            "近轴估算：视场 ≈ 传感器尺寸 × 工作距离 ÷ 焦距；建议以实测视场复核。"
        }
        .into();
    }
    if line_scan_lens && !p.measured {
        e.model_note = "线扫镜头的面阵适用性待确认；不直接套用近轴模型，请核对厂家规格并填写当前安装条件下的实测视场。".into();
    } else if lens_type.eq_ignore_ascii_case("FixedMagnification") && !p.measured {
        e.model_note = "定倍率镜头仅有目录标称倍率，当前安装距离下的视场需要实测确认。".into();
    }
    bound(
        e,
        "fov_x",
        "水平视场",
        e.fov_x,
        Some(p.width + 2. * p.margin),
        false,
        "mm",
        "实际视场需覆盖工件和两侧装夹余量。",
    );
    bound(
        e,
        "fov_y",
        "垂直视场",
        e.fov_y,
        Some(p.height + 2. * p.margin),
        false,
        "mm",
        "按当前工件方向判断，不自动交换宽高。",
    );
    let px = e.fov_x.zip(rx).map(|(f, r)| f * 1000. / r);
    let py = e.fov_y.zip(ry).map(|(f, r)| f * 1000. / r);
    bound(
        e,
        "sampling_x",
        "水平采样",
        px,
        Some(p.pixel),
        true,
        "μm/px",
        "物方像素越小，几何采样越细。",
    );
    bound(
        e,
        "sampling_y",
        "垂直采样",
        py,
        Some(p.pixel),
        true,
        "μm/px",
        "采样通过不代表系统测量精度已达标。",
    );
    let camera_mount = mount(&text(c, "lens_mount"));
    let lens_mount = mount(&text(l, "lens_mount"));
    push(
        e,
        "mount",
        "镜头接口",
        if camera_mount.is_empty() || lens_mount.is_empty() {
            Status::Unknown
        } else if camera_mount == lens_mount {
            Status::Passed
        } else {
            Status::Failed
        },
        None,
        None,
        "",
        &format!(
            "相机 {} / 镜头 {}。C 与 CS 的法兰距不同，不能仅凭外形视为兼容。",
            if camera_mount.is_empty() {
                "未公开"
            } else {
                &camera_mount
            },
            if lens_mount.is_empty() {
                "未公开"
            } else {
                &lens_mount
            }
        ),
    );
    let diagonal = sensor_x.zip(sensor_y).map(|(x, y)| x.hypot(y));
    let circle = match (
        number(l, "image_circle_mm"),
        number(l, "max_sensor_diagonal_mm"),
    ) {
        (Some(a), Some(b)) => Some(a.min(b)),
        (Some(a), None) => Some(a),
        (None, b) => b,
    };
    bound(
        e,
        "image_circle",
        "像圈覆盖",
        circle,
        diagonal,
        false,
        "mm",
        "镜头像圈不得小于传感器对角线。",
    );
    if tele {
        let delta = number(l, "nominal_wd_mm").map(|wd| (p.distance - wd).abs());
        bound(
            e,
            "distance",
            "远心安装距离偏差",
            delta,
            number(l, "wd_tolerance_mm"),
            true,
            "mm",
            "比较当前距离与标称距离的差值；未公开容差时必须确认。",
        );
    } else if number(l, "focal_length_mm").is_some_and(|f| p.distance <= f) {
        push(
            e,
            "distance",
            "安装距离",
            Status::Failed,
            Some(p.distance),
            number(l, "focal_length_mm"),
            "mm",
            "当前距离不大于焦距，近轴估算不适用。",
        );
    } else {
        bound(
            e,
            "distance",
            "最小工作距离",
            Some(p.distance),
            number(l, "min_wd_mm"),
            false,
            "mm",
            "工作距离须与厂家给出的机械基准一致；光学估算需实测复核。",
        );
    }
    let confirmed =
        p.dof_confirmed || l.and_then(|h| h.flag("dof_conditions_confirmed")) == Some(true);
    if p.variation == 0. {
        push(
            e,
            "dof",
            "景深",
            Status::NotApplicable,
            None,
            Some(0.),
            "mm",
            "未设置工件高度变化。",
        );
    } else if confirmed {
        bound(
            e,
            "dof",
            "景深覆盖",
            number(l, "dof_mm"),
            Some(p.variation * 1.5),
            false,
            "mm",
            "景深要求为高度峰峰值 × 1.5；确认当前倍率、光圈和评价标准。",
        );
    } else {
        push(
            e,
            "dof",
            "景深条件",
            Status::Unknown,
            number(l, "dof_mm"),
            Some(p.variation * 1.5),
            "mm",
            "目录景深不能直接用于任意倍率和光圈。确认适用条件后再参与判断。",
        );
    }
    bound(
        e,
        "fps",
        "相机帧率",
        number(c, "max_fps"),
        Some(p.fps),
        false,
        "fps",
        "仅校核标称上限；曝光、ROI 与读出还可能降低实际帧率。",
    );
    bound(
        e,
        "frame_exposure",
        "曝光与帧周期",
        Some(p.exposure),
        Some(1e6 / p.fps),
        true,
        "μs",
        "曝光不能超过目标帧周期；此项尚未计入相机读出时间。",
    );
    if p.speed > 0. {
        let blur = px
            .zip(py)
            .map(|(x, y)| p.speed * p.exposure / (1000. * x.min(y)));
        bound(
            e,
            "motion",
            "运动拖影",
            blur,
            Some(p.blur),
            true,
            "px",
            "按更细的轴向像素当量估算匀速拖影。",
        );
        let shutter = text(c, "shutter_type").to_lowercase();
        push(
            e,
            "shutter",
            "运动快门",
            if shutter.is_empty() {
                Status::Unknown
            } else if shutter.contains("global") || shutter.contains("全局") {
                Status::Passed
            } else {
                Status::Failed
            },
            None,
            None,
            "",
            "连续运动需确认全局快门及同步触发。",
        );
    } else {
        push(
            e,
            "motion",
            "运动拖影",
            Status::NotApplicable,
            None,
            None,
            "px",
            "当前工件速度为 0。",
        );
    }
    let format = if p.pixel_format.is_empty() {
        text(c, "pixel_format")
    } else {
        p.pixel_format.clone()
    };
    let bytes = rx.zip(ry).and_then(|(x, y)| frame_bytes(x, y, &format));
    let bandwidth = bytes.map(|b| b * p.fps / 1e6);
    let capacity = if text(c, "bandwidth_source") == "specified" {
        number(c, "bandwidth_mbps").map(|n| n * 0.8)
    } else {
        None
    };
    bound(
        e,
        "bandwidth",
        "传输带宽",
        bandwidth,
        capacity,
        true,
        "MB/s",
        "仅对明确额定带宽取 80% 可用容量；历史估算值和未公开像素格式保持待确认。",
    );
    if bytes.is_none() {
        push(
            e,
            "pixel_format",
            "传输像素格式",
            Status::Unknown,
            None,
            None,
            "",
            "目录未提供可计算的传输格式。可在采集参数中明确选择实际格式，不从彩色模式或位深猜测。",
        );
    }
    if let Some(light) = light {
        push(
            e,
            "illumination",
            "照明实拍",
            Status::Unknown,
            None,
            None,
            "",
            &format!(
                "{} · {}。发光面尺寸不等于工件上的有效照明面积；需实拍确认反光、均匀性和对比度。",
                light.model,
                light.text("color")
            ),
        );
    }
    metric(e, "fov_x", "水平视场", e.fov_x, "mm");
    metric(e, "fov_y", "垂直视场", e.fov_y, "mm");
    metric(
        e,
        "pixel",
        "物方像素",
        px.zip(py).map(|(x, y)| x.max(y)),
        "μm/px",
    );
    metric(e, "bandwidth", "传输载荷", bandwidth, "MB/s");
    metric(
        e,
        "storage",
        "每小时存储",
        bytes.map(|b| b * p.fps * 3600. / 1e9),
        "GB",
    );
}
fn scanning_metrics(e: &mut Evaluation, values: [Option<f64>; 6]) {
    for ((key, title, unit), value) in [
        ("profiles", "预计采集轮廓", "条"),
        ("required_profiles", "最低所需轮廓", "条"),
        ("actual_interval", "实际 Y 间距", "mm"),
        ("pitch", "目录 X 间隔", "μm"),
        ("speed", "采样速度上限", "mm/s"),
        ("rate", "最低轮廓频率", "Hz"),
    ]
    .into_iter()
    .zip(values)
    {
        metric(e, key, title, value, unit);
    }
}

fn profile_count(length: f64, interval: f64) -> Option<f64> {
    let count = length / interval;
    if !interval.is_finite()
        || interval <= 0.
        || !count.is_finite()
        || count > 9_007_199_254_740_991.
    {
        return None;
    }
    // 修正十进制输入在整数边界附近的舍入误差，保持 [0, 长度) 不重复采终点。
    let rounded = count.round();
    Some(if (count - rounded).abs() <= 1e-9 {
        rounded.max(1.)
    } else {
        count.ceil().max(1.)
    })
}

fn scanning(project: &Project, e: &mut Evaluation) {
    let p = &project.parameters;
    let Some(camera) = project.hardware[3].as_ref() else {
        push(
            e,
            "camera",
            "3D 相机",
            Status::Unknown,
            None,
            None,
            "",
            "选择设备后，按技术路线和公开规格校核。",
        );
        e.model_note = "三维设备必须先确认技术路线，再使用对应的几何和采样模型。".into();
        scanning_metrics(e, [None; 6]);
        return;
    };
    // 包括旧方案及参考快照；仅对本次计算使用修正值，不改写用户保存内容。
    let camera = crate::catalog_corrections::corrected(camera);
    bound(
        e,
        "z_range",
        "Z 量程",
        camera.number("zMeasurementRangeMm"),
        Some(p.z_range),
        false,
        "mm",
        "量程、重复精度与测量精度分别比较，不相互替代。",
    );
    bound(
        e,
        "z_repeat",
        "Z 重复精度",
        camera.number("zRepeatabilityUm"),
        Some(p.z_repeat),
        true,
        "μm",
        "只使用重复精度字段；不以 Z 分辨率或线性度代替。",
    );
    let mut fov = None;
    if let Some(distance) = p.scan_distance {
        for (wd, key) in [
            ("workingDistanceMinMm", "xFovNearMm"),
            ("referenceDistanceMm", "xFovReferenceMm"),
            ("workingDistanceMaxMm", "xFovFarMm"),
        ] {
            if camera
                .number(wd)
                .is_some_and(|d| (d - distance).abs() < 1e-6)
            {
                fov = camera.number(key);
                if fov.is_some() {
                    break;
                }
            }
        }
        match (
            camera.number("workingDistanceMinMm"),
            camera.number("workingDistanceMaxMm"),
        ) {
            (Some(min), Some(max)) => push(
                e,
                "scan_wd",
                "三维安装距离",
                if distance >= min && distance <= max {
                    Status::Passed
                } else {
                    Status::Failed
                },
                Some(distance),
                None,
                "mm",
                &format!("公开安装范围 {min}–{max} mm。"),
            ),
            _ => push(
                e,
                "scan_wd",
                "三维安装距离",
                Status::Unknown,
                Some(distance),
                None,
                "mm",
                "尚未获得完整工作距离范围，请查看厂家资料。",
            ),
        }
    }
    bound(
        e,
        "scan_fov",
        "当前截面 X 覆盖",
        fov,
        Some(p.scan_width),
        false,
        "mm",
        &format!(
            "仅使用同一已公开测量截面，不在近端、参考点和远端之间自行插值；请填写实际工作距离。{}",
            camera.text("geometryCorrectionNote")
        ),
    );
    let technology = camera.text("technology");
    let output = camera.text("outputTypes");
    let snapshot =
        technology.contains("快照") || technology.contains("双目") || technology.contains("结构光");
    let profile = !snapshot
        && (technology.contains("轮廓")
            || technology.contains("线共焦")
            || (camera.number("scanRateMaxHz").is_some() && output.contains("轮廓")));
    if !profile {
        push(
            e,
            "scan_model",
            "运动采样模型",
            Status::Unknown,
            None,
            None,
            "",
            "尚未确认当前设备为轮廓扫描路线。快照、立体和其他设备不套用线扫描公式。",
        );
        scanning_metrics(e, [None; 6]);
        e.model_note = "当前型号未使用轮廓扫描模型，几何规格仍可逐项评估。".into();
        return;
    }
    let rate = if p.trigger == "encoder" {
        p.pulse_rate / f64::from(p.per_profile)
    } else {
        p.rate
    };
    let required = p.scan_speed / p.interval;
    let max_speed = rate * p.interval;
    let period = 1e6 / rate;
    let actual_interval = if p.trigger == "encoder" {
        p.travel / f64::from(p.pulses) * f64::from(p.per_profile)
    } else {
        p.scan_speed / rate
    };
    let count = profile_count(p.scan_length, actual_interval);
    let required_count = profile_count(p.scan_length, p.interval);
    bound(
        e,
        "rate_limit",
        "相机轮廓频率",
        Some(rate),
        camera.number("scanRateMaxHz"),
        true,
        "Hz",
        "使用最大扫描频率，不以快照帧率代替。",
    );
    bound(
        e,
        "scan_speed",
        "运动速度",
        Some(p.scan_speed),
        Some(max_speed),
        true,
        "mm/s",
        "按有效触发频率 × 目标间距判断是否采样不足；同时必须满足相机频率和编码器一致性约束。建议裕量另行提示。",
    );
    let margin_interval = p.interval * p.safety;
    push(
        e,
        "sampling_margin",
        "建议采样裕量",
        if actual_interval <= margin_interval + 1e-9 * margin_interval.abs().max(1.) {
            Status::Passed
        } else {
            Status::Unknown
        },
        Some(actual_interval),
        Some(margin_interval),
        "mm",
        "建议实际 Y 间距不超过目标间距 × 采样裕量系数；未达到建议裕量需确认，不作为硬件冲突。它不是相机频率容量余量。",
    );
    bound(
        e,
        "scan_exposure",
        "曝光周期",
        Some(p.scan_exposure + p.readout),
        Some(period),
        true,
        "μs",
        "当前值包含曝光与读出/复位余量，不能超过有效轮廓周期。",
    );
    let min = camera.number("exposureTimeMinUs");
    let max = camera.number("exposureTimeMaxUs");
    let violated =
        min.is_some_and(|m| p.scan_exposure < m) || max.is_some_and(|m| p.scan_exposure > m);
    push(
        e,
        "exposure_range",
        "型号曝光范围",
        if violated {
            Status::Failed
        } else if min.is_some() && max.is_some() {
            Status::Passed
        } else {
            Status::Unknown
        },
        Some(p.scan_exposure),
        max,
        "μs",
        "按厂家最短和最长曝光校验；公开任一边界被违反即判冲突。",
    );
    if p.trigger != "free" {
        let key = if p.trigger == "encoder" {
            "supportsEncoder"
        } else {
            "supportsExternalTrigger"
        };
        push(
            e,
            "trigger",
            "触发能力",
            match camera.flag(key) {
                Some(true) => Status::Passed,
                Some(false) => Status::Failed,
                None => Status::Unknown,
            },
            None,
            None,
            "",
            "按当前型号的公开触发能力判断。",
        );
    }
    if p.trigger == "encoder" {
        bound(
            e,
            "encoder_rate",
            "编码器输入频率",
            Some(p.pulse_rate),
            camera.number("encoderRateMaxHz"),
            true,
            "Hz",
            "编码器脉冲频率与轮廓频率是不同约束。",
        );
        bound(
            e,
            "encoder_spacing",
            "编码器实际轮廓间距",
            Some(actual_interval),
            Some(p.interval),
            true,
            "mm",
            "标定移动距离 ÷ 脉冲数量 × 每轮廓脉冲数；大于目标间距将造成采样不足。",
        );
        let actual_speed = p.pulse_rate * p.travel / f64::from(p.pulses);
        let diff = (actual_speed - p.scan_speed).abs();
        push(
            e,
            "encoder_speed",
            "编码器与轴速度一致性",
            if diff <= p.scan_speed * 0.01 {
                Status::Passed
            } else {
                Status::Failed
            },
            Some(actual_speed),
            Some(p.scan_speed),
            "mm/s",
            "脉冲频率 × 每脉冲移动距离应与实际轴速度一致，容许 1% 输入差异。",
        );
    }
    let pitch = camera.number("profileDataIntervalUm");
    if pitch.is_none() {
        push(
            e,
            "profile_pitch",
            "X 数据间隔",
            Status::Unknown,
            None,
            None,
            "μm",
            "未公开轮廓数据间隔，不以分辨率、重复精度或点数推定。",
        );
    }
    if count.is_none_or(|n| n > 100000.) {
        push(
            e,
            "buffer",
            "数据容量复核",
            Status::Unknown,
            count,
            None,
            "条",
            if count.is_some() {
                "按当前触发设置预计采集轮廓超过十万，请确认控制器缓存、网络和处理能力。"
            } else {
                "预计采集量超出可可靠计数的范围，请检查单位，并确认控制器缓存、网络和处理能力。"
            },
        );
    }
    scanning_metrics(
        e,
        [
            count,
            required_count,
            Some(actual_interval),
            pitch,
            Some(max_speed),
            Some(required),
        ],
    );
    e.model_note = format!(
        "{}；最低所需轮廓按目标间距计算。预计数量假定匀速、起点触发且无丢触发，区间 [0, 长度) 不重复采终点。{}",
        if p.trigger == "encoder" {
            "编码器：实际 Y 间距由标定距离和每轮廓脉冲数确定，预计数量 = 长度 ÷ 实际间距，向上取整"
        } else {
            "定时触发：实际 Y 间距 = 轴速度 ÷ 有效触发频率，预计数量 = 长度 ÷ 实际间距，向上取整"
        },
        camera.text("geometryCorrectionNote")
    );
}
fn escape(value: &str) -> String {
    value
        .replace('&', "&amp;")
        .replace('<', "&lt;")
        .replace('>', "&gt;")
        .replace('"', "&quot;")
}
pub fn report(project: &Project) -> Result<String> {
    let e = evaluate(project)?;
    let mut html = format!(
        "<!doctype html><html lang='zh-CN'><head><meta charset='utf-8'><title>{}</title><style>body{{font:14px/1.8 'Microsoft YaHei',sans-serif;max-width:960px;margin:40px auto;color:#24333f;padding:0 24px}}h1{{font-size:28px}}h2{{border-bottom:1px solid #ddd;padding-top:22px}}table{{width:100%;border-collapse:collapse}}th,td{{padding:9px;text-align:left;border-bottom:1px solid #e5e7eb;overflow-wrap:anywhere}}.muted{{color:#687786}}@media print{{body{{margin:0;font-size:11px}}tr{{break-inside:avoid}}h2,h3{{break-after:avoid}}}}</style></head><body><p class='muted'>VISIONSELECT / 工程校核记录</p><h1>{}</h1><p>生成于 {} · 通过 {} 项 / 冲突 {} 项 / 待确认 {} 项</p><p>{}</p><h2>设备组合</h2><table>",
        escape(&project.name),
        escape(&project.name),
        chrono::Local::now().format("%Y-%m-%d %H:%M"),
        e.passed,
        e.failed,
        e.unknown,
        escape(&e.model_note)
    );
    for kind in Kind::ALL {
        if (project.mode == Mode::Imaging && kind == Kind::ThreeD)
            || (project.mode == Mode::Scanning && kind != Kind::ThreeD)
        {
            continue;
        }
        let value = project.hardware[kind.index()]
            .as_ref()
            .map(|h| format!("{} / {}", h.manufacturer, h.model))
            .unwrap_or("未选择".into());
        html += &format!(
            "<tr><th>{}</th><td>{}</td></tr>",
            kind.title(),
            escape(&value)
        );
    }
    html += "</table>";
    if project.mode == Mode::Scanning {
        html += "<h2>采样指标</h2><table><tr><th>指标</th><th>数值</th><th>单位</th></tr>";
        for m in &e.metrics {
            let value = m
                .value
                .map(|v| format!("{v:.6}"))
                .unwrap_or("待确认".into());
            html += &format!(
                "<tr><th>{}</th><td>{}</td><td>{}</td></tr>",
                escape(&m.label),
                value,
                escape(&m.unit)
            );
        }
        html += "</table>";
    }
    html += "<h2>逐项校核</h2><table><tr><th>项目</th><th>状态</th><th>依据</th></tr>";
    for c in &e.checks {
        let state = match c.status {
            Status::Passed => "通过",
            Status::Failed => "冲突",
            Status::Unknown => "待确认",
            Status::NotApplicable => "不适用",
        };
        let fmt = |v: Option<f64>| v.map(|n| format!("{n:.3}")).unwrap_or("未公开".into());
        html += &format!(
            "<tr><th>{}</th><td>{}</td><td>当前 {} / 要求 {} {}<br>{}</td></tr>",
            escape(&c.title),
            state,
            fmt(c.actual),
            fmt(c.target),
            escape(&c.unit),
            escape(&c.detail)
        );
    }
    html += "</table><h2>原始方案参数与设备快照</h2><p>以下数据用于复核和复现；字段含义见应用内对应输入标签。</p><pre style='white-space:pre-wrap;overflow-wrap:anywhere;font-size:10px'>";
    html += &escape(&serde_json::to_string_pretty(project).map_err(|e| e.to_string())?);
    html += "</pre><p class='muted'>此记录是参数校核结果。照明效果、测量精度及未公开规格需结合厂家资料和现场验证。</p></body></html>";
    Ok(html)
}
