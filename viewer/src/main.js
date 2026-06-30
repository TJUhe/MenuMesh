import * as THREE from "three";
import { OrbitControls } from "three/examples/jsm/controls/OrbitControls.js";
import { STLLoader } from "three/examples/jsm/loaders/STLLoader.js";
import "./styles.css";

const inputByCase = {
  bump_height: "bump.stl",
  clustered_plane: "clustered_plane.stl",
  clustered_plane_boundary: "clustered_plane.stl",
  cube_dihedral: "cube.stl",
  cube_ratio_dihedral: "cube.stl",
  cylinder: "cylinder.stl",
  flange_face_ladder: "flange.stl",
  flange_line_budget: "flange.stl",
  flange_ratio_dihedral: "flange.stl",
  flange_standard_budget: "flange.stl",
  hole_plane_boundary: "hole_plane.stl",
  noisy_plane: "noisy_plane.stl",
  ridge_dihedral: "ridge.stl",
  ridge_ratio_line: "ridge.stl",
  ridge_uniform: "ridge.stl",
  sine_terrain: "sine_terrain.stl",
  sine_terrain_ratio_line: "sine_terrain.stl",
  terrace_dihedral: "terrace.stl",
  terrace_uniform: "terrace.stl",
  thin_fin_dihedral: "thin_fin.stl",
  thin_fin_uniform: "thin_fin.stl",
  torus: "torus.stl",
};

const scenarioSpecs = [
  {
    id: "flanged_boss_face_ladder",
    title: "Flanged boss face ladder",
    note: "Use the slider to step target faces from 1000 down to 100.",
    baselineCase: "flange_face_ladder",
    variantCase: "flange_face_ladder",
    baselineOriginal: true,
    variants: (rows) => rows.filter((row) => row.target_faces),
  },
  {
    id: "flanged_boss_reduction",
    title: "Flanged boss reduction",
    note: "Original flange on the left; reduced face budgets on the right.",
    baselineCase: "flange_ratio_dihedral",
    variantCase: "flange_ratio_dihedral",
    baselineOriginal: true,
    defaultVariant: "last",
    variants: (rows) => rows.filter((row) => row.ratio),
  },
  {
    id: "flanged_boss_budget",
    title: "Flanged boss QEM vs line",
    note: "Same 15% face budget: compare standard QEM with line-quadrics weighting.",
    baselineCase: "flange_standard_budget",
    variantCase: "flange_line_budget",
    baseline: (rows) => rows.find((row) => row.method === "standard"),
    variants: (rows) => rows.filter((row) => row.method === "line"),
  },
  {
    id: "ratio_sine",
    title: "Simplification ladder",
    note: "Target ratio changes, so faces should visibly drop.",
    baselineCase: "sine_terrain_ratio_line",
    variantCase: "sine_terrain_ratio_line",
    defaultVariant: "last",
    baseline: (rows) => maxBy(rows, (row) => Number(row.ratio)),
    variants: (rows) => rows.filter((row) => row.ratio),
  },
  {
    id: "planar_regularization",
    title: "Planar QEM degeneracy",
    note: "Same face budget: line quadrics fix planar collapse quality.",
    baselineCase: "clustered_plane",
    variantCase: "clustered_plane",
    baseline: (rows) => rows.find((row) => row.method === "standard"),
    variants: (rows) => rows.filter((row) => row.method === "line"),
  },
  {
    id: "same_budget_ridge",
    title: "Same budget quality",
    note: "Faces stay similar; triangle quality and placement change.",
    baselineCase: "ridge_uniform",
    variantCase: "ridge_uniform",
    baseline: (rows) => rows.find((row) => row.method === "standard"),
    variants: (rows) => rows.filter((row) => row.method === "line"),
  },
  {
    id: "soft_feature",
    title: "Soft feature weighting",
    note: "Dihedral scores raise line weights near crease vertices.",
    baselineCase: "ridge_uniform",
    variantCase: "ridge_dihedral",
    baseline: (rows) =>
      nearestBy(rows.filter((row) => row.method === "line"), 0.001),
    variants: (rows) => rows,
  },
  {
    id: "noise_limit",
    title: "Noise limitation",
    note: "Line quadrics respect noisy normals; they are not denoising.",
    baselineCase: "noisy_plane",
    variantCase: "noisy_plane",
    baseline: (rows) => rows.find((row) => row.method === "standard"),
    variants: (rows) => rows.filter((row) => row.method === "line"),
  },
  {
    id: "hard_edge_ladder",
    title: "Hard-edge ratio ladder",
    note: "CAD-like hard edges at progressively lower face budgets.",
    baselineCase: "cube_ratio_dihedral",
    variantCase: "cube_ratio_dihedral",
    defaultVariant: "last",
    baseline: (rows) => maxBy(rows, (row) => Number(row.ratio)),
    variants: (rows) => rows.filter((row) => row.ratio),
  },
];

const caseSelect = document.querySelector("#caseSelect");
const variantSelect = document.querySelector("#variantSelect");
const variantRange = document.querySelector("#variantRange");
const showOriginal = document.querySelector("#showOriginal");
const showWire = document.querySelector("#showWire");
const autoCycle = document.querySelector("#autoCycle");
const prevBtn = document.querySelector("#prevBtn");
const nextBtn = document.querySelector("#nextBtn");
const statusEl = document.querySelector("#status");
const qualityMetric = document.querySelector("#qualityMetric");
const edgeMetric = document.querySelector("#edgeMetric");
const faceMetric = document.querySelector("#faceMetric");
const distanceMetric = document.querySelector("#distanceMetric");
const leftLabel = document.querySelector("#leftLabel");
const rightLabel = document.querySelector("#rightLabel");

const canvas = document.querySelector("#scene");
const renderer = new THREE.WebGLRenderer({
  canvas,
  antialias: true,
  preserveDrawingBuffer: true,
});
renderer.setPixelRatio(Math.min(window.devicePixelRatio, 2));
renderer.setClearColor(0xf2f1ea, 1);

const scene = new THREE.Scene();
scene.fog = new THREE.Fog(0xf2f1ea, 6, 18);

const camera = new THREE.PerspectiveCamera(42, 1, 0.01, 100);
camera.position.set(4.4, 2.8, 4.2);

const controls = new OrbitControls(camera, renderer.domElement);
controls.enableDamping = true;
controls.dampingFactor = 0.08;

scene.add(new THREE.HemisphereLight(0xffffff, 0x8d8170, 2.0));
const key = new THREE.DirectionalLight(0xffffff, 2.8);
key.position.set(3.5, 4.5, 2.5);
scene.add(key);
const rim = new THREE.DirectionalLight(0xbdded4, 1.7);
rim.position.set(-3, 2, -4);
scene.add(rim);

const grid = new THREE.GridHelper(5.8, 24, 0x82918c, 0xc8d2cb);
grid.position.y = -0.92;
grid.material.opacity = 0.32;
grid.material.transparent = true;
scene.add(grid);

const loader = new STLLoader();
const rootGroup = new THREE.Group();
scene.add(rootGroup);

const targetMaterial = new THREE.MeshStandardMaterial({
  color: 0x0c7c7b,
  metalness: 0.08,
  roughness: 0.54,
  side: THREE.DoubleSide,
});
const baselineMaterial = new THREE.MeshStandardMaterial({
  color: 0x64736f,
  metalness: 0.04,
  roughness: 0.62,
  side: THREE.DoubleSide,
});
const originalMaterial = new THREE.MeshBasicMaterial({
  color: 0xd3644b,
  opacity: 0.34,
  transparent: true,
  side: THREE.DoubleSide,
  wireframe: true,
});
const wireMaterial = new THREE.MeshBasicMaterial({
  color: 0x273330,
  opacity: 0.3,
  transparent: true,
  wireframe: true,
});

let rows = [];
let rowsByCase = new Map();
let scenarios = [];
let cycleTimer = 0;

init();

async function init() {
  resize();
  window.addEventListener("resize", resize);
  rows = await loadMetrics();
  rowsByCase = groupRowsByCase(rows);
  scenarios = buildScenarios();
  fillScenarios();
  bindEvents();
  await loadSelected();
  renderer.setAnimationLoop(render);
}

async function loadMetrics() {
  const response = await fetch("/examples/output/demo_summary.csv", {
    cache: "no-store",
  });
  if (!response.ok) {
    statusEl.textContent = "Run VSCode task: run: quick viewer data";
    return [];
  }
  const text = await response.text();
  return parseCsv(text).filter((row) => row.case && row.label);
}

function parseCsv(text) {
  const lines = text.trim().split(/\r?\n/);
  if (lines.length < 2) return [];
  const headers = splitCsvLine(lines[0]);
  return lines.slice(1).map((line) => {
    const values = splitCsvLine(line);
    return Object.fromEntries(headers.map((header, i) => [header, values[i] ?? ""]));
  });
}

function splitCsvLine(line) {
  const out = [];
  let current = "";
  let quoted = false;
  for (let i = 0; i < line.length; i += 1) {
    const ch = line[i];
    if (ch === '"') {
      quoted = !quoted;
    } else if (ch === "," && !quoted) {
      out.push(current);
      current = "";
    } else {
      current += ch;
    }
  }
  out.push(current);
  return out;
}

function groupRowsByCase(allRows) {
  const map = new Map();
  for (const row of allRows) {
    if (!map.has(row.case)) map.set(row.case, []);
    map.get(row.case).push(row);
  }
  for (const caseRows of map.values()) {
    caseRows.sort((a, b) => {
      const af = Number(a.target_faces);
      const bf = Number(b.target_faces);
      if (Number.isFinite(af) && Number.isFinite(bf) && af !== bf) return bf - af;
      const ar = Number(a.ratio);
      const br = Number(b.ratio);
      if (Number.isFinite(ar) && Number.isFinite(br) && ar !== br) return br - ar;
      return Number(a.line_weight) - Number(b.line_weight);
    });
  }
  return map;
}

function buildScenarios() {
  return scenarioSpecs
    .map((spec) => {
      const baselineRows = rowsByCase.get(spec.baselineCase) ?? [];
      const variantRows = rowsByCase.get(spec.variantCase) ?? [];
      const baseline = spec.baselineOriginal
        ? { method: "original", line_weight: "", ratio: "", label: "original" }
        : spec.baseline(baselineRows);
      const variants = spec.variants(variantRows).filter(Boolean);
      return { ...spec, baseline, variants };
    })
    .filter((scenario) => scenario.baseline && scenario.variants.length > 0);
}

function fillScenarios() {
  caseSelect.innerHTML = "";
  for (const scenario of scenarios) {
    const option = document.createElement("option");
    option.value = scenario.id;
    option.textContent = scenario.title;
    caseSelect.append(option);
  }
  fillVariants();
}

function currentScenario() {
  return scenarios.find((scenario) => scenario.id === caseSelect.value) ?? scenarios[0];
}

function fillVariants() {
  const scenario = currentScenario();
  variantSelect.innerHTML = "";
  scenario.variants.forEach((row, index) => {
    const option = document.createElement("option");
    option.value = String(index);
    option.textContent = optionText(row);
    variantSelect.append(option);
  });
  variantRange.max = String(Math.max(0, scenario.variants.length - 1));
  const defaultIndex =
    scenario.defaultVariant === "last" ? Math.max(0, scenario.variants.length - 1) : 0;
  variantRange.value = String(defaultIndex);
  variantSelect.value = String(defaultIndex);
}

function bindEvents() {
  caseSelect.addEventListener("change", async () => {
    fillVariants();
    await loadSelected();
  });
  variantSelect.addEventListener("change", async () => {
    variantRange.value = variantSelect.value;
    await loadSelected();
  });
  variantRange.addEventListener("input", async () => {
    variantSelect.value = variantRange.value;
    await loadSelected();
  });
  showOriginal.addEventListener("change", updateVisibility);
  showWire.addEventListener("change", updateVisibility);
  prevBtn.addEventListener("click", () => shiftVariant(-1));
  nextBtn.addEventListener("click", () => shiftVariant(1));
}

async function shiftVariant(delta) {
  const scenario = currentScenario();
  const count = scenario.variants.length;
  const next = (Number(variantSelect.value) + delta + count) % count;
  variantSelect.value = String(next);
  variantRange.value = String(next);
  await loadSelected();
}

async function loadSelected() {
  const scenario = currentScenario();
  if (!scenario) return;
  const targetRow = scenario.variants[Number(variantSelect.value)] ?? scenario.variants[0];
  const baselineRow = scenario.baseline;

  statusEl.textContent = scenario.note;
  const inputCase = scenario.baselineCase;
  const inputPath = `/examples/input/${inputByCase[inputCase]}`;
  const baselinePath = scenario.baselineOriginal ? inputPath : rowPath(baselineRow);
  const [originalGeometry, baselineGeometry, targetGeometry] = await Promise.all([
    loadGeometry(inputPath),
    loadGeometry(baselinePath),
    loadGeometry(rowPath(targetRow)),
  ]);

  const { center, scale, originalFaces } = normalizationFrom(originalGeometry);
  rootGroup.clear();
  rootGroup.add(
    createSide(
      baselineRow,
      baselineGeometry,
      originalGeometry,
      center,
      scale,
      -1.35,
      baselineMaterial,
    ),
  );
  rootGroup.add(
    createSide(targetRow, targetGeometry, originalGeometry, center, scale, 1.35, targetMaterial),
  );
  updateVisibility();
  frameRoot();
  updateMetrics(baselineRow, targetRow, originalFaces);
}

function rowPath(row) {
  return `/examples/output/${row.case}/${row.label}.stl`;
}

function loadGeometry(path) {
  return new Promise((resolve, reject) => {
    loader.load(
      path,
      (geometry) => {
        geometry.rotateX(-Math.PI / 2);
        geometry.computeVertexNormals();
        resolve(geometry);
      },
      undefined,
      reject,
    );
  });
}

function normalizationFrom(originalGeometry) {
  originalGeometry.computeBoundingBox();
  const box = originalGeometry.boundingBox;
  const size = box.getSize(new THREE.Vector3());
  const center = box.getCenter(new THREE.Vector3());
  const scale = 1.8 / Math.max(size.x, size.y, size.z, 1e-6);
  return { center, scale, originalFaces: triangleCount(originalGeometry) };
}

function normalizedClone(geometry, center, scale) {
  const clone = geometry.clone();
  clone.translate(-center.x, -center.y, -center.z);
  clone.scale(scale, scale, scale);
  clone.computeVertexNormals();
  return clone;
}

function createSide(row, resultGeometry, originalGeometry, center, scale, x, material) {
  const side = new THREE.Group();
  side.position.x = x;
  const original = new THREE.Mesh(
    normalizedClone(originalGeometry, center, scale),
    originalMaterial,
  );
  original.name = "original";
  const result = new THREE.Mesh(normalizedClone(resultGeometry, center, scale), material);
  result.name = "result";
  const wire = new THREE.Mesh(normalizedClone(resultGeometry, center, scale), wireMaterial);
  wire.name = "wire";
  side.userData = { row, original, wire };
  side.add(original, result, wire);
  return side;
}

function updateVisibility() {
  for (const side of rootGroup.children) {
    if (side.userData.original) side.userData.original.visible = showOriginal.checked;
    if (side.userData.wire) side.userData.wire.visible = showWire.checked;
  }
}

function frameRoot() {
  const box = new THREE.Box3().setFromObject(rootGroup);
  const size = box.getSize(new THREE.Vector3());
  const center = box.getCenter(new THREE.Vector3());
  const maxDim = Math.max(size.x, size.y, size.z, 1e-6);
  controls.target.copy(center);
  camera.near = 0.01;
  camera.far = 100;
  camera.position.set(center.x + maxDim * 1.55, center.y + maxDim * 0.9, center.z + maxDim * 1.35);
  camera.updateProjectionMatrix();
  controls.update();
}

function updateMetrics(baselineRow, targetRow, originalFaces) {
  const baselineFaces = baselineRow.method === "original" ? originalFaces : Number(baselineRow.faces);
  const targetFaces = Number(targetRow.faces);
  const reduction = originalFaces > 0 ? 1 - targetFaces / originalFaces : 0;
  qualityMetric.textContent = `${formatShortNumber(baselineRow.mean_triangle_quality)}/${formatShortNumber(targetRow.mean_triangle_quality)}`;
  edgeMetric.textContent = `${formatShortNumber(baselineRow.edge_length_cv)}/${formatShortNumber(targetRow.edge_length_cv)}`;
  faceMetric.textContent = `${formatInt(targetFaces)}/${formatInt(originalFaces)}`;
  faceMetric.title = `${formatInt(baselineFaces)} baseline faces, ${formatInt(targetFaces)} target faces from ${formatInt(originalFaces)} original faces`;
  distanceMetric.textContent = formatNumber(targetRow.mean_orig_to_simp);
  leftLabel.textContent = `baseline: ${shortLabel(baselineRow)}`;
  rightLabel.textContent = `target: ${shortLabel(targetRow)} / ${formatPercent(reduction)} fewer`;
}

function render(time) {
  if (autoCycle.checked && time - cycleTimer > 1500) {
    cycleTimer = time;
    shiftVariant(1);
  }
  controls.update();
  renderer.render(scene, camera);
}

function resize() {
  const width = window.innerWidth;
  const height = window.innerHeight;
  renderer.setSize(width, height, false);
  camera.aspect = width / height;
  camera.updateProjectionMatrix();
}

function optionText(row) {
  return `${row.method}${stepText(row)} w=${formatWeight(row.line_weight)} q=${formatNumber(row.mean_triangle_quality)}`;
}

function shortLabel(row) {
  if (!row) return "-";
  if (row.method === "original") return "original";
  return `${row.method}${stepText(row)} w=${formatWeight(row.line_weight)}`;
}

function stepText(row) {
  if (row.target_faces) return ` target=${formatInt(row.target_faces)}`;
  if (row.ratio) return ` r=${formatPercent(Number(row.ratio))}`;
  return "";
}

function formatWeight(value) {
  const n = Number(value);
  if (!Number.isFinite(n)) return value;
  if (n === 0) return "0";
  return n.toExponential(0);
}

function formatNumber(value) {
  const n = Number(value);
  if (!Number.isFinite(n)) return "-";
  return n >= 1 ? n.toFixed(2) : n.toFixed(3);
}

function formatShortNumber(value) {
  const n = Number(value);
  if (!Number.isFinite(n)) return "-";
  return n.toFixed(2);
}

function triangleCount(geometry) {
  const position = geometry.getAttribute("position");
  return position ? Math.floor(position.count / 3) : 0;
}

function formatInt(value) {
  return new Intl.NumberFormat("en-US").format(Number(value) || 0);
}

function formatPercent(value) {
  const n = Number(value);
  if (!Number.isFinite(n)) return "0%";
  return `${(100 * n).toFixed(1)}%`;
}

function maxBy(items, score) {
  return items.reduce((best, item) => {
    if (!best) return item;
    return score(item) > score(best) ? item : best;
  }, null);
}

function nearestBy(items, targetWeight) {
  return items.reduce((best, item) => {
    if (!best) return item;
    return Math.abs(Number(item.line_weight) - targetWeight) <
      Math.abs(Number(best.line_weight) - targetWeight)
      ? item
      : best;
  }, null);
}
