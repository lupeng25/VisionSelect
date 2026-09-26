import { useEffect, useId, useRef, useState, type RefObject } from "react";
import { Check, SlidersHorizontal, X } from "lucide-react";
import { format } from "../format";
import type { CatalogFilterDefinition } from "../types";

export interface CatalogFilterDraft {
  search: string;
  manufacturer: string;
  choices: Record<string, string | null>;
  ranges: Record<string, { min: string; max: string }>;
}
export const emptyCatalogFilters = (): CatalogFilterDraft => ({
  search: "",
  manufacturer: "",
  choices: {},
  ranges: {},
});
export function catalogFilterError(
  draft: CatalogFilterDraft,
  schema: CatalogFilterDefinition[],
) {
  for (const [key, range] of Object.entries(draft.ranges)) {
    const label = schema.find((d) => d.key === key)?.label ?? key;
    if (
      [range.min, range.max].some(
        (v) =>
          v !== "" &&
          (!Number.isFinite(Number(v)) || Number(v) < 0 || Number(v) > 1e12),
      )
    )
      return `${label}范围必须为有效的非负数。`;
    if (
      range.min !== "" &&
      range.max !== "" &&
      Number(range.min) > Number(range.max)
    )
      return `${label}下限不能大于上限。`;
  }
  return "";
}
export function catalogFilterPayload(draft: CatalogFilterDraft) {
  return {
    choices: draft.choices,
    ranges: Object.fromEntries(
      Object.entries(draft.ranges)
        .filter(([, r]) => r.min !== "" || r.max !== "")
        .map(([key, r]) => [
          key,
          {
            min: r.min === "" ? null : Number(r.min),
            max: r.max === "" ? null : Number(r.max),
          },
        ]),
    ),
  };
}

export function CatalogFilters({
  schema,
  draft,
  onChange,
  open,
  onClose,
  trigger,
  panelId,
}: {
  schema: CatalogFilterDefinition[];
  draft: CatalogFilterDraft;
  onChange: (patch: Partial<CatalogFilterDraft>) => void;
  open: boolean;
  onClose: () => void;
  trigger: RefObject<HTMLButtonElement | null>;
  panelId: string;
}) {
  const root = useRef<HTMLDivElement>(null);
  const panel = useRef<HTMLDivElement>(null);
  const prefix = useId();
  const [position, setPosition] = useState({
    top: 0,
    left: 0,
    width: 760,
    maxHeight: 420,
  });
  useEffect(() => {
    if (!open) return;
    const align = () => {
      const anchor = trigger.current?.getBoundingClientRect();
      const bounds = root.current?.getBoundingClientRect();
      if (!anchor) return;
      const width = Math.min(
        800,
        bounds?.width ?? innerWidth - 32,
        innerWidth - 32,
      );
      const top = Math.min(anchor.bottom + 8, innerHeight - 240);
      setPosition({
        top,
        left: Math.max(
          16,
          Math.min(
            (bounds?.right ?? anchor.right) - width,
            innerWidth - width - 16,
          ),
        ),
        width,
        maxHeight: Math.min(440, innerHeight - top - 32),
      });
    };
    const pointer = (event: PointerEvent) => {
      if (
        !panel.current?.contains(event.target as Node) &&
        !trigger.current?.contains(event.target as Node)
      )
        onClose();
    };
    const keyboard = (event: KeyboardEvent) => {
      if (event.key === "Escape") {
        event.preventDefault();
        event.stopPropagation();
        onClose();
        trigger.current?.focus();
      }
    };
    align();
    const timer = setTimeout(
      () =>
        panel.current
          ?.querySelector<HTMLElement>(
            "select:not(:disabled), input:not(:disabled), button",
          )
          ?.focus(),
      0,
    );
    window.addEventListener("resize", align);
    document.addEventListener("pointerdown", pointer);
    document.addEventListener("keydown", keyboard, true);
    return () => {
      clearTimeout(timer);
      window.removeEventListener("resize", align);
      document.removeEventListener("pointerdown", pointer);
      document.removeEventListener("keydown", keyboard, true);
    };
  }, [open, onClose, trigger]);

  function choice(key: string, value: string) {
    const next = { ...draft.choices };
    if (!value) delete next[key];
    else next[key] = value === "missing" ? null : value.slice(6);
    onChange({ choices: next });
  }
  function range(key: string, side: "min" | "max", value: string) {
    const next = {
      ...draft.ranges,
      [key]: { ...(draft.ranges[key] ?? { min: "", max: "" }), [side]: value },
    };
    if (next[key].min === "" && next[key].max === "") delete next[key];
    onChange({ ranges: next });
  }
  function remove(key: string, control: "choice" | "range") {
    if (control === "choice") {
      const next = { ...draft.choices };
      delete next[key];
      onChange({ choices: next });
    } else {
      const next = { ...draft.ranges };
      delete next[key];
      onChange({ ranges: next });
    }
  }
  function field(definition: CatalogFilterDefinition) {
    const id = `${prefix}-${definition.key}`;
    const r = draft.ranges[definition.key] ?? { min: "", max: "" };
    const rangeError = catalogFilterError(
      { ...emptyCatalogFilters(), ranges: { [definition.key]: r } },
      [definition],
    );
    const selected = definition.key in draft.choices;
    const chosen = draft.choices[definition.key];
    const hasOption = definition.options.some((o) => o.value === chosen);
    return (
      <div className="catalog-filter-field" key={definition.key}>
        <label id={id} htmlFor={id + "-input"}>
          {definition.label}
          {definition.unit && <small>{definition.unit}</small>}
        </label>
        {definition.control === "choice" ? (
          <select
            id={id + "-input"}
            aria-label={"筛选" + definition.label}
            value={
              selected ? (chosen === null ? "missing" : "value:" + chosen) : ""
            }
            onChange={(e) => choice(definition.key, e.target.value)}
          >
            <option value="">全部</option>
            {selected && !hasOption && (
              <option value={chosen === null ? "missing" : "value:" + chosen}>
                {chosen ?? "未公开"}（已选）
              </option>
            )}
            {definition.options.map((option) => (
              <option
                key={option.value ?? "missing"}
                value={
                  option.value === null ? "missing" : "value:" + option.value
                }
              >
                {option.label}
              </option>
            ))}
          </select>
        ) : (
          <div
            className={"catalog-range" + (rangeError ? " invalid" : "")}
            role="group"
            aria-labelledby={id}
            title={
              definition.known_count
                ? `当前公开范围：${format(definition.min, 6)}–${format(definition.max, 6)} ${definition.unit}`
                : "当前资料未公开此规格"
            }
          >
            <input
              id={id + "-input"}
              type="number"
              min="0"
              max="1000000000000"
              step="any"
              aria-label={definition.label + "下限"}
              aria-invalid={!!rangeError}
              value={r.min}
              placeholder={definition.known_count ? "最低" : "未公开"}
              disabled={!definition.known_count && !r.min && !r.max}
              onChange={(e) => range(definition.key, "min", e.target.value)}
            />
            <span>至</span>
            <input
              type="number"
              min="0"
              max="1000000000000"
              step="any"
              aria-label={definition.label + "上限"}
              aria-invalid={!!rangeError}
              value={r.max}
              placeholder={definition.known_count ? "最高" : "未公开"}
              disabled={!definition.known_count && !r.min && !r.max}
              onChange={(e) => range(definition.key, "max", e.target.value)}
            />
          </div>
        )}
      </div>
    );
  }
  const selected = schema.filter(
    (d) =>
      d.key in draft.choices ||
      (draft.ranges[d.key] &&
        (draft.ranges[d.key].min !== "" || draft.ranges[d.key].max !== "")),
  );
  const count =
    selected.length + Number(!!draft.search) + Number(!!draft.manufacturer);
  return (
    <div className="catalog-specifications" ref={root}>
      <div className="catalog-quick-filters" aria-label="常用规格筛选">
        {schema.filter((d) => d.primary).map(field)}
      </div>
      {!!count && (
        <div className="catalog-active-filters">
          <span>已选 {count} 项</span>
          <div className="catalog-filter-chips">
            {draft.search && (
              <button
                aria-label="移除搜索条件"
                onClick={() => onChange({ search: "" })}
              >
                <span>搜索：{draft.search}</span>
                <X size={12} />
              </button>
            )}
            {draft.manufacturer && (
              <button
                aria-label="移除厂家筛选"
                onClick={() => onChange({ manufacturer: "" })}
              >
                <span>{draft.manufacturer}</span>
                <X size={12} />
              </button>
            )}
            {selected.map((d) => {
              const r = draft.ranges[d.key];
              const value =
                d.control === "choice"
                  ? (d.options.find((o) => o.value === draft.choices[d.key])
                      ?.label ??
                    draft.choices[d.key] ??
                    "未公开")
                  : `${r.min || "不限"}–${r.max || "不限"}${d.unit ? " " + d.unit : ""}`;
              return (
                <button
                  key={d.key}
                  aria-label={"移除" + d.label + "筛选"}
                  onClick={() => remove(d.key, d.control)}
                >
                  <span>
                    {d.label}：{value}
                  </span>
                  <X size={12} />
                </button>
              );
            })}
          </div>
          <button
            className="catalog-clear"
            onClick={() => onChange(emptyCatalogFilters())}
          >
            清空全部筛选
          </button>
        </div>
      )}
      {open && (
        <div
          className="catalog-filter-panel"
          id={panelId}
          ref={panel}
          style={position}
          role="region"
          aria-label="更多规格筛选"
        >
          <header>
            <div>
              <SlidersHorizontal size={16} />
              <strong>更多规格筛选</strong>
            </div>
            <button
              className="icon-button"
              aria-label="关闭更多筛选"
              onClick={() => {
                onClose();
                trigger.current?.focus();
              }}
            >
              <X size={17} />
            </button>
          </header>
          <div className="catalog-filter-panel-body">
            <div className="catalog-extra-filters">
              {schema.filter((d) => !d.primary).map(field)}
            </div>
            <p>
              条件可组合使用；设置数值范围时，未公开规格不参与匹配。选项来自当前分类全库。
            </p>
          </div>
          <footer>
            <button
              className="ghost small"
              onClick={() => onChange({ choices: {}, ranges: {} })}
            >
              清空规格条件
            </button>
            <button
              className="primary small"
              onClick={() => {
                onClose();
                trigger.current?.focus();
              }}
            >
              <Check size={14} />
              完成
            </button>
          </footer>
        </div>
      )}
    </div>
  );
}
