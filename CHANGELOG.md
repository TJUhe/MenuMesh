# Changelog

## 2026-07-05

### Added

- Added `FeatureProtectionMode` for feature-curve simplification:
  `none`, `circular-only`, `primitive-curves`, and `all-feature-edges`.
  The default `primitive-curves` policy hard-protects only circle,
  near-circle, and ellipse primitives; generic polygonal/dihedral creases stay
  as soft line-quadric costs plus the existing topology, normal, quality, and
  local-error filters.
- Added `--feature-protection-mode` to the CLI and
  `LqFeatureProtectionMode` to the C ABI. The legacy
  `--protect-all-feature-edges` / `protect_all_feature_edges` switch remains a
  compatibility alias for the strict `all-feature-edges` behavior.
- Added primitive/generic feature rejection counters to C++ and C reports so
  validation can prove whether the new policy is reducing generic hard locks.
- Added `docs/design/feature_protection_roadmap.md` to record the external
  model probe results and the first implemented CGAL/OpenMesh-style policy
  split: primitive-curve hard protection plus soft generic-crease behavior.
- Added a shared "Feature Protection Roadmap" section to every generated HTML
  note under `docs/generated/notes/` so the browser-readable documentation
  carries the same algorithm direction as the Markdown design docs.

### Changed

- Reworked feature-curve collapse policy so polygonal/generic crease vertices
  are no longer automatically rejected in the default protected mode. The strict
  old behavior is still available via `all-feature-edges`.
- Reworked `validate-features` to use finished external STL fixtures by
  default: Thingi10K spindle, NASA antenna azimuth track, Thingi10K mini
  pulley, and OpenFOAM flange. The old procedural shaft/coupling/pulley
  validation path is no longer the default industrial feature test.
- Updated feature validation docs and generated HTML results to report the
  new primitive/generic policy split and the before/after probe results where
  `primitive-curves` removes generic hard locks on fragmented industrial STL
  feature graphs.
- Refreshed the documentation so user-facing commands, generated HTML notes,
  and algorithm explanations follow the current C++ implementation instead of
  older experiment paths.
- Documented the Windows MinGW/Ninja configure requirement to specify both
  `-DCMAKE_C_COMPILER=gcc` and `-DCMAKE_CXX_COMPILER=g++`; specifying only the
  C++ compiler can leave CMake mixing `cl.exe` with MinGW and fail before build.
- Updated feature-curve docs and practice-result HTML to describe the current
  line/curve validation outputs, circular/elliptic/polygonal feature policies,
  projection counters, curve-budget rejections, and cases where curve
  protection improves geometry but not every match count.
- Corrected generated algorithm notes to use the current
  `collapseRejectReason(...)` legality path, including link-condition topology,
  triangle quality, normal deviation, local error, and optional local
  self-intersection guards.
- Clarified source boundaries: Garland-Heckbert QEM, Liu/Rahimzadeh/Zordan line
  quadrics, Tsuchie-Higashi normal tensor feature lines, and Xu et al. CWF are
  cited as source ideas; the docs now separate those ideas from what this
  repository currently implements.

### Verified

- Deleted `build/`, then configured with
  `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DLQ_ENABLE_INSTALL=ON -DLQ_GOOGLETEST_PROVIDER=prebuilt -DLQ_EIGEN_PROVIDER=vendored`
- `cmake --build build --parallel`
- `ctest --test-dir build -C Release --output-on-failure`
- `.\build\bin\linequadrics.exe validate-features --ratio 0.20 --samples 1000 --input-dir tests\output\generated_inputs --output-dir tests\output\feature_curve_validation`
- Feature-policy validation under `tests/output/feature_policy_validation/`:
  `primitive-curves` reached target on `nasa_mars2020_wheel` at 9066 faces with
  31 feature rejections and 0 generic feature rejections, while
  `all-feature-edges` stopped at 10974 faces with 468702 feature rejections.
  On `thingi10k_37880_functional_differential_gear_system`,
  `primitive-curves` reached target at 1236 faces with 0 feature rejections,
  while `all-feature-edges` stopped at 2662 faces with 68993 feature
  rejections. On `fandisk_2014`, both reached target, but generic feature
  rejections dropped from 513 to 0.
- External probe on `nasa_cubesat_middle`, `nasa_mars2020_wheel`,
  `casting_aimshape_2014`, `fandisk_2014`,
  `thingi10k_37880_functional_differential_gear_system`, and
  `large/rocker_arm.stl`; outputs are under
  `tests/output/new_model_validation/` and expose the current
  feature-policy over-protection risks.
- `cmake -S . -B build/doccheck -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ -DLQ_BUILD_DOCS=OFF`
- `cmake --build build/doccheck --parallel`
- `cmake -E chdir build/doccheck ctest --output-on-failure`
- `.\build\doccheck\bin\linequadrics.exe --help`
- `.\build\doccheck\bin\linequadrics.exe feature-report tests\data\feature_fixtures\coaxial_hole_plate.obj --feature-angle-deg 25 --circle-fit-threshold 0.04 --ellipse-fit-threshold 0.05 --normal-tensor-threshold 0.06 --normal-tensor-edge-alignment 0.2 --csv output\doccheck\features.csv`
- `.\build\doccheck\bin\linequadrics.exe simplify tests\data\feature_fixtures\coaxial_hole_plate.obj output\doccheck\simplified.stl --method line --ratio 0.50 --line-weight 1e-3 --weight-mode dihedral --feature-boost 0.08 --feature-angle-deg 25 --preserve-feature-curves --feature-curve-weight 0.08 --max-feature-curve-deviation-ratio 0.05 --circle-fit-threshold 0.04 --ellipse-fit-threshold 0.05 --min-circular-feature-loop-vertices 12 --samples 128 --metrics-csv output\doccheck\metrics.csv`
- `.\build\doccheck\bin\linequadrics.exe validate-features --ratio 0.20 --samples 64 --output-dir output\doccheck\feature_validation`
- `.\build\doccheck\bin\linequadrics.exe validate-external --ratio 0.25 --samples 64 --output-dir output\doccheck\external_validation`
- `.\build\doccheck\bin\linequadrics.exe demo --quick --samples 64 --output-dir output\doccheck\demo --input-dir output\doccheck\demo_input`

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
