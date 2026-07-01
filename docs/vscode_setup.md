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

For VSCode 1.70.2 or offline machines, this repository also carries compatible
VSIX packages under `thirdParty/vscode-extensions/`:

- clangd (`llvm-vs-code-extensions.vscode-clangd` 0.1.34, `engines.vscode:
  ^1.65.0`)
- CMake Tools (`ms-vscode.cmake-tools` 1.19.52, `engines.vscode: ^1.67.0`)

Install them from the repository root:

```powershell
code --install-extension thirdParty\vscode-extensions\llvm-vs-code-extensions.vscode-clangd-0.1.34.vsix
code --install-extension thirdParty\vscode-extensions\ms-vscode.cmake-tools-1.19.52.vsix
```

## CMake 3.18.6 Workflow

The project supports CMake 3.18.6 and does not require `CMakePresets.json`.
VSCode tasks call `cmake -S ... -B ...` directly so the same commands work in an
ordinary terminal.

Available configure/build layouts:

| Build directory | Generator | Compiler | Notes |
| --- | --- | --- | --- |
| `build/mingw-ninja-release` / `debug` | Ninja | MinGW `g++` | Recommended on modern machines when Ninja is installed. |
| `build/mingw-makefiles-release` / `debug` | MinGW Makefiles | MinGW `g++` | Best fallback for Windows 7; no Ninja required. |
| `build/msvc-vs2022-release` / `debug` | Visual Studio 17 2022 | MSVC | Use on machines with Visual Studio Build Tools. |
| `build/msvc-ninja-release` / `debug` | Ninja | MSVC `cl` | Use from a VS Developer Command Prompt or a VSCode terminal where `cl.exe` is on `PATH`. |

For Windows 7, prefer:

```text
build/mingw-makefiles-release
```

or, if Ninja is available:

```text
build/mingw-ninja-release
```

Use CMake 3.18.6 or newer. Do not use CMake 4.x as the Win7 baseline.
MinGW-w64 `GCC 10.5.0 x86_64 POSIX SEH MSVCRT` is the recommended
compiler family for the old-machine path.

`msvc-ninja-*` also needs the Visual Studio resource/link tools (`rc.exe`,
`mt.exe`, Windows SDK libraries) on `PATH`. If configure fails with missing
`rc` or `mt`, start VSCode from "x64 Native Tools Command Prompt for VS 2022" or
use `msvc-vs2022-*` instead.

## VSCode Tasks

Open `Terminal > Run Task...` and choose:

- `build: mingw+ninja release`
- `build: mingw makefiles release`
- `build: msvc vs2022 release`
- `build: msvc+ninja release`
- `run: quick demo data`
- `run: quick demo data (mingw makefiles)`
- `run: quick demo data (msvc vs2022)`
- `run: full demo data`
- `run: feature validation`
- `run: external validation`

On Windows 7 / 4GB RAM, run the C++ command-line program only and inspect STL
files with a lightweight viewer.

## Command-Line Equivalents

MinGW + Ninja:

```powershell
cmake -S . -B build/mingw-ninja-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++ -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build/mingw-ninja-release --target linequadrics --parallel 2
```

MinGW Makefiles, safest on Windows 7:

```powershell
cmake -S . -B build/mingw-makefiles-release -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++ -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build/mingw-makefiles-release --target linequadrics --parallel 1
```

MSVC Visual Studio generator:

```powershell
cmake -S . -B build/msvc-vs2022-release -G "Visual Studio 17 2022" -A x64
cmake --build build/msvc-vs2022-release --target linequadrics --config Release --parallel 2
```

MSVC + Ninja, from a VS Developer Command Prompt:

```powershell
cmake -S . -B build/msvc-ninja-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=cl -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build/msvc-ninja-release --target linequadrics --parallel 2
```

## Native Workflow Commands

Generate quick demo data:

```powershell
.\build\mingw-ninja-release\bin\linequadrics.exe demo --quick --samples 500
```

Generate the full demo set:

```powershell
.\build\mingw-ninja-release\bin\linequadrics.exe demo --samples 1000
```

Summarize metrics manually:

```powershell
.\build\mingw-ninja-release\bin\linequadrics.exe summarize-metrics examples\output examples\output\demo_summary.csv
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
