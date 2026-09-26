export type Kind = "camera" | "lens" | "light" | "three_d";
export type Mode = "imaging" | "scanning";
export type Status = "passed" | "failed" | "unknown" | "not_applicable";
export interface Hardware {
  id: string;
  kind: Kind;
  manufacturer: string;
  model: string;
  specs: Record<string, unknown>;
  origin: string;
}
export interface Parameters {
  width: number;
  height: number;
  margin: number;
  pixel: number;
  distance: number;
  fps: number;
  speed: number;
  exposure: number;
  blur: number;
  variation: number;
  dof_confirmed: boolean;
  measured: boolean;
  measured_width: number;
  measured_height: number;
  pixel_format: string;
  scan_width: number;
  z_range: number;
  z_repeat: number;
  scan_distance: number | null;
  scan_length: number;
  interval: number;
  scan_speed: number;
  rate: number;
  safety: number;
  scan_exposure: number;
  readout: number;
  trigger: string;
  travel: number;
  pulses: number;
  per_profile: number;
  pulse_rate: number;
}
export interface Reference {
  name: string;
  mode: Mode;
  parameters: Parameters;
  hardware: (Hardware | null)[];
}
export interface Project extends Reference {
  version: number;
  id: string;
  reference: Reference | null;
}
export interface Check {
  key: string;
  title: string;
  status: Status;
  detail: string;
  actual: number | null;
  target: number | null;
  unit: string;
}
export interface Metric {
  key: string;
  label: string;
  value: number | null;
  unit: string;
}
export interface Evaluation {
  checks: Check[];
  metrics: Metric[];
  fov_x: number | null;
  fov_y: number | null;
  passed: number;
  failed: number;
  unknown: number;
  model_note: string;
}
export interface SavedProject {
  id: string;
  name: string;
  updated_at: string;
}
export interface Bootstrap {
  counts: Record<Kind, number>;
  projects: SavedProject[];
  default_project: Project;
  data_directory: string;
}
export interface CatalogPage {
  kind: Kind;
  items: Hardware[];
  total: number;
  category_total: number;
  brands: string[];
  offset: number;
  limit: number;
  filter_schema: CatalogFilterDefinition[];
}
export interface CatalogFilterDefinition {
  key: string;
  label: string;
  unit: string;
  control: "choice" | "range";
  primary: boolean;
  options: { value: string | null; label: string; count: number }[];
  min: number | null;
  max: number | null;
  known_count: number;
}
export const kinds: Kind[] = ["camera", "lens", "light", "three_d"];
export const titles: Record<Kind, string> = {
  camera: "面阵相机",
  lens: "成像镜头",
  light: "照明光源",
  three_d: "3D 相机",
};
export const statusText: Record<Status, string> = {
  passed: "通过",
  failed: "冲突",
  unknown: "待确认",
  not_applicable: "不适用",
};
export const defaultParameters: Parameters = {
  width: 20,
  height: 20,
  margin: 2,
  pixel: 10,
  distance: 110,
  fps: 20,
  speed: 0,
  exposure: 100,
  blur: 1,
  variation: 2,
  dof_confirmed: false,
  measured: false,
  measured_width: 24,
  measured_height: 24,
  pixel_format: "",
  scan_width: 40,
  z_range: 10,
  z_repeat: 10,
  scan_distance: null,
  scan_length: 300,
  interval: 0.05,
  scan_speed: 40,
  rate: 1000,
  safety: 0.8,
  scan_exposure: 100,
  readout: 3,
  trigger: "free",
  travel: 10,
  pulses: 10000,
  per_profile: 50,
  pulse_rate: 50000,
};
export function newProject(): Project {
  return {
    version: 1,
    id: crypto.randomUUID(),
    name: "未命名方案",
    mode: "imaging",
    parameters: { ...defaultParameters },
    hardware: [null, null, null, null],
    reference: null,
  };
}
