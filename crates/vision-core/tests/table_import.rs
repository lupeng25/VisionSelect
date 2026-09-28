use base64::Engine;
use serde_json::{Value, json};
use vision_core::{store::Store, table_import};

fn payload(content: &str) -> Value {
    json!({"kind":"camera","format":"csv","content":content,"header_row":1})
}
fn mapped(mut value: Value) -> Value {
    value["mapping"] = table_import::inspect(&value).unwrap()["mapping"].clone();
    value
}
fn lookup(store: &mut Store, model: &str) -> Value {
    store
        .dispatch("catalog", json!({"kind":"camera","search":model}))
        .unwrap()["items"][0]
        .clone()
}

#[test]
fn excel_sheet_header_and_original_row_errors_are_preserved() {
    let bytes = include_bytes!("fixtures/硬件导入样例.xlsx");
    let mut value = json!({"kind":"camera","format":"excel","content":base64::engine::general_purpose::STANDARD.encode(bytes)});
    let first = table_import::inspect(&value).unwrap();
    assert_eq!(first["sheets"], json!(["说明", "相机参数"]));
    value["sheet"] = json!("相机参数");
    value["header_row"] = json!(2);
    let inspected = table_import::inspect(&value).unwrap();
    assert_eq!(
        inspected["mapping"],
        json!([
            "model",
            "manufacturer",
            "resolution_x",
            "resolution_y",
            "pixel_size_um",
            "shutter_type"
        ])
    );
    value["mapping"] = inspected["mapping"].clone();
    let (items, errors) = table_import::parse(&value).unwrap();
    assert_eq!(items.len(), 1);
    assert_eq!(items[0].0, 3);
    assert_eq!(items[0].1.number("pixel_size_um"), Some(3.45));
    assert_eq!(items[0].1.text("shutter_type"), "Global");
    assert_eq!(errors[0]["row"], 4);
}

#[test]
fn csv_errors_require_explicit_skip_and_commit_creates_backup() {
    let dir = tempfile::tempdir().unwrap();
    let mut store = Store::open(dir.path()).unwrap();
    let mut value = mapped(payload(
        "型号,厂家,水平像素(px)\n表格有效,测试厂家,2000\n表格有效,测试厂家,3000\n表格无效,测试厂家,1.5\n,测试厂家,100\n数值单位,测试厂家,5mm",
    ));
    let preview = store
        .dispatch("table_import_preview", value.clone())
        .unwrap();
    assert!(preview["token"].is_null());
    assert_eq!(preview["invalid"], 4);
    assert_eq!(preview["added"], 1);
    assert!(lookup(&mut store, "表格有效").is_null());
    value["skip_invalid"] = json!(true);
    let preview = store.dispatch("table_import_preview", value).unwrap();
    let result = store
        .dispatch("import_commit", json!({"token":preview["token"]}))
        .unwrap();
    assert_eq!(result["imported"], 1);
    assert!(std::path::Path::new(result["backup"].as_str().unwrap()).exists());
    assert_eq!(
        lookup(&mut store, "表格有效")["specs"]["resolution_x"],
        2000.
    );
    assert!(
        store
            .dispatch("import_commit", json!({"token":preview["token"]}))
            .is_err()
    );
}

#[test]
fn merge_policies_preserve_nonempty_values_and_blank_cells_never_clear_specs() {
    let dir = tempfile::tempdir().unwrap();
    let mut store = Store::open(dir.path()).unwrap();
    let old=store.dispatch("import_preview",payload("model,manufacturer,resolution_x,notes,custom\n合并测试,测试厂家,1024,已有备注,保留扩展字段")).unwrap();
    store
        .dispatch("import_commit", json!({"token":old["token"]}))
        .unwrap();
    let mut project = vision_core::model::Project::default();
    project.hardware[0] = Some(serde_json::from_value(lookup(&mut store, "合并测试")).unwrap());
    store.save(&project).unwrap();
    let mut value = mapped(payload(
        "型号,厂家,水平像素(px),像元尺寸(μm),备注\n合并测试,测试厂家,2048,3.45,",
    ));
    value["policy"] = json!("skip");
    let preview = store
        .dispatch("table_import_preview", value.clone())
        .unwrap();
    assert_eq!(preview["skipped"], 1);
    assert!(preview["token"].is_null());
    value["policy"] = json!("fill");
    let preview = store
        .dispatch("table_import_preview", value.clone())
        .unwrap();
    assert_eq!(preview["rows"][0]["changes"].as_array().unwrap().len(), 1);
    store
        .dispatch("import_commit", json!({"token":preview["token"]}))
        .unwrap();
    let item = lookup(&mut store, "合并测试");
    assert_eq!(item["specs"]["resolution_x"], "1024");
    assert_eq!(item["specs"]["pixel_size_um"], 3.45);
    value["policy"] = json!("replace");
    let preview = store
        .dispatch("table_import_preview", value.clone())
        .unwrap();
    store
        .dispatch("import_commit", json!({"token":preview["token"]}))
        .unwrap();
    let item = lookup(&mut store, "合并测试");
    assert_eq!(item["specs"]["resolution_x"], 2048.);
    assert_eq!(item["specs"]["notes"], "已有备注");
    assert_eq!(item["specs"]["custom"], "保留扩展字段");
    let saved = store
        .dispatch("load_project", json!({"id":project.id}))
        .unwrap();
    assert_eq!(saved["hardware"][0]["specs"]["resolution_x"], "1024");
    assert!(saved["hardware"][0]["specs"].get("pixel_size_um").is_none());
    let unchanged = store.dispatch("table_import_preview", value).unwrap();
    assert_eq!(unchanged["unchanged"], 1);
    assert!(unchanged["token"].is_null());
}

#[test]
fn invalid_mapping_and_repreview_cannot_commit_an_old_table_batch() {
    let dir = tempfile::tempdir().unwrap();
    let mut store = Store::open(dir.path()).unwrap();
    let mut value = mapped(payload("型号,厂家,像元尺寸(μm)\n映射测试,测试厂家,5"));
    let preview = store
        .dispatch("table_import_preview", value.clone())
        .unwrap();
    value["mapping"] = json!(["model", "model", "pixel_size_um"]);
    assert!(
        store
            .dispatch("table_import_preview", value.clone())
            .is_err()
    );
    assert!(
        store
            .dispatch("import_commit", json!({"token":preview["token"]}))
            .is_err()
    );
    value["mapping"] = json!(["model", "manufacturer", "origin"]);
    assert!(store.dispatch("table_import_preview", value).is_err());
}

#[test]
fn units_are_not_guessed_and_csv_delimiters_are_explicit() {
    let mut value = payload("型号;厂家;像元尺寸(mm)\n单位测试;厂家;0.005");
    value["delimiter"] = json!(";");
    let inspection = table_import::inspect(&value).unwrap();
    assert_eq!(
        inspection["mapping"],
        json!(["model", "manufacturer", null])
    );
    let mut value = mapped(payload(
        "型号,厂家,备注\n多行,厂家,\"第一行\n第二行\"\n后一行,厂家,说明",
    ));
    let (items, _) = table_import::parse(&value).unwrap();
    assert_eq!(items[1].0, 4);
    value["header_row"] = json!(0);
    assert!(table_import::inspect(&value).is_err());
    value["header_row"] = json!(1.5);
    assert!(table_import::inspect(&value).is_err());
}
