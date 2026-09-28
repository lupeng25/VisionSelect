#![cfg_attr(not(debug_assertions), windows_subsystem = "windows")]
use serde_json::Value;
use std::{
    io::Write,
    sync::{Arc, Mutex},
};
use tauri::Manager;
use vision_core::{model::Project, store::Store};

fn read_import_path(path: &str) -> Result<Value, String> {
    use base64::Engine;
    use std::io::Read;
    let path = std::path::Path::new(path);
    let extension = path
        .extension()
        .and_then(|e| e.to_str())
        .unwrap_or("")
        .to_ascii_lowercase();
    if !["xlsx", "xls", "csv", "json", "db", "sqlite"].contains(&extension.as_str()) {
        return Err("请选择 Excel、CSV、三维 JSON 或旧数据库文件".into());
    }
    let file = std::fs::File::open(path).map_err(|e| format!("无法读取文件：{e}"))?;
    let metadata = file.metadata().map_err(|e| e.to_string())?;
    const LIMIT: u64 = 32 * 1024 * 1024;
    if !metadata.is_file() {
        return Err("请拖入文件，不是文件夹".into());
    }
    if metadata.len() > LIMIT {
        return Err("文件超过 32 MB 限制".into());
    }
    let mut bytes = Vec::new();
    file.take(LIMIT + 1)
        .read_to_end(&mut bytes)
        .map_err(|e| e.to_string())?;
    if bytes.len() as u64 > LIMIT {
        return Err("文件超过 32 MB 限制".into());
    }
    Ok(
        serde_json::json!({"name":path.file_name().unwrap_or_default().to_string_lossy(),"content":base64::engine::general_purpose::STANDARD.encode(bytes)}),
    )
}

#[tauri::command]
async fn read_hardware_import_file(path: String) -> Result<Value, String> {
    tauri::async_runtime::spawn_blocking(move || read_import_path(&path))
        .await
        .map_err(|e| e.to_string())?
}

#[tauri::command]
async fn request(
    state: tauri::State<'_, Arc<Mutex<Store>>>,
    operation: String,
    payload: Value,
) -> Result<Value, String> {
    let store = state.inner().clone();
    tauri::async_runtime::spawn_blocking(move || {
        store
            .lock()
            .map_err(|_| "数据访问锁异常".to_string())?
            .dispatch(&operation, payload)
    })
    .await
    .map_err(|e| e.to_string())?
}
#[tauri::command]
fn export_file(path: String, project: Project, format: String) -> Result<(), String> {
    project.validate()?;
    let body = match format.as_str() {
        "project" => serde_json::to_string_pretty(&project).map_err(|e| e.to_string())?,
        "report" => vision_core::engine::report(&project)?,
        _ => return Err("不支持的导出类型".into()),
    };
    let target = std::path::Path::new(&path);
    let parent = target.parent().ok_or("导出路径无效")?;
    let mut file = tempfile::NamedTempFile::new_in(parent).map_err(|e| e.to_string())?;
    file.write_all(body.as_bytes()).map_err(|e| e.to_string())?;
    file.as_file().sync_all().map_err(|e| e.to_string())?;
    file.persist(target).map_err(|e| e.to_string())?;
    Ok(())
}
fn main() {
    tauri::Builder::default()
        .plugin(tauri_plugin_dialog::init())
        .setup(|app| {
            let directory = std::env::var_os("VISIONSELECT_DATA_DIR")
                .map(std::path::PathBuf::from)
                .unwrap_or(app.path().app_data_dir()?);
            app.manage(Arc::new(Mutex::new(
                Store::open(&directory).map_err(std::io::Error::other)?,
            )));
            Ok(())
        })
        .invoke_handler(tauri::generate_handler![
            request,
            export_file,
            read_hardware_import_file
        ])
        .run(tauri::generate_context!())
        .expect("无法启动 VisionSelect 桌面应用");
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn dropped_import_file_preserves_bytes_and_rejects_invalid_inputs() {
        use base64::Engine;
        let dir = tempfile::tempdir().unwrap();
        let path = dir.path().join("中文参数.CSV");
        let bytes = b"model,manufacturer\ntest,vendor";
        std::fs::write(&path, bytes).unwrap();
        let result = read_import_path(path.to_str().unwrap()).unwrap();
        assert_eq!(result["name"], "中文参数.CSV");
        assert_eq!(
            base64::engine::general_purpose::STANDARD
                .decode(result["content"].as_str().unwrap())
                .unwrap(),
            bytes
        );
        let unsupported = dir.path().join("程序.exe");
        std::fs::write(&unsupported, bytes).unwrap();
        assert!(read_import_path(unsupported.to_str().unwrap()).is_err());
        assert!(read_import_path(dir.path().to_str().unwrap()).is_err());
        let large = dir.path().join("超大.csv");
        std::fs::File::create(&large)
            .unwrap()
            .set_len(32 * 1024 * 1024 + 1)
            .unwrap();
        assert!(
            read_import_path(large.to_str().unwrap())
                .unwrap_err()
                .contains("32 MB")
        );
    }

    #[test]
    fn native_export_roundtrips_and_replaces_existing_file() {
        let dir = tempfile::tempdir().unwrap();
        let path = dir
            .path()
            .join("中文工程方案.json")
            .to_string_lossy()
            .into_owned();
        let mut project = Project::default();
        export_file(path.clone(), project.clone(), "project".into()).unwrap();
        let actual: Project =
            serde_json::from_str(&std::fs::read_to_string(&path).unwrap()).unwrap();
        assert_eq!(actual, project);
        project.name = "更新后的方案".into();
        export_file(path.clone(), project.clone(), "project".into()).unwrap();
        let updated: Project =
            serde_json::from_str(&std::fs::read_to_string(path).unwrap()).unwrap();
        assert_eq!(updated, project);
    }

    #[test]
    fn invalid_export_preserves_existing_file() {
        let dir = tempfile::tempdir().unwrap();
        let path = dir
            .path()
            .join("已存在的文件.json")
            .to_string_lossy()
            .into_owned();
        std::fs::write(&path, "原始内容").unwrap();
        let mut project = Project::default();
        project.parameters.width = -1.;
        assert!(export_file(path.clone(), project, "project".into()).is_err());
        assert!(export_file(path.clone(), Project::default(), "unknown".into()).is_err());
        assert_eq!(std::fs::read_to_string(path).unwrap(), "原始内容");
    }

    #[test]
    fn native_report_exports_printable_chinese_document() {
        let dir = tempfile::tempdir().unwrap();
        let path = dir
            .path()
            .join("校核记录.html")
            .to_string_lossy()
            .into_owned();
        export_file(path.clone(), Project::default(), "report".into()).unwrap();
        let text = std::fs::read_to_string(path).unwrap();
        assert!(text.contains("逐项校核") && text.contains("@media print"));
    }
}
