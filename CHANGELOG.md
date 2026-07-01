# Changelog

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

- Added `docs/industrial_validation.md` with command-level validation coverage,
  output locations, metrics, and pass criteria.

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
- Added `docs/industrial_library.md` with integration, install, runtime, and
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
