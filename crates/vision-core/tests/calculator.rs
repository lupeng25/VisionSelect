use serde_json::{Value, json};
use vision_core::{
    calculator::{self, CalculationInput, CalculationResult, MotionInput},
    store::Store,
};

fn compute(input: Value) -> CalculationResult {
    calculator::calculate(serde_json::from_value(input).unwrap()).unwrap()
}
fn metric<'a>(result: &'a CalculationResult, key: &str) -> &'a calculator::CalculationMetric {
    result.metrics.iter().find(|m| m.key == key).unwrap()
}
fn close(actual: f64, expected: f64) {
    assert!(
        (actual - expected).abs() < 1e-8 * expected.abs().max(1.),
        "{actual} != {expected}"
    );
}
fn optical(method: &str) -> Value {
    json!({"kind":"optics","method":method,"resolution_x":2000,"resolution_y":1000,"pixel_size_um":5,"distance_mm":400,"focal_mm":25,"fov_width":100,"fov_height":80,"magnification":0.25})
}

#[test]
fn resolution_checks_both_axes_and_preserves_aspect_ratio() {
    let r = compute(
        json!({"kind":"resolution","object_width":80,"object_height":60,"margin":10,"pixel_limit":25,"resolution_x":4096,"resolution_y":3000,"pixel_size_um":3.45}),
    );
    assert_eq!(r.status, "failed");
    let minimum = metric(&r, "minimum_resolution");
    assert_eq!((minimum.value, minimum.other), (Some(4000.), Some(3200.)));
    close(metric(&r, "sampling").value.unwrap(), 26.6666666666667);
    let geometry = r.geometry.unwrap();
    close(geometry.width / geometry.height, 4096. / 3000.);
    assert_eq!(
        r.apply,
        json!({"width":80.0,"height":60.0,"margin":10.0,"pixel":25.0})
    );
}

#[test]
fn minimum_pixels_round_up_and_equality_passes() {
    let r = compute(
        json!({"kind":"resolution","object_width":20,"object_height":15.001,"margin":0,"pixel_limit":10,"resolution_x":2000,"resolution_y":2000,"pixel_size_um":5}),
    );
    assert_eq!(metric(&r, "minimum_resolution").other, Some(1501.));
    assert_eq!(r.status, "passed");
}

#[test]
fn optical_inverse_covers_the_more_constraining_vertical_axis() {
    let focal = compute(optical("focal"));
    close(metric(&focal, "focal").value.unwrap(), 25.);
    let distance = compute(optical("distance"));
    close(metric(&distance, "distance").value.unwrap(), 400.);
    let fov = compute(optical("fov"));
    assert_eq!(
        (metric(&fov, "fov").value, metric(&fov, "fov").other),
        (Some(160.), Some(80.))
    );
    assert_eq!(fov.apply, json!({"distance":400.0}));
    close(metric(&fov, "image_circle").value.unwrap(), 125_f64.sqrt());
    assert!(!fov.apply.as_object().unwrap().contains_key("measured"));
}

#[test]
fn telecentric_calculations_do_not_apply_a_fictitious_distance() {
    let r = compute(optical("telecentric_fov"));
    assert_eq!(
        (metric(&r, "fov").value, metric(&r, "fov").other),
        (Some(40.), Some(20.))
    );
    assert_eq!(r.apply, json!({}));
    let r = compute(optical("magnification"));
    close(metric(&r, "magnification").value.unwrap(), 0.0625);
    assert_eq!(r.apply, json!({}));
}

#[test]
fn high_magnification_warns_and_impossible_distance_rejects() {
    let mut input = optical("fov");
    input["distance_mm"] = json!(100);
    assert_eq!(compute(input.clone()).status, "unknown");
    input["distance_mm"] = json!(25);
    assert!(calculator::calculate(serde_json::from_value(input).unwrap()).is_err());
    let mut input = optical("distance");
    input["focal_mm"] = json!(1e8);
    assert!(calculator::calculate(serde_json::from_value(input).unwrap()).is_err());
}

#[test]
fn motion_units_and_frame_period_are_independent_limits() {
    let mut input =
        json!({"kind":"motion","sampling_um":10,"speed":100,"exposure":100,"blur":1,"fps":30});
    let r = compute(input.clone());
    assert_eq!(metric(&r, "motion_blur").value, Some(1.));
    assert_eq!(metric(&r, "exposure_limit").value, Some(100.));
    assert_eq!(metric(&r, "travel").value, Some(10.));
    assert_eq!(r.status, "passed");
    input["exposure"] = json!(101);
    assert_eq!(compute(input.clone()).status, "failed");
    input["speed"] = json!(0);
    input["exposure"] = json!(50000);
    let r = compute(input);
    assert_eq!(r.status, "failed");
    assert_eq!(metric(&r, "motion_limit").value, None);
    assert_eq!(metric(&r, "motion_blur").value, Some(0.));
    close(metric(&r, "exposure_limit").value.unwrap(), 1e6 / 30.);
    assert!(r.apply.get("pixel").is_none());
}

#[test]
fn transfer_uses_packed_rows_and_whole_recorded_frames() {
    let mut input = json!({"kind":"data","resolution_x":3,"resolution_y":2,"pixel_format":"Mono12p","fps":3,"duration_seconds":0.7,"bandwidth_mbps":1,"utilization_percent":80});
    let r = compute(input.clone());
    close(metric(&r, "frame_size").value.unwrap(), 10e-6);
    assert_eq!(metric(&r, "frame_count").value, Some(3.));
    close(metric(&r, "storage").value.unwrap(), 30e-9);
    input["pixel_format"] = json!("Mono12");
    close(
        metric(&compute(input.clone()), "frame_size").value.unwrap(),
        12e-6,
    );
    input["bandwidth_mbps"] = json!(0.00001);
    assert_eq!(compute(input).status, "failed");
}

#[test]
fn calculator_rejects_invalid_numbers_and_unknown_formats() {
    for value in [f64::NAN, f64::INFINITY, 0., -1.] {
        assert!(
            calculator::calculate(CalculationInput::Motion(MotionInput {
                sampling_um: value,
                speed: 0.,
                exposure: 1.,
                blur: 1.,
                fps: 1.
            }))
            .is_err()
        );
    }
    let mut input = optical("fov");
    input["resolution_x"] = json!(1.5);
    assert!(calculator::calculate(serde_json::from_value(input).unwrap()).is_err());
    let mut input = json!({"kind":"data","resolution_x":100,"resolution_y":100,"pixel_format":"Mono12Packed-未知","fps":30,"duration_seconds":60,"bandwidth_mbps":125,"utilization_percent":80});
    assert!(calculator::calculate(serde_json::from_value(input.clone()).unwrap()).is_err());
    input["pixel_format"] = json!("Mono8");
    input["utilization_percent"] = json!(101);
    assert!(calculator::calculate(serde_json::from_value(input).unwrap()).is_err());
}

#[test]
fn shared_dispatch_is_stateless_and_returns_applicable_parameters() {
    let directory = tempfile::tempdir().unwrap();
    let mut store = Store::open(directory.path()).unwrap();
    let before = store.dispatch("bootstrap", json!({})).unwrap();
    let r = store
        .dispatch("calculate_2d", json!({"input":optical("distance")}))
        .unwrap();
    assert_eq!(r["apply"]["distance"], 400.);
    let after = store.dispatch("bootstrap", json!({})).unwrap();
    assert_eq!(before["counts"], after["counts"]);
    assert_eq!(before["projects"], after["projects"]);
    assert!(
        store
            .dispatch("calculate_2d", json!({"input":{"kind":"motion"}}))
            .is_err()
    );
}
