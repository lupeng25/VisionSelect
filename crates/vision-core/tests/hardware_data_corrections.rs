use serde_json::json;
use vision_core::{catalog, catalog_corrections, model::Kind, store::Store};

#[test]
fn daheng_legacy_db_values_are_not_exposed_as_verified_dynamic_range() {
    let items = catalog::built_in().unwrap();
    let daheng: Vec<_> = items
        .iter()
        .filter(|item| item.kind == Kind::Camera && item.manufacturer == "Daheng Imaging")
        .collect();
    assert_eq!(daheng.len(), 436);
    assert!(
        daheng
            .iter()
            .all(|item| item.number("dynamic_range_db").is_none())
    );
    assert_eq!(
        daheng
            .iter()
            .filter(|item| item.number("signal_noise_ratio_db").is_some())
            .count(),
        5
    );
    assert_eq!(
        daheng
            .iter()
            .filter(|item| item.number("unverifiedLegacyDb").is_some())
            .count(),
        430
    );
    for (model, snr) in [
        ("MER3-515-131X2M", 39.8),
        ("MER3-515-131X2C", 38.728),
        ("MER3-1610-18G3M-P", 40.07),
        ("MER2-041-302GC", 43.),
        ("MARS-251-1938X2C-NF", 45.331),
    ] {
        let item = daheng.iter().find(|item| item.model == model).unwrap();
        assert_eq!(item.number("signal_noise_ratio_db"), Some(snr));
        assert!(item.text("dataCorrectionSource").starts_with("https://"));
    }
    assert!(
        daheng
            .iter()
            .find(|item| item.model == "MER3-515-131X2M")
            .unwrap()
            .text("signal_noise_ratio_condition")
            .contains("Mono10")
    );
    // 其他厂商的真实动态范围不受大恒批次的修正影响。
    let coe = items
        .iter()
        .find(|item| item.model == "COE-023-M-USB-060-IR-C")
        .unwrap();
    assert_eq!(coe.number("dynamic_range_db"), Some(70.));

    let mut verified_later = catalog::parse_csv(
        Kind::Camera,
        include_str!("../../../resources/data/cameras.csv"),
        "内置资料",
    )
    .unwrap()
    .into_iter()
    .find(|item| item.model == "MER3-515-131X2M")
    .unwrap();
    verified_later.specs.insert(
        "dynamic_range_source".into(),
        json!("https://example.com/更新后的原厂规格"),
    );
    assert!(!catalog_corrections::apply(&mut verified_later));
    assert_eq!(verified_later.number("dynamic_range_db"), Some(39.8));
}

#[test]
fn lens_correction_and_old_database_query_preserve_raw_and_user_records() {
    let raw = catalog::parse_csv(
        Kind::Lens,
        include_str!("../../../resources/data/lenses.csv"),
        "内置资料",
    )
    .unwrap()
    .into_iter()
    .find(|item| item.model == "APO-DTJCA25M-2C-110-G")
    .unwrap();
    assert_eq!(raw.number("telecentricity_deg"), Some(0.1));
    let mut corrected = raw.clone();
    assert!(catalog_corrections::apply(&mut corrected));
    assert_eq!(corrected.number("telecentricity_deg"), Some(0.06));
    assert_eq!(corrected.number("dof_mm"), None);
    assert!(!catalog_corrections::apply(&mut corrected));

    let mut imported = raw.clone();
    imported.origin = "用户导入".into();
    assert!(!catalog_corrections::apply(&mut imported));
    assert_eq!(imported.number("telecentricity_deg"), Some(0.1));

    let dir = tempfile::tempdir().unwrap();
    let mut store = Store::open(dir.path()).unwrap();
    let connection = rusqlite::Connection::open(dir.path().join("visionselect.sqlite")).unwrap();
    let original_document = serde_json::to_string(&raw).unwrap();
    connection
        .execute(
            "UPDATE hardware SET document=?1 WHERE id=?2",
            rusqlite::params![original_document, raw.id],
        )
        .unwrap();
    let page = store
        .dispatch(
            "catalog",
            json!({"kind":"lens","search":"APO-DTJCA25M-2C-110-G"}),
        )
        .unwrap();
    assert_eq!(page["items"][0]["specs"]["telecentricity_deg"], "0.06");
    let stored: String = connection
        .query_row(
            "SELECT document FROM hardware WHERE id=?1",
            [raw.id],
            |row| row.get(0),
        )
        .unwrap();
    assert_eq!(stored, original_document);
}
