# Changelog

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
- `ctest --test-dir build\\codex-industrial -C Debug --output-on-failure`
- `cmake --build build\\codex-industrial --config Debug --target check-format`
- `cmake --build build\\codex-industrial --config Debug --target docs-api`
- `cmake --install build\\codex-industrial --config Debug --prefix build\\codex-industrial\\stage-copy-helper-3`
