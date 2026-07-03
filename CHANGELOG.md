# Changelog

## 2026-07-03

### Added

- Added local geometric tolerance guards for edge collapse, including
  `maxLocalError`, `maxLocalErrorRatio`, and rejection accounting for collapses
  that exceed the local drift budget.
- Added an explicit feature-graph layer for feature loops, shared vertices,
  junctions, multi-feature ownership, and curve-aware collapse policies.
- Added per-loop feature-curve budgets for circular and elliptical loops, with
  circular projection and resampling-oriented protection replacing the old
  fixed `minFeatureLoopVertices`-only behavior.
- Added multi-scale, locally normalized normal-tensor feature detection knobs
  and reporting so weak features can be compared against dihedral-only
  detection.
- Added dataset-level validation coverage for feature recall, curve drift,
  sampled distance, topology, triangle quality, and rejection-count consistency.
- Added VS Code demonstration tasks for selected meshes, algorithm presets,
  ratio sweeps, feature reports, and algorithm comparisons.

### Changed

- Simplification now combines QEM ranking with pre-collapse legality and
  tolerance guards instead of relying only on post-hoc sampled distance.
- The C API report now exposes the new rejection counters, including
  curve-budget and local-error rejections.
- VS Code workflows were trimmed to the two supported Ninja chains:
  `mingw+ninja` as the primary path and `msvc+ninja` as the backup/debug path.
  Fixed `--parallel 2` limits were removed from Ninja tasks so CMake/Ninja can
  use all available parallelism.
- Documentation now calls out demonstration-ready meshes and parameter
  examples for algorithm selection, feature protection, normal-tensor detection,
  and conservative industrial-safe simplification.

### Verified

- `ctest --test-dir build\\mingw-ninja-debug --output-on-failure`
- `cmake --build build\\mingw-ninja-release --target linequadrics --parallel`
- `linequadrics feature-report tests\\data\\feature_fixtures\\coaxial_hole_plate.obj --feature-angle-deg 25 --circle-fit-threshold 0.04 --ellipse-fit-threshold 0.05 --normal-tensor-threshold 0.06 --normal-tensor-edge-alignment 0.2 --csv examples\\output\\vscode_demo\\coaxial_hole_plate\\feature_report\\features.csv`
- `linequadrics simplify tests\\data\\feature_fixtures\\coaxial_hole_plate.obj examples\\output\\vscode_demo\\coaxial_hole_plate\\feature-curves_r0_50\\simplified.stl --method line --ratio 0.50 --line-weight 1e-3 --weight-mode dihedral --feature-boost 0.08 --feature-angle-deg 25 --preserve-feature-curves --feature-curve-weight 0.08 --max-feature-curve-deviation-ratio 0.05 --circle-fit-threshold 0.04 --ellipse-fit-threshold 0.05 --min-circular-feature-loop-vertices 12 --samples 128 --metrics-csv examples\\output\\vscode_demo\\coaxial_hole_plate\\feature-curves_r0_50\\metrics.csv`

## 2026-07-01

### Changed

- Adapted CMake and VSCode workflows for CMake 3.18.6 environments by removing
  preset-based commands and using explicit build directories/generators.
- Reframed the repository documentation around a C++ geometry-kernel workflow:
  CLI-generated STL/CSV outputs, CTest/API smoke checks, and external STL/CAD
  viewers replace the previous browser-preview-first path.
- Updated VSCode launch/tasks and user-facing command examples for the
  `bin/linequadrics.exe` runtime layout produced by the library build.
- Expanded industrial-library notes with source-layout boundaries, validation
  expectations, and guidance for treating generated outputs as inspection
  artifacts instead of source dependencies.
- Refreshed feature-curve validation STL/CSV outputs for flange, pipe coupling,
  pulley, and stepped-shaft cases.

### Added

- Added `Status`, `Result`, typed topology handles, and `MeshTopology` as the
  first reusable topology-cache layer for the mesh-kernel direction.
- Added `docs/design/architecture.md` to clarify the Polygonica-style industrial mesh
  kernel target and module boundaries.
- Added public Fandisk and AIM@SHAPE Casting STL fixtures for reproducing the
  Tsuchie and Higashi 2014 CAD-model experiments locally.
- Added 10 public large STL validation meshes above 10k faces plus
  `docs/design/large_model_validation.md` with 90% and 50% line-quadrics batch results.
- Added `docs/design/industrial_validation.md` with command-level validation coverage,
  output locations, metrics, and pass criteria.
- Added vendored GoogleTest under `thirdParty/googletest` for offline test
  builds.

### Changed

- Routed mesh metric edge, boundary, and non-manifold calculations through
  `MeshTopology` instead of rebuilding ad hoc edge maps in `Metrics.cpp`.
- Added a link-condition topology legality filter to QEM edge collapses so
  closed two-manifold inputs do not simplify into unexpected holes or
  non-manifold edges.
- Replaced repeated full-face scans during QEM collapse validation and updates
  with an incremental incident-face topology, greatly improving large-mesh
  simplification runtime.

### Removed

- Removed the Vite/Node browser viewer stack from the project workflow.
- Removed `CMakePresets.json`; CMake 3.18.6 does not support presets.

## 2026-06-30

### Added

- Added a cross-platform `line_quadrics_qem` shared-library target with public
  headers under `include/line_quadrics_qem`.
- Added DLL export/import handling for Windows and default symbol visibility for
  shared-library builds.
- Added install/export rules so external CMake projects can use
  `find_package(line_quadrics_qem CONFIG REQUIRED)`.
- Added `line_quadrics_qem_copy_runtime_dependencies(target)` for external
  Windows CMake consumers to copy the runtime DLL next to their executable.
- Added a library-consuming example program in `examples/basic_simplify.cpp`.
- Added GoogleTest coverage and CTest discovery for simplification, feature
  detection, and mesh metrics.
- Added clang-format configuration plus `format` and `check-format` targets.
- Added Doxygen configuration plus a `docs-api` target.
- Added `docs/design/industrial_library.md` with integration, install, runtime, and
  tooling notes.

### Changed

- Refactored the CLI to link against the new reusable library target instead of
  compiling algorithm sources directly into the executable.
- Moved public API declarations into installable headers while keeping the old
  `src/*.h` headers as compatibility forwarding headers.
- Formatted existing C++ sources with the new clang-format profile.

### Verified

- `cmake --build build\\codex-industrial --config Debug --parallel`
- `cmake -E chdir build\\codex-industrial ctest -C Debug --output-on-failure`
- `cmake --build build\\codex-industrial --config Debug --target check-format`
- `cmake --build build\\codex-industrial --config Debug --target docs-api`
- `cmake --install build\\codex-industrial --config Debug --prefix build\\codex-industrial\\stage-copy-helper-3`
