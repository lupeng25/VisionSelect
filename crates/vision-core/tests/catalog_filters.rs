use serde_json::{Value, json};
use vision_core::{
    catalog,
    catalog_filter::{self, Filters},
    model::{Hardware, Kind},
    store::Store,
};

fn hardware(kind: Kind, specs: Value) -> Hardware {
    let mut specs = specs.as_object().unwrap().clone();
    specs.insert("manufacturer".into(), json!("筛选回归"));
    specs.insert("model".into(), json!("参数样本"));
    catalog::from_specs(kind, specs, "回归验证").unwrap()
}
fn filters(kind: Kind, value: Value) -> Filters {
    Filters::parse(&value, &catalog_filter::definitions(kind)).unwrap()
}

#[test]
fn combined_filters_run_before_total_and_pagination() {
    let directory = tempfile::tempdir().unwrap();
    let mut store = Store::open(directory.path()).unwrap();
    let csv = "model,manufacturer,resolution_x,resolution_y,pixel_size_um,interface,color_mode,max_fps\nCAT-A,筛选回归,2000,1000,5,USB3,Mono,60\nCAT-B,筛选回归,2000,1000,5,USB3,Mono,120\nCAT-C,筛选回归,2000,1000,5,GigE,Color,120\nCAT-D,筛选回归,2000,1000,5,USB3,Mono,0";
    let preview = store
        .dispatch(
            "import_preview",
            json!({"kind":"camera","format":"csv","content":csv}),
        )
        .unwrap();
    store
        .dispatch("import_commit", json!({"token":preview["token"]}))
        .unwrap();
    let mut query = json!({"kind":"camera","manufacturer":"筛选回归","limit":1,"filters":{"choices":{"interface":"USB3.0","color_mode":"Mono"},"ranges":{"megapixels":{"min":2,"max":2},"max_fps":{"min":60,"max":120}}}});
    let first = store.dispatch("catalog", query.clone()).unwrap();
    assert_eq!(first["total"], 2);
    assert_eq!(first["items"][0]["model"], "CAT-A");
    query["offset"] = json!(1);
    let next = store.dispatch("catalog", query.clone()).unwrap();
    assert_eq!(next["total"], 2);
    assert_eq!(next["items"][0]["model"], "CAT-B");
    query["offset"] = json!(0);
    query["search"] = json!("  cat-b  ");
    assert_eq!(
        store.dispatch("catalog", query.clone()).unwrap()["total"],
        1
    );
    query["search"] = json!("没有这个型号");
    let empty = store.dispatch("catalog", query).unwrap();
    assert_eq!(empty["total"], 0);
    assert_eq!(empty["filter_schema"], first["filter_schema"]);
    assert!(empty["category_total"].as_u64().unwrap() > 4);
    assert_eq!(first["items"][0]["specs"]["interface"], "USB3");
}

#[test]
fn numeric_ranges_are_inclusive_and_do_not_treat_unknown_as_zero() {
    let query = filters(
        Kind::Camera,
        json!({"ranges":{"megapixels":{"min":2,"max":2},"max_fps":{"min":60,"max":60}}}),
    );
    assert!(query.matches(&hardware(
        Kind::Camera,
        json!({"resolution_x":" 2000 ","resolution_y":1000,"max_fps":"60"})
    )));
    assert!(!query.matches(&hardware(
        Kind::Camera,
        json!({"resolution_x":2000,"resolution_y":1000,"max_fps":59.999})
    )));
    let zero = filters(
        Kind::Camera,
        json!({"ranges":{"max_fps":{"min":0,"max":0}}}),
    );
    for value in [
        Value::Null,
        json!(0),
        json!(-1),
        json!(""),
        json!("NaN"),
        json!("unknown"),
    ] {
        assert!(!zero.matches(&hardware(Kind::Camera, json!({"max_fps":value}))));
    }
    let inactive = filters(Kind::Camera, json!({"ranges":{"max_fps":{}}}));
    assert!(inactive.matches(&hardware(Kind::Camera, json!({}))));
}

#[test]
fn enumerations_match_whole_values_and_normalize_aliases() {
    let query = filters(
        Kind::Camera,
        json!({"choices":{"interface":"gige","lens_mount":"C-Mount","color_mode":"NIR"}}),
    );
    assert!(query.matches(&hardware(
        Kind::Camera,
        json!({"interface":"GigE","lens_mount":"C","color_mode":"Near-infrared"})
    )));
    assert!(!query.matches(&hardware(
        Kind::Camera,
        json!({"interface":"10GigE","lens_mount":"C","color_mode":"NIR"})
    )));
    assert!(!query.matches(&hardware(
        Kind::Camera,
        json!({"interface":"GigE","lens_mount":"CS-mount","color_mode":"NIR"})
    )));
    let global = filters(Kind::Camera, json!({"choices":{"shutter_type":"Global"}}));
    assert!(!global.matches(&hardware(
        Kind::Camera,
        json!({"shutter_type":"Rolling/GlobalResetRelease"})
    )));
    let unknown = filters(Kind::Camera, json!({"choices":{"shutter_type":null}}));
    assert!(unknown.matches(&hardware(Kind::Camera, json!({"shutter_type":"Unknown"}))));
    assert!(unknown.matches(&hardware(Kind::Camera, json!({}))));
    assert!(!unknown.matches(&hardware(Kind::Camera, json!({"shutter_type":"Rolling"}))));
}

#[test]
fn lens_type_and_numeric_conditions_match_real_units() {
    let fixed = filters(
        Kind::Lens,
        json!({"choices":{"lens_type":"fixed_focal","lens_mount":"c"},"ranges":{"focal_length_mm":{"min":25,"max":35},"image_circle_mm":{"min":11}}}),
    );
    assert!(fixed.matches(&hardware(Kind::Lens, json!({"lens_type":"FixedFocal","lens_mount":"C-mount","focal_length_mm":"25","image_circle_mm":11}))));
    assert!(!fixed.matches(&hardware(
        Kind::Lens,
        json!({"lens_type":"BiTelecentric","lens_mount":"C","pmag":0.3,"image_circle_mm":11})
    )));
    let telecentric = filters(
        Kind::Lens,
        json!({"choices":{"lens_type":"bi_telecentric"},"ranges":{"pmag":{"max":0.3},"nominal_wd_mm":{"min":110}}}),
    );
    assert!(telecentric.matches(&hardware(
        Kind::Lens,
        json!({"lens_type":"BiTelecentric","pmag":0.3,"nominal_wd_mm":110})
    )));
}

#[test]
fn multicolor_lights_and_array_interfaces_keep_their_meaning() {
    let blue = filters(
        Kind::Light,
        json!({"choices":{"light_type":"bar","color":"blue"},"ranges":{"active_width_mm":{"min":100}}}),
    );
    let item = hardware(
        Kind::Light,
        json!({"light_type":"Bar","color":"White/Blue/Green","active_width_mm":"100"}),
    );
    let original = item.clone();
    assert!(blue.matches(&item));
    assert_eq!(item, original);
    assert!(!blue.matches(&hardware(
        Kind::Light,
        json!({"light_type":"Bar","color":"RGB","active_width_mm":100})
    )));
    let interface = filters(Kind::ThreeD, json!({"choices":{"interfaces":"I/O"}}));
    let item = hardware(Kind::ThreeD, json!({"interfaces":["10GigE","I/O","SDK"]}));
    assert!(interface.matches(&item));
    assert!(!filters(Kind::ThreeD, json!({"choices":{"interfaces":"GigE"}})).matches(&item));
    assert!(!filters(Kind::ThreeD, json!({"choices":{"interfaces":"I"}})).matches(&item));
}

#[test]
fn boolean_false_is_distinct_from_missing_and_counts_are_per_device() {
    let no = hardware(Kind::ThreeD, json!({"supportsEncoder":false}));
    let unknown = hardware(Kind::ThreeD, json!({}));
    let yes = hardware(Kind::ThreeD, json!({"supportsEncoder":"TRUE"}));
    let query = filters(Kind::ThreeD, json!({"choices":{"supportsEncoder":"false"}}));
    assert!(query.matches(&no));
    assert!(!query.matches(&unknown));
    assert!(!query.matches(&yes));
    assert!(filters(Kind::ThreeD, json!({"choices":{"supportsEncoder":null}})).matches(&unknown));
    let mut schema = catalog_filter::definitions(Kind::ThreeD);
    let definition = schema
        .iter_mut()
        .find(|d| d.key == "supportsEncoder")
        .unwrap();
    for item in [&yes, &no, &unknown] {
        definition.observe(item);
    }
    definition.finish();
    assert_eq!(definition.known_count, 2);
    assert_eq!(definition.options.len(), 3);
    assert!(definition.options.iter().all(|o| o.count == 1));
    let mut schema = catalog_filter::definitions(Kind::Light);
    let definition = schema.iter_mut().find(|d| d.key == "color").unwrap();
    definition.observe(&hardware(Kind::Light, json!({"color":"Red/Red/Blue"})));
    definition.finish();
    assert!(definition.options.iter().all(|o| o.count == 1));
}

#[test]
fn invalid_or_cross_category_filters_fail_explicitly() {
    let schema = catalog_filter::definitions(Kind::Camera);
    for value in [
        json!({"ranges":{"max_fps":{"min":100,"max":60}}}),
        json!({"ranges":{"max_fps":{"min":-1}}}),
        json!({"ranges":{"max_fps":{"min":"NaN"}}}),
        json!({"choices":{"max_fps":"60"}}),
        json!({"ranges":{"interface":{"min":1}}}),
        json!({"choices":{"technology":"线激光轮廓"}}),
        json!({"unknown_field":{}}),
    ] {
        assert!(Filters::parse(&value, &schema).is_err());
    }
}
