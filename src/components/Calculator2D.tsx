import { useEffect, useState } from "react";
import {
  ArrowDownToLine,
  ArrowRight,
  Calculator,
  CheckCircle2,
  CircleHelp,
  CircleDot,
  Database,
  Focus,
  Gauge,
  LoaderCircle,
  Ruler,
  TriangleAlert,
} from "lucide-react";
import { request, errorText } from "../api";
import { format, spec, specNumber } from "../format";
import type { Evaluation, Project } from "../types";
import {
  CALCULATION_KEY,
  calculationKinds,
  opticalMethods,
  pixelFormats,
  restoreCalculation,
  formatCalculation,
  calculationApplyError,
  type CalculationDraft,
  type CalculationGeometry,
  type CalculationKind,
  type CalculationPatch,
  type CalculationResult,
} from "../calculation";

const tools = {
  resolution: {
    title: "视场与分辨率",
    subtitle: "从工件与采样要求推算像素",
    icon: Ruler,
  },
  optics: {
    title: "镜头与工作距离",
    subtitle: "视场、焦距与倍率相互推算",
    icon: Focus,
  },
  motion: {
    title: "运动与曝光",
    subtitle: "控制运动拖影与曝光时间",
    icon: Gauge,
  },
  data: {
    title: "传输与存储",
    subtitle: "估算图像载荷与记录容量",
    icon: Database,
  },
};
interface Field {
  key: string;
  title: string;
  unit: string;
  min?: number;
  max?: number;
  integer?: boolean;
}
const sensorFields: Field[] = [
  {
    key: "resolution_x",
    title: "水平有效像素",
    unit: "px",
    min: 1,
    max: 1e6,
    integer: true,
  },
  {
    key: "resolution_y",
    title: "垂直有效像素",
    unit: "px",
    min: 1,
    max: 1e6,
    integer: true,
  },
  { key: "pixel_size_um", title: "像元尺寸", unit: "μm" },
];
const fovFields: Field[] = [
  { key: "fov_width", title: "目标视场宽度", unit: "mm" },
  { key: "fov_height", title: "目标视场高度", unit: "mm" },
];
function groups(draft: CalculationDraft): { title: string; fields: Field[] }[] {
  switch (draft.active) {
    case "resolution":
      return [
        {
          title: "现场需求",
          fields: [
            { key: "object_width", title: "工件宽度", unit: "mm" },
            { key: "object_height", title: "工件高度", unit: "mm" },
            { key: "margin", title: "单边余量", unit: "mm", min: 0 },
            { key: "pixel_limit", title: "像素当量上限", unit: "μm/px" },
          ],
        },
        { title: "待评估相机", fields: sensorFields },
      ];
    case "optics": {
      const method = draft.optics.method;
      const opticalFields: Field[] = [];
      if (["focal", "distance", "magnification"].includes(method))
        opticalFields.push(...fovFields);
      if (["fov", "distance"].includes(method))
        opticalFields.push({ key: "focal_mm", title: "镜头焦距", unit: "mm" });
      if (["fov", "focal"].includes(method))
        opticalFields.push({
          key: "distance_mm",
          title: "工作距离",
          unit: "mm",
        });
      if (method === "telecentric_fov")
        opticalFields.push({
          key: "magnification",
          title: "远心倍率",
          unit: "×",
        });
      return [
        { title: "已知光学条件", fields: opticalFields },
        { title: "相机传感器", fields: sensorFields },
      ];
    }
    case "motion":
      return [
        {
          title: "成像与运动",
          fields: [
            { key: "sampling_um", title: "物方像素当量", unit: "μm/px" },
            { key: "speed", title: "运动速度", unit: "mm/s", min: 0 },
          ],
        },
        {
          title: "采集要求",
          fields: [
            { key: "exposure", title: "曝光时间", unit: "μs" },
            { key: "blur", title: "允许拖影", unit: "px" },
            { key: "fps", title: "目标帧率", unit: "fps" },
          ],
        },
      ];
    case "data":
      return [
        {
          title: "图像与记录",
          fields: [
            ...sensorFields.slice(0, 2),
            { key: "fps", title: "目标帧率", unit: "fps" },
            { key: "duration_seconds", title: "记录时长", unit: "s" },
          ],
        },
        {
          title: "链路预算",
          fields: [
            { key: "bandwidth_mbps", title: "链路额定带宽", unit: "MB/s" },
            {
              key: "utilization_percent",
              title: "可用带宽比例",
              unit: "%",
              min: 1,
              max: 100,
            },
          ],
        },
      ];
  }
}
function valid(value: unknown, field: Field) {
  return (
    typeof value === "number" &&
    Number.isFinite(value) &&
    value >= (field.min ?? 0.000001) &&
    value <= (field.max ?? 1e8) &&
    (!field.integer || Number.isInteger(value))
  );
}

function Geometry({ geometry }: { geometry: CalculationGeometry }) {
  const maxWidth = Math.max(geometry.width, geometry.required_width ?? 0);
  const maxHeight = Math.max(geometry.height, geometry.required_height ?? 0);
  const scale = Math.min(250 / maxWidth, 94 / maxHeight);
  const w = geometry.width * scale,
    h = geometry.height * scale;
  return (
    <div className="calculation-geometry">
      <svg
        viewBox="0 0 480 144"
        role="img"
        aria-label="计算视场与所需覆盖范围示意"
      >
        <path
          d="M240 10v110M42 64h396"
          stroke="var(--diagram-axis)"
          strokeDasharray="3 5"
        />
        <rect
          x={240 - w / 2}
          y={62 - h / 2}
          width={w}
          height={h}
          rx="3"
          fill="var(--diagram-fov)"
          stroke="var(--accent)"
          strokeWidth="1.5"
        />
        {geometry.required_width !== null &&
          geometry.required_height !== null && (
            <rect
              x={240 - (geometry.required_width * scale) / 2}
              y={62 - (geometry.required_height * scale) / 2}
              width={geometry.required_width * scale}
              height={geometry.required_height * scale}
              fill="var(--diagram-object)"
              fillOpacity=".55"
              stroke="var(--diagram-object-line)"
              strokeDasharray="4 3"
            />
          )}
        <text
          x="240"
          y="138"
          textAnchor="middle"
          fill="var(--ink)"
          fontSize="12"
        >
          {format(geometry.width, 3)} × {format(geometry.height, 3)} mm
        </text>
      </svg>
      <div className="calculation-legend">
        <span>
          <i />
          计算覆盖视场
        </span>
        {geometry.required_width !== null && (
          <span>
            <i className="required" />
            所需覆盖范围
          </span>
        )}
        <span>按比例绘制</span>
      </div>
    </div>
  );
}

export function Calculator2D({
  project,
  onApply,
}: {
  project: Project;
  onApply: (patch: CalculationPatch) => void;
}) {
  const [draft, setDraft] = useState(() => {
    try {
      return restoreCalculation(localStorage.getItem(CALCULATION_KEY));
    } catch {
      return restoreCalculation(null);
    }
  });
  const [answer, setAnswer] = useState<{
    input: string;
    result: CalculationResult;
  } | null>(null);
  const [failure, setFailure] = useState<{
    input: string;
    message: string;
  } | null>(null);
  const [source, setSource] = useState(
    "参数可手动输入；计算草稿与工程方案分别保存。",
  );
  const [reading, setReading] = useState(false);
  const [storageError, setStorageError] = useState(false);
  const active = draft.active;
  const groupList = groups(draft);
  const values = draft[active] as unknown as Record<
    string,
    number | string | null
  >;
  const invalid = groupList
    .flatMap((g) => g.fields)
    .find((f) => !valid(values[f.key], f));
  const input = JSON.stringify({ kind: active, ...draft[active] });
  const error = invalid
    ? `请填写有效的${invalid.title}${invalid.integer ? "（正整数）" : ""}。`
    : failure?.input === input
      ? failure.message
      : "";
  const result = !error && answer?.input === input ? answer.result : null;
  const pending = !error && !result;
  const applicationError = result ? calculationApplyError(result.apply) : null;

  useEffect(() => {
    try {
      localStorage.setItem(CALCULATION_KEY, JSON.stringify(draft));
      setStorageError(false);
    } catch {
      setStorageError(true);
    }
  }, [draft]);
  useEffect(() => {
    if (invalid) return;
    let current = true;
    const timeout = setTimeout(() => {
      request<CalculationResult>("calculate_2d", { input: JSON.parse(input) })
        .then((result) => {
          if (current) {
            setAnswer({ input, result });
            setFailure(null);
          }
        })
        .catch((e) => {
          if (current) setFailure({ input, message: errorText(e) });
        });
    }, 120);
    return () => {
      current = false;
      clearTimeout(timeout);
    };
  }, [input, invalid]);

  function update(key: string, value: number | string | null) {
    setDraft((d) => ({ ...d, [active]: { ...d[active], [key]: value } }));
    setSource("手动调整中；计算结果不会自动改写当前方案。");
  }
  function select(kind: CalculationKind) {
    setDraft((d) => ({ ...d, active: kind }));
  }
  async function readProject() {
    setReading(true);
    const p = project.parameters,
      camera = project.hardware[0],
      lens = project.hardware[1];
    let evaluation: Evaluation | null = null;
    try {
      if (project.mode === "imaging")
        evaluation = await request<Evaluation>("evaluate", { project });
    } catch {
      /* 无法获得成像值时保留手动输入并说明。 */
    }
    const rx = specNumber(camera, "resolution_x"),
      ry = specNumber(camera, "resolution_y");
    const pitch = specNumber(camera, "pixel_size_um"),
      focal = specNumber(lens, "focal_length_mm"),
      mag = specNumber(lens, "pmag");
    const tele =
      spec(lens, "lens_type").toLowerCase().includes("telecentric") ||
      spec(lens, "lens_type").includes("远心");
    const rawSampling =
      evaluation?.fov_x && evaluation?.fov_y && rx && ry
        ? Math.min(evaluation.fov_x / rx, evaluation.fov_y / ry) * 1000
        : null;
    const sampling =
      rawSampling === null ? null : Number(rawSampling.toPrecision(12));
    const keep = (
      value: number | null,
      previous: number | null,
      zero = false,
    ) =>
      value !== null && Number.isFinite(value) && value >= (zero ? 0 : 0.000001)
        ? value
        : previous;
    const formatValue = pixelFormats.find(
      (f) =>
        f.toLowerCase() ===
        (p.pixel_format || spec(camera, "pixel_format")).toLowerCase(),
    );
    setDraft((d) => {
      const sensor = {
        resolution_x: keep(rx, d.resolution.resolution_x),
        resolution_y: keep(ry, d.resolution.resolution_y),
        pixel_size_um: keep(pitch, d.resolution.pixel_size_um),
      };
      return {
        ...d,
        resolution: {
          ...d.resolution,
          ...sensor,
          object_width: keep(p.width, d.resolution.object_width),
          object_height: keep(p.height, d.resolution.object_height),
          margin: keep(p.margin, d.resolution.margin, true),
          pixel_limit: keep(p.pixel, d.resolution.pixel_limit),
        },
        optics: {
          ...d.optics,
          resolution_x: keep(rx, d.optics.resolution_x),
          resolution_y: keep(ry, d.optics.resolution_y),
          pixel_size_um: keep(pitch, d.optics.pixel_size_um),
          focal_mm: keep(focal, d.optics.focal_mm),
          distance_mm: keep(p.distance, d.optics.distance_mm),
          fov_width: keep(p.width + 2 * p.margin, d.optics.fov_width),
          fov_height: keep(p.height + 2 * p.margin, d.optics.fov_height),
          magnification: keep(mag, d.optics.magnification),
          method: lens ? (tele ? "telecentric_fov" : "fov") : d.optics.method,
        },
        motion: {
          ...d.motion,
          sampling_um: keep(sampling, d.motion.sampling_um),
          speed: keep(p.speed, d.motion.speed, true),
          exposure: keep(p.exposure, d.motion.exposure),
          blur: keep(p.blur, d.motion.blur),
          fps: keep(p.fps, d.motion.fps),
        },
        data: {
          ...d.data,
          resolution_x: keep(rx, d.data.resolution_x),
          resolution_y: keep(ry, d.data.resolution_y),
          fps: keep(p.fps, d.data.fps),
          pixel_format: formatValue ?? d.data.pixel_format,
        },
      };
    });
    const missing = [
      !camera ? "未选相机" : !rx || !ry || !pitch ? "相机规格不全" : "",
      !lens ? "未选镜头" : !(tele ? mag : focal) ? "镜头光学规格不全" : "",
      !sampling ? "无法读取实际像素当量" : "",
      !formatValue ? "像素格式未指定或不在本页支持范围" : "",
    ].filter(Boolean);
    setSource(
      `已读取「${project.name}」的可用参数。${missing.length ? missing.join("、") + "，对应项保留原手动值。" : "其余独立计算参数保留原值。"}`,
    );
    setReading(false);
  }
  function continueOptics() {
    const fov = result?.metrics.find((m) => m.key === "required_fov");
    if (!fov?.value || !fov.other) return;
    setDraft((d) => ({
      ...d,
      active: "optics",
      optics: {
        ...d.optics,
        resolution_x: d.resolution.resolution_x,
        resolution_y: d.resolution.resolution_y,
        pixel_size_um: d.resolution.pixel_size_um,
        fov_width: fov.value,
        fov_height: fov.other,
        method: "focal",
      },
    }));
    setSource("已带入所需视场与相机参数；输入工作距离后继续计算焦距。");
  }
  const StatusIcon =
    result?.status === "passed"
      ? CheckCircle2
      : result?.status === "neutral"
        ? CircleHelp
        : TriangleAlert;
  const activeTool = tools[active];
  return (
    <div className="page calculator-page">
      <div className="page-title">
        <div>
          <span className="eyebrow">2D VISION CALCULATOR</span>
          <h1>
            2D 选型计算
            <span className="title-dot" />
          </h1>
          <p>把现场条件转成相机与镜头参数，先计算，再选型。</p>
        </div>
        <button
          className="secondary"
          onClick={() => void readProject()}
          disabled={reading}
        >
          {reading ? (
            <LoaderCircle size={16} className="spin" />
          ) : (
            <ArrowDownToLine size={16} />
          )}
          读取当前方案
        </button>
      </div>
      <div className="calculation-tabs" role="tablist" aria-label="计算类型">
        {calculationKinds.map((kind, index) => {
          const tool = tools[kind];
          return (
            <button
              key={kind}
              id={`calculator-tab-${kind}`}
              type="button"
              role="tab"
              aria-selected={kind === active}
              aria-controls="calculator-panel"
              tabIndex={kind === active ? 0 : -1}
              className={kind === active ? "active" : ""}
              onClick={() => select(kind)}
              onKeyDown={(e) => {
                const next =
                  e.key === "ArrowRight"
                    ? (index + 1) % 4
                    : e.key === "ArrowLeft"
                      ? (index + 3) % 4
                      : e.key === "Home"
                        ? 0
                        : e.key === "End"
                          ? 3
                          : -1;
                if (next >= 0) {
                  e.preventDefault();
                  select(calculationKinds[next]);
                  e.currentTarget.parentElement
                    ?.querySelectorAll<HTMLButtonElement>("[role=tab]")
                    [next]?.focus();
                }
              }}
            >
              <tool.icon size={19} />
              <span>
                <strong>{tool.title}</strong>
                <small>{tool.subtitle}</small>
              </span>
            </button>
          );
        })}
      </div>
      <div className="calculation-source" role="status">
        <CircleDot size={14} />
        <span>
          {storageError
            ? "计算草稿无法写入本地，请在关闭前记录重要参数。"
            : source}
        </span>
      </div>
      <div
        className="calculation-layout"
        key={active}
        id="calculator-panel"
        role="tabpanel"
        aria-labelledby={`calculator-tab-${active}`}
      >
        <section className="calculation-inputs" aria-label="计算条件">
          <header>
            <div>
              <span className="eyebrow">INPUT</span>
              <h2>已知条件</h2>
            </div>
            <span className="calculation-state">{activeTool.title}</span>
          </header>
          {active === "optics" && (
            <label className="select-field calculation-select">
              <span>计算目标</span>
              <select
                aria-label="计算目标"
                value={draft.optics.method}
                onChange={(e) => update("method", e.target.value)}
              >
                {opticalMethods.map((m) => (
                  <option key={m.value} value={m.value}>
                    {m.label}
                  </option>
                ))}
              </select>
            </label>
          )}
          {active === "data" && (
            <label className="select-field calculation-select">
              <span>传输像素格式</span>
              <select
                aria-label="传输像素格式"
                value={draft.data.pixel_format}
                onChange={(e) => update("pixel_format", e.target.value)}
              >
                {pixelFormats.map((f) => (
                  <option key={f}>{f}</option>
                ))}
              </select>
            </label>
          )}
          {groupList.map((group) => (
            <fieldset key={group.title}>
              <legend>{group.title}</legend>
              <div className="parameter-grid">
                {group.fields.map((field) => {
                  const value = values[field.key];
                  const ok = valid(value, field);
                  return (
                    <label
                      key={field.key}
                      className={"parameter-field" + (!ok ? " invalid" : "")}
                    >
                      <span>{field.title}</span>
                      <div>
                        <input
                          type="number"
                          aria-label={field.title}
                          aria-invalid={!ok}
                          min={field.min ?? 0.000001}
                          max={field.max ?? 1e8}
                          step={field.integer ? 1 : "any"}
                          value={typeof value === "number" ? value : ""}
                          onChange={(e) =>
                            update(
                              field.key,
                              Number.isFinite(e.target.valueAsNumber)
                                ? e.target.valueAsNumber
                                : null,
                            )
                          }
                        />
                        <span className="unit">{field.unit}</span>
                      </div>
                    </label>
                  );
                })}
              </div>
            </fieldset>
          ))}
          <p className="calculation-input-note">
            <CircleHelp size={14} />
            {active === "motion"
              ? "请使用实际成像的物方像素当量，不能用传感器像元尺寸直接替代。"
              : active === "data"
                ? "按所填有效图像尺寸估算；链路带宽的单位为 MB/s（每秒兆字节）。"
                : "相机输入使用有效像素；像元尺寸为传感器单个像元的物理尺寸。"}
          </p>
        </section>
        <section
          className="calculation-results"
          aria-label="计算结果"
          aria-busy={pending}
        >
          <header>
            <div>
              <span className="eyebrow">RESULT</span>
              <h2>即时结果</h2>
            </div>
            <span className="live-indicator">
              <i />
              {pending ? "计算中" : error ? "待修正" : "已更新"}
            </span>
          </header>
          {error ? (
            <div className="calculation-empty" role="alert">
              <TriangleAlert size={26} />
              <h3>请检查计算条件</h3>
              <p>{error}</p>
            </div>
          ) : !result ? (
            <div className="calculation-empty" role="status">
              <LoaderCircle size={25} className="spin" />
              <p>正在计算…</p>
            </div>
          ) : (
            <>
              <div className={`calculation-verdict ${result.status}`}>
                <StatusIcon size={17} />
                <span>{result.summary}</span>
              </div>
              <div className="calculation-metrics">
                {result.metrics.map((metric, index) => (
                  <div className={index < 2 ? "featured" : ""} key={metric.key}>
                    <span>{metric.label}</span>
                    <div>
                      <strong data-testid={`calc-${metric.key}`}>
                        {formatCalculation(metric.value, metric.digits)}
                        {metric.other !== null
                          ? " × " +
                            formatCalculation(metric.other, metric.digits)
                          : ""}
                      </strong>
                      <small>{metric.unit}</small>
                    </div>
                  </div>
                ))}
              </div>
            </>
          )}
          <footer className="calculation-actions">
            <div>
              <strong>用于当前二维方案</strong>
              <p>
                {applicationError ||
                  result?.apply_description ||
                  (error || pending
                    ? "有效结果生成后可应用参数。"
                    : "远心倍率需在硬件资料库中选择对应镜头。")}
              </p>
            </div>
            <button
              className="primary"
              disabled={
                !result ||
                !Object.keys(result.apply).length ||
                reading ||
                !!applicationError
              }
              onClick={() => {
                if (result && !applicationError) onApply(result.apply);
              }}
            >
              应用参数
              <ArrowRight size={15} />
            </button>
          </footer>
          {active === "resolution" && (
            <button
              className="calculation-next"
              disabled={!result}
              onClick={continueOptics}
            >
              用这个视场继续算镜头
              <ArrowRight size={15} />
            </button>
          )}
          {result && (
            <>
              {result.geometry && <Geometry geometry={result.geometry} />}
              <div className="calculation-notes">
                {result.notes.map((note) => (
                  <p key={note}>{note}</p>
                ))}
              </div>
              <details className="calculation-formulas">
                <summary>
                  <Calculator size={14} />
                  计算依据与公式
                </summary>
                {result.formulas.map((formula) => (
                  <p key={formula}>{formula}</p>
                ))}
              </details>
            </>
          )}
        </section>
      </div>
    </div>
  );
}
