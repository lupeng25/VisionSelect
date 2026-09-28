use rusqlite::params;
use serde_json::json;
use std::collections::HashSet;
use vision_core::{catalog, engine, model::*, store::Store};

#[test]
fn canrill_catalog_has_unique_model_sources_and_conservative_specs() {
    let lenses = catalog::canrill_lenses().unwrap();
    assert_eq!(lenses.len(), 584);
    let ids: HashSet<_> = lenses.iter().map(|lens| &lens.id).collect();
    assert_eq!(ids.len(), lenses.len());
    assert!(lenses.iter().all(|lens| {
        lens.manufacturer == "灿锐光学"
            && lens
                .text("source_url")
                .starts_with("https://www.canrilloptics.com/")
    }));

    let telecentric = lenses
        .iter()
        .find(|lens| lens.model == "XF-PTL03708-C")
        .unwrap();
    assert_eq!(telecentric.number("pmag"), Some(0.218));
    assert_eq!(telecentric.number("nominal_wd_mm"), Some(110.));
    assert_eq!(telecentric.number("image_circle_mm"), Some(8.));

    let adjustable = lenses
        .iter()
        .find(|lens| lens.model == "XF-10MDT00679X350-1C-VI")
        .unwrap();
    assert_eq!(adjustable.text("lens_type"), "BiTelecentric");
    assert_eq!(adjustable.number("dof_mm"), None);
    assert_eq!(adjustable.number("f_number"), None);
}

#[test]
fn mvotem_online_catalog_deduplicates_and_keeps_conditional_specs_as_text() {
    let lenses = catalog::mvotem_lenses().unwrap();
    assert_eq!(lenses.len(), 214);
    let ids: HashSet<_> = lenses.iter().map(|lens| &lens.id).collect();
    assert_eq!(ids.len(), lenses.len());
    assert!(lenses.iter().all(|lens| {
        lens.manufacturer == "慕藤光"
            && lens.text("source_url") == "https://www.mvotem.com/list_99/"
    }));
    let telecentric = lenses
        .iter()
        .find(|lens| lens.model == "MT007-300-150-11-C")
        .unwrap();
    assert_eq!(telecentric.text("lens_type"), "BiTelecentric");
    assert_eq!(telecentric.number("pmag"), Some(0.07));
    assert_eq!(telecentric.number("nominal_wd_mm"), Some(300.));
    assert_eq!(telecentric.number("max_sensor_diagonal_mm"), Some(11.));
    let zoom = lenses.iter().find(|lens| lens.model == "MZ7.0XC").unwrap();
    assert_eq!(zoom.text("catalog_magnification"), "0.7~4.5");
    assert_eq!(zoom.number("pmag"), None);
}

#[test]
fn guiguang_catalog_keeps_conditional_dof_out_of_the_general_check() {
    let lenses = catalog::guiguang_lenses().unwrap();
    assert_eq!(lenses.len(), 7);
    let ids: HashSet<_> = lenses.iter().map(|lens| &lens.id).collect();
    assert_eq!(ids.len(), 7);
    let fixed = lenses
        .iter()
        .find(|lens| lens.model == "DXT2-WS50T")
        .unwrap();
    assert_eq!(fixed.text("lens_type"), "FixedMagnification");
    assert_eq!(fixed.number("pmag"), Some(2.));
    assert_eq!(fixed.number("nominal_wd_mm"), Some(49.7));
    assert_eq!(fixed.number("dof_mm"), None);
    assert_eq!(fixed.text("catalog_dof_mm"), "0.4");
    let motorized = lenses.iter().find(|lens| lens.model == "TL-62").unwrap();
    assert_eq!(motorized.text("lens_type"), "Zoom");
    assert_eq!(motorized.text("catalog_magnification"), "0.7X-4.5X");
    assert_eq!(motorized.number("pmag"), None);
}

#[test]
fn new_lenses_reach_existing_library_without_overwriting_a_user_record() {
    let directory = tempfile::tempdir().unwrap();
    Store::open(directory.path()).unwrap();
    let database = directory.path().join("visionselect.sqlite");
    let connection = rusqlite::Connection::open(&database).unwrap();
    connection
        .execute("DELETE FROM hardware WHERE manufacturer='灿锐光学'", [])
        .unwrap();
    connection
        .execute("DELETE FROM hardware WHERE manufacturer='慕藤光'", [])
        .unwrap();
    connection
        .execute(
            "DELETE FROM hardware WHERE manufacturer='桂林桂光仪器有限公司'",
            [],
        )
        .unwrap();
    connection
        .execute(
            "DELETE FROM metadata WHERE key IN ('builtin_canrill_lenses_20260928','builtin_mvotem_lenses_20260928','builtin_guiguang_lenses_20260928')",
            [],
        )
        .unwrap();
    let custom = catalog::from_specs(
        Kind::Lens,
        json!({"model":"XF-PTL03708-C","manufacturer":"灿锐光学","notes":"用户自行核对"})
            .as_object()
            .unwrap()
            .clone(),
        "用户导入",
    )
    .unwrap();
    connection
        .execute(
            "INSERT INTO hardware(id,kind,manufacturer,model,document) VALUES(?1,?2,?3,?4,?5)",
            params![
                custom.id,
                custom.kind.key(),
                custom.manufacturer,
                custom.model,
                serde_json::to_string(&custom).unwrap()
            ],
        )
        .unwrap();
    let guiguang_custom = catalog::from_specs(
        Kind::Lens,
        json!({"model":"DXT2-WS50T","manufacturer":"桂林桂光仪器有限公司","notes":"用户自行核对"})
            .as_object()
            .unwrap()
            .clone(),
        "用户导入",
    )
    .unwrap();
    connection
        .execute(
            "INSERT INTO hardware(id,kind,manufacturer,model,document) VALUES(?1,?2,?3,?4,?5)",
            params![
                guiguang_custom.id,
                guiguang_custom.kind.key(),
                guiguang_custom.manufacturer,
                guiguang_custom.model,
                serde_json::to_string(&guiguang_custom).unwrap()
            ],
        )
        .unwrap();
    let mvotem_custom = catalog::from_specs(
        Kind::Lens,
        json!({"model":"MT007-300-150-11-C","manufacturer":"慕藤光","notes":"用户自行核对"})
            .as_object()
            .unwrap()
            .clone(),
        "用户导入",
    )
    .unwrap();
    connection
        .execute(
            "INSERT INTO hardware(id,kind,manufacturer,model,document) VALUES(?1,?2,?3,?4,?5)",
            params![
                mvotem_custom.id,
                mvotem_custom.kind.key(),
                mvotem_custom.manufacturer,
                mvotem_custom.model,
                serde_json::to_string(&mvotem_custom).unwrap()
            ],
        )
        .unwrap();
    drop(connection);

    let mut store = Store::open(directory.path()).unwrap();
    assert_eq!(
        store.dispatch("bootstrap", json!({})).unwrap()["counts"]["lens"],
        1809
    );
    let page = store
        .dispatch(
            "catalog",
            json!({"kind":"lens","manufacturer":"灿锐光学","search":"XF-PTL03708-C"}),
        )
        .unwrap();
    assert_eq!(page["total"], 1);
    assert_eq!(page["items"][0]["origin"], "用户导入");
    assert_eq!(page["items"][0]["specs"]["notes"], "用户自行核对");
    let mvotem_page = store
        .dispatch(
            "catalog",
            json!({"kind":"lens","manufacturer":"慕藤光","search":"MT007-300-150-11-C"}),
        )
        .unwrap();
    assert_eq!(mvotem_page["total"], 1);
    assert_eq!(mvotem_page["items"][0]["origin"], "用户导入");
    let guiguang_page = store
        .dispatch(
            "catalog",
            json!({"kind":"lens","manufacturer":"桂林桂光仪器有限公司","search":"DXT2-WS50T"}),
        )
        .unwrap();
    assert_eq!(guiguang_page["total"], 1);
    assert_eq!(guiguang_page["items"][0]["origin"], "用户导入");
}

#[test]
fn line_scan_lens_requires_confirmation_but_preserves_measured_fov() {
    let lenses = catalog::canrill_lenses().unwrap();
    let lens = lenses
        .into_iter()
        .find(|lens| lens.model == "XF-ZFL3296-1.82X")
        .unwrap();
    let camera = catalog::from_specs(
        Kind::Camera,
        json!({"model":"面阵测试","manufacturer":"测试","resolution_x":2048,"resolution_y":1536,"pixel_size_um":3.45,"lens_mount":"C"})
            .as_object()
            .unwrap()
            .clone(),
        "测试",
    )
    .unwrap();
    let mut project = Project::default();
    project.hardware[0] = Some(camera);
    project.hardware[1] = Some(lens);
    let result = engine::evaluate(&project).unwrap();
    assert_eq!(result.fov_x, None);
    assert_eq!(result.fov_y, None);
    assert!(
        result
            .checks
            .iter()
            .any(|check| { check.key == "lens_application" && check.status == Status::Unknown })
    );
    project.parameters.measured = true;
    project.parameters.measured_width = 24.;
    project.parameters.measured_height = 12.;
    let measured = engine::evaluate(&project).unwrap();
    assert_eq!(measured.fov_x, Some(24.));
    assert_eq!(measured.fov_y, Some(12.));
    assert!(measured.model_note.contains("实测视场"));
    let check = |key: &str| measured.checks.iter().find(|c| c.key == key).unwrap();
    assert_eq!(check("lens_application").status, Status::Unknown);
    assert_eq!(check("sampling_x").actual, Some(24. * 1000. / 2048.));
    assert_eq!(check("sampling_y").actual, Some(12. * 1000. / 1536.));
    assert_eq!(check("fov_x").status, Status::Passed);
    assert_eq!(check("fov_y").status, Status::Failed);
    project.parameters.measured = false;
    let unmeasured = engine::evaluate(&project).unwrap();
    assert_eq!(unmeasured.fov_x, None);
    assert_eq!(unmeasured.fov_y, None);
}
