import {
  useCallback,
  useEffect,
  useId,
  useLayoutEffect,
  useRef,
  useState,
} from "react";
import {
  ArrowRight,
  Camera,
  ChevronLeft,
  ChevronRight,
  CircleDot,
  Database,
  Lightbulb,
  LoaderCircle,
  Search,
  ScanLine,
  SlidersHorizontal,
  Upload,
} from "lucide-react";
import { request, errorText } from "../api";
import {
  kinds,
  titles,
  type CatalogPage,
  type CatalogFilterDefinition,
  type Hardware,
  type Kind,
} from "../types";
import { format, spec, specificationLabels, summary } from "../format";
import {
  CatalogFilters,
  catalogFilterError,
  catalogFilterPayload,
  emptyCatalogFilters,
  type CatalogFilterDraft,
} from "./CatalogFilters";
export const hardwareIcons = {
  camera: Camera,
  lens: CircleDot,
  light: Lightbulb,
  three_d: ScanLine,
};
export function HardwareLibrary({
  initialKind,
  onChoose,
  onNotice,
  onImported,
}: {
  initialKind: Kind;
  onChoose: (h: Hardware) => void;
  onNotice: (text: string, error?: boolean) => void;
  onImported: () => void;
}) {
  const [kind, setKind] = useState(initialKind);
  const [filtersByKind, setFiltersByKind] = useState<
    Record<Kind, CatalogFilterDraft>
  >(
    () =>
      Object.fromEntries(
        kinds.map((k) => [k, emptyCatalogFilters()]),
      ) as Record<Kind, CatalogFilterDraft>,
  );
  const filters = filtersByKind[kind];
  const { search, manufacturer: brand } = filters;
  const [metadata, setMetadata] = useState<
    Partial<
      Record<
        Kind,
        { schema: CatalogFilterDefinition[]; brands: string[]; total: number }
      >
    >
  >({});
  const schema = metadata[kind]?.schema ?? [];
  const [moreOpen, setMoreOpen] = useState(false);
  const closeMore = useCallback(() => setMoreOpen(false), []);
  const moreButton = useRef<HTMLButtonElement>(null);
  const moreId = useId();
  const [offset, setOffset] = useState(0);
  const [page, setPage] = useState<CatalogPage | null>(null);
  const [selected, setSelected] = useState<Hardware | null>(null);
  const [fetching, setFetching] = useState(true);
  const [requestError, setError] = useState("");
  const [loadedKey, setLoadedKey] = useState("");
  const [revision, setRevision] = useState(0);
  const [preview, setPreview] = useState<{
    token: string;
    added: number;
    updated: number;
  } | null>(null);
  const [importing, setImporting] = useState(false);
  const input = useRef<HTMLInputElement>(null);
  const tableScroll = useRef<HTMLDivElement>(null);
  useLayoutEffect(() => {
    if (tableScroll.current) tableScroll.current.scrollTop = 0;
  }, [page]);
  const [importNotice, setImportNotice] = useState<{
    text: string;
    error: boolean;
  } | null>(null);
  const filterError = catalogFilterError(filters, schema);
  const query = {
    kind,
    search,
    manufacturer: brand,
    filters: catalogFilterPayload(filters),
    offset,
    limit: 60,
  };
  const queryKey = JSON.stringify({ ...query, revision });
  const loading = !filterError && (fetching || loadedKey !== queryKey);
  const error = filterError || (loadedKey === queryKey ? requestError : "");
  function changeFilters(patch: Partial<CatalogFilterDraft>) {
    setFiltersByKind((previous) => ({
      ...previous,
      [kind]: { ...previous[kind], ...patch },
    }));
    setOffset(0);
    setSelected(null);
  }
  const setSearch = (value: string) => changeFilters({ search: value });
  const setBrand = (value: string) => changeFilters({ manufacturer: value });
  useEffect(() => {
    let current = true;
    setFetching(!filterError);
    setSelected(null);
    setError("");
    if (filterError) return;
    const timeout = setTimeout(() => {
      request<CatalogPage>("catalog", query)
        .then((data) => {
          if (current) {
            setPage(data);
            setLoadedKey(queryKey);
            setMetadata((previous) => ({
              ...previous,
              [kind]: {
                schema: data.filter_schema,
                brands: data.brands,
                total: data.category_total,
              },
            }));
          }
        })
        .catch((e) => {
          if (current) {
            setError(errorText(e));
            setLoadedKey(queryKey);
          }
        })
        .finally(() => {
          if (current) setFetching(false);
        });
    }, 130);
    return () => {
      current = false;
      clearTimeout(timeout);
    };
  }, [queryKey, filterError]);
  async function previewFile(file: File) {
    setImporting(true);
    setPreview(null);
    setImportNotice(null);
    try {
      if (file.size > 32 * 1024 * 1024)
        throw new Error("文件超过 32 MB 限制。");
      const extension = file.name.split(".").pop()?.toLowerCase();
      let content: string;
      let type: string;
      if (extension === "db" || extension === "sqlite") {
        const bytes = new Uint8Array(await file.arrayBuffer());
        let binary = "";
        for (let i = 0; i < bytes.length; i += 16384)
          binary += String.fromCharCode(...bytes.subarray(i, i + 16384));
        content = btoa(binary);
        type = "legacy";
      } else {
        content = await file.text();
        type = extension === "json" ? "json" : "csv";
      }
      setPreview(
        await request("import_preview", { kind, format: type, content }),
      );
    } catch (e) {
      setImportNotice({ text: errorText(e), error: true });
    } finally {
      setImporting(false);
    }
  }
  async function commit() {
    if (!preview) return;
    setImporting(true);
    try {
      const result = await request<{ imported: number; backup: string }>(
        "import_commit",
        { token: preview.token },
      );
      setPreview(null);
      setRevision((v) => v + 1);
      setOffset(0);
      onImported();
      onNotice(
        "已合并 " + result.imported + " 条设备，原库备份：" + result.backup,
      );
      setImportNotice({
        text: "已合并 " + result.imported + " 条设备。原库已自动备份。",
        error: false,
      });
    } catch (e) {
      setImportNotice({ text: errorText(e), error: true });
    } finally {
      setImporting(false);
    }
  }
  return (
    <div className="library">
      {importNotice && (
        <div
          className={"notice " + (importNotice.error ? "error" : "")}
          role={importNotice.error ? "alert" : "status"}
        >
          {importNotice.text}
        </div>
      )}
      <div className="library-toolbar">
        <div className="category-tabs" role="tablist" aria-label="硬件分类">
          {kinds.map((k) => {
            const Icon = hardwareIcons[k];
            return (
              <button
                role="tab"
                aria-selected={kind === k}
                className={kind === k ? "active" : ""}
                key={k}
                onClick={() => {
                  setKind(k);
                  setMoreOpen(false);
                  setSelected(null);
                  setOffset(0);
                  setPreview(null);
                }}
              >
                <Icon size={16} />
                {titles[k]}
              </button>
            );
          })}
        </div>
        <button
          className="secondary small"
          onClick={() => input.current?.click()}
          disabled={importing}
        >
          <Upload size={15} />
          导入硬件
        </button>
        <input
          ref={input}
          className="hidden"
          type="file"
          accept=".csv,.json,.db,.sqlite"
          aria-label="导入硬件文件"
          onChange={(e) => {
            const file = e.target.files?.[0];
            if (file) void previewFile(file);
            e.target.value = "";
          }}
        />
      </div>
      {preview && (
        <div className="import-preview">
          <div>
            <strong>合并预览</strong>
            <span>
              新增 {preview.added} 条 / 更新 {preview.updated}{" "}
              条。提交前自动备份，当前方案快照不变。
            </span>
          </div>
          <button className="secondary small" onClick={() => setPreview(null)}>
            取消
          </button>
          <button
            className="primary small"
            disabled={importing}
            onClick={() => void commit()}
          >
            确认合并
          </button>
        </div>
      )}
      <div className="filter-row">
        <label className="search-box">
          <Search size={17} />
          <input
            value={search}
            onChange={(e) => {
              setSearch(e.target.value);
              setOffset(0);
            }}
            placeholder="搜索型号、厂家或规格…"
            aria-label="搜索硬件"
          />
          {search && (
            <button
              onClick={() => {
                setSearch("");
                setOffset(0);
              }}
              aria-label="清空搜索"
            >
              ×
            </button>
          )}
        </label>
        <select
          aria-label="筛选厂家"
          value={brand}
          onChange={(e) => {
            setBrand(e.target.value);
            setOffset(0);
          }}
        >
          <option value="">全部厂家</option>
          {metadata[kind]?.brands.map((b) => (
            <option key={b}>{b}</option>
          ))}
        </select>
        <button
          ref={moreButton}
          className={
            "secondary small catalog-more-button" + (moreOpen ? " active" : "")
          }
          disabled={!schema.length}
          aria-expanded={moreOpen}
          aria-controls={moreId}
          aria-label="更多筛选"
          onClick={() => setMoreOpen((v) => !v)}
        >
          <SlidersHorizontal size={15} />
          更多筛选
        </button>
        <span className="subtle catalog-match-count" aria-live="polite">
          {error
            ? "条件待修正"
            : loading
              ? "筛选中…"
              : `${format(page?.total, 0)} / ${format(metadata[kind]?.total, 0)} 条设备`}
        </span>
      </div>
      <CatalogFilters
        schema={schema}
        draft={filters}
        onChange={changeFilters}
        open={moreOpen}
        onClose={closeMore}
        trigger={moreButton}
        panelId={moreId}
      />
      {filterError && (
        <p className="catalog-filter-error" role="alert">
          {filterError}
        </p>
      )}
      <div className="library-body">
        <div className="library-results" aria-busy={loading}>
          <div
            ref={tableScroll}
            className="hardware-table-wrap"
            role="region"
            aria-label="硬件设备列表"
            tabIndex={0}
          >
            <table className="hardware-table">
              <thead>
                <tr>
                  <th>厂家 / 型号</th>
                  <th>核心规格</th>
                  <th>接口 / 类型</th>
                  <th></th>
                </tr>
              </thead>
              <tbody>
                {!loading &&
                  !error &&
                  page?.items.map((item) => (
                    <tr
                      key={item.id}
                      className={selected?.id === item.id ? "selected" : ""}
                      onClick={() => setSelected(item)}
                      onDoubleClick={() => {
                        if (!loading && !error) onChoose(item);
                      }}
                    >
                      <td>
                        <button
                          className="model-link"
                          onClick={() => setSelected(item)}
                        >
                          <strong>{item.model}</strong>
                          <span>{item.manufacturer}</span>
                        </button>
                      </td>
                      <td>{summary(item)}</td>
                      <td>
                        {spec(
                          item,
                          item.kind === "camera"
                            ? "interface"
                            : item.kind === "lens"
                              ? "lens_mount"
                              : item.kind === "light"
                                ? "light_type"
                                : "interfaces",
                        ) || "未公开"}
                      </td>
                      <td>
                        <ChevronRight size={14} />
                      </td>
                    </tr>
                  ))}
              </tbody>
            </table>
            {loading && (
              <div className="empty-state">
                <LoaderCircle className="spin" size={24} />
                <p>读取硬件资料…</p>
              </div>
            )}
            {!loading && (error || !page?.items.length) && (
              <div className="empty-state">
                <Search size={30} />
                <h3>{error || "没有找到匹配设备"}</h3>
                <p>
                  {filterError
                    ? "修正范围后自动重新筛选。"
                    : "可放宽规格范围、更换厂家，或清除已选条件。"}
                </p>
                {!error && (
                  <button
                    className="secondary small"
                    onClick={() => changeFilters(emptyCatalogFilters())}
                  >
                    显示全部设备
                  </button>
                )}
              </div>
            )}
          </div>
          <footer className="pagination">
            <span>
              {!loading && !error && page?.total
                ? offset +
                  1 +
                  "–" +
                  (offset + page.items.length) +
                  " / " +
                  format(page.total, 0)
                : "0 条结果"}
            </span>
            <div>
              <button
                className="secondary small"
                disabled={offset === 0 || loading || !!error}
                aria-label="上一页"
                onClick={() => setOffset((v) => Math.max(0, v - 60))}
              >
                <ChevronLeft size={17} />
                上一页
              </button>
              <button
                className="secondary small"
                disabled={
                  loading || !!error || offset + 60 >= (page?.total ?? 0)
                }
                aria-label="下一页"
                onClick={() => setOffset((v) => v + 60)}
              >
                下一页
                <ChevronRight size={17} />
              </button>
            </div>
          </footer>
        </div>
        <aside className="hardware-detail">
          {selected ? (
            <>
              <div
                key={selected.id}
                className="hardware-detail-scroll"
                role="region"
                aria-label="设备详细信息"
                tabIndex={0}
              >
                <div className="detail-heading">
                  <span className="eyebrow">
                    {titles[selected.kind]} / {selected.origin}
                  </span>
                  <h3>{selected.model}</h3>
                  <p>{selected.manufacturer}</p>
                </div>
                <dl className="specification-list" aria-label="设备详细规格">
                  {Object.entries(specificationLabels)
                    .filter(([key]) => key in selected.specs)
                    .map(([key, title]) => {
                      const value = selected.specs[key];
                      const content =
                        typeof value === "boolean"
                          ? value
                            ? "是"
                            : "否"
                          : spec(selected, key);
                      return (
                        <div key={key}>
                          <dt>{title}</dt>
                          <dd>
                            {content === "" ||
                            (Number(content) <= 0 &&
                              ![
                                "distortion_percent",
                                "telecentricity_deg",
                              ].includes(key))
                              ? "未公开 / 不适用"
                              : content}
                          </dd>
                        </div>
                      );
                    })}
                </dl>
              </div>
              <div className="detail-action">
                <button
                  className="primary"
                  disabled={loading || !!error}
                  onClick={() => {
                    if (!loading && !error) onChoose(selected);
                  }}
                >
                  加入当前方案
                  <ArrowRight size={16} />
                </button>
                <p>设备以参数快照加入，硬件原始数据不会改变。</p>
              </div>
            </>
          ) : (
            <div className="empty-state detail-empty">
              <Database size={32} strokeWidth={1.3} />
              <h3>先了解，再选择</h3>
              <p>选择一行查看型号、公开规格和数据来源，然后加入当前方案。</p>
            </div>
          )}
        </aside>
      </div>
      <p className="library-note">
        支持当前分类的 CSV、3D 相机 JSON 或旧版 SQLite
        产品库。导入采用合并模式，保留原始文件。
      </p>
    </div>
  );
}
