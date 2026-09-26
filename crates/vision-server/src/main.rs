use axum::{
    Json, Router,
    extract::{DefaultBodyLimit, Path, State},
    http::{HeaderMap, StatusCode},
    routing::{get, post},
};
use serde_json::{Value, json};
use std::{
    path::PathBuf,
    sync::{Arc, Mutex},
};
use vision_core::store::Store;

async fn request(
    State(store): State<Arc<Mutex<Store>>>,
    Path(operation): Path<String>,
    headers: HeaderMap,
    Json(payload): Json<Value>,
) -> (StatusCode, Json<Value>) {
    // 开发 API 仅接受同机开发界面，不作为公共网络服务。
    if headers
        .get("origin")
        .and_then(|v| v.to_str().ok())
        .is_some_and(|origin| !["http://127.0.0.1:1420", "http://localhost:1420"].contains(&origin))
    {
        return (
            StatusCode::FORBIDDEN,
            Json(json!({"error":"拒绝非本地开发界面来源"})),
        );
    }
    let result = tokio::task::spawn_blocking(move || {
        store
            .lock()
            .map_err(|_| "数据访问锁异常".to_string())?
            .dispatch(&operation, payload)
    })
    .await;
    match result {
        Ok(Ok(value)) => (StatusCode::OK, Json(value)),
        Ok(Err(error)) => (StatusCode::BAD_REQUEST, Json(json!({"error":error}))),
        Err(error) => (
            StatusCode::INTERNAL_SERVER_ERROR,
            Json(json!({"error":error.to_string()})),
        ),
    }
}
#[tokio::main]
async fn main() -> std::result::Result<(), Box<dyn std::error::Error>> {
    let directory = std::env::var_os("VISIONSELECT_DATA_DIR")
        .map(PathBuf::from)
        .unwrap_or_else(|| PathBuf::from(".codex_tmp/rust-dev-data"));
    let store = Store::open(&directory).map_err(std::io::Error::other)?;
    let app = Router::new()
        .route(
            "/api/health",
            get(|| async { Json(json!({"status":"ok","engine":"rust"})) }),
        )
        .route("/api/{operation}", post(request))
        .layer(DefaultBodyLimit::max(64 * 1024 * 1024))
        .with_state(Arc::new(Mutex::new(store)));
    let listener = tokio::net::TcpListener::bind("127.0.0.1:4318").await?;
    println!("VisionSelect Rust 开发 API：http://127.0.0.1:4318");
    axum::serve(listener, app)
        .with_graceful_shutdown(async {
            let _ = tokio::signal::ctrl_c().await;
        })
        .await?;
    Ok(())
}
