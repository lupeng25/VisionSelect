use serde_json::{Value, json};
use vision_core::{catalog, engine, model::*, store::Store};

fn hardware(kind: Kind, specs: Value) -> Hardware {
    let mut map = specs.as_object().unwrap().clone();
    map.insert("model".into(), json!("回归设备"));
    map.insert("manufacturer".into(), json!("测试厂家"));
    catalog::from_specs(kind, map, "回归测试").unwrap()
}
fn imaging() -> Project {
    let mut project = Project::default();
    project.parameters.distance = 200.;
    project.hardware[0] = Some(hardware(
        Kind::Camera,
        json!({
            "resolution_x":2000,"resolution_y":1000,"pixel_size_um":5,
            "lens_mount":"C-Mount","max_fps":100,"shutter_type":"Global",
            "bandwidth_mbps":125,"bandwidth_source":"specified"
        }),
    ));
    project.hardware[1] = Some(hardware(
        Kind::Lens,
        json!({
            "lens_type":"fixed","focal_length_mm":25,"image_circle_mm":12,
            "min_wd_mm":100,"lens_mount":"C","dof_mm":6
        }),
    ));
    project
}
fn scanning() -> Project {
    let mut project = Project {
        mode: Mode::Scanning,
        ..Project::default()
    };
    project.hardware[3] = Some(hardware(
        Kind::ThreeD,
        json!({
            "technology":"激光轮廓","scanRateMaxHz":2000,"profileDataIntervalUm":5,
            "zMeasurementRangeMm":20,"zRepeatabilityUm":2,
            "workingDistanceMinMm":100,"referenceDistanceMm":150,"workingDistanceMaxMm":200,
            "xFovNearMm":30,"xFovReferenceMm":50,"xFovFarMm":70,
            "supportsEncoder":true,"supportsExternalTrigger":false,"encoderRateMaxHz":100000,
            "exposureTimeMinUs":10,"exposureTimeMaxUs":500
        }),
    ));
    project
}
fn check<'a>(e: &'a Evaluation, key: &str) -> &'a Check {
    e.checks.iter().find(|c| c.key == key).unwrap()
}
fn metric(e: &Evaluation, key: &str) -> Option<f64> {
    e.metrics.iter().find(|m| m.key == key).unwrap().value
}
fn close(a: Option<f64>, b: f64) {
    assert!((a.unwrap() - b).abs() < 1e-6);
}

#[test]
fn packed_pixel_formats_keep_row_padding_and_unknown_formats() {
    assert_eq!(engine::frame_bytes(3., 2., "Mono10p"), Some(8.));
    assert_eq!(engine::frame_bytes(3., 2., "Mono12p"), Some(10.));
    assert_eq!(engine::frame_bytes(3., 2., "Mono12"), Some(12.));
    assert_eq!(engine::frame_bytes(3., 2., "RGB8"), Some(18.));
    for format in ["", "Mono10Packed", "未知格式"] {
        assert_eq!(engine::frame_bytes(3., 2., format), None);
    }
    assert_eq!(engine::frame_bytes(3.5, 2., "Mono8"), None);
    assert_eq!(engine::frame_bytes(f64::INFINITY, 2., "Mono8"), None);
}
#[test]
fn imaging_geometry_responds_to_distance_and_sampling() {
    let mut p = imaging();
    let e = engine::evaluate(&p).unwrap();
    close(e.fov_x, 80.);
    close(e.fov_y, 40.);
    assert_eq!(check(&e, "fov_x").status, Status::Passed);
    assert_eq!(check(&e, "sampling_x").status, Status::Failed);
    p.parameters.distance = 400.;
    close(engine::evaluate(&p).unwrap().fov_x, 160.);
}
#[test]
fn measured_fov_is_used_for_both_axes_and_motion_blur() {
    let mut p = imaging();
    p.parameters.measured = true;
    p.parameters.measured_width = 24.;
    p.parameters.measured_height = 12.;
    p.parameters.speed = 120.;
    p.parameters.exposure = 100.;
    let e = engine::evaluate(&p).unwrap();
    close(e.fov_x, 24.);
    close(metric(&e, "pixel"), 12.);
    close(check(&e, "motion").actual, 1.);
    assert_eq!(check(&e, "motion").status, Status::Passed);
    assert_eq!(check(&e, "fov_y").status, Status::Failed);
}
#[test]
fn unknown_format_and_historical_bandwidth_never_pass() {
    let mut p = imaging();
    assert_eq!(
        check(&engine::evaluate(&p).unwrap(), "bandwidth").status,
        Status::Unknown
    );
    p.parameters.pixel_format = "Mono8".into();
    let e = engine::evaluate(&p).unwrap();
    close(metric(&e, "bandwidth"), 40.);
    assert_eq!(check(&e, "bandwidth").status, Status::Passed);
    p.hardware[0]
        .as_mut()
        .unwrap()
        .specs
        .insert("bandwidth_source".into(), json!("estimated"));
    assert_eq!(
        check(&engine::evaluate(&p).unwrap(), "bandwidth").status,
        Status::Unknown
    );
    p.hardware[0]
        .as_mut()
        .unwrap()
        .specs
        .insert("bandwidth_source".into(), json!("specified"));
    p.parameters.pixel_format = "RGB8".into();
    assert_eq!(
        check(&engine::evaluate(&p).unwrap(), "bandwidth").status,
        Status::Failed
    );
}
#[test]
fn c_and_cs_mounts_are_distinct() {
    let mut p = imaging();
    assert_eq!(
        check(&engine::evaluate(&p).unwrap(), "mount").status,
        Status::Passed
    );
    p.hardware[1]
        .as_mut()
        .unwrap()
        .specs
        .insert("lens_mount".into(), json!("CS"));
    assert_eq!(
        check(&engine::evaluate(&p).unwrap(), "mount").status,
        Status::Failed
    );
}
#[test]
fn image_circle_accepts_explicit_sensor_diagonal_limit() {
    let mut p = imaging();
    let specs = &mut p.hardware[1].as_mut().unwrap().specs;
    specs.remove("image_circle_mm");
    specs.insert("max_sensor_diagonal_mm".into(), json!(12));
    assert_eq!(
        check(&engine::evaluate(&p).unwrap(), "image_circle").status,
        Status::Passed
    );
}
#[test]
fn invalid_lens_distance_does_not_produce_plausible_fov() {
    let mut p = imaging();
    p.parameters.distance = 25.;
    let e = engine::evaluate(&p).unwrap();
    assert_eq!(e.fov_x, None);
    assert_eq!(check(&e, "distance").status, Status::Failed);
}
#[test]
fn telecentric_distance_needs_tolerance_and_dof_needs_conditions() {
    let mut p = imaging();
    p.hardware[1] = Some(hardware(
        Kind::Lens,
        json!({"lens_type":"telecentric","pmag":0.5,"nominal_wd_mm":200,"dof_mm":6}),
    ));
    let e = engine::evaluate(&p).unwrap();
    close(e.fov_x, 20.);
    assert_eq!(check(&e, "distance").status, Status::Unknown);
    assert_eq!(check(&e, "dof").status, Status::Unknown);
    p.parameters.dof_confirmed = true;
    assert_eq!(
        check(&engine::evaluate(&p).unwrap(), "dof").status,
        Status::Passed
    );
    p.parameters.variation = 10.;
    assert_eq!(
        check(&engine::evaluate(&p).unwrap(), "dof").status,
        Status::Failed
    );
}
#[test]
fn empty_hardware_is_unknown_not_all_passed() {
    let e = engine::evaluate(&Project::default()).unwrap();
    assert!(e.unknown > 0);
    assert_eq!(e.fov_x, None);
    assert_eq!(check(&e, "fov_x").status, Status::Unknown);
}
#[test]
fn scanning_fov_only_uses_known_section() {
    let mut p = scanning();
    assert_eq!(
        check(&engine::evaluate(&p).unwrap(), "scan_fov").status,
        Status::Unknown
    );
    p.parameters.scan_distance = Some(150.);
    assert_eq!(
        check(&engine::evaluate(&p).unwrap(), "scan_fov").status,
        Status::Passed
    );
    p.parameters.scan_distance = Some(125.);
    assert_eq!(
        check(&engine::evaluate(&p).unwrap(), "scan_fov").status,
        Status::Unknown
    );
    p.parameters.scan_distance = Some(250.);
    assert_eq!(
        check(&engine::evaluate(&p).unwrap(), "scan_wd").status,
        Status::Failed
    );
}
#[test]
fn scanning_counts_half_open_range_and_checks_frequency_separately() {
    let mut p = scanning();
    let e = engine::evaluate(&p).unwrap();
    close(metric(&e, "profiles"), 7500.);
    close(metric(&e, "required_profiles"), 6000.);
    close(metric(&e, "speed"), 50.);
    assert_eq!(check(&e, "scan_speed").status, Status::Passed);
    p.parameters.scan_length = 300.01;
    p.parameters.rate = 3000.;
    let e = engine::evaluate(&p).unwrap();
    close(metric(&e, "profiles"), 22501.);
    close(metric(&e, "required_profiles"), 6001.);
    assert_eq!(check(&e, "rate_limit").status, Status::Failed);
}
#[test]
fn snapshot_devices_do_not_use_profile_equations() {
    let mut p = scanning();
    p.hardware[3]
        .as_mut()
        .unwrap()
        .specs
        .insert("technology".into(), json!("结构光快照"));
    let e = engine::evaluate(&p).unwrap();
    assert_eq!(metric(&e, "profiles"), None);
    assert_eq!(check(&e, "scan_model").status, Status::Unknown);
    assert!(!e.checks.iter().any(|c| c.key == "rate_limit"));
}
#[test]
fn resolution_is_not_substituted_for_repeatability_or_spacing() {
    let mut p = scanning();
    let specs = &mut p.hardware[3].as_mut().unwrap().specs;
    specs.remove("zRepeatabilityUm");
    specs.remove("profileDataIntervalUm");
    specs.insert("zResolutionUm".into(), json!(0.1));
    let e = engine::evaluate(&p).unwrap();
    assert_eq!(check(&e, "z_repeat").status, Status::Unknown);
    assert_eq!(metric(&e, "pitch"), None);
}
#[test]
fn partial_exposure_spec_still_rejects_known_violation() {
    let mut p = scanning();
    p.hardware[3]
        .as_mut()
        .unwrap()
        .specs
        .remove("exposureTimeMinUs");
    p.parameters.scan_exposure = 501.;
    assert_eq!(
        check(&engine::evaluate(&p).unwrap(), "exposure_range").status,
        Status::Failed
    );
    p.parameters.scan_exposure = 100.;
    assert_eq!(
        check(&engine::evaluate(&p).unwrap(), "exposure_range").status,
        Status::Unknown
    );
}
#[test]
fn encoder_checks_spacing_axis_consistency_and_input_limit() {
    let mut p = scanning();
    p.parameters.trigger = "encoder".into();
    p.parameters.pulse_rate = 40000.;
    let e = engine::evaluate(&p).unwrap();
    assert_eq!(check(&e, "encoder_spacing").status, Status::Passed);
    assert_eq!(check(&e, "encoder_speed").status, Status::Passed);
    assert_eq!(check(&e, "scan_speed").status, Status::Passed);
    assert_eq!(check(&e, "sampling_margin").status, Status::Unknown);
    p.parameters.per_profile = 100;
    p.parameters.pulse_rate = 150000.;
    let e = engine::evaluate(&p).unwrap();
    assert_eq!(check(&e, "encoder_spacing").status, Status::Failed);
    assert_eq!(check(&e, "encoder_speed").status, Status::Failed);
    assert_eq!(check(&e, "encoder_rate").status, Status::Failed);
}
#[test]
fn parameters_reject_nonfinite_zero_and_mismatched_hardware() {
    let mut p = imaging();
    p.parameters.distance = f64::NAN;
    assert!(engine::evaluate(&p).is_err());
    p.parameters.distance = 200.;
    p.parameters.per_profile = 0;
    assert!(p.validate().is_err());
    p.parameters.per_profile = 50;
    p.hardware.swap(0, 1);
    assert!(p.validate().is_err());
    let mut value = serde_json::to_value(Project::default()).unwrap();
    value["parameters"].as_object_mut().unwrap().remove("width");
    assert!(serde_json::from_value::<Project>(value).is_err());
}
#[test]
fn reports_escape_names_and_include_reproducible_parameters() {
    let mut p = imaging();
    p.name = "<script>alert(1)</script> & 测试".into();
    let html = engine::report(&p).unwrap();
    assert!(!html.contains("<script>"));
    assert!(html.contains("&lt;script&gt;"));
    assert!(html.contains("原始方案参数与设备快照"));
    assert!(html.contains("待确认"));
}
#[test]
fn catalog_preserves_all_builtin_devices_and_unknown_fields() {
    let all = catalog::built_in().unwrap();
    for (kind, count) in [
        (Kind::Camera, 1393),
        (Kind::Lens, 1809),
        (Kind::Light, 2180),
        (Kind::ThreeD, 267),
    ] {
        assert_eq!(all.iter().filter(|h| h.kind == kind).count(), count);
    }
    let items = catalog::parse_csv(
        Kind::Camera,
        "\u{feff}model,manufacturer,custom,pixel_size_um\n特殊型号,厂家,\"包含,逗号\",0",
        "测试",
    )
    .unwrap();
    assert_eq!(items[0].text("custom"), "包含,逗号");
    assert_eq!(items[0].number("pixel_size_um"), None);
    assert!(catalog::parse_csv(Kind::Lens, "model\n无厂家", "测试").is_err());
    assert!(catalog::parse_three_d("{\"schemaVersion\":2,\"cameras\":[]}", "测试").is_err());
}
#[test]
fn saved_snapshot_survives_reopen_and_import_gets_new_identity() {
    let dir = tempfile::tempdir().unwrap();
    let mut p = imaging();
    p.name = "存储回归".into();
    p.reference = Some(Reference {
        name: "参考".into(),
        mode: p.mode,
        parameters: p.parameters.clone(),
        hardware: p.hardware.clone(),
    });
    {
        let mut store = Store::open(dir.path()).unwrap();
        store.save(&p).unwrap();
    }
    let mut store = Store::open(dir.path()).unwrap();
    let loaded = store.dispatch("load_project", json!({"id":p.id})).unwrap();
    assert_eq!(serde_json::from_value::<Project>(loaded).unwrap(), p);
    let copy = store
        .dispatch(
            "import_project",
            json!({"content":serde_json::to_string(&p).unwrap()}),
        )
        .unwrap();
    assert_ne!(copy["project"]["id"], p.id);
    assert_eq!(copy["projects"].as_array().unwrap().len(), 2);
    p.parameters.width = -1.;
    assert!(store.save(&p).is_err());
    assert_eq!(
        store.dispatch("load_project", json!({"id":p.id})).unwrap()["parameters"]["width"],
        20.
    );
}
#[test]
fn hardware_import_previews_backs_up_then_commits_once() {
    let dir = tempfile::tempdir().unwrap();
    let mut store = Store::open(dir.path()).unwrap();
    let payload = json!({"kind":"camera","format":"csv","content":"model,manufacturer,resolution_x\n导入回归,测试厂家,1234"});
    let preview = store.dispatch("import_preview", payload.clone()).unwrap();
    assert_eq!(preview["added"], 1);
    let query = json!({"kind":"camera","search":"导入回归"});
    assert_eq!(
        store.dispatch("catalog", query.clone()).unwrap()["total"],
        0
    );
    assert!(
        store
            .dispatch("import_commit", json!({"token":"过期预览"}))
            .is_err()
    );
    let result = store
        .dispatch("import_commit", json!({"token":preview["token"]}))
        .unwrap();
    assert!(std::path::Path::new(result["backup"].as_str().unwrap()).exists());
    let backup = rusqlite::Connection::open(result["backup"].as_str().unwrap()).unwrap();
    let count: i64 = backup
        .query_row("SELECT COUNT(*) FROM hardware", [], |r| r.get(0))
        .unwrap();
    assert_eq!(count, 5649);
    assert_eq!(store.dispatch("catalog", query).unwrap()["total"], 1);
    assert!(
        store
            .dispatch("import_commit", json!({"token":preview["token"]}))
            .is_err()
    );
    assert_eq!(
        store.dispatch("import_preview", payload).unwrap()["updated"],
        1
    );
}
#[test]
fn failed_or_duplicate_import_does_not_change_catalog() {
    let dir = tempfile::tempdir().unwrap();
    let mut store = Store::open(dir.path()).unwrap();
    for content in [
        "model,manufacturer\n同名,厂商\n同名,厂商",
        "model,manufacturer\n合法,厂商\n,厂商",
        "model,manufacturer\n合法,厂商\n破损",
    ] {
        assert!(
            store
                .dispatch(
                    "import_preview",
                    json!({"kind":"lens","format":"csv","content":content})
                )
                .is_err()
        );
    }
    assert_eq!(
        store.dispatch("bootstrap", json!({})).unwrap()["counts"]["lens"],
        1809
    );
    assert!(
        store
            .dispatch("import_commit", json!({"token":"不存在"}))
            .is_err()
    );
}
#[test]
fn hardware_updates_do_not_modify_saved_project() {
    let dir = tempfile::tempdir().unwrap();
    let mut store = Store::open(dir.path()).unwrap();
    let p = imaging();
    store.save(&p).unwrap();
    let preview=store.dispatch("import_preview",json!({"kind":"camera","format":"csv","content":"model,manufacturer,resolution_x\n回归设备,测试厂家,9999"})).unwrap();
    store
        .dispatch("import_commit", json!({"token":preview["token"]}))
        .unwrap();
    let saved = store.dispatch("load_project", json!({"id":p.id})).unwrap();
    assert_eq!(saved["hardware"][0]["specs"]["resolution_x"], 2000);
}
#[test]
fn legacy_sqlite_is_read_without_changing_source() {
    let dir = tempfile::tempdir().unwrap();
    let path = dir.path().join("旧产品库.sqlite");
    {
        let conn = rusqlite::Connection::open(&path).unwrap();
        conn.execute_batch("CREATE TABLE camera_products(id INTEGER,model TEXT,manufacturer TEXT,resolution_x INTEGER,pixel_size_um REAL); INSERT INTO camera_products VALUES(1,'旧型号','旧厂家',2048,3.45);").unwrap();
    }
    let before = std::fs::read(&path).unwrap();
    let items = catalog::parse_legacy_database(&before).unwrap();
    assert_eq!(items.len(), 1);
    assert_eq!(items[0].number("resolution_x"), Some(2048.));
    assert_eq!(std::fs::read(&path).unwrap(), before);
    assert!(catalog::parse_legacy_database(b"not sqlite").is_err());
}
#[test]
fn future_database_version_is_not_downgraded() {
    let dir = tempfile::tempdir().unwrap();
    let path = dir.path().join("visionselect.sqlite");
    let conn = rusqlite::Connection::open(path).unwrap();
    conn.execute_batch("PRAGMA user_version=99").unwrap();
    assert!(Store::open(dir.path()).is_err());
    assert_eq!(
        conn.query_row::<u32, _, _>("PRAGMA user_version", [], |r| r.get(0))
            .unwrap(),
        99
    );
}
