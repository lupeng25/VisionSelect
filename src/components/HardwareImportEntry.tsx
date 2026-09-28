import { useEffect, useRef, useState } from "react";
import { FileSpreadsheet, Upload, Download, LoaderCircle } from "lucide-react";
import { invoke, isTauri } from "@tauri-apps/api/core";
import { request, errorText } from "../api";
import { kinds, titles, type Kind } from "../types";
import { Modal } from "./Modal";

export function HardwareImportEntry({
  kind: initialKind,
  onClose,
  onFile,
}: {
  kind: Kind;
  onClose: () => void;
  onFile: (file: File, kind: Kind) => void;
}) {
  const [kind, setKind] = useState(initialKind);
  const [fields, setFields] = useState<{ key: string; label: string }[]>([]);
  const [error, setError] = useState("");
  const [loading, setLoading] = useState(true);
  const [reading, setReading] = useState(false);
  const [dragging, setDragging] = useState(false);
  const input = useRef<HTMLInputElement>(null);
  const receiver = useRef(onFile);
  receiver.current = onFile;
  function accept(file: File) {
    if (!/\.(xlsx|xls|csv|json|db|sqlite)$/i.test(file.name)) {
      setError("请选择 Excel、CSV、三维 JSON 或旧数据库文件。");
      return;
    }
    if (file.size > 32 * 1024 * 1024) {
      setError("文件超过 32 MB 限制。");
      return;
    }
    receiver.current(file, kind);
  }
  useEffect(() => {
    let current = true;
    setLoading(true);
    setFields([]);
    setError("");
    request<{ key: string; label: string }[]>("table_import_fields", { kind })
      .then((result) => {
        if (current) setFields(result);
      })
      .catch((e) => {
        if (current) setError(errorText(e));
      })
      .finally(() => {
        if (current) setLoading(false);
      });
    return () => {
      current = false;
    };
  }, [kind]);
  useEffect(() => {
    if (!isTauri()) return;
    let disposed = false;
    let inFlight = false;
    let release: (() => void) | undefined;
    void import("@tauri-apps/api/webview")
      .then(async ({ getCurrentWebview }) => {
        const unlisten = await getCurrentWebview().onDragDropEvent((event) => {
          if (disposed || inFlight) return;
          if (event.payload.type !== "drop") {
            setDragging(
              event.payload.type === "enter" || event.payload.type === "over",
            );
            return;
          }
          setDragging(false);
          if (event.payload.paths.length !== 1) {
            setError("每次请选择一个文件。");
            return;
          }
          inFlight = true;
          setReading(true);
          setError("");
          void invoke<{ name: string; content: string }>(
            "read_hardware_import_file",
            { path: event.payload.paths[0] },
          )
            .then((result) => {
              if (disposed) return;
              const bytes = Uint8Array.from(atob(result.content), (char) =>
                char.charCodeAt(0),
              );
              receiver.current(new File([bytes], result.name), kind);
            })
            .catch((e) => {
              if (!disposed) setError(errorText(e));
            })
            .finally(() => {
              inFlight = false;
              if (!disposed) setReading(false);
            });
        });
        if (disposed) unlisten();
        else release = unlisten;
      })
      .catch((e) => {
        if (!disposed)
          setError("拖拽暂不可用，请点击选择文件。" + errorText(e));
      });
    return () => {
      disposed = true;
      release?.();
    };
  }, [kind]);
  function template() {
    const blob = new Blob(
      ["\ufeff" + fields.map((f) => f.key).join(",") + "\r\n"],
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
      title="导入硬件"
      subtitle="先选择导入方式，再核对字段与参数差异。"
      onClose={onClose}
    >
      <div
        className="import-entry"
        onDragOver={(e) => {
          e.preventDefault();
          e.dataTransfer.dropEffect = "copy";
          setDragging(true);
        }}
        onDragLeave={(e) => {
          if (
            !(e.relatedTarget instanceof Node) ||
            !e.currentTarget.contains(e.relatedTarget)
          )
            setDragging(false);
        }}
        onDrop={(e) => {
          e.preventDefault();
          setDragging(false);
          if (reading) return;
          if (e.dataTransfer.files.length !== 1) {
            setError("每次请选择一个文件。");
            return;
          }
          accept(e.dataTransfer.files[0]);
        }}
      >
        {error && (
          <p className="notice error" role="alert">
            {error}
          </p>
        )}
        <label className="import-entry-kind">
          导入到分类
          <select
            aria-label="导入到分类"
            value={kind}
            disabled={reading}
            onChange={(e) => setKind(e.target.value as Kind)}
          >
            {kinds.map((k) => (
              <option key={k} value={k}>
                {titles[k]}
              </option>
            ))}
          </select>
        </label>
        <div className={"import-drop-zone" + (dragging ? " dragging" : "")}>
          <span className="import-entry-icon">
            <FileSpreadsheet size={32} />
          </span>
          <h3>{reading ? "正在读取文件…" : "拖入硬件参数文件"}</h3>
          <p>Excel / CSV 参数表，可选择工作表、对应中文字段。</p>
          <button
            className="primary"
            disabled={reading}
            onClick={() => input.current?.click()}
          >
            {reading ? (
              <LoaderCircle size={16} className="spin" />
            ) : (
              <Upload size={16} />
            )}
            选择文件
          </button>
          <input
            ref={input}
            type="file"
            className="hidden"
            aria-label="选择导入文件"
            accept=".xlsx,.xls,.csv,.json,.db,.sqlite"
            onChange={(e) => {
              const file = e.target.files?.[0];
              if (file) accept(file);
              e.target.value = "";
            }}
          />
          <small>
            支持 XLSX、XLS、CSV；也可导入三维 JSON、旧 SQLite 库。单文件不超过
            32 MB。
          </small>
        </div>
        <div className="import-entry-template">
          <div>
            <strong>还没有参数表？</strong>
            <p>下载{titles[kind]} CSV 表头模板，再用 Excel 填写。</p>
          </div>
          <button
            className="secondary small"
            disabled={loading || !fields.length}
            onClick={template}
          >
            <Download size={14} />
            {loading ? "加载模板…" : "下载模板"}
          </button>
        </div>
        <p className="import-entry-note">
          下一步会展示字段对应、错误行及参数变化，确认后才写入；导入前自动备份。
        </p>
      </div>
    </Modal>
  );
}
