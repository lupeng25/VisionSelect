use crate::{
    Result,
    model::{Hardware, Kind},
};
use serde_json::{Map, Value};

pub fn from_specs(kind: Kind, specs: Map<String, Value>, origin: &str) -> Result<Hardware> {
    let model = specs
        .get("model")
        .and_then(Value::as_str)
        .unwrap_or("")
        .trim()
        .to_owned();
    let manufacturer = specs
        .get("manufacturer")
        .and_then(Value::as_str)
        .unwrap_or("未标注")
        .trim()
        .to_owned();
    let item = Hardware {
        id: format!(
            "{}:{}:{}",
            kind.key(),
            manufacturer.to_lowercase(),
            model.to_lowercase()
        ),
        kind,
        model,
        manufacturer,
        specs,
        origin: origin.into(),
    };
    item.validate()?;
    Ok(item)
}
pub fn parse_csv(kind: Kind, content: &str, origin: &str) -> Result<Vec<Hardware>> {
    if kind == Kind::ThreeD {
        return Err("3D 资料请使用 JSON 格式导入".into());
    }
    let mut reader = csv::ReaderBuilder::new()
        .trim(csv::Trim::All)
        .from_reader(content.trim_start_matches('\u{feff}').as_bytes());
    let headers = reader
        .headers()
        .map_err(|e| format!("CSV 表头无效：{e}"))?
        .clone();
    for field in ["model", "manufacturer"] {
        if !headers.iter().any(|h| h == field) {
            return Err(format!("CSV 缺少必要字段：{field}"));
        }
    }
    let mut items = Vec::new();
    for (index, row) in reader.records().enumerate() {
        let row = row.map_err(|e| format!("CSV 第 {} 行：{e}", index + 2))?;
        let specs = headers
            .iter()
            .zip(row.iter())
            .map(|(key, value)| (key.into(), Value::String(value.into())))
            .collect();
        items.push(
            from_specs(kind, specs, origin).map_err(|e| format!("CSV 第 {} 行：{e}", index + 2))?,
        );
    }
    if items.is_empty() {
        return Err("文件中没有有效设备".into());
    }
    Ok(items)
}
pub fn parse_three_d(content: &str, origin: &str) -> Result<Vec<Hardware>> {
    let root: Value = serde_json::from_str(content.trim_start_matches('\u{feff}'))
        .map_err(|e| format!("3D JSON 无效：{e}"))?;
    if root.get("schemaVersion").and_then(Value::as_u64) != Some(1) {
        return Err("不支持的 3D 数据版本".into());
    }
    let array = root
        .get("cameras")
        .and_then(Value::as_array)
        .ok_or("3D JSON 缺少 cameras 数组")?;
    let items = array
        .iter()
        .map(|item| {
            from_specs(
                Kind::ThreeD,
                item.as_object().ok_or("3D 设备记录必须为对象")?.clone(),
                origin,
            )
        })
        .collect::<Result<Vec<_>>>()?;
    if items.is_empty() {
        return Err("3D 文件中没有设备".into());
    }
    Ok(items)
}
pub fn built_in() -> Result<Vec<Hardware>> {
    let mut items = parse_csv(
        Kind::Camera,
        include_str!("../../../resources/data/cameras.csv"),
        "内置资料",
    )?;
    items.extend(parse_csv(
        Kind::Lens,
        include_str!("../../../resources/data/lenses.csv"),
        "内置资料",
    )?);
    items.extend(parse_csv(
        Kind::Light,
        include_str!("../../../resources/data/lights.csv"),
        "内置资料",
    )?);
    items.extend(parse_three_d(
        include_str!("../../../resources/data/three_d_cameras.json"),
        "内置资料",
    )?);
    for item in &mut items {
        crate::catalog_corrections::apply(item);
    }
    Ok(items)
}
pub fn parse_legacy_database(bytes: &[u8]) -> Result<Vec<Hardware>> {
    use rusqlite::{Connection, OpenFlags, types::ValueRef};
    use std::io::Write;
    if !bytes.starts_with(b"SQLite format 3\0") {
        return Err("不是有效的 SQLite 数据库".into());
    }
    let mut file = tempfile::NamedTempFile::new().map_err(|e| e.to_string())?;
    file.write_all(bytes).map_err(|e| e.to_string())?;
    file.flush().map_err(|e| e.to_string())?;
    let connection = Connection::open_with_flags(file.path(), OpenFlags::SQLITE_OPEN_READ_ONLY)
        .map_err(|e| e.to_string())?;
    let mut items = Vec::new();
    for (kind, table) in [
        (Kind::Camera, "camera_products"),
        (Kind::Lens, "lens_products"),
        (Kind::Light, "light_products"),
    ] {
        let exists: bool = connection
            .query_row(
                "SELECT EXISTS(SELECT 1 FROM sqlite_master WHERE type='table' AND name=?1)",
                [table],
                |row| row.get(0),
            )
            .map_err(|e| e.to_string())?;
        if !exists {
            continue;
        }
        let mut statement = connection
            .prepare(&format!("SELECT * FROM {table}"))
            .map_err(|e| e.to_string())?;
        let columns = statement
            .column_names()
            .into_iter()
            .map(str::to_owned)
            .collect::<Vec<_>>();
        let mut rows = statement.query([]).map_err(|e| e.to_string())?;
        while let Some(row) = rows.next().map_err(|e| e.to_string())? {
            let mut specs = Map::new();
            for (index, key) in columns.iter().enumerate() {
                if [
                    "id",
                    "manufacturer_key",
                    "model_key",
                    "search_text",
                    "created_at",
                    "updated_at",
                ]
                .contains(&key.as_str())
                {
                    continue;
                }
                let value = match row.get_ref(index).map_err(|e| e.to_string())? {
                    ValueRef::Null => Value::Null,
                    ValueRef::Integer(n) => Value::from(n),
                    ValueRef::Real(n) => Value::from(n),
                    ValueRef::Text(s) => Value::String(
                        String::from_utf8(s.to_vec()).map_err(|_| "旧库包含无效 UTF-8")?,
                    ),
                    ValueRef::Blob(_) => continue,
                };
                specs.insert(key.clone(), value);
            }
            items.push(from_specs(kind, specs, "旧库导入")?);
        }
    }
    if items.is_empty() {
        return Err("未找到旧版 camera_products、lens_products 或 light_products 数据".into());
    }
    Ok(items)
}
