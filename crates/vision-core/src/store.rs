use crate::{Result, calculator, catalog, catalog_filter, engine, model::*};
use chrono::Utc;
use rusqlite::{Connection, OptionalExtension, params};
use serde_json::{Value, json};
use std::{
    collections::{BTreeMap, HashSet},
    path::{Path, PathBuf},
};

pub struct Store {
    connection: Connection,
    directory: PathBuf,
    pending_import: Option<(String, Vec<Hardware>)>,
}
impl Store {
    pub fn open(directory: &Path) -> Result<Self> {
        std::fs::create_dir_all(directory).map_err(|e| e.to_string())?;
        let mut connection =
            Connection::open(directory.join("visionselect.sqlite")).map_err(|e| e.to_string())?;
        connection
            .busy_timeout(std::time::Duration::from_secs(5))
            .map_err(|e| e.to_string())?;
        let version: u32 = connection
            .query_row("PRAGMA user_version", [], |row| row.get(0))
            .map_err(|e| e.to_string())?;
        if version > 1 {
            return Err("产品库版本较新，请更新应用后打开".into());
        }
        connection.execute_batch(
            "PRAGMA journal_mode=WAL;
             CREATE TABLE IF NOT EXISTS hardware(id TEXT PRIMARY KEY, kind TEXT NOT NULL, manufacturer TEXT NOT NULL, model TEXT NOT NULL, document TEXT NOT NULL);
             CREATE INDEX IF NOT EXISTS hardware_kind ON hardware(kind,manufacturer,model);
             CREATE TABLE IF NOT EXISTS projects(id TEXT PRIMARY KEY, name TEXT NOT NULL, document TEXT NOT NULL, updated_at TEXT NOT NULL);
             CREATE TABLE IF NOT EXISTS metadata(key TEXT PRIMARY KEY, value TEXT);
             PRAGMA user_version=1;"
        ).map_err(|e|e.to_string())?;
        let seeded: bool = connection
            .query_row(
                "SELECT EXISTS(SELECT 1 FROM metadata WHERE key='seeded')",
                [],
                |row| row.get(0),
            )
            .map_err(|e| e.to_string())?;
        if !seeded {
            let items = catalog::built_in()?;
            let tx = connection.transaction().map_err(|e| e.to_string())?;
            for item in items {
                upsert(&tx, &item)?;
            }
            tx.execute("INSERT INTO metadata(key,value) VALUES('seeded','1')", [])
                .map_err(|e| e.to_string())?;
            tx.commit().map_err(|e| e.to_string())?;
        }
        // 旧用户库只在首次启动时写入过内置资料。新增厂家按批次补入，
        // 已存在的同名型号保持原记录，避免覆盖用户导入和已有方案快照。
        for (lens_batch, load) in [
            (
                "builtin_canrill_lenses_20260928",
                catalog::canrill_lenses as fn() -> Result<Vec<Hardware>>,
            ),
            ("builtin_mvotem_lenses_20260928", catalog::mvotem_lenses),
            ("builtin_guiguang_lenses_20260928", catalog::guiguang_lenses),
        ] {
            let applied: bool = connection
                .query_row(
                    "SELECT EXISTS(SELECT 1 FROM metadata WHERE key=?1)",
                    [lens_batch],
                    |row| row.get(0),
                )
                .map_err(|e| e.to_string())?;
            if applied {
                continue;
            }
            let tx = connection.transaction().map_err(|e| e.to_string())?;
            for item in load()? {
                tx.execute(
                    "INSERT OR IGNORE INTO hardware(id,kind,manufacturer,model,document) VALUES(?1,?2,?3,?4,?5)",
                    params![
                        item.id,
                        item.kind.key(),
                        item.manufacturer,
                        item.model,
                        serde_json::to_string(&item).map_err(|e| e.to_string())?
                    ],
                )
                .map_err(|e| e.to_string())?;
            }
            tx.execute(
                "INSERT INTO metadata(key,value) VALUES(?1,'1')",
                [lens_batch],
            )
            .map_err(|e| e.to_string())?;
            tx.commit().map_err(|e| e.to_string())?;
        }
        Ok(Self {
            connection,
            directory: directory.to_path_buf(),
            pending_import: None,
        })
    }
    pub fn dispatch(&mut self, operation: &str, payload: Value) -> Result<Value> {
        match operation {
            "bootstrap" => {
                let mut counts = BTreeMap::new();
                for kind in Kind::ALL {
                    let count: u32 = self
                        .connection
                        .query_row(
                            "SELECT COUNT(*) FROM hardware WHERE kind=?1",
                            [kind.key()],
                            |row| row.get(0),
                        )
                        .map_err(|e| e.to_string())?;
                    counts.insert(kind.key(), count);
                }
                Ok(
                    json!({"counts":counts,"projects":self.projects()?,"default_project":Project::default(),"data_directory":self.directory}),
                )
            }
            "catalog" => self.query(&payload),
            "calculate_2d" => {
                let input: calculator::CalculationInput =
                    serde_json::from_value(payload["input"].clone())
                        .map_err(|e| format!("计算输入无效：{e}"))?;
                serde_json::to_value(calculator::calculate(input)?).map_err(|e| e.to_string())
            }
            "evaluate" => {
                let project: Project = serde_json::from_value(payload["project"].clone())
                    .map_err(|e| format!("方案格式无效：{e}"))?;
                Ok(serde_json::to_value(engine::evaluate(&project)?).map_err(|e| e.to_string())?)
            }
            "save_project" => {
                let project = parse_project(&payload["project"])?;
                self.save(&project)?;
                Ok(json!({"projects":self.projects()?,"saved_at":Utc::now().to_rfc3339()}))
            }
            "load_project" => {
                let id = payload["id"].as_str().ok_or("缺少方案标识")?;
                let body: Option<String> = self
                    .connection
                    .query_row("SELECT document FROM projects WHERE id=?1", [id], |row| {
                        row.get(0)
                    })
                    .optional()
                    .map_err(|e| e.to_string())?;
                let body = body.ok_or("方案不存在")?;
                let project: Project = serde_json::from_str(&body).map_err(|e| e.to_string())?;
                project.validate()?;
                serde_json::to_value(project).map_err(|e| e.to_string())
            }
            "delete_project" => {
                let id = payload["id"].as_str().ok_or("缺少方案标识")?;
                self.connection
                    .execute("DELETE FROM projects WHERE id=?1", [id])
                    .map_err(|e| e.to_string())?;
                Ok(json!({"projects":self.projects()?}))
            }
            "import_project" => {
                let content = payload["content"].as_str().ok_or("缺少方案内容")?;
                if content.len() > 4 * 1024 * 1024 {
                    return Err("方案文件超过 4 MB 限制".into());
                }
                let mut project: Project =
                    serde_json::from_str(content.trim_start_matches('\u{feff}'))
                        .map_err(|e| format!("方案 JSON 无效：{e}"))?;
                project.validate()?;
                // 导入副本获得新标识，不覆盖本机已有方案。
                project.id = uuid::Uuid::new_v4().to_string();
                self.save(&project)?;
                Ok(json!({"project":project,"projects":self.projects()?}))
            }
            "export_project" => {
                let project = parse_project(&payload["project"])?;
                Ok(
                    json!({"content":serde_json::to_string_pretty(&project).map_err(|e|e.to_string())?}),
                )
            }
            "report" => {
                let project = parse_project(&payload["project"])?;
                Ok(json!({"content":engine::report(&project)?}))
            }
            "import_preview" => self.preview(&payload),
            "table_import_inspect" => crate::table_import::inspect(&payload),
            "table_import_fields" => {
                let kind: Kind = serde_json::from_value(payload["kind"].clone())
                    .map_err(|_| "请选择硬件分类")?;
                serde_json::to_value(crate::table_import::fields(kind)).map_err(|e| e.to_string())
            }
            "table_import_preview" => self.preview_table(&payload),
            "import_commit" => self.commit(&payload),
            _ => Err("未知操作".into()),
        }
    }
    pub fn projects(&self) -> Result<Vec<Value>> {
        let mut statement = self
            .connection
            .prepare("SELECT id,name,updated_at FROM projects ORDER BY updated_at DESC,id")
            .map_err(|e| e.to_string())?;
        statement.query_map([],|row| Ok(json!({"id":row.get::<_,String>(0)?,"name":row.get::<_,String>(1)?,"updated_at":row.get::<_,String>(2)?})))
            .map_err(|e|e.to_string())?.map(|r|r.map_err(|e|e.to_string())).collect()
    }
    pub fn save(&mut self, project: &Project) -> Result<()> {
        project.validate()?;
        self.connection.execute("INSERT INTO projects(id,name,document,updated_at) VALUES(?1,?2,?3,?4) ON CONFLICT(id) DO UPDATE SET name=excluded.name,document=excluded.document,updated_at=excluded.updated_at",
            params![project.id, project.name, serde_json::to_string(project).map_err(|e|e.to_string())?, Utc::now().to_rfc3339()]).map_err(|e|e.to_string())?;
        Ok(())
    }
    fn query(&self, payload: &Value) -> Result<Value> {
        let kind: Kind =
            serde_json::from_value(payload["kind"].clone()).map_err(|_| "请选择硬件类型")?;
        let search = payload["search"]
            .as_str()
            .unwrap_or("")
            .trim()
            .to_lowercase();
        let mut schema = catalog_filter::definitions(kind);
        let filters = catalog_filter::Filters::parse(&payload["filters"], &schema)?;
        let manufacturer = payload["manufacturer"].as_str().unwrap_or("");
        let offset = payload["offset"].as_u64().unwrap_or(0).min(1_000_000) as usize;
        let limit = payload["limit"].as_u64().unwrap_or(60).clamp(1, 100) as usize;
        let mut statement = self.connection.prepare("SELECT document FROM hardware WHERE kind=?1 ORDER BY manufacturer COLLATE NOCASE,model COLLATE NOCASE,id").map_err(|e|e.to_string())?;
        let rows = statement
            .query_map([kind.key()], |row| row.get::<_, String>(0))
            .map_err(|e| e.to_string())?;
        let mut brands = std::collections::BTreeSet::new();
        let mut items = Vec::new();
        let mut total = 0;
        let mut category_total = 0;
        for row in rows {
            let mut item: Hardware = serde_json::from_str(&row.map_err(|e| e.to_string())?)
                .map_err(|e| e.to_string())?;
            // 旧资料库同样应用修正，查询不回写记录；筛选、详情和新快照使用同一组值。
            crate::catalog_corrections::apply(&mut item);
            brands.insert(item.manufacturer.clone());
            category_total += 1;
            for definition in &mut schema {
                definition.observe(&item);
            }
            if !manufacturer.is_empty() && item.manufacturer != manufacturer {
                continue;
            }
            let haystack = format!(
                "{} {} {}",
                item.manufacturer,
                item.model,
                serde_json::to_string(&item.specs).unwrap_or_default()
            )
            .to_lowercase();
            if !search.is_empty() && !haystack.contains(&search) {
                continue;
            }
            if !filters.matches(&item) {
                continue;
            }
            if total >= offset && items.len() < limit {
                items.push(item);
            }
            total += 1;
        }
        for definition in &mut schema {
            definition.finish();
        }
        Ok(
            json!({"kind":kind,"items":items,"total":total,"category_total":category_total,"brands":brands,"offset":offset,"limit":limit,"filter_schema":schema}),
        )
    }
    fn preview_table(&mut self, payload: &Value) -> Result<Value> {
        self.pending_import = None;
        let policy = payload["policy"].as_str().unwrap_or("fill");
        if !["fill", "skip", "replace"].contains(&policy) {
            return Err("未知重复处理方式".into());
        }
        let (items, errors) = crate::table_import::parse(payload)?;
        let mut pending = Vec::new();
        let mut rows = Vec::new();
        let (mut added, mut updated, mut skipped, mut unchanged) = (0, 0, 0, 0);
        for (row, incoming) in items {
            let body: Option<String> = self
                .connection
                .query_row(
                    "SELECT document FROM hardware WHERE id=?1",
                    [&incoming.id],
                    |r| r.get(0),
                )
                .optional()
                .map_err(|e| e.to_string())?;
            let original = body
                .map(|s| serde_json::from_str::<Hardware>(&s).map_err(|e| e.to_string()))
                .transpose()?;
            let mut changes = Vec::new();
            let action;
            if let Some(mut existing) = original {
                crate::catalog_corrections::apply(&mut existing);
                if policy == "skip" {
                    skipped += 1;
                    action = "skipped";
                } else {
                    for (key, value) in &incoming.specs {
                        let before = existing.specs.get(key);
                        let blank = before.is_none_or(|v| {
                            v.is_null() || v.as_str().is_some_and(|s| s.trim().is_empty())
                        });
                        let equivalent = before.is_some_and(|v| {
                            v == value
                                || value.as_f64().is_some_and(|n| {
                                    v.as_str().and_then(|s| s.parse::<f64>().ok()) == Some(n)
                                })
                        });
                        if !equivalent && (policy == "replace" || blank) {
                            changes.push(json!({"key":key,"before":before,"after":value}));
                            existing.specs.insert(key.clone(), value.clone());
                        }
                    }
                    if changes.is_empty() {
                        unchanged += 1;
                        action = "unchanged";
                    } else {
                        existing.model = existing.text("model");
                        existing.manufacturer = existing.text("manufacturer");
                        existing.origin = "用户导入".into();
                        existing.validate()?;
                        pending.push(existing);
                        updated += 1;
                        action = "updated";
                    }
                }
            } else {
                for (key, value) in &incoming.specs {
                    changes.push(json!({"key":key,"before":null,"after":value}));
                }
                pending.push(incoming.clone());
                added += 1;
                action = "added";
            }
            if rows.len() < 100 {
                rows.push(json!({"row":row,"model":incoming.model,"manufacturer":incoming.manufacturer,"action":action,"changes":changes}));
            }
        }
        let total = added + updated + skipped + unchanged + errors.len();
        let can_commit = !pending.is_empty()
            && (errors.is_empty() || payload["skip_invalid"].as_bool() == Some(true));
        let token = if can_commit {
            let token = uuid::Uuid::new_v4().to_string();
            self.pending_import = Some((token.clone(), pending));
            Some(token)
        } else {
            None
        };
        Ok(
            json!({"token":token,"added":added,"updated":updated,"skipped":skipped,"unchanged":unchanged,"total":total,"rows":rows,"invalid":errors.len(),"errors":errors.into_iter().take(100).collect::<Vec<_>>() }),
        )
    }
    fn preview(&mut self, payload: &Value) -> Result<Value> {
        let kind: Kind =
            serde_json::from_value(payload["kind"].clone()).map_err(|_| "请选择硬件类型")?;
        let content = payload["content"].as_str().ok_or("缺少导入内容")?;
        if content.len() > 48 * 1024 * 1024 {
            return Err("导入内容超过大小限制".into());
        }
        let items = match payload["format"].as_str() {
            Some("csv") => catalog::parse_csv(kind, content, "用户导入")?,
            Some("json") => catalog::parse_three_d(content, "用户导入")?,
            Some("legacy") => {
                use base64::Engine;
                let bytes = base64::engine::general_purpose::STANDARD
                    .decode(content)
                    .map_err(|_| "数据库内容编码无效")?;
                catalog::parse_legacy_database(&bytes)?
            }
            _ => return Err("不支持的硬件文件格式".into()),
        };
        if items.len() > 100_000 {
            return Err("单次导入超过十万条记录".into());
        }
        let mut unique = HashSet::new();
        let mut added = 0;
        let mut updated = 0;
        for item in &items {
            if !unique.insert(&item.id) {
                return Err(format!(
                    "文件中厂家和型号重复：{} {}",
                    item.manufacturer, item.model
                ));
            }
            let exists: bool = self
                .connection
                .query_row(
                    "SELECT EXISTS(SELECT 1 FROM hardware WHERE id=?1)",
                    [&item.id],
                    |row| row.get(0),
                )
                .map_err(|e| e.to_string())?;
            if exists {
                updated += 1;
            } else {
                added += 1;
            }
        }
        let token = uuid::Uuid::new_v4().to_string();
        self.pending_import = Some((token.clone(), items));
        Ok(json!({"token":token,"added":added,"updated":updated,"total":added+updated}))
    }
    fn commit(&mut self, payload: &Value) -> Result<Value> {
        let token = payload["token"].as_str().ok_or("缺少导入预览标识")?;
        let (expected, items) = self.pending_import.as_ref().ok_or("请重新预览后导入")?;
        if token != expected {
            return Err("导入预览已过期，请重新预览".into());
        }
        let backup = self.directory.join(format!(
            "导入前备份-{}.sqlite",
            Utc::now().format("%Y%m%d-%H%M%S-%f")
        ));
        self.connection
            .execute("VACUUM INTO ?1", [backup.to_string_lossy().as_ref()])
            .map_err(|e| format!("备份失败，未执行导入：{e}"))?;
        let tx = self.connection.transaction().map_err(|e| e.to_string())?;
        for item in items {
            upsert(&tx, item)?;
        }
        tx.commit().map_err(|e| e.to_string())?;
        let count = items.len();
        self.pending_import = None;
        Ok(json!({"imported":count,"backup":backup}))
    }
}
fn upsert(connection: &Connection, item: &Hardware) -> Result<()> {
    connection.execute("INSERT INTO hardware(id,kind,manufacturer,model,document) VALUES(?1,?2,?3,?4,?5) ON CONFLICT(id) DO UPDATE SET manufacturer=excluded.manufacturer,model=excluded.model,document=excluded.document",
        params![item.id,item.kind.key(),item.manufacturer,item.model,serde_json::to_string(item).map_err(|e|e.to_string())?]).map_err(|e|e.to_string())?;
    Ok(())
}
fn parse_project(value: &Value) -> Result<Project> {
    let project: Project =
        serde_json::from_value(value.clone()).map_err(|e| format!("方案格式无效：{e}"))?;
    project.validate()?;
    Ok(project)
}
