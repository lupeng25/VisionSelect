// 硬件算法核对：通过真实 Rust API 记录当前结果及独立基准，不修改硬件或方案。
// 运行前使用独立目录 .codex_tmp/algorithm-audit/data 启动 vision-server。
import assert from 'node:assert/strict';
import { createHash } from 'node:crypto';
import { readFile, writeFile, mkdir } from 'node:fs/promises';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const temporary = path.join(root, '.codex_tmp', 'algorithm-audit');
const calls = [];
const cases = [];
async function api(operation, payload = {}) {
  const response = await fetch(`http://127.0.0.1:4318/api/${operation}`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(payload),
    signal: AbortSignal.timeout(15000),
  });
  const result = await response.json();
  assert.ok(response.ok, `${operation}：${JSON.stringify(result)}`);
  calls.push({ operation, payload, result });
  return result;
}
const bootstrap = await api('bootstrap');
assert.equal(path.resolve(bootstrap.data_directory).toLowerCase(),
  path.join(temporary, 'data').toLowerCase(), '请使用专用审计资料库');
assert.deepEqual(bootstrap.counts, { camera: 1393, lens: 1004, light: 2180, three_d: 267 });
const makeProject = (parameters = {}, hardware = [null, null, null, null], mode = 'imaging') => ({
  ...structuredClone(bootstrap.default_project), mode, name: '硬件算法核对', hardware,
  parameters: { ...bootstrap.default_project.parameters, ...parameters },
});
const hardware = (kind, specs) => ({
  id: `audit:${kind}`, kind, model: `审计构造${kind}`, manufacturer: '数学基准',
  origin: '隔离审计构造数据', specs,
});
const check = (result, key) => {
  const found = result.checks.find(item => item.key === key);
  assert.ok(found, `缺少校核项 ${key}`);
  return found;
};
const metric = (result, key) => {
  const found = result.metrics.find(item => item.key === key);
  assert.ok(found, `缺少指标 ${key}`);
  return found;
};
function record(id, title, category, actual, expected, note) {
  const agrees = typeof actual === 'number' && typeof expected === 'number'
    ? Math.abs(actual - expected) <= 1e-9 * Math.max(Math.abs(expected), 1)
    : JSON.stringify(actual) === JSON.stringify(expected);
  cases.push({ 编号: id, 项目: title, 分类: category, 当前结果: actual,
    独立基准: expected, 一致: agrees, 说明: note });
}
async function lookup(kind, model) {
  const result = await api('catalog', { kind, search: model, limit: 100 });
  const found = result.items.find(item => item.model === model);
  assert.ok(found, `未找到真实型号 ${model}`);
  return found;
}

// 真实目录中的全部混合快门记录，逐项验证运动快门结论。
const grrCatalog = await api('catalog', { kind: 'camera', search: 'GlobalReset', limit: 100 });
assert.ok(grrCatalog.total <= grrCatalog.items.length, '混合快门查询需要翻页');
const grrCameras = grrCatalog.items.filter(item => /globalreset/i.test(item.specs.shutter_type));
const grrResults = [];
for (const camera of grrCameras) {
  const result = await api('evaluate', { project: makeProject(
    { speed: 100, exposure: 50, fps: 10, measured: true, measured_width: 20, measured_height: 15 },
    [camera, null, null, null],
  ) });
  grrResults.push({ 型号: camera.model, 厂家: camera.manufacturer,
    目录快门: camera.specs.shutter_type, 状态: check(result, 'shutter').status });
}
record('A1', '混合快门被无条件判定通过', '确认缺陷',
  grrResults.filter(item => item.状态 === 'passed').length, 0,
  `遍历 ${grrResults.length} 条真实目录记录；GRR 需要额外闪光及同步条件，尚未建模时应待确认。`);
const unknownCamera = await lookup('camera', 'OPT-CC1-C250-UG3-01');
const unknownResult = await api('evaluate', { project: makeProject({ speed: 100 }, [unknownCamera, null, null, null]) });
record('A2', '目录快门 Unknown 被判冲突', '确认缺陷',
  check(unknownResult, 'shutter').status, 'unknown', '未知能力应与已知不满足区分。');

// 固定焦距近似与独立计算页的置信度应一致。
const camera = hardware('camera', { resolution_x: 2000, resolution_y: 1000,
  pixel_size_um: 5, lens_mount: 'C', max_fps: 100, shutter_type: 'Global' });
const lens = hardware('lens', { lens_type: 'FixedFocal', focal_length_mm: 25,
  lens_mount: 'C', image_circle_mm: 15, min_wd_mm: 100 });
const opticalProject = makeProject({ width: 75, height: 35, margin: 0, pixel: 100, distance: 200 }, [camera, lens, null, null]);
const opticalResult = await api('evaluate', { project: opticalProject });
const opticalCalculation = await api('calculate_2d', { input: {
  kind: 'optics', method: 'fov', resolution_x: 2000, resolution_y: 1000,
  pixel_size_um: 5, focal_mm: 25, distance_mm: 200,
} });
record('B1', '高倍率定焦估算的置信度不一致', '模型状态问题',
  check(opticalResult, 'fov_x').status, opticalCalculation.status,
  '同一组输入：倍率近似 0.125，工程工作台视场通过，独立页为待确认。');
record('B2', '有限共轭薄透镜示例', '近似适用边界', opticalResult.fov_x,
  10 * (200 - 25) / 25,
  '单位 mm。仅在理想薄透镜、200 mm 为主平面物距时，基准视场为 70 mm；不能将该值当成任意真实镜头的实测视场。');

// 同一台真实轮廓相机，保持速度一致，分别使用定时和编码器触发。
const scanner = await lookup('three_d', 'Gocator 2320');
const scanBase = { scan_length: 300, interval: 0.05, scan_speed: 40,
  rate: 1000, safety: 0.8, scan_exposure: 100, readout: 3 };
const scanning = async parameters => api('evaluate', {
  project: makeProject({ ...scanBase, ...parameters }, [null, null, null, scanner], 'scanning'),
});
const timed = await scanning({ trigger: 'free' });
record('C1', '定时扫描的实际轮廓数量', '确认缺陷', metric(timed, 'profiles').value,
  300 / 40 * 1000, '匀速、起点触发、区间 [0,300) mm；实际间距 0.04 mm，目标上限 0.05 mm。');
const encoded = await scanning({ trigger: 'encoder', travel: 100, pulses: 4000,
  per_profile: 1, pulse_rate: 1600 });
record('C2', '编码器扫描的实际轮廓数量', '确认缺陷', metric(encoded, 'profiles').value,
  300 / (100 / 4000), '实际间距 0.025 mm，编码器速度 40 mm/s；以空间触发间距计算 [0,300) 内数量。');
const large = await scanning({ trigger: 'encoder', scan_length: 3000, travel: 100,
  pulses: 4000, per_profile: 1, pulse_rate: 1600 });
record('C3', '轮廓数量低估导致容量提醒缺失', '确认缺陷', large.checks.some(item => item.key === 'buffer'),
  true, '3000 mm 扫描实际 120000 条，超过代码自己的十万条提醒阈值；旧版计算 60000 条。');
const margin = await scanning({ trigger: 'encoder', travel: 100, pulses: 2000,
  per_profile: 1, pulse_rate: 800 });
record('C4', '编码器安全系数的工程语义', '策略待明确', check(margin, 'scan_speed').status,
  'passed', '基准仅校核物理采样能力：实际间距等于目标 0.05 mm、频率 800 Hz，速度 40 mm/s；旧版 0.8 系数将速度上限降为 32 mm/s。该系数对应实际间距不超过目标的 80%；修复后建议裕量单独提示，物理约束应通过。');

// 原厂 Gocator 2300 数据表第 2 页：CD 对应近端视场，不能配对中间视场。
const fovMappingResults = [];
for (const [model, clearance, nearFov] of [
  ['Gocator 2320', 40, 18], ['Gocator 2330', 90, 47], ['Gocator 2340', 190, 96],
  ['Gocator 2350', 300, 158], ['Gocator 2370', 400, 308],
  ['Gocator 2375', 650, 324], ['Gocator 2380', 350, 390],
]) {
  const item = model === scanner.model ? scanner : await lookup('three_d', model);
  const wanted = nearFov + 2;
  const result = await api('evaluate', { project: makeProject({ ...scanBase,
    scan_distance: clearance, scan_width: wanted }, [null, null, null, item], 'scanning') });
  const coverage = check(result, 'scan_fov');
  fovMappingResults.push({ 型号: model, 原厂近端距离mm: clearance, 原厂近端视场mm: nearFov,
    需求宽度mm: wanted, 程序采用视场mm: coverage.actual, 当前覆盖状态: coverage.status,
    应有覆盖状态: 'failed' });
}
record('C5', '三维资料把近端距离与中间视场错误配对', '确认资料映射缺陷',
  fovMappingResults.filter(item => item.当前覆盖状态 === 'passed').length, 0,
  '已逐列查看同一份原厂规格表及 CD/近端示意图，对表内七个型号复核。2320 在 40 mm 处视场应为 18 mm；旧版取 22 mm，错误通过 20 mm 宽度。');

// 正确公式的独立数字基准，包含两轴、单位、打包位深和未知值边界。
const resolution = await api('calculate_2d', { input: { kind: 'resolution',
  resolution_x: 2000, resolution_y: 1000, pixel_size_um: 5,
  object_width: 20, object_height: 15, margin: 1, pixel_limit: 20 } });
record('D1', '双轴几何采样', '基础公式', metric(resolution, 'sampling').value, 17,
  'max(22/2000,17/1000) × 1000 = 17 μm/px；同时覆盖两轴。');
const telecentric = await api('calculate_2d', { input: { kind: 'optics', method: 'telecentric_fov',
  resolution_x: 2000, resolution_y: 1000, pixel_size_um: 5, magnification: 0.5 } });
record('D2', '远心镜头固定倍率视场', '基础公式',
  [metric(telecentric, 'fov').value, metric(telecentric, 'fov').other], [20, 10],
  '传感器 10 × 5 mm，倍率 0.5，视场 20 × 10 mm。');
const motion = await api('calculate_2d', { input: { kind: 'motion', sampling_um: 10,
  speed: 100, exposure: 100, blur: 1, fps: 50 } });
record('D3', '匀速运动拖影单位换算', '基础公式', metric(motion, 'motion_blur').value, 1,
  '100 mm/s × 100 μs = 10 μm 位移，除以 10 μm/px 得 1 px。');
for (const [format, bytesPerPixel] of [['Mono8', 1], ['Mono12p', 1.5], ['Mono12', 2], ['RGB8', 3]]) {
  const result = await api('calculate_2d', { input: { kind: 'data', resolution_x: 1920,
    resolution_y: 1200, pixel_format: format, fps: 30, duration_seconds: 60,
    bandwidth_mbps: 120, utilization_percent: 80 } });
  const expectedPayload = 1920 * 1200 * bytesPerPixel * 30 / 1e6;
  record(`D4-${format}`, `${format} 图像载荷`, '基础公式', metric(result, 'payload').value,
    expectedPayload, '仅图像数据，单位 MB/s；不包含协议、额外行填充、Chunk 和文件头。');
  record(`D5-${format}`, `${format} 存储量`, '基础公式', metric(result, 'storage').value,
    expectedPayload * 60 / 1000, '持续 60 秒，十进制 GB。');
}
record('D6', '编码器实际间距', '基础公式', check(encoded, 'encoder_spacing').actual, 0.025,
  '100 mm / 4000 脉冲 × 1 脉冲/轮廓。');
record('D7', '编码器与轴速度一致性', '基础公式', check(encoded, 'encoder_speed').actual, 40,
  '1600 Hz × 100 mm / 4000 脉冲。');
const light = (await api('catalog', { kind: 'light', limit: 1 })).items[0];
const lightResult = await api('evaluate', { project: makeProject({}, [camera, lens, light, null]) });
record('D8', '照明缺少实拍依据时保持待确认', '功能边界', check(lightResult, 'illumination').status,
  'unknown', '代码没有根据光源外形或发光面尺寸推导照度、均匀性或有效照明面积。');

const sourceFiles = ['crates/vision-core/src/engine.rs', 'crates/vision-core/src/calculator.rs',
  'crates/vision-core/src/catalog_corrections.rs', 'crates/vision-core/src/catalog.rs', 'crates/vision-core/src/store.rs',
  'crates/vision-core/src/model.rs', 'resources/data/cameras.csv', 'resources/data/lenses.csv',
  'resources/data/lights.csv', 'resources/data/three_d_cameras.json'];
const hashes = [];
for (const file of sourceFiles) {
  hashes.push({ 文件: file, SHA256: createHash('sha256').update(await readFile(path.join(root, file))).digest('hex') });
}
const rawThreeD = JSON.parse(await readFile(path.join(root, 'resources/data/three_d_cameras.json'), 'utf8')).cameras;
const coverageFields = ['scanRateMaxHz', 'zRepeatabilityUm', 'profileDataIntervalUm',
  'exposureTimeMinUs', 'exposureTimeMaxUs', 'encoderRateMaxHz', 'supportsEncoder', 'supportsExternalTrigger'];
const fieldCoverage = Object.fromEntries(coverageFields.map(key => [key,
  rawThreeD.filter(item => item[key] !== undefined && item[key] !== null && item[key] !== '').length,
]));
const evidence = { 核对时间: new Date().toISOString(), 方式: 'Google 厂商资料核对 + 真实 Rust API 数值复现',
  说明: '本文件记录运行时结果；不一致包括确认缺陷、模型适用边界及策略差异，并不全部等价于公式错误。',
  目录数量: bootstrap.counts, 源文件摘要: hashes, 混合快门记录: grrResults,
  三维字段覆盖数量: fieldCoverage, 三维近端视场对照: fovMappingResults,
  三维样本: { 型号: scanner.model, 厂家: scanner.manufacturer, 最大目录轮廓频率: scanner.specs.scanRateMaxHz,
    来源: scanner.specs.sourceUrl }, 核对项目: cases };
await mkdir(temporary, { recursive: true });
await writeFile(path.join(temporary, 'api-calls.json'), JSON.stringify(calls, null, 2) + '\n', 'utf8');
// 历史审计底稿保持不变；重跑结果单独保存，避免修复后覆盖问题发生时的证据。
await writeFile(path.join(temporary, 'latest-results.json'), JSON.stringify(evidence, null, 2) + '\n', 'utf8');
console.log(JSON.stringify({ 项目数: cases.length, 一致: cases.filter(item => item.一致).length,
  差异: cases.filter(item => !item.一致), 混合快门数量: grrResults.length,
  三维近端视场对照: fovMappingResults, 三维样本: evidence.三维样本 }, null, 2));
