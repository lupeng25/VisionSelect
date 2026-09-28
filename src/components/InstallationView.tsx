import { useId, useState } from "react";
import { Camera, CircleDot, Lightbulb } from "lucide-react";
import type { Parameters, Project } from "../types";
import { fields } from "../format";
import {
  lightingLabels,
  installationDimension as dimension,
  lightingLayout,
  positiveDimension,
  type LightingLayout,
} from "../installation";

export function InstallationView({
  project,
  onParameter,
}: {
  project: Project;
  onParameter: (key: keyof Parameters, value: number | null) => void;
}) {
  const id = useId().replace(/:/g, "");
  const [preview, setPreview] = useState<"auto" | LightingLayout>("auto");
  const [camera, lens, light] = project.hardware;
  const p = project.parameters;
  const layout = preview === "auto" ? lightingLayout(light) : preview;
  const distance = positiveDimension(p.distance);
  const lightDistance = positiveDimension(p.light_distance);
  const backlight = layout === "backlight";
  const objectY = 318;
  const lensBottom = 122;
  const lightBase = backlight
    ? 365
    : objectY -
      (distance && lightDistance
        ? Math.min(
            135,
            Math.max(46, (lightDistance / distance) * (objectY - lensBottom)),
          )
        : 80);
  const lightTop = backlight
    ? lightBase
    : lightBase - (layout === "coaxial" ? 46 : layout === "dome" ? 48 : 16);
  const annotation = (x: number, start: number, end: number) => (
    <>
      <path
        d={`M316 ${start}H${x + 7}M316 ${end}H${x + 7}`}
        fill="none"
        stroke="var(--diagram-axis)"
        strokeDasharray="7 3 1 3"
      />
      <path
        d={`M${x} ${start}V${end}`}
        fill="none"
        stroke="var(--text)"
        markerStart={`url(#${id}-arrow)`}
        markerEnd={`url(#${id}-arrow)`}
      />
    </>
  );
  const needsReview =
    !backlight &&
    layout !== "side" &&
    distance &&
    lightDistance &&
    lightDistance >= distance;
  return (
    <div className="installation-view">
      <div className="installation-toolbar">
        <span>安装布置 · 正投影示意</span>
        <label>
          光源布置
          <select
            aria-label="照明布置预览"
            value={preview}
            onChange={(event) =>
              setPreview(event.target.value as typeof preview)
            }
          >
            <option value="auto">
              自动 ·{" "}
              {light ? lightingLabels[lightingLayout(light)] : "未选光源"}
            </option>
            {Object.entries(lightingLabels)
              .filter(([key]) => key !== "unknown")
              .map(([key, label]) => (
                <option key={key} value={key}>
                  {label}预览
                </option>
              ))}
          </select>
        </label>
      </div>
      <div className="installation-canvas">
        <svg
          viewBox="0 0 680 405"
          role="img"
          aria-label="相机、镜头、光源与工件安装主视图"
          className="installation-drawing"
          data-layout={layout}
        >
          <defs>
            <linearGradient id={`${id}-body`}>
              <stop stopColor="var(--diagram-camera)" />
              <stop offset=".45" stopColor="var(--diagram-plane)" />
              <stop offset="1" stopColor="var(--diagram-camera)" />
            </linearGradient>
            <marker
              id={`${id}-arrow`}
              viewBox="0 0 10 10"
              refX="1"
              refY="5"
              markerWidth="8"
              markerHeight="8"
              orient="auto-start-reverse"
            >
              <path
                d="M9 1L1 5L9 9"
                fill="none"
                stroke="var(--text)"
                strokeWidth="1.2"
              />
            </marker>
          </defs>
          <g
            opacity={camera ? 1 : 0.55}
            stroke="var(--diagram-axis)"
            strokeDasharray={camera ? undefined : "4 3"}
          >
            <rect
              x="294"
              y="19"
              width="12"
              height="9"
              rx="2"
              fill="var(--diagram-camera)"
            />
            <rect
              x="272"
              y="28"
              width="46"
              height="53"
              rx="3"
              fill={`url(#${id}-body)`}
            />
            <rect
              x="279"
              y="33"
              width="33"
              height="43"
              rx="1"
              fill="var(--accent)"
              fillOpacity=".5"
            />
            <path d="M275 31V78" />
          </g>
          <g
            opacity={lens ? 1 : 0.55}
            stroke="var(--diagram-axis)"
            strokeDasharray={lens ? undefined : "4 3"}
          >
            <rect
              x="276"
              y="81"
              width="36"
              height="41"
              rx="2"
              fill={`url(#${id}-body)`}
            />
            <path d="M276 86H312M276 96H312M276 111H312M282 88V94M289 88V94M296 88V94M303 88V94M282 99V108M289 99V108M296 99V108M303 99V108" />
            <rect
              x="277"
              y="117"
              width="34"
              height="5"
              fill="var(--diagram-lens)"
            />
          </g>
          <g
            stroke="var(--diagram-axis)"
            fill={`url(#${id}-body)`}
            opacity={light ? 1 : 0.55}
            strokeDasharray={light ? undefined : "4 3"}
            data-testid="installation-light"
          >
            {layout === "coaxial" && (
              <>
                <rect
                  x="264"
                  y={lightTop}
                  width="60"
                  height="46"
                  rx="2"
                  fill="var(--surface)"
                />
                <path
                  d={`M267 ${lightBase - 3}L315 ${lightTop + 3}`}
                  strokeWidth="2"
                />
                <rect x="317" y={lightTop} width="10" height="46" />
                <path
                  d={`M321 ${lightTop + 4}V${lightBase - 4}`}
                  stroke="var(--warning)"
                  strokeWidth="3"
                />
              </>
            )}
            {layout === "ring" && (
              <>
                <rect x="252" y={lightTop} width="28" height="16" rx="2" />
                <rect x="308" y={lightTop} width="28" height="16" rx="2" />
                <path
                  d={`M256 ${lightBase - 2}H277M311 ${lightBase - 2}H332`}
                  stroke="var(--warning)"
                  strokeWidth="3"
                />
              </>
            )}
            {layout === "side" && (
              <>
                <path
                  d={`M211 ${lightBase - 28}L240 ${lightBase - 12}L232 ${lightBase}L203 ${lightBase - 16}Z`}
                />
                <path
                  d={`M377 ${lightBase - 28}L348 ${lightBase - 12}L356 ${lightBase}L385 ${lightBase - 16}Z`}
                />
                <path
                  d={`M206 ${lightBase - 16}L231 ${lightBase - 2}M357 ${lightBase - 2}L382 ${lightBase - 16}`}
                  stroke="var(--warning)"
                  strokeWidth="3"
                />
              </>
            )}
            {layout === "backlight" && (
              <>
                <rect x="238" y={lightBase} width="112" height="14" rx="2" />
                <path
                  d={`M242 ${lightBase + 2}H346`}
                  stroke="var(--warning)"
                  strokeWidth="3"
                />
              </>
            )}
            {layout === "dome" && (
              <>
                <path
                  d={`M234 ${lightBase}Q238 ${lightTop} 284 ${lightTop}V${lightTop + 7}Q248 ${lightTop + 9} 244 ${lightBase}ZM354 ${lightBase}Q350 ${lightTop} 304 ${lightTop}V${lightTop + 7}Q340 ${lightTop + 9} 344 ${lightBase}Z`}
                />
              </>
            )}
            {layout === "unknown" && (
              <rect
                x="264"
                y={lightBase - 32}
                width="60"
                height="32"
                rx="2"
                fill="none"
                strokeDasharray="5 4"
              />
            )}
          </g>
          <rect
            x="262"
            y={objectY}
            width="64"
            height="17"
            rx="3"
            fill="var(--diagram-object)"
            stroke="var(--diagram-object-line)"
            strokeWidth="1.5"
          />
          <path
            d="M294 8V393"
            stroke="var(--text)"
            strokeDasharray="9 4 2 4"
            opacity=".7"
          />
          <g fill="var(--text)" fontSize="14" fontWeight="500">
            <text x="80" y="58">
              {camera ? "相机" : "相机待选"}
            </text>
            <text x="80" y="107">
              {lens ? "镜头" : "镜头待选"}
            </text>
            <text x="80" y={lightTop + (backlight ? 12 : 19)}>
              {light ? "光源" : "光源待选"}
            </text>
            <text x="80" y={objectY + 14}>
              被测物
            </text>
          </g>
          {annotation(530, lensBottom, objectY)}
          {annotation(
            426,
            backlight ? objectY + 17 : lightBase,
            backlight ? lightBase : objectY,
          )}
          <g fill="var(--text)" fontSize="13">
            <text x="541" y="168" fill="var(--muted)" fontSize="11">
              镜头前端 → 工件
            </text>
            <text x="541" y="189" data-testid="installation-distance">
              {dimension(distance, p.distance_tolerance)}
            </text>
            <text
              x="416"
              textAnchor="end"
              y={backlight ? 351 : (lightBase + objectY) / 2 + 5}
              data-testid="installation-light-distance"
            >
              {dimension(lightDistance, p.light_distance_tolerance)}
            </text>
          </g>
        </svg>
      </div>
      <div className="installation-inputs">
        {fields.installation.map((field) => {
          const value = p[field.key] as number | null | undefined;
          const valid =
            value == null ||
            (Number.isFinite(value) && value >= field.min && value <= 1e8);
          return (
            <label
              key={field.key}
              className={`parameter-field${valid ? "" : " invalid"}`}
            >
              <span>{field.title}</span>
              <div>
                <input
                  aria-label={field.title}
                  aria-invalid={!valid}
                  type="number"
                  min={field.min}
                  max={1e8}
                  step={field.step}
                  value={value != null && Number.isFinite(value) ? value : ""}
                  placeholder="待确认"
                  onChange={(event) =>
                    onParameter(
                      field.key,
                      event.target.value === ""
                        ? null
                        : event.target.valueAsNumber,
                    )
                  }
                />
                <span className="unit">mm</span>
              </div>
            </label>
          );
        })}
      </div>
      <div className="installation-devices">
        {[
          { device: camera, Icon: Camera, title: "相机" },
          { device: lens, Icon: CircleDot, title: "镜头" },
          { device: light, Icon: Lightbulb, title: "光源" },
        ].map(({ device, Icon, title }) => (
          <div key={title}>
            <Icon size={14} />
            <span>
              <small>{title}</small>
              <strong title={device?.model}>
                {device?.model || "尚未选择"}
              </strong>
            </span>
          </div>
        ))}
      </div>
      {needsReview && (
        <p className="installation-warning" role="status">
          光源距离不小于镜头工作距离，请确认实际安装位置；图中间隔仅为示意。
        </p>
      )}
      <p className="installation-caption">
        {preview !== "auto"
          ? "当前为布置预览，不改变所选光源。"
          : "按光源类型展示典型布置。"}
        {backlight
          ? "光源距离从上发光面量至工件下表面。"
          : layout === "side"
            ? "侧向光源标注安装高度，不代表斜向照射距离。"
            : "光源距离从下出光面（穹顶为开口面）量至工件上表面。"}
        外形非等比例，未校核机械干涉；公差仅为安装标注，不代表景深或厂家允许范围。
      </p>
    </div>
  );
}
