use serde_json::json;
use vision_core::{catalog, catalog_corrections, engine, model::*, store::Store};

fn original(model: &str) -> Hardware {
    catalog::parse_three_d(
        include_str!("../../../resources/data/three_d_cameras.json"),
        "内置资料",
    )
    .unwrap()
    .into_iter()
    .find(|h| h.model == model)
    .unwrap()
}
fn project() -> Project {
    let mut project = Project {
        mode: Mode::Scanning,
        ..Project::default()
    };
    project.hardware[3] = Some(original("Gocator 2320"));
    project
}
fn check<'a>(e: &'a Evaluation, key: &str) -> &'a Check {
    e.checks.iter().find(|c| c.key == key).unwrap()
}
fn metric(e: &Evaluation, key: &str) -> Option<f64> {
    e.metrics.iter().find(|m| m.key == key).unwrap().value
}
fn near(a: Option<f64>, b: f64) {
    assert!((a.unwrap() - b).abs() < 1e-8, "{a:?} != {b}");
}

#[test]
fn seven_lmi_models_match_near_far_and_do_not_invent_a_reference_section() {
    // 从原厂规格表独立抄录的边界，远端距离为 CD + MR。
    for (model, cd, far_distance, near_fov, far_fov) in [
        ("Gocator 2320", 40., 65., 18., 26.),
        ("Gocator 2330", 90., 170., 47., 85.),
        ("Gocator 2340", 190., 400., 96., 194.),
        ("Gocator 2350", 300., 700., 158., 365.),
        ("Gocator 2370", 400., 900., 308., 687.),
        ("Gocator 2375", 650., 2000., 324., 1010.),
        ("Gocator 2380", 350., 1150., 390., 1260.),
    ] {
        let raw = original(model);
        let fixed = catalog_corrections::corrected(&raw);
        near(fixed.number("workingDistanceMinMm"), cd);
        near(fixed.number("workingDistanceMaxMm"), far_distance);
        near(fixed.number("wavelengthNm"), 660.);
        assert_eq!(fixed.text("lightSource"), "红色激光（标准配置）");
        assert_eq!(raw.number("wavelengthNm"), Some(405.));
        assert_eq!(fixed.number("referenceDistanceMm"), None);
        assert_eq!(fixed.number("xFovReferenceMm"), None);
        assert_eq!(fixed.specs["rawSpecs"], raw.specs["rawSpecs"]);
        assert_eq!(
            fixed.specs["geometryCorrection"]["original"]["xFovReferenceMm"],
            raw.specs["xFovReferenceMm"]
        );
        assert_eq!(*fixed, *catalog_corrections::corrected(&fixed));
        let mut p = project();
        p.hardware[3] = Some(raw.clone());
        p.parameters.scan_width = near_fov + 1.;
        p.parameters.scan_distance = Some(cd);
        let e = engine::evaluate(&p).unwrap();
        near(check(&e, "scan_fov").actual, near_fov);
        assert_eq!(check(&e, "scan_fov").status, Status::Failed);
        assert_eq!(check(&e, "scan_wd").status, Status::Passed);
        p.parameters.scan_distance = Some(far_distance);
        near(
            check(&engine::evaluate(&p).unwrap(), "scan_fov").actual,
            far_fov,
        );
        p.parameters.scan_distance = Some((cd + far_distance) / 2.);
        assert_eq!(
            check(&engine::evaluate(&p).unwrap(), "scan_fov").status,
            Status::Unknown
        );
        p.parameters.scan_distance = Some(far_distance + 1.);
        assert_eq!(
            check(&engine::evaluate(&p).unwrap(), "scan_wd").status,
            Status::Failed
        );
        assert_eq!(p.hardware[3], Some(raw), "校核不得修改输入快照");
    }
}

#[test]
fn correction_preserves_custom_records_and_only_changes_verified_builtin_models() {
    let raw = original("Gocator 2320");
    let mut imported = raw.clone();
    imported.origin = "用户导入".into();
    let before = imported.clone();
    assert!(!catalog_corrections::apply(&mut imported));
    assert_eq!(imported, before);
    for (key, value) in [
        ("xFovReferenceMm", json!(19)),
        ("sourceUrl", json!("https://example.com/现场标定")),
        ("workingDistanceMinMm", json!(40)),
    ] {
        let mut custom = raw.clone();
        custom.specs.insert(key.into(), value);
        let before = custom.clone();
        assert!(!catalog_corrections::apply(&mut custom));
        assert_eq!(custom, before);
    }
    let mut custom_laser = raw.clone();
    custom_laser.specs.insert(
        "laserVariantSource".into(),
        json!("https://example.com/特定订货配置"),
    );
    assert!(catalog_corrections::apply(&mut custom_laser));
    assert_eq!(custom_laser.number("wavelengthNm"), Some(405.));
    // 升级前数据库可能已经保存几何校正，后续仍须补上激光修正。
    custom_laser.specs.remove("laserVariantSource");
    assert!(catalog_corrections::apply(&mut custom_laser));
    assert_eq!(custom_laser.number("wavelengthNm"), Some(660.));
    let raw_items = catalog::parse_three_d(
        include_str!("../../../resources/data/three_d_cameras.json"),
        "内置资料",
    )
    .unwrap();
    let fixed = catalog::built_in().unwrap();
    let changed = raw_items
        .iter()
        .filter(|raw| fixed.iter().find(|h| h.id == raw.id).unwrap() != *raw)
        .count();
    assert_eq!(changed, 8);
    let unknown = fixed.iter().find(|h| h.model == "Gocator 2390").unwrap();
    assert_eq!(unknown.number("wavelengthNm"), None);
    assert!(unknown.text("lightSource").is_empty());
    assert_eq!(original("Gocator 2390").number("wavelengthNm"), Some(405.));
}

#[test]
fn existing_database_and_saved_reference_use_corrections_without_rewriting_snapshots() {
    let dir = tempfile::tempdir().unwrap();
    let mut p = project();
    p.parameters.scan_distance = Some(40.);
    p.parameters.scan_width = 20.;
    p.reference = Some(Reference {
        name: "旧参考".into(),
        mode: p.mode,
        parameters: p.parameters.clone(),
        hardware: p.hardware.clone(),
    });
    let original_document = serde_json::to_string(p.hardware[3].as_ref().unwrap()).unwrap();
    {
        let mut store = Store::open(dir.path()).unwrap();
        store.save(&p).unwrap();
        // 模拟升级前已经初始化过的产品库，保持 seeded 标志。
        let connection =
            rusqlite::Connection::open(dir.path().join("visionselect.sqlite")).unwrap();
        connection
            .execute(
                "UPDATE hardware SET document=?1 WHERE id=?2",
                rusqlite::params![original_document, p.hardware[3].as_ref().unwrap().id],
            )
            .unwrap();
    }
    let mut store = Store::open(dir.path()).unwrap();
    let page = store
        .dispatch("catalog", json!({"kind":"three_d","search":"Gocator 2320"}))
        .unwrap();
    assert_eq!(page["items"][0]["specs"]["workingDistanceMinMm"], 40.);
    assert!(page["items"][0]["specs"].get("xFovReferenceMm").is_none());
    assert_eq!(page["total"], 1);
    let loaded: Project =
        serde_json::from_value(store.dispatch("load_project", json!({"id":p.id})).unwrap())
            .unwrap();
    assert_eq!(loaded, p);
    let e: Evaluation = serde_json::from_value(
        store
            .dispatch("evaluate", json!({"project":loaded}))
            .unwrap(),
    )
    .unwrap();
    near(check(&e, "scan_fov").actual, 18.);
    let reference = loaded.reference.as_ref().unwrap();
    let reference_project = Project {
        hardware: reference.hardware.clone(),
        parameters: reference.parameters.clone(),
        reference: None,
        ..loaded.clone()
    };
    near(
        check(&engine::evaluate(&reference_project).unwrap(), "scan_fov").actual,
        18.,
    );
    let imported = store
        .dispatch(
            "import_project",
            json!({"content":serde_json::to_string(&p).unwrap()}),
        )
        .unwrap();
    let imported_project: Project = serde_json::from_value(imported["project"].clone()).unwrap();
    assert_ne!(imported_project.id, p.id);
    near(
        check(&engine::evaluate(&imported_project).unwrap(), "scan_fov").actual,
        18.,
    );
    let connection = rusqlite::Connection::open(dir.path().join("visionselect.sqlite")).unwrap();
    let stored: String = connection
        .query_row(
            "SELECT document FROM hardware WHERE id=?1",
            [&p.hardware[3].as_ref().unwrap().id],
            |row| row.get(0),
        )
        .unwrap();
    assert_eq!(stored, original_document);
    let report = engine::report(&p).unwrap();
    assert!(report.contains("已按原厂规格表校正"));
    assert!(report.contains("当前 18.000 / 要求 20.000"));
    assert!(report.contains("原始方案参数与设备快照"));
}

#[test]
fn timed_counts_use_effective_frequency_and_half_open_interval() {
    let mut p = project();
    let e = engine::evaluate(&p).unwrap();
    near(metric(&e, "profiles"), 7500.);
    near(metric(&e, "required_profiles"), 6000.);
    near(metric(&e, "actual_interval"), 0.04);
    p.parameters.trigger = "external".into();
    p.parameters.rate = 1600.;
    near(metric(&engine::evaluate(&p).unwrap(), "profiles"), 12000.);
    p.parameters.rate = 1000.;
    p.parameters.scan_length = 0.12;
    near(metric(&engine::evaluate(&p).unwrap(), "profiles"), 3.);
    p.parameters.scan_length = 0.120001;
    near(metric(&engine::evaluate(&p).unwrap(), "profiles"), 4.);
    p.parameters.scan_length = 0.01;
    near(metric(&engine::evaluate(&p).unwrap(), "profiles"), 1.);
}

fn encoded() -> Project {
    let mut p = project();
    p.parameters.trigger = "encoder".into();
    p.parameters.travel = 100.;
    p.parameters.pulses = 4000;
    p.parameters.per_profile = 1;
    p.parameters.pulse_rate = 1600.;
    p
}

#[test]
fn encoder_counts_follow_spatial_ticks_and_buffer_uses_acquired_count() {
    let mut p = encoded();
    let e = engine::evaluate(&p).unwrap();
    near(metric(&e, "profiles"), 12000.);
    near(metric(&e, "required_profiles"), 6000.);
    near(metric(&e, "actual_interval"), 0.025);
    p.parameters.rate = 1.; // 隐藏的定时频率不参与编码器计数。
    p.parameters.scan_length = 3000.;
    let e = engine::evaluate(&p).unwrap();
    near(metric(&e, "profiles"), 120000.);
    near(check(&e, "buffer").actual, 120000.);
    p.parameters.scan_length = 2500.;
    assert!(
        !engine::evaluate(&p)
            .unwrap()
            .checks
            .iter()
            .any(|c| c.key == "buffer")
    );
    p.parameters.scan_length = 2500.0001;
    assert!(
        engine::evaluate(&p)
            .unwrap()
            .checks
            .iter()
            .any(|c| c.key == "buffer")
    );
    p.parameters.scan_speed = 20.;
    assert_eq!(
        check(&engine::evaluate(&p).unwrap(), "encoder_speed").status,
        Status::Failed
    );
    near(metric(&engine::evaluate(&p).unwrap(), "profiles"), 100001.);
}

#[test]
fn sampling_margin_is_separate_from_insufficient_sampling() {
    let mut p = encoded();
    p.parameters.pulses = 2000;
    p.parameters.pulse_rate = 800.;
    let e = engine::evaluate(&p).unwrap();
    assert_eq!(check(&e, "scan_speed").status, Status::Passed);
    assert_eq!(check(&e, "encoder_spacing").status, Status::Passed);
    assert_eq!(check(&e, "encoder_speed").status, Status::Passed);
    assert_eq!(check(&e, "sampling_margin").status, Status::Unknown);
    p.parameters.safety = 1.;
    assert_eq!(
        check(&engine::evaluate(&p).unwrap(), "sampling_margin").status,
        Status::Passed
    );
    p.parameters.per_profile = 2;
    let e = engine::evaluate(&p).unwrap();
    assert_eq!(check(&e, "encoder_spacing").status, Status::Failed);
    assert_eq!(check(&e, "scan_speed").status, Status::Failed);
}

#[test]
fn unsafe_count_and_snapshot_devices_do_not_show_plausible_acquisition_counts() {
    let mut p = project();
    p.parameters.scan_speed = 1e-12;
    let e = engine::evaluate(&p).unwrap();
    assert_eq!(metric(&e, "profiles"), None);
    assert_eq!(check(&e, "buffer").status, Status::Unknown);
    p.hardware[3]
        .as_mut()
        .unwrap()
        .specs
        .insert("technology".into(), json!("结构光快照"));
    let e = engine::evaluate(&p).unwrap();
    assert!(e.metrics.iter().all(|m| m.value.is_none()));
    assert_eq!(e.metrics.len(), 6);
}

#[test]
fn reports_show_actual_and_required_counts_with_assumptions() {
    let report = engine::report(&encoded()).unwrap();
    for text in [
        "预计采集轮廓",
        "最低所需轮廓",
        "12000.000000",
        "6000.000000",
        "实际 Y 间距",
        "无丢触发",
    ] {
        assert!(report.contains(text), "缺少 {text}");
    }
}
