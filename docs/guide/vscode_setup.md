# VSCode Build Setup

This project is set up around VSCode tasks, explicit CMake command lines, and
CMake Tools. The old root PowerShell scripts were removed; the common workflows
are available as VSCode tasks and native `linequadrics.exe` commands.

This repository is now treated as a C++ geometry kernel. Browser preview tasks
were removed; generated STL files should be opened with an external STL/CAD
viewer, while CSV files provide the measurable validation record.

## Recommended VSCode Extensions

Install the extensions listed in `.vscode/extensions.json`:

- C/C++ (`ms-vscode.cpptools`)
- CMake Tools (`ms-vscode.cmake-tools`)
- Vim (`vscodevim.vim`), optional editor keybindings

For VSCode 1.70.2 or offline machines, this repository also carries compatible
VSIX packages under `thirdParty/vscode-extensions/`:

- CMake Tools (`ms-vscode.cmake-tools` 1.19.52, `engines.vscode: ^1.67.0`)
- Vim (`vscodevim.vim` 1.24.3, `engines.vscode: ^1.67.0`)

The old clangd VSIX is still kept in `thirdParty/vscode-extensions/` as a
legacy optional package, but the workspace settings now use Microsoft's C/C++
extension (`ms-vscode.cpptools`) instead of clangd.

Install them from the repository root:

```powershell
code --install-extension thirdParty\vscode-extensions\ms-vscode.cmake-tools-1.19.52.vsix
code --install-extension thirdParty\vscode-extensions\vscodevim.vim-1.24.3.vsix
```

Microsoft's C/C++ extension reads the CMake Tools configuration provider and
the fallback paths in `.vscode/settings.json`. If Eigen diagnostics appear
before CMake has configured the project, configure once so FetchContent creates
`build/_deps/eigen-src`, then run `C/C++: Reset IntelliSense Database` from the
VSCode command palette.

The primary editor configuration is MinGW-first:

```text
Compiler: g++ from PATH
Build directory: build/mingw-ninja-release
Compile database: build/mingw-ninja-release/compile_commands.json
```

The repository settings intentionally avoid machine-specific absolute MinGW
paths. Put your MinGW `bin` directory on `PATH` before configuring. If
diagnostics say `Eigen/Dense` or standard headers are missing, check that
`where g++` resolves to your intended MinGW compiler, then regenerate
`compile_commands.json` and reset the C/C++ IntelliSense database.

For a one-session PowerShell setup, run this before CMake if MinGW is not already
on `PATH`:

```powershell
$env:PATH = "D:\path\to\mingw64\bin;$env:PATH"
```

The important rule is that CMake, tasks, and C/C++ IntelliSense must all point
at the same MinGW toolchain and the same `compile_commands.json`.

If the C/C++ extension reports missing headers after a fresh checkout, configure
once:

```powershell
cmake -S . -B build/mingw-ninja-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++ -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
```

Then run `C/C++: Reset IntelliSense Database` from the VSCode command palette.
Do not point IntelliSense at an MSVC build directory while using the MinGW
compiler in VSCode tasks; the headers and predefined macros will not match.

## CMake 3.18.6 Workflow

The project supports CMake 3.18.6 and does not require `CMakePresets.json`.
VSCode tasks call `cmake -S ... -B ...` directly so the same commands work in an
ordinary terminal.

Available configure/build layouts:

| Build directory | Generator | Compiler | Notes |
| --- | --- | --- | --- |
| `build/mingw-ninja-release` / `debug` | Ninja | MinGW `g++` | Primary live-demo and development path. |
| `build/msvc-ninja-release` / `debug` | Ninja | MSVC `cl` | Backup path when debugging with MSVC tools is useful. |

The workspace tasks intentionally keep only these Ninja chains. Older
MinGW-Makefiles and Visual-Studio-generator tasks are not exposed in VS Code so
the live-demo menu stays short and the build directories stay predictable.

Use CMake 3.18.6 or newer. MinGW-w64 `GCC 10.5.0 x86_64 POSIX SEH
MSVCRT` is still the recommended compiler family for old-machine compatibility,
but the maintained workspace path is `mingw+ninja`.

`msvc-ninja-*` also needs the Visual Studio resource/link tools (`rc.exe`,
`mt.exe`, Windows SDK libraries) on `PATH`. If configure fails with missing
`rc` or `mt`, start VSCode from "x64 Native Tools Command Prompt for VS 2022" or
use the MinGW+Ninja tasks.

## VSCode Tasks

Open `Terminal > Run Task...` and choose:

- `build: mingw+ninja release`
- `build: msvc+ninja release`
- `demo: feature report selected mesh`
- `demo: simplify selected mesh (mingw+ninja release)`
- `demo: algorithm comparison selected mesh`
- `demo: ratio sweep selected mesh`
- `run: feature validation`
- `run: external validation`
- `test: mingw+ninja debug`
- `test: msvc+ninja debug`
- `open: vscode demo output`

The `demo:*` tasks ask for a mesh, algorithm preset, ratio, feature angle, and
sample count. Outputs are written under `examples/output/vscode_demo/`.

## Command-Line Equivalents

MinGW + Ninja:

```powershell
cmake -S . -B build/mingw-ninja-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++ -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build/mingw-ninja-release --target linequadrics --parallel
```

MSVC + Ninja, from a VS Developer Command Prompt:

```powershell
cmake -S . -B build/msvc-ninja-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=cl -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build/msvc-ninja-release --target linequadrics --parallel
```

## Demo-Oriented Workflow Commands

Feature report for a clean circular-loop fixture:

```powershell
.\build\mingw-ninja-release\bin\linequadrics.exe feature-report `
  tests\data\feature_fixtures\coaxial_hole_plate.obj `
  --feature-angle-deg 25 `
  --circle-fit-threshold 0.04 `
  --ellipse-fit-threshold 0.05 `
  --csv examples\output\vscode_demo\coaxial_hole_plate\feature_report\features.csv
```

Feature-curve simplification with local curve budget:

```powershell
.\build\mingw-ninja-release\bin\linequadrics.exe simplify `
  tests\data\feature_fixtures\coaxial_hole_plate.obj `
  examples\output\vscode_demo\coaxial_hole_plate\feature-curves_r0_50\simplified.stl `
  --method line --ratio 0.50 --line-weight 1e-3 `
  --weight-mode dihedral --feature-boost 0.08 --feature-angle-deg 25 `
  --preserve-feature-curves --feature-curve-weight 0.08 `
  --max-feature-curve-deviation-ratio 0.05 `
  --circle-fit-threshold 0.04 --ellipse-fit-threshold 0.05 `
  --min-circular-feature-loop-vertices 12 `
  --samples 128 `
  --metrics-csv examples\output\vscode_demo\coaxial_hole_plate\feature-curves_r0_50\metrics.csv
```

Algorithm comparison for a hard-edge CAD model:

```powershell
.\build\mingw-ninja-release\bin\linequadrics.exe ratio-sweep `
  tests\data\external\fandisk_2014.stl `
  examples\output\vscode_demo\fandisk_2014\ratio_sweep_dihedral-line `
  --method line --line-weight 1e-3 `
  --weight-mode dihedral --feature-boost 0.08 --feature-angle-deg 25 `
  --ratios "0.8,0.5,0.25,0.1" --samples 512
```

Run generated industrial feature validation:

```powershell
.\build\mingw-ninja-release\bin\linequadrics.exe validate-features --ratio 0.20 --n 96 --samples 1000
```

Run external model validation after placing OBJ files under
`examples/external/common_3d_test_models/`:

```powershell
.\build\mingw-ninja-release\bin\linequadrics.exe validate-external --ratio 0.25 --samples 800
```

Expected external file names:

```text
fandisk.obj
rocker-arm.obj or rocker_arm.obj
beetle.obj
cow.obj
suzanne.obj
```

## Most Useful Demo Cases

| Purpose | Mesh | Preset / parameters | What to explain |
| --- | --- | --- | --- |
| Feature-loop preservation | `tests/data/feature_fixtures/coaxial_hole_plate.obj` | `feature-curves`, ratio `0.50` or `0.25` | Four circular loops, curve projection, and `projected_feature_placements`. |
| Ellipse/circle budget tradeoff | `tests/data/feature_fixtures/elliptical_hole_plate.obj` | `feature-curves` | Why per-loop budgets are better than only a global minimum vertex count. |
| Hard-edge line quadrics | `tests/data/external/fandisk_2014.stl` | `dihedral-line`, `--feature-angle-deg 15/25/45` | How dihedral feature weighting changes preservation and triangle quality. |
| Conservative industrial run | `tests/data/external/casting_aimshape_2014.stl` | `industrial-safe`, `--max-local-error-ratio 0.02` | Local error guards, quality guards, and rejection counters. |
| Weak-feature experiment | `tests/data/feature_fixtures/boss_pocket_plate.obj` | `normal-tensor` | Normal-tensor scoring as a supplement to dihedral features. |

## Debugging Workflow

Use the Run and Debug panel:

- `Debug CLI Feature Curves (MinGW Ninja)` for curve budgets and circular
  projection.
- `Debug CLI Industrial Safe (MinGW Ninja)` for local error, quality, normal
  flip, and topology rejection paths.
- `Debug CLI Normal Tensor (MinGW Ninja)` for tensor feature scoring.
- `Debug Feature Report (MinGW Ninja)` for feature graph, loop tracing, and
  primitive fitting.
- `Debug Unit Tests Filter (MinGW Ninja)` for focused GoogleTest debugging.

Recommended breakpoints:

- `src/simplification/QEMSimplifier.cpp`: collapse candidate construction,
  `tryCollapse`, `collapseRejectReason`, curve-budget rejection, local-error
  rejection, and report counter increments.
- `src/features/FeatureDetection.cpp`: feature edge collection, feature graph
  traversal, circle/ellipse fitting, and normal-tensor feature scoring.

For live demos, start with `coaxial_hole_plate` or `fandisk_2014`. Save
`industrial-safe` on `casting_aimshape_2014` for the moment when you want to
explain why stronger guards cost more time and produce more rejected collapses.
