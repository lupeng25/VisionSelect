import { useCallback, useEffect, useRef, useState } from "react";
import { isTauri } from "@tauri-apps/api/core";
import {
  ArrowDownToLine,
  ArrowRight,
  Calculator,
  Check,
  ChevronDown,
  CircleHelp,
  Copy,
  Database,
  FileJson,
  FolderClosed,
  LayoutDashboard,
  LoaderCircle,
  Plus,
  Save,
  Search,
  Settings2,
  Trash2,
  Upload,
  X,
} from "lucide-react";
import { errorText, exportProject, request } from "./api";
import { fields, format, inputError } from "./format";
import {
  kinds,
  newProject,
  titles,
  type Bootstrap,
  type Evaluation,
  type Hardware,
  type Kind,
  type Project,
  type SavedProject,
} from "./types";
import { HardwareLibrary } from "./components/HardwareLibrary";
import { Workbench } from "./components/Workbench";
import { Calculator2D } from "./components/Calculator2D";
import { Modal } from "./components/Modal";
import { projectSignature } from "./projectState";
import { useTheme } from "./theme";
import { ThemePicker } from "./components/ThemePicker";

const DRAFT_KEY = "visionselect.rust-workbench.draft.v1";
export default function App() {
  const appearance = useTheme();
  const [bootstrap, setBootstrap] = useState<Bootstrap | null>(null);
  const [project, setProject] = useState<Project | null>(null);
  const [savedBody, setSavedBody] = useState("");
  const [route, setRoute] = useState<
    "workspace" | "calculator" | "catalog" | "projects"
  >("workspace");
  const [catalogKind, setCatalogKind] = useState<Kind | null>(null);
  const [evaluation, setEvaluation] = useState<Evaluation | null>(null);
  const [pending, setPending] = useState(false);
  const [connectionError, setConnectionError] = useState("");
  const [calculationError, setCalculationError] = useState("");
  const [notice, setNotice] = useState<{ text: string; error: boolean } | null>(
    null,
  );
  const [saving, setSaving] = useState(false);
  const [exportMenu, setExportMenu] = useState(false);
  const [comparing, setComparing] = useState(false);
  const [about, setAbout] = useState(false);
  const [confirm, setConfirm] = useState<{
    title: string;
    text: string;
    action: () => void;
  } | null>(null);
  const [projectSearch, setProjectSearch] = useState("");
  const input = useRef<HTMLInputElement>(null);
  const closeAllowed = useRef(false);
  const dirty = project !== null && projectSignature(project) !== savedBody;
  const validation = project
    ? inputError(project.parameters) ||
      (!project.name.trim() ? "请填写方案名称。" : null)
    : null;
  const notify = useCallback(
    (text: string, error = false) => setNotice({ text, error }),
    [],
  );
  async function refresh() {
    const data = await request<Bootstrap>("bootstrap");
    setBootstrap(data);
    return data;
  }
  useEffect(() => {
    let current = true;
    request<Bootstrap>("bootstrap")
      .then(async (data) => {
        let draft: Project | null = null;
        let saved = projectSignature(data.default_project);
        try {
          const text = localStorage.getItem(DRAFT_KEY);
          if (text) {
            const parsed = JSON.parse(text) as Project;
            await request("evaluate", { project: parsed });
            if (!inputError(parsed.parameters)) {
              draft = parsed;
              if (data.projects.some((p) => p.id === parsed.id))
                saved = projectSignature(
                  await request<Project>("load_project", { id: parsed.id }),
                );
            }
          }
        } catch {
          /* 损坏草稿不影响启动。 */
        }
        if (!current) return;
        setBootstrap(data);
        setProject(draft ?? data.default_project);
        setSavedBody(saved);
        if (draft)
          notify("已恢复上次的本地草稿。需要长期保留时请保存到“我的方案”。");
      })
      .catch((e) => {
        if (current) setConnectionError(errorText(e));
      });
    return () => {
      current = false;
    };
  }, [notify]);
  useEffect(() => {
    if (!project) return;
    if (!validation) {
      try {
        localStorage.setItem(DRAFT_KEY, JSON.stringify(project));
      } catch {
        notify("本地草稿存储不可用，请及时保存方案。", true);
      }
    }
    let current = true;
    if (validation) {
      setEvaluation(null);
      setPending(false);
      return;
    }
    setPending(true);
    setCalculationError("");
    const timeout = setTimeout(() => {
      request<Evaluation>("evaluate", { project })
        .then((result) => {
          if (current) setEvaluation(result);
        })
        .catch((e) => {
          if (current) {
            setCalculationError(errorText(e));
            setEvaluation(null);
          }
        })
        .finally(() => {
          if (current) setPending(false);
        });
    }, 100);
    return () => {
      current = false;
      clearTimeout(timeout);
    };
  }, [project, validation, notify]);
  useEffect(() => {
    // 桌面关闭由 Tauri 统一处理，避免浏览器卸载提示再次拦截已确认的关闭。
    if (isTauri()) return;
    const warn = (e: BeforeUnloadEvent) => {
      if (dirty) {
        e.preventDefault();
        e.returnValue = "";
      }
    };
    window.addEventListener("beforeunload", warn);
    return () => window.removeEventListener("beforeunload", warn);
  }, [dirty]);
  useEffect(() => {
    let unlisten: (() => void) | undefined;
    let disposed = false;
    void (async () => {
      if (!isTauri()) return;
      const { getCurrentWindow } = await import("@tauri-apps/api/window");
      const release = await getCurrentWindow().onCloseRequested(
        async (event) => {
          if (!dirty || closeAllowed.current) return;
          event.preventDefault();
          setConfirm({
            title: "关闭当前方案？",
            text: "当前修改尚未保存到方案库。关闭后可从本地草稿恢复；建议先保存。",
            action: () => {
              closeAllowed.current = true;
              void getCurrentWindow()
                .close()
                .catch((e) => {
                  closeAllowed.current = false;
                  notify("关闭窗口失败：" + errorText(e), true);
                });
            },
          });
        },
      );
      if (disposed) release();
      else unlisten = release;
    })().catch((e) => notify("关闭事件初始化失败：" + errorText(e), true));
    return () => {
      disposed = true;
      unlisten?.();
    };
  }, [dirty, notify]);
  const guarded = (action: () => void) =>
    dirty
      ? setConfirm({
          title: "离开当前方案？",
          text: "当前有未保存修改。继续操作会替换工作台内容，请先保存需要保留的方案。",
          action,
        })
      : action();
  async function save() {
    if (!project || validation) return;
    setSaving(true);
    const snapshot = project;
    try {
      const result = await request<{ projects: SavedProject[] }>(
        "save_project",
        { project: snapshot },
      );
      setSavedBody(projectSignature(snapshot));
      setBootstrap((b) => (b ? { ...b, projects: result.projects } : b));
      notify("方案已保存。设备参数以快照保留。");
    } catch (e) {
      notify(errorText(e), true);
    } finally {
      setSaving(false);
    }
  }
  async function open(id: string) {
    try {
      const loaded = await request<Project>("load_project", { id });
      setProject(loaded);
      setSavedBody(projectSignature(loaded));
      setRoute("workspace");
      setEvaluation(null);
    } catch (e) {
      notify(errorText(e), true);
    }
  }
  async function importScheme(file: File) {
    try {
      if (file.size > 4 * 1024 * 1024)
        throw new Error("方案文件超过 4 MB 限制。");
      const content = await file.text();
      const result = await request<{
        project: Project;
        projects: SavedProject[];
      }>("import_project", { content });
      setProject(result.project);
      setSavedBody(projectSignature(result.project));
      setBootstrap((b) => (b ? { ...b, projects: result.projects } : b));
      setRoute("workspace");
      notify("已导入为独立方案，不覆盖已有方案。");
    } catch (e) {
      notify(errorText(e), true);
    }
  }
  function choose(hardware: Hardware) {
    if (!project) return;
    const items = [...project.hardware];
    items[kinds.indexOf(hardware.kind)] = hardware;
    setProject({
      ...project,
      mode: hardware.kind === "three_d" ? "scanning" : "imaging",
      hardware: items,
      parameters: {
        ...project.parameters,
        ...(hardware.kind === "lens" ? { dof_confirmed: false } : {}),
      },
    });
    setCatalogKind(null);
    setRoute("workspace");
    notify("已加入 " + hardware.model + "，正在按当前条件校核。");
  }
  async function download(type: "project" | "report") {
    if (!project || validation) return;
    setExportMenu(false);
    try {
      if (await exportProject(project, type))
        notify(
          type === "project"
            ? "方案文件已导出。"
            : "校核记录已导出，可在浏览器中打开并打印为 PDF。",
        );
    } catch (e) {
      notify(errorText(e), true);
    }
  }
  if (!bootstrap || !project)
    return (
      <div className="boot-screen">
        <img src="/brand.svg" alt="" />
        <h1>VisionSelect</h1>
        {connectionError ? (
          <>
            <p role="alert">{connectionError}</p>
            <p>请确认桌面应用正常启动；浏览器开发模式需同时启动 Rust API。</p>
            <button className="primary" onClick={() => location.reload()}>
              重新连接
            </button>
          </>
        ) : (
          <>
            <LoaderCircle className="spin" size={22} />
            <p>准备你的工程工作台…</p>
          </>
        )}
      </div>
    );
  const total = Object.values(bootstrap.counts).reduce((a, b) => a + b, 0);
  const projects = bootstrap.projects.filter((p) =>
    p.name.toLowerCase().includes(projectSearch.toLowerCase()),
  );
  return (
    <div className="application">
      <aside className="sidebar">
        <a
          className="brand"
          aria-label="返回方案工作台"
          title="返回方案工作台"
          href="#"
          onClick={(e) => {
            e.preventDefault();
            setRoute("workspace");
          }}
        >
          <img src="/brand.svg" alt="" />
          <span>
            VisionSelect<small>视觉选型助手</small>
          </span>
        </a>
        <div className="sidebar-label">工作空间</div>
        <nav>
          {[
            {
              route: "workspace" as const,
              title: "方案工作台",
              icon: LayoutDashboard,
            },
            {
              route: "calculator" as const,
              title: "2D 选型计算",
              icon: Calculator,
            },
            { route: "catalog" as const, title: "硬件资料库", icon: Database },
            {
              route: "projects" as const,
              title: "我的方案",
              icon: FolderClosed,
            },
          ].map((item) => (
            <button
              key={item.route}
              className={route === item.route ? "active" : ""}
              title={item.title}
              onClick={() => setRoute(item.route)}
            >
              <item.icon size={18} strokeWidth={1.7} />
              <span>{item.title}</span>
              {item.route === "projects" && (
                <small>{bootstrap.projects.length}</small>
              )}
            </button>
          ))}
        </nav>
        <div className="sidebar-bottom">
          <div className="catalog-status">
            <span className="online-dot" />
            <div>
              <strong>{format(total, 0)} 条硬件资料</strong>
              <small>本地数据 · 随时可用</small>
            </div>
          </div>
          <ThemePicker {...appearance} />
          <button
            className="sidebar-help"
            aria-label="工作台说明"
            title="工作台说明"
            onClick={() => setAbout(true)}
          >
            <CircleHelp size={17} />
            <span>工作台说明</span>
            <small>3.0</small>
          </button>
        </div>
      </aside>
      <main className="main-shell">
        <header className="app-header">
          {route === "calculator" ? (
            <div className="project-breadcrumb">
              <Calculator size={17} />
              <span>独立计算</span>
              <span>/</span>
              <strong>2D 参数推算</strong>
              <span className="save-state">自动保留草稿</span>
            </div>
          ) : (
            <div className="project-breadcrumb">
              <FolderClosed size={16} />
              <span>工程方案</span>
              <span>/</span>
              <input
                aria-label="方案名称"
                maxLength={100}
                value={project.name}
                onChange={(e) =>
                  setProject({ ...project, name: e.target.value })
                }
              />
              <span className={"save-state" + (dirty ? " dirty" : "")}>
                {dirty
                  ? "未保存"
                  : bootstrap.projects.some((p) => p.id === project.id)
                    ? "已保存"
                    : "草稿"}
              </span>
            </div>
          )}
          {route === "calculator" ? (
            <button className="secondary" onClick={() => setRoute("workspace")}>
              返回工作台
              <ArrowRight size={15} />
            </button>
          ) : (
            <div className="header-actions">
              <button
                className="ghost"
                title="新建方案"
                aria-label="新建方案"
                onClick={() =>
                  guarded(() => {
                    const next = newProject();
                    setProject(next);
                    setSavedBody(projectSignature(next));
                    setRoute("workspace");
                  })
                }
              >
                <Plus size={16} />
                <span>新建</span>
              </button>
              <button
                className="secondary"
                disabled={!!validation || saving}
                onClick={() => void save()}
              >
                {saving ? (
                  <LoaderCircle size={15} className="spin" />
                ) : (
                  <Save size={15} />
                )}
                保存方案
              </button>
              <div className="export-control">
                <button
                  className="primary"
                  disabled={!!validation || pending || !!calculationError}
                  onClick={() => setExportMenu(!exportMenu)}
                >
                  <ArrowDownToLine size={15} />
                  导出
                  <ChevronDown size={13} />
                </button>
                {exportMenu && (
                  <div className="export-menu">
                    <button onClick={() => void download("project")}>
                      <FileJson size={16} />
                      工程方案 JSON
                    </button>
                    <button onClick={() => void download("report")}>
                      <ArrowDownToLine size={16} />
                      工程校核记录
                    </button>
                  </div>
                )}
              </div>
            </div>
          )}
        </header>
        {notice && (
          <div
            className={"notice " + (notice.error ? "error" : "")}
            role={notice.error ? "alert" : "status"}
          >
            <span>
              {notice.error ? <CircleHelp size={16} /> : <Check size={16} />}
              {notice.text}
            </span>
            <button
              className="icon-button"
              onClick={() => setNotice(null)}
              aria-label="关闭提示"
            >
              <X size={15} />
            </button>
          </div>
        )}
        {route === "workspace" && (
          <Workbench
            project={project}
            evaluation={evaluation}
            pending={pending}
            validation={validation || calculationError}
            onChange={setProject}
            onChoose={setCatalogKind}
            onReference={() => {
              if (validation) {
                notify(validation, true);
                return;
              }
              const reference = {
                name: project.name,
                mode: project.mode,
                parameters: structuredClone(project.parameters),
                hardware: structuredClone(project.hardware),
              };
              setProject({ ...project, reference });
              notify("参考方案已固定，后续调整不会改变参考快照。");
            }}
            onCompare={() => setComparing(true)}
          />
        )}
        {route === "calculator" && (
          <Calculator2D
            project={project}
            onApply={(patch) => {
              setProject({
                ...project,
                mode: "imaging",
                parameters: { ...project.parameters, ...patch },
              });
              setRoute("workspace");
              notify(
                "计算参数已应用到二维方案，可继续选择设备并查看校核结果。",
              );
            }}
          />
        )}
        {route === "catalog" && (
          <div className="page catalog-page">
            <div className="page-title">
              <div>
                <span className="eyebrow">HARDWARE LIBRARY</span>
                <h1>硬件资料库</h1>
                <p>从真实规格出发，为你的方案找到合适的设备。</p>
              </div>
              <span className="data-badge">
                <Database size={15} />
                {format(total, 0)} 条资料
              </span>
            </div>
            <HardwareLibrary
              initialKind="camera"
              onChoose={choose}
              onNotice={notify}
              onImported={() => void refresh()}
            />
          </div>
        )}
        {route === "projects" && (
          <div className="page projects-page">
            <div className="page-title">
              <div>
                <span className="eyebrow">YOUR PROJECTS</span>
                <h1>我的方案</h1>
                <p>保留设备组合、现场参数和判断依据，随时继续。</p>
              </div>
              <button
                className="secondary"
                onClick={() => guarded(() => input.current?.click())}
              >
                <Upload size={15} />
                导入方案
              </button>
            </div>
            <label className="search-box project-search">
              <Search size={17} />
              <input
                value={projectSearch}
                onChange={(e) => setProjectSearch(e.target.value)}
                placeholder="搜索方案名称…"
                aria-label="搜索方案"
              />
            </label>
            <div className="project-grid">
              {projects.map((p) => (
                <article className="project-card" key={p.id}>
                  <div>
                    <span className="project-card-icon">
                      <FolderClosed size={21} />
                    </span>
                    <button
                      className="icon-button"
                      aria-label={"删除方案 " + p.name}
                      onClick={() =>
                        setConfirm({
                          title: "删除这个已保存方案？",
                          text:
                            p.name +
                            " 将从方案库删除。当前工作台与导出文件不受影响。",
                          action: () => {
                            void request<{ projects: SavedProject[] }>(
                              "delete_project",
                              { id: p.id },
                            )
                              .then((result) =>
                                setBootstrap({
                                  ...bootstrap,
                                  projects: result.projects,
                                }),
                              )
                              .catch((e) => notify(errorText(e), true));
                          },
                        })
                      }
                    >
                      <Trash2 size={15} />
                    </button>
                  </div>
                  <h3>{p.name}</h3>
                  <p>
                    更新于{" "}
                    {new Date(p.updated_at).toLocaleString("zh-CN", {
                      hour12: false,
                    })}
                  </p>
                  <button onClick={() => guarded(() => void open(p.id))}>
                    继续编辑
                    <ArrowRight size={16} />
                  </button>
                </article>
              ))}
            </div>
            {!projects.length && (
              <div className="empty-state projects-empty">
                <FolderClosed size={44} strokeWidth={1} />
                <h2>
                  {projectSearch
                    ? "没有找到这个方案"
                    : "把你的第一个方案留在这里"}
                </h2>
                <p>在工作台组合设备、调整参数，然后点击“保存方案”。</p>
                <button
                  className="primary"
                  onClick={() => setRoute("workspace")}
                >
                  前往工作台
                  <ArrowRight size={16} />
                </button>
              </div>
            )}
          </div>
        )}
        <footer className="app-footer">
          <span>
            <span className="online-dot" />
            本地工作台
          </span>
          <span>
            {route === "calculator" ? (
              "独立计算 · 参数按需应用"
            ) : (
              <>
                {project.mode === "imaging" ? "二维成像" : "三维采样"} ·{" "}
                {project.parameters.measured && project.mode === "imaging"
                  ? "实测视场"
                  : "目录规格校核"}
              </>
            )}
          </span>
          <span>VisionSelect 3.0</span>
        </footer>
      </main>
      <input
        ref={input}
        className="hidden"
        type="file"
        accept=".json,.vswork"
        aria-label="导入方案文件"
        onChange={(e) => {
          const file = e.target.files?.[0];
          if (file) void importScheme(file);
          e.target.value = "";
        }}
      />
      {catalogKind && (
        <Modal
          title={"选择" + titles[catalogKind]}
          subtitle="查看规格后加入当前方案。替换设备不会修改原始资料。"
          wide
          onClose={() => setCatalogKind(null)}
        >
          <HardwareLibrary
            initialKind={catalogKind}
            onChoose={choose}
            onNotice={notify}
            onImported={() => void refresh()}
          />
        </Modal>
      )}
      {confirm && (
        <Modal title={confirm.title} onClose={() => setConfirm(null)}>
          <div className="confirm-body">
            <p>{confirm.text}</p>
            <div className="dialog-actions">
              <button className="secondary" onClick={() => setConfirm(null)}>
                返回
              </button>
              <button
                className="primary"
                onClick={() => {
                  const action = confirm.action;
                  setConfirm(null);
                  action();
                }}
              >
                继续操作
              </button>
            </div>
          </div>
        </Modal>
      )}
      {comparing && project.reference && (
        <Modal
          title="方案对照"
          subtitle="参考快照固定不变。着色行标记当前方案的变化。"
          wide
          onClose={() => setComparing(false)}
        >
          <Comparison project={project} evaluation={evaluation} />
        </Modal>
      )}
      {about && (
        <Modal
          title="围绕方案，做工程判断"
          subtitle="VisionSelect · Rust 核心 / React 界面"
          onClose={() => setAbout(false)}
        >
          <div className="about-body">
            <Settings2 size={26} />
            <p>
              选择相机、镜头和光源，调整工件尺寸与现场采集条件，逐项查看通过、冲突和待确认状态。
            </p>
            <p>
              方案保存设备参数快照；硬件库更新不会悄悄改变已保存方案。固定焦距采用近轴估算，也可以输入实测视场。
            </p>
            <p>
              采样不是测量精度保证。三维的重复精度、分辨率和量程不能相互代替，照明效果需实拍确认。
            </p>
            <h3>本地数据目录</h3>
            <code>{bootstrap.data_directory}</code>
            <p>
              原始硬件数据保持不变。导入旧库或新资料时先预览合并，并在提交前自动备份。
            </p>
          </div>
        </Modal>
      )}
    </div>
  );
}
function Comparison({
  project,
  evaluation,
}: {
  project: Project;
  evaluation: Evaluation | null;
}) {
  const ref = project.reference!;
  const [referenceEvaluation, setReferenceEvaluation] =
    useState<Evaluation | null>(null);
  const [error, setError] = useState("");
  useEffect(() => {
    let current = true;
    request<Evaluation>("evaluate", {
      project: { ...project, ...ref, reference: null },
    })
      .then((result) => {
        if (current) setReferenceEvaluation(result);
      })
      .catch((e) => {
        if (current) setError(errorText(e));
      });
    return () => {
      current = false;
    };
  }, [project, ref]);
  const groups = [...new Set([ref.mode, project.mode])].flatMap((mode) =>
    mode === "imaging"
      ? [
          "target",
          "acquisition",
          "installation",
          ...(project.parameters.measured || ref.parameters.measured
            ? ["measured"]
            : []),
        ]
      : [
          "scanning",
          "trigger",
          ...(project.parameters.trigger === "encoder" ||
          ref.parameters.trigger === "encoder"
            ? ["encoder"]
            : []),
        ],
  );
  const translated = (value: unknown) =>
    typeof value === "boolean"
      ? value
        ? "是"
        : "否"
      : ({ free: "自由运行", external: "外部触发", encoder: "编码器" }[
          String(value)
        ] ?? String(value || "未设置"));
  const resultRows =
    project.mode === ref.mode
      ? (evaluation?.metrics ?? []).map((m) => ({
          title: m.label,
          a:
            format(
              referenceEvaluation?.metrics.find((r) => r.key === m.key)?.value,
              3,
            ) +
            " " +
            m.unit,
          b: format(m.value, 3) + " " + m.unit,
        }))
      : [];
  if (evaluation && referenceEvaluation)
    resultRows.unshift({
      title: "校核状态",
      a:
        referenceEvaluation.failed +
        " 冲突 / " +
        referenceEvaluation.unknown +
        " 待确认",
      b: evaluation.failed + " 冲突 / " + evaluation.unknown + " 待确认",
    });
  const rows: { title: string; a: string; b: string }[] = [
    ...resultRows,
    {
      title: "工作模式",
      a: ref.mode === "imaging" ? "二维成像" : "三维采样",
      b: project.mode === "imaging" ? "二维成像" : "三维采样",
    },
    ...kinds.map((k, i) => ({
      title: titles[k],
      a: ref.hardware[i]?.model ?? "未选择",
      b: project.hardware[i]?.model ?? "未选择",
    })),
    ...groups
      .flatMap((g) => fields[g])
      .map((f) => ({
        title: f.title,
        a: format(ref.parameters[f.key] as number | null, 3) + " " + f.unit,
        b: format(project.parameters[f.key] as number | null, 3) + " " + f.unit,
      })),
    ...(["pixel_format", "measured", "dof_confirmed", "trigger"] as const).map(
      (k, i) => ({
        title: ["传输格式", "实测视场", "景深条件确认", "触发方式"][i],
        a: translated(ref.parameters[k]),
        b: translated(project.parameters[k]),
      }),
    ),
  ];
  return (
    <div className="comparison-body">
      <div className="comparison-intro">
        <Copy size={18} />
        <span>参考：{ref.name}</span>
        <ArrowRight size={16} />
        <span>当前：{project.name}</span>
      </div>
      {error && <p role="alert">{error}</p>}
      <table className="comparison-table">
        <thead>
          <tr>
            <th>比较项目</th>
            <th>参考方案</th>
            <th>当前方案</th>
          </tr>
        </thead>
        <tbody>
          {rows.map((row, i) => (
            <tr key={i} className={row.a !== row.b ? "changed" : ""}>
              <th>{row.title}</th>
              <td>{row.a}</td>
              <td>{row.b}</td>
            </tr>
          ))}
        </tbody>
      </table>
    </div>
  );
}
