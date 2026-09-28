use serde_json::json;
use vision_core::{engine, model::Project, store::Store};

#[test]
fn old_projects_accept_missing_installation_dimensions() {
    let mut value = serde_json::to_value(Project::default()).unwrap();
    for key in [
        "light_distance",
        "distance_tolerance",
        "light_distance_tolerance",
    ] {
        value["parameters"].as_object_mut().unwrap().remove(key);
    }
    let project: Project = serde_json::from_value(value).unwrap();
    project.validate().unwrap();
    assert_eq!(project.parameters.light_distance, None);
    assert_eq!(project.parameters.distance_tolerance, None);
    assert_eq!(project.parameters.light_distance_tolerance, None);
}

#[test]
fn installation_annotations_survive_save_reopen_and_import_without_altering_optics() {
    let dir = tempfile::tempdir().unwrap();
    let mut project = Project::default();
    let before = serde_json::to_value(engine::evaluate(&project).unwrap()).unwrap();
    project.parameters.light_distance = Some(60.);
    project.parameters.distance_tolerance = Some(10.);
    project.parameters.light_distance_tolerance = Some(0.);
    assert_eq!(
        serde_json::to_value(engine::evaluate(&project).unwrap()).unwrap(),
        before
    );
    Store::open(dir.path()).unwrap().save(&project).unwrap();
    let mut reopened = Store::open(dir.path()).unwrap();
    let loaded = reopened
        .dispatch("load_project", json!({"id": project.id}))
        .unwrap();
    assert_eq!(serde_json::from_value::<Project>(loaded).unwrap(), project);
    let imported = reopened
        .dispatch(
            "import_project",
            json!({"content": serde_json::to_string(&project).unwrap()}),
        )
        .unwrap();
    assert_eq!(
        imported["project"]["parameters"],
        serde_json::to_value(project.parameters).unwrap()
    );
}

#[test]
fn invalid_installation_distances_and_tolerances_are_rejected() {
    for value in [-1., f64::NAN, f64::INFINITY, 1e9] {
        let mut p = Project::default();
        p.parameters.light_distance = Some(value);
        assert!(p.validate().is_err());
        p.parameters.light_distance = None;
        p.parameters.distance_tolerance = Some(value);
        assert!(p.validate().is_err());
        p.parameters.distance_tolerance = None;
        p.parameters.light_distance_tolerance = Some(value);
        assert!(p.validate().is_err());
    }
    let mut p = Project::default();
    p.parameters.light_distance = Some(0.);
    assert!(p.validate().is_err());
}
