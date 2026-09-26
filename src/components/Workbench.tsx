import { useState } from "react";
import {
  ArrowDown,
  ArrowUpRight,
  Check,
  CheckCircle2,
  ChevronDown,
  CircleHelp,
  Copy,
  Crosshair,
  Layers2,
  Plus,
  RefreshCw,
  ScanLine,
  SlidersHorizontal,
  Trash2,
  TriangleAlert,
} from "lucide-react";
import {
  kinds,
  titles,
  statusText,
  type Evaluation,
  type Kind,
  type Parameters,
  type Project,
} from "../types";
import { fields, format, summary, type Field } from "../format";
import { hardwareIcons } from "./HardwareLibrary";

function FieldInput({
  field,
  parameters,
  onChange,
}: {
  field: Field;
  parameters: Parameters;
  onChange: (key: keyof Parameters, value: number | null) => void;
}) {
  const value = parameters[field.key] as number | null;
  const valid =
    (value === null && field.key === "scan_distance") ||
    (typeof value === "number" &&
      Number.isFinite(value) &&
      value >= field.min &&
      value <= (field.max ?? 1e8));
  return (
    <label className={"parameter-field" + (!valid ? " invalid" : "")}>
      <span>{field.title}</span>
      <div>
        <input
          aria-label={field.title}
          type="number"
          min={field.min}
          max={field.max ?? 1e8}
          step={field.step}
          value={value !== null && Number.isFinite(value) ? value : ""}
          placeholder={field.key === "scan_distance" ? "未确定" : ""}
          onChange={(e) =>
            onChange(
              field.key,
              e.target.value === ""
                ? field.key === "scan_distance"
                  ? null
                  : NaN
                : e.target.valueAsNumber,
            )
          }
        />
        <span className="unit">{field.unit}</span>
      </div>
    </label>
  );
}
function Scene({
  project,
  evaluation,
}: {
  project: Project;
  evaluation: Evaluation | null;
}) {
  const p = project.parameters;
  const known = evaluation?.fov_x != null && evaluation.fov_y != null;
  const w = Math.max(0.01, Number.isFinite(p.width) ? p.width : 20);
  const h = Math.max(0.01, Number.isFinite(p.height) ? p.height : 20);
  const margin = Number.isFinite(p.margin) ? p.margin : 0;
  const fw = evaluation?.fov_x ?? w + 2 * margin;
  const fh = evaluation?.fov_y ?? h + 2 * margin;
  const scale = Math.min(
    270 / Math.max(fw, w + margin * 2),
    125 / Math.max(fh, h + margin * 2),
  );
  const fits = fw >= w + 2 * margin && fh >= h + 2 * margin;
  return (
    <div className="scene">
      <div className="scene-heading">
        <span>
          <Crosshair size={14} />
          {project.mode === "imaging" ? "成像平面" : "运动采样"}
        </span>
        <span className="scene-caption">
          {project.mode === "imaging"
            ? known
              ? "按当前参数绘制"
              : "等待设备参数"
            : "结构示意 · 非等比例"}
        </span>
      </div>
      {project.mode === "imaging" ? (
        <svg
          viewBox="0 0 560 180"
          role="img"
          aria-label="工件、装夹余量与实际视场示意"
        >
          <defs>
            <pattern
              id="grid"
              width="16"
              height="16"
              patternUnits="userSpaceOnUse"
            >
              <circle cx="1" cy="1" r=".7" fill="var(--diagram-grid)" />
            </pattern>
          </defs>
          <rect width="560" height="180" fill="url(#grid)" />
          <path
            d="M280 8V166M95 87H465"
            stroke="var(--diagram-axis)"
            strokeDasharray="4 5"
          />
          <rect
            x={280 - (fw * scale) / 2}
            y={87 - (fh * scale) / 2}
            width={fw * scale}
            height={fh * scale}
            rx="3"
            fill={fits ? "var(--diagram-fov)" : "var(--danger-soft)"}
            fillOpacity=".6"
            stroke={known && !fits ? "var(--danger)" : "var(--accent)"}
            strokeWidth="1.5"
            strokeDasharray={known ? undefined : "5 4"}
          />
          <rect
            x={280 - ((w + 2 * margin) * scale) / 2}
            y={87 - ((h + 2 * margin) * scale) / 2}
            width={(w + 2 * margin) * scale}
            height={(h + 2 * margin) * scale}
            stroke="var(--diagram-margin)"
            fill="none"
            strokeDasharray="4 4"
          />
          <rect
            x={280 - (w * scale) / 2}
            y={87 - (h * scale) / 2}
            width={w * scale}
            height={h * scale}
            rx="2"
            fill="var(--diagram-object)"
            stroke="var(--diagram-object-line)"
          />
          <path d="M274 87h12M280 81v12" stroke="var(--diagram-object-line)" />
          <text
            x="280"
            y="168"
            textAnchor="middle"
            fill="var(--muted)"
            fontSize="11"
          >
            工件 {format(w)} × {format(h)} mm
          </text>
          <text x="18" y="24" fill="var(--muted)" fontSize="11">
            Y
          </text>
          <text x="533" y="166" fill="var(--muted)" fontSize="11">
            X
          </text>
          <text x="18" y="164" fill="var(--muted)" fontSize="11">
            {known
              ? "实际视场 " + format(fw) + " × " + format(fh) + " mm"
              : "虚线表示所需视场"}
          </text>
        </svg>
      ) : (
        <svg viewBox="0 0 560 180" role="img" aria-label="轮廓扫描与轴运动示意">
          <defs>
            <pattern
              id="scanGrid"
              width="16"
              height="16"
              patternUnits="userSpaceOnUse"
            >
              <circle cx="1" cy="1" r=".7" fill="var(--diagram-grid)" />
            </pattern>
          </defs>
          <rect width="560" height="180" fill="url(#scanGrid)" />
          <path
            d="M132 105l135-58 158 63-135 56z"
            fill="var(--diagram-plane)"
            stroke="var(--diagram-margin)"
          />
          {Array.from({ length: 10 }, (_, i) => (
            <path
              key={i}
              d={"M" + (145 + i * 13) + " " + (100 - i * 5.6) + "l158 63"}
              stroke="var(--diagram-line)"
            />
          ))}
          <path
            d="M206 76l158 63"
            stroke="var(--diagram-laser)"
            strokeWidth="3"
          />
          <path
            d="M280 39l-74 37m74-37 84 100"
            stroke="var(--diagram-laser)"
            strokeDasharray="4 3"
          />
          <rect
            x="257"
            y="14"
            width="46"
            height="30"
            rx="5"
            fill="var(--diagram-camera)"
          />
          <circle cx="280" cy="33" r="6" fill="var(--diagram-lens)" />
          <text x="32" y="166" fill="var(--muted)" fontSize="11">
            扫描长度 {format(p.scan_length)} mm / 目标间距上限{" "}
            {format(p.interval, 3)} mm
          </text>
        </svg>
      )}
    </div>
  );
}
export function Workbench({
  project,
  evaluation,
  pending,
  validation,
  onChange,
  onChoose,
  onReference,
  onCompare,
}: {
  project: Project;
  evaluation: Evaluation | null;
  pending: boolean;
  validation: string | null;
  onChange: (project: Project) => void;
  onChoose: (kind: Kind) => void;
  onReference: () => void;
  onCompare: () => void;
}) {
  const [parameterTab, setParameterTab] = useState("target");
  const [issuesOnly, setIssuesOnly] = useState(true);
  const p = project.parameters;
  const parameter = (
    key: keyof Parameters,
    value: number | null | boolean | string,
  ) =>
    onChange({
      ...project,
      parameters: {
        ...p,
        [key]: value,
        ...([
          "distance",
          "measured",
          "measured_width",
          "measured_height",
        ].includes(key)
          ? { dof_confirmed: false }
          : {}),
      },
    });
  const groups =
    project.mode === "imaging"
      ? parameterTab === "target"
        ? ["target"]
        : ["acquisition"]
      : parameterTab === "target"
        ? ["scanning"]
        : ["trigger"];
  const visibleChecks =
    evaluation?.checks.filter(
      (c) => !issuesOnly || c.status === "unknown" || c.status === "failed",
    ) ?? [];
  const count = project.hardware.filter(
    (h, i) => h && (project.mode === "imaging" ? i < 3 : i === 3),
  ).length;
  return (
    <div className="workbench">
      <div className="workspace-intro">
        <div>
          <span className="eyebrow">ENGINEERING WORKSPACE</span>
          <h1>
            方案工作台
            <span className="title-dot" />
          </h1>
          <p>让硬件、现场参数和工程判断，保持在同一个视野。</p>
        </div>
        <div className="mode-switch" role="group" aria-label="选型模式">
          <button
            className={project.mode === "imaging" ? "active" : ""}
            onClick={() => onChange({ ...project, mode: "imaging" })}
          >
            <Layers2 size={15} />
            二维成像
          </button>
          <button
            className={project.mode === "scanning" ? "active" : ""}
            onClick={() => onChange({ ...project, mode: "scanning" })}
          >
            <ScanLine size={15} />
            三维采样
          </button>
        </div>
      </div>
      <div className="workspace-grid">
        <section className="equipment-column">
          <div className="section-label">
            <span>硬件组合</span>
            <span>
              {count} / {project.mode === "imaging" ? 3 : 1}
            </span>
          </div>
          <div className="equipment-chain">
            {kinds
              .filter((k) =>
                project.mode === "imaging" ? k !== "three_d" : k === "three_d",
              )
              .map((kind, index) => {
                const h = project.hardware[kinds.indexOf(kind)];
                const Icon = hardwareIcons[kind];
                return (
                  <div key={kind}>
                    <article
                      className={"equipment-card" + (h ? " populated" : "")}
                    >
                      <header>
                        <span className="hardware-icon">
                          <Icon size={22} strokeWidth={1.6} />
                        </span>
                        <div>
                          <span className="small-index">
                            {String(index + 1).padStart(2, "0")}
                          </span>
                          <h3>{titles[kind]}</h3>
                        </div>
                        {h && (
                          <button
                            className="icon-button remove"
                            aria-label={"移除" + titles[kind]}
                            onClick={() => {
                              const hardware = [...project.hardware];
                              hardware[kinds.indexOf(kind)] = null;
                              onChange({ ...project, hardware });
                            }}
                          >
                            <Trash2 size={13} />
                          </button>
                        )}
                      </header>
                      {h ? (
                        <>
                          <p className="manufacturer">{h.manufacturer}</p>
                          <strong className="hardware-model" title={h.model}>
                            {h.model}
                          </strong>
                          <p className="hardware-summary">{summary(h)}</p>
                        </>
                      ) : (
                        <p className="equipment-empty">
                          从硬件库选择一台
                          {kind === "lens"
                            ? "镜头"
                            : kind === "light"
                              ? "光源"
                              : "相机"}
                          ，建立你的设备组合。
                        </p>
                      )}
                      <button
                        className={
                          h
                            ? "equipment-action selected-action"
                            : "equipment-action"
                        }
                        onClick={() => onChoose(kind)}
                      >
                        {h ? (
                          <>
                            <RefreshCw size={13} />
                            替换设备
                          </>
                        ) : (
                          <>
                            <Plus size={15} />
                            选择{titles[kind]}
                          </>
                        )}
                        <ArrowUpRight size={14} />
                      </button>
                    </article>
                    {project.mode === "imaging" && index < 2 && (
                      <div className="chain-link">
                        <ArrowDown size={12} />
                      </div>
                    )}
                  </div>
                );
              })}
          </div>
          <div className="reference-card">
            <div className="reference-icon">
              <Copy size={17} />
            </div>
            <h3>方案参考</h3>
            <p>保存一份当前快照，查看调整前后的差异。</p>
            <div>
              <button onClick={onReference}>
                {project.reference ? "更新参考" : "设为参考"}
              </button>
              <button disabled={!project.reference} onClick={onCompare}>
                对比方案 <ArrowUpRight size={13} />
              </button>
            </div>
          </div>
        </section>
        <section className="design-column">
          <div className="section-label">
            <span>成像与现场参数</span>
            <span className="live-indicator">
              <i />
              {pending ? "正在校核" : "即时联动"}
            </span>
          </div>
          <div className="imaging-card">
            <Scene project={project} evaluation={evaluation} />
            <div className="scene-note">
              <CircleHelp size={12} />
              <span>
                {evaluation?.model_note || "选择设备后查看成像模型与适用条件。"}
              </span>
            </div>
          </div>
          <div
            className={
              "metric-row" +
              (project.mode === "scanning" ? " scanning-metrics" : "")
            }
          >
            {(
              evaluation?.metrics.filter(
                (m) => m.key !== "fov_y" && m.key !== "storage",
              ) ?? [
                { key: "first", label: "实际视场", value: null, unit: "mm" },
                {
                  key: "second",
                  label: "物方像素",
                  value: null,
                  unit: "μm/px",
                },
                { key: "third", label: "传输载荷", value: null, unit: "MB/s" },
              ]
            ).map((m) => (
              <div className="metric" key={m.key}>
                <span>{m.key === "fov_x" ? "实际视场" : m.label}</span>
                <div>
                  <strong data-testid={"metric-" + m.key}>
                    {format(
                      m.value,
                      m.key === "actual_interval"
                        ? 6
                        : m.key === "pitch"
                          ? 3
                          : 2,
                    )}
                    {m.key === "fov_x" ? " × " + format(evaluation?.fov_y) : ""}
                  </strong>
                  <small>{m.unit}</small>
                </div>
              </div>
            ))}
          </div>
          <div className="parameter-card">
            <div className="parameter-tabs">
              <button
                className={parameterTab === "target" ? "active" : ""}
                onClick={() => setParameterTab("target")}
              >
                <Crosshair size={14} />
                {project.mode === "imaging" ? "成像目标" : "覆盖与采样"}
              </button>
              <button
                className={parameterTab === "acquisition" ? "active" : ""}
                onClick={() => setParameterTab("acquisition")}
              >
                <SlidersHorizontal size={14} />
                {project.mode === "imaging" ? "采集与校准" : "触发与曝光"}
              </button>
            </div>
            <div className="parameter-body" key={project.mode + parameterTab}>
              {project.mode === "scanning" &&
                parameterTab === "acquisition" && (
                  <label className="select-field">
                    触发方式
                    <select
                      value={p.trigger}
                      onChange={(e) => parameter("trigger", e.target.value)}
                    >
                      <option value="free">自由运行</option>
                      <option value="external">外部触发</option>
                      <option value="encoder">编码器</option>
                    </select>
                  </label>
                )}
              <div className="parameter-grid">
                {groups
                  .flatMap((group) => fields[group])
                  .filter(
                    (f) =>
                      !(
                        project.mode === "scanning" &&
                        p.trigger === "encoder" &&
                        f.key === "rate"
                      ),
                  )
                  .map((f) => (
                    <FieldInput
                      key={f.key}
                      field={f}
                      parameters={p}
                      onChange={parameter}
                    />
                  ))}
              </div>
              {project.mode === "imaging" && parameterTab === "acquisition" && (
                <>
                  <label className="select-field">
                    实际传输像素格式
                    <select
                      aria-label="实际传输像素格式"
                      value={p.pixel_format}
                      onChange={(e) =>
                        parameter("pixel_format", e.target.value)
                      }
                    >
                      <option value="">使用目录值 / 未公开</option>
                      {[
                        "Mono8",
                        "Mono10",
                        "Mono10p",
                        "Mono12",
                        "Mono12p",
                        "Mono16",
                        "BayerRG8",
                        "BayerBG8",
                        "RGB8",
                        "BGR8",
                      ].map((v) => (
                        <option key={v}>{v}</option>
                      ))}
                    </select>
                  </label>
                  <label className="checkbox-line">
                    <input
                      type="checkbox"
                      checked={p.measured}
                      onChange={(e) => parameter("measured", e.target.checked)}
                    />
                    使用实测视场覆盖估算
                  </label>
                  {p.measured && (
                    <div className="parameter-grid">
                      {fields.measured.map((f) => (
                        <FieldInput
                          key={f.key}
                          field={f}
                          parameters={p}
                          onChange={parameter}
                        />
                      ))}
                    </div>
                  )}
                  <label className="checkbox-line">
                    <input
                      type="checkbox"
                      checked={p.dof_confirmed}
                      onChange={(e) =>
                        parameter("dof_confirmed", e.target.checked)
                      }
                    />
                    已确认目录景深适用于当前倍率与光圈
                  </label>
                </>
              )}
              {project.mode === "scanning" &&
                parameterTab === "acquisition" &&
                p.trigger === "encoder" && (
                  <>
                    <h4>编码器标定</h4>
                    <div className="parameter-grid">
                      {fields.encoder.map((f) => (
                        <FieldInput
                          key={f.key}
                          field={f}
                          parameters={p}
                          onChange={parameter}
                        />
                      ))}
                    </div>
                  </>
                )}
              <p className="parameter-footnote">
                {project.mode === "imaging"
                  ? "像素当量表示每个像素覆盖的物方尺寸，不等于测量精度。"
                  : "目标间距是允许的上限；预计数量按实际触发设置计算。采样裕量系数只影响建议裕量，不代表相机频率余量。"}
              </p>
            </div>
          </div>
        </section>
        <section className={"inspection-column" + (pending ? " pending" : "")}>
          <div className="section-label">
            <span>工程校核</span>
            <span>独立判断每一项</span>
          </div>
          <div
            className={
              "inspection-summary " +
              (evaluation?.failed
                ? "has-failure"
                : evaluation?.unknown
                  ? "has-unknown"
                  : "")
            }
          >
            <span className="summary-icon">
              {evaluation?.failed ? (
                <TriangleAlert size={21} />
              ) : !evaluation || evaluation.unknown ? (
                <CircleHelp size={21} />
              ) : (
                <CheckCircle2 size={21} />
              )}
            </span>
            <h2>
              {validation
                ? "请完成参数输入"
                : !evaluation
                  ? "准备校核"
                  : evaluation.failed
                    ? evaluation.failed + " 项冲突待处理"
                    : evaluation.unknown
                      ? evaluation.unknown + " 项需要确认"
                      : "当前计算项通过"}
            </h2>
            <p>{validation || "计算结果随参数更新，未公开规格保留待确认。"}</p>
            <div className="check-counts">
              <span>
                <i className="passed" />
                {evaluation?.passed ?? 0} 通过
              </span>
              <span>
                <i className="failed" />
                {evaluation?.failed ?? 0} 冲突
              </span>
              <span>
                <i className="unknown" />
                {evaluation?.unknown ?? 0} 待确认
              </span>
            </div>
          </div>
          <label className="checkbox-line inspection-filter">
            <input
              type="checkbox"
              checked={issuesOnly}
              onChange={(e) => setIssuesOnly(e.target.checked)}
            />
            仅查看冲突和待确认
          </label>
          <div className="check-list">
            {visibleChecks.map((c) => (
              <details
                className={"check-item " + c.status}
                data-testid={"check-" + c.key}
                key={c.key}
                open={c.status === "failed"}
              >
                <summary>
                  <span className="check-symbol">
                    {c.status === "passed" ? (
                      <Check size={13} />
                    ) : c.status === "failed" ? (
                      <TriangleAlert size={13} />
                    ) : (
                      <CircleHelp size={13} />
                    )}
                  </span>
                  <span>{c.title}</span>
                  <small>{statusText[c.status]}</small>
                  <ChevronDown size={12} />
                </summary>
                <div className="check-details">
                  {(c.actual !== null || c.target !== null) && (
                    <div className="check-values">
                      <span>
                        当前 <b>{format(c.actual, 3)}</b>
                      </span>
                      <span>
                        要求 <b>{format(c.target, 3)}</b> {c.unit}
                      </span>
                    </div>
                  )}
                  <p>{c.detail}</p>
                </div>
              </details>
            ))}
            {evaluation &&
              !validation &&
              !pending &&
              visibleChecks.length === 0 && (
                <div className="all-passed">
                  <CheckCircle2 size={24} />
                  <p>当前没有冲突或待确认项。</p>
                </div>
              )}
          </div>
          <p className="inspection-note">
            <CircleHelp size={13} />
            工程校核辅助判断，最终以厂家资料及现场验证为准。
          </p>
        </section>
      </div>
    </div>
  );
}
