import { useEffect, useState } from "react";
import {
  ArrowLeft,
  ArrowRight,
  FileSpreadsheet,
  LoaderCircle,
} from "lucide-react";
import { request, errorText } from "../api";
import { kinds, titles, type Kind } from "../types";
import { Modal } from "./Modal";

interface Inspection {
  headers: string[];
  sample: { row: number; values: string[] }[];
  total: number;
  sheets: string[];
  sheet: string;
  fields: { key: string; label: string; numeric: boolean }[];
  mapping: (string | null)[];
}
interface Preview {
  token: string | null;
  added: number;
  updated: number;
  skipped: number;
  unchanged: number;
  total: number;
  invalid: number;
  errors: { row: number; message: string }[];
  rows: {
    row: number;
    model: string;
    manufacturer: string;
    action: string;
    changes: { key: string; before: unknown; after: unknown }[];
  }[];
}
export interface ImportResult {
  imported: number;
  backup: string;
}
const actionNames: Record<string, string> = {
  added: "新增",
  updated: "更新",
  skipped: "跳过",
  unchanged: "无变化",
};
const display = (value: unknown) =>
  value == null || value === ""
    ? "空"
    : typeof value === "boolean"
      ? value
        ? "是"
        : "否"
      : String(value);
export function HardwareImport({
  file: initialFile,
  kind: initialKind,
  onClose,
  onImported,
}: {
  file: File;
  kind: Kind;
  onClose: () => void;
  onImported: (result: ImportResult, kind: Kind) => void;
}) {
  const [file, setFile] = useState(initialFile);
  const [kind, setKind] = useState(initialKind);
  const [encoding, setEncoding] = useState("auto");
  const [decodedAs, setDecodedAs] = useState("");
  const [content, setContent] = useState<string | null>(null);
  const [sheet, setSheet] = useState("");
  const [headerRow, setHeaderRow] = useState(1);
  const [delimiter, setDelimiter] = useState(",");
  const [inspection, setInspection] = useState<Inspection | null>(null);
  const [mapping, setMapping] = useState<(string | null)[]>([]);
  const [preview, setPreview] = useState<Preview | null>(null);
  const [policy, setPolicy] = useState("fill");
  const [skipInvalid, setSkipInvalid] = useState(false);
  const [busy, setBusy] = useState(false);
  const [inspecting, setInspecting] = useState(true);
  const [error, setError] = useState("");
  const excel = /\.(xlsx|xls)$/i.test(file.name);
  const [readError, setReadError] = useState("");
  useEffect(() => {
    let current = true;
    setContent(null);
    setReadError("");
    setPreview(null);
    setInspection(null);
    void (async () => {
      if (file.size > 32 * 1024 * 1024)
        throw new Error("文件超过 32 MB 限制。");
      if (!/\.(xlsx|xls|csv)$/i.test(file.name))
        throw new Error("请选择 XLSX、XLS 或 CSV 文件。");
      const bytes = new Uint8Array(await file.arrayBuffer());
      let result: string,
        label = "Excel";
      if (excel) {
        let binary = "";
        for (let i = 0; i < bytes.length; i += 16384)
          binary += String.fromCharCode(...bytes.subarray(i, i + 16384));
        result = btoa(binary);
      } else {
        label = encoding === "auto" ? "utf-8" : encoding;
        try {
          result = new TextDecoder(label, { fatal: true }).decode(bytes);
        } catch (e) {
          if (encoding !== "auto")
            throw new Error("所选编码无法读取文件，请切换编码。");
          label = "gb18030";
          result = new TextDecoder(label, { fatal: true }).decode(bytes);
        }
      }
      if (current) {
        setContent(result);
        setDecodedAs(label);
      }
    })().catch((e) => {
      if (current) setReadError(errorText(e));
    });
    return () => {
      current = false;
    };
  }, [file, encoding, excel]);
  useEffect(() => {
    let current = true;
    setPreview(null);
    setInspection(null);
    setError("");
    if (content == null) {
      setInspecting(false);
      return;
    }
    if (!Number.isInteger(headerRow) || headerRow < 1 || headerRow > 1000) {
      setInspecting(false);
      setError("表头行应为 1–1000 之间的整数。");
      return;
    }
    setInspecting(true);
    const timer = setTimeout(() => {
      request<Inspection>("table_import_inspect", {
        kind,
        format: excel ? "excel" : "csv",
        content,
        sheet,
        header_row: headerRow,
        delimiter,
      })
        .then((result) => {
          if (current) {
            setInspection(result);
            setMapping(result.mapping);
          }
        })
        .catch((e) => {
          if (current) setError(errorText(e));
        })
        .finally(() => {
          if (current) setInspecting(false);
        });
    }, 150);
    return () => {
      current = false;
      clearTimeout(timer);
    };
  }, [content, kind, excel, sheet, headerRow, delimiter]);
  const payload = {
    kind,
    format: excel ? "excel" : "csv",
    content,
    sheet,
    header_row: headerRow,
    delimiter,
    mapping,
    policy,
    skip_invalid: skipInvalid,
  };
  const ready =
    inspection &&
    inspection.total > 0 &&
    mapping.includes("model") &&
    mapping.includes("manufacturer") &&
    mapping.filter(Boolean).length === new Set(mapping.filter(Boolean)).size;
  async function inspectChanges() {
    setBusy(true);
    setError("");
    setPreview(null);
    try {
      setPreview(await request<Preview>("table_import_preview", payload));
    } catch (e) {
      setError(errorText(e));
    } finally {
      setBusy(false);
    }
  }
  async function commit() {
    if (!preview?.token) return;
    setBusy(true);
    setError("");
    try {
      onImported(
        await request<ImportResult>("import_commit", { token: preview.token }),
        kind,
      );
    } catch (e) {
      setError(errorText(e));
    } finally {
      setBusy(false);
    }
  }
  function template() {
    if (!inspection) return;
    const blob = new Blob(
      ["\ufeff" + inspection.fields.map((f) => f.key).join(",") + "\r\n"],
      { type: "text/csv;charset=utf-8" },
    );
    const url = URL.createObjectURL(blob);
    const link = document.createElement("a");
    link.href = url;
    link.download = titles[kind] + "导入模板.csv";
    link.click();
    setTimeout(() => URL.revokeObjectURL(url), 1000);
  }
  return (
    <Modal
      title="导入硬件参数表"
      subtitle="对应字段、核对差异，再写入产品库。"
      wide
      onClose={() => {
        if (!busy) onClose();
      }}
    >
      <fieldset className="hardware-import" disabled={busy}>
        <ol className="import-steps" aria-label="导入步骤">
          <li className={!preview ? "active" : ""}>1 · 文件与字段</li>
          <li className={preview ? "active" : ""}>2 · 差异与校验</li>
          <li>3 · 确认导入</li>
        </ol>
        {(readError || error) && (
          <div className="notice error" role="alert">
            {readError || error}
          </div>
        )}
        {!preview ? (
          <>
            <div className="import-file-heading">
              <FileSpreadsheet size={23} />
              <div>
                <strong>{file.name}</strong>
                <small>
                  {(file.size / 1024).toFixed(1)} KB
                  {decodedAs ? ` · ${decodedAs.toUpperCase()}` : ""}
                </small>
              </div>
              <label className="secondary small">
                更换文件
                <input
                  className="hidden"
                  type="file"
                  accept=".xlsx,.xls,.csv"
                  aria-label="更换参数表"
                  disabled={busy}
                  onChange={(event) => {
                    const next = event.target.files?.[0];
                    if (next) {
                      setFile(next);
                      setSheet("");
                      setHeaderRow(1);
                    }
                    event.target.value = "";
                  }}
                />
              </label>
            </div>
            <div className="import-options">
              <label>
                硬件分类
                <select
                  aria-label="导入硬件分类"
                  value={kind}
                  disabled={busy}
                  onChange={(e) => setKind(e.target.value as Kind)}
                >
                  {kinds.map((k) => (
                    <option key={k} value={k}>
                      {titles[k]}
                    </option>
                  ))}
                </select>
              </label>
              {excel ? (
                <label>
                  工作表
                  <select
                    aria-label="工作表"
                    value={sheet || inspection?.sheet || ""}
                    disabled={busy || !inspection}
                    onChange={(e) => setSheet(e.target.value)}
                  >
                    {inspection?.sheets.map((name) => (
                      <option key={name}>{name}</option>
                    ))}
                  </select>
                </label>
              ) : (
                <>
                  <label>
                    文件编码
                    <select
                      aria-label="CSV 编码"
                      value={encoding}
                      onChange={(e) => setEncoding(e.target.value)}
                    >
                      <option value="auto">自动识别</option>
                      <option value="utf-8">UTF-8</option>
                      <option value="gb18030">GB18030 / GBK</option>
                    </select>
                  </label>
                  <label>
                    分隔符
                    <select
                      aria-label="CSV 分隔符"
                      value={delimiter}
                      onChange={(e) => setDelimiter(e.target.value)}
                    >
                      <option value=",">逗号</option>
                      <option value=";">分号</option>
                      <option value={"\t"}>制表符</option>
                    </select>
                  </label>
                </>
              )}
              <label>
                表头所在行
                <input
                  aria-label="表头所在行"
                  type="number"
                  min="1"
                  max="1000"
                  step="1"
                  value={Number.isFinite(headerRow) ? headerRow : ""}
                  onChange={(e) => setHeaderRow(e.target.valueAsNumber)}
                />
              </label>
            </div>
            {inspecting ? (
              <p role="status">
                <LoaderCircle size={14} className="spin" /> 正在读取参数表…
              </p>
            ) : (
              inspection && (
                <>
                  <div className="import-section-heading">
                    <h3>
                      字段对应{" "}
                      <small>
                        {inspection.headers.length} 列 · {inspection.total}{" "}
                        条记录
                      </small>
                    </h3>
                    <button
                      className="text-button"
                      type="button"
                      onClick={template}
                    >
                      下载 CSV 表头模板
                    </button>
                  </div>
                  <p className="import-help">
                    型号和厂家为必填项。未识别列可手动对应或忽略；数值按目标字段单位填写，不自动换算单位。Excel
                    公式读取文件中已保存的结果。
                  </p>
                  {!inspection.headers.length || !inspection.total ? (
                    <p className="notice error">
                      表头或数据为空，请选择其他工作表或调整表头行。
                    </p>
                  ) : (
                    <div className="import-table-scroll">
                      <table className="import-table">
                        <thead>
                          <tr>
                            <th>原始表头</th>
                            <th>样例数据</th>
                            <th>导入到字段</th>
                          </tr>
                        </thead>
                        <tbody>
                          {inspection.headers.map((header, index) => (
                            <tr key={index}>
                              <td>
                                {header || `第 ${index + 1} 列（无表头）`}
                              </td>
                              <td
                                title={inspection.sample
                                  .map((s) => s.values[index] || "空")
                                  .join(" / ")}
                              >
                                {inspection.sample
                                  .slice(0, 2)
                                  .map((s) => s.values[index] || "空")
                                  .join(" / ")}
                              </td>
                              <td>
                                <select
                                  aria-label={`第 ${index + 1} 列字段对应`}
                                  value={mapping[index] || ""}
                                  onChange={(e) =>
                                    setMapping((values) =>
                                      values.map((v, i) =>
                                        i === index
                                          ? e.target.value || null
                                          : v,
                                      ),
                                    )
                                  }
                                >
                                  <option value="">忽略此列</option>
                                  {inspection.fields.map((f) => (
                                    <option key={f.key} value={f.key}>
                                      {f.label}
                                      {["model", "manufacturer"].includes(f.key)
                                        ? "（必填）"
                                        : ""}
                                    </option>
                                  ))}
                                </select>
                              </td>
                            </tr>
                          ))}
                        </tbody>
                      </table>
                    </div>
                  )}
                  {!ready && (
                    <p className="import-help">
                      请保证型号与厂家已对应，且每个目标字段只对应一列。
                    </p>
                  )}
                </>
              )
            )}
            <div className="import-policy">
              <label>
                同厂家、同型号记录
                <select
                  aria-label="重复型号处理"
                  value={policy}
                  onChange={(e) => setPolicy(e.target.value)}
                >
                  <option value="fill">仅补充空参数（推荐）</option>
                  <option value="skip">跳过已有型号</option>
                  <option value="replace">覆盖表中提供的参数</option>
                </select>
              </label>
              <p>空单元格不清除已有参数；当前文件内的重复型号会标为错误。</p>
            </div>
            <label className="import-skip">
              <input
                type="checkbox"
                checked={skipInvalid}
                onChange={(e) => setSkipInvalid(e.target.checked)}
              />
              跳过错误行，仅导入通过校验的记录
            </label>
          </>
        ) : (
          <>
            <div className="import-summary" aria-label="合并预览">
              <div>
                <strong>{preview.added}</strong>新增
              </div>
              <div>
                <strong>{preview.updated}</strong>更新
              </div>
              <div>
                <strong>{preview.skipped + preview.unchanged}</strong>保留原记录
              </div>
              <div>
                <strong>{preview.invalid}</strong>错误行
              </div>
            </div>
            <p className="import-help">
              处理方式：
              {policy === "fill"
                ? "仅补充空参数"
                : policy === "skip"
                  ? "跳过已有型号"
                  : "覆盖表中提供的参数"}
              。
              {skipInvalid
                ? "已选择跳过错误行。"
                : "存在错误行时整批不写入，请返回修改或选择跳过。"}
              共 {preview.total} 条记录，详情最多展示前 100 条。
            </p>
            {!!preview.errors.length && (
              <div className="import-errors" role="alert">
                <strong>需要处理的行</strong>
                <ul>
                  {preview.errors.map((e, i) => (
                    <li key={i}>
                      第 {e.row} 行：{e.message}
                    </li>
                  ))}
                </ul>
                {preview.invalid > preview.errors.length && (
                  <p>仅展示前 {preview.errors.length} 条错误。</p>
                )}
              </div>
            )}
            <div className="import-table-scroll">
              <table className="import-table import-diff">
                <thead>
                  <tr>
                    <th>原表行 / 型号</th>
                    <th>处理</th>
                    <th>参数变化</th>
                  </tr>
                </thead>
                <tbody>
                  {preview.rows.map((row) => (
                    <tr key={row.row}>
                      <td>
                        <small>
                          第 {row.row} 行 · {row.manufacturer}
                        </small>
                        <strong>{row.model}</strong>
                      </td>
                      <td>{actionNames[row.action]}</td>
                      <td>
                        {row.changes.length ? (
                          <details>
                            <summary>{row.changes.length} 项参数变化</summary>
                            <dl>
                              {row.changes.map((c) => (
                                <div key={c.key}>
                                  <dt>
                                    {inspection?.fields.find(
                                      (f) => f.key === c.key,
                                    )?.label || c.key}
                                  </dt>
                                  <dd>
                                    <del>{display(c.before)}</del>
                                    <ArrowRight size={12} />
                                    <span>{display(c.after)}</span>
                                  </dd>
                                </div>
                              ))}
                            </dl>
                          </details>
                        ) : (
                          "保留现有参数"
                        )}
                      </td>
                    </tr>
                  ))}
                </tbody>
              </table>
            </div>
            {!preview.token && !preview.invalid && (
              <p className="import-help">没有需要写入的变化。</p>
            )}
          </>
        )}
      </fieldset>
      <footer className="import-footer">
        <p>提交前自动备份。已有方案的设备快照保持不变。</p>
        <div>
          {preview ? (
            <>
              <button
                className="secondary"
                disabled={busy}
                onClick={() => {
                  setPreview(null);
                  setError("");
                }}
              >
                <ArrowLeft size={14} />
                返回调整
              </button>
              <button
                className="primary"
                disabled={busy || !preview.token}
                onClick={() => void commit()}
              >
                {busy
                  ? "正在导入…"
                  : `确认导入 ${preview.added + preview.updated} 条`}
              </button>
            </>
          ) : (
            <>
              <button className="secondary" disabled={busy} onClick={onClose}>
                取消
              </button>
              <button
                className="primary"
                disabled={busy || inspecting || !ready || !!readError}
                onClick={() => void inspectChanges()}
              >
                {busy ? "正在校验…" : "检查导入差异"}
                <ArrowRight size={14} />
              </button>
            </>
          )}
        </div>
      </footer>
    </Modal>
  );
}
