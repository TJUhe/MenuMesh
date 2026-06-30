# VSCode Build Setup

This project is now set up around VSCode, CMake presets, and CMake Tools. The
old root PowerShell scripts were removed; the common workflows are available as
VSCode tasks and native `linequadrics.exe` commands.

## Recommended VSCode Extensions

Install the extensions listed in `.vscode/extensions.json`:

- C/C++ (`ms-vscode.cpptools`)
- CMake Tools (`ms-vscode.cmake-tools`)

## Presets

`CMakePresets.json` uses schema version 2, so it works with CMake 3.20+. This is
intentional for Windows 7 compatibility.

Available configure/build presets:

| Preset | Generator | Compiler | Notes |
| --- | --- | --- | --- |
| `mingw-ninja-release` / `debug` | Ninja | MinGW `g++` | Recommended on modern machines when Ninja is installed. |
| `mingw-makefiles-release` / `debug` | MinGW Makefiles | MinGW `g++` | Best fallback for Windows 7; no Ninja required. |
| `msvc-vs2022-release` / `debug` | Visual Studio 17 2022 | MSVC | Use on machines with Visual Studio Build Tools. |
| `msvc-ninja-release` / `debug` | Ninja | MSVC `cl` | Use from a VS Developer Command Prompt or a VSCode terminal where `cl.exe` is on `PATH`. |

For Windows 7, prefer:

```text
mingw-makefiles-release
```

or, if Ninja is available:

```text
mingw-ninja-release
```

Use CMake 3.20.x to 3.25.x on Windows 7. Do not use CMake 4.x as the Win7
baseline. MinGW-w64 `GCC 10.5.0 x86_64 POSIX SEH MSVCRT` is the recommended
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
- `run: quick viewer data`
- `run: quick viewer data (mingw makefiles)`
- `run: quick viewer data (msvc vs2022)`
- `run: full demo data`
- `run: feature validation`
- `run: external validation`
- `viewer: start`
- `viewer: start (mingw makefiles)`
- `viewer: start (msvc vs2022)`

The viewer tasks are meant for a modern machine. On Windows 7 / 4GB RAM, run the
C++ command-line program only and inspect STL files with a lightweight viewer.

## Command-Line Equivalents

MinGW + Ninja:

```powershell
cmake --preset mingw-ninja-release
cmake --build --preset mingw-ninja-release --target linequadrics --parallel 2
```

MinGW Makefiles, safest on Windows 7:

```powershell
cmake --preset mingw-makefiles-release
cmake --build --preset mingw-makefiles-release --target linequadrics --parallel 1
```

MSVC Visual Studio generator:

```powershell
cmake --preset msvc-vs2022-release
cmake --build --preset msvc-vs2022-release --target linequadrics --config Release --parallel 2
```

MSVC + Ninja, from a VS Developer Command Prompt:

```powershell
cmake --preset msvc-ninja-release
cmake --build --preset msvc-ninja-release --target linequadrics --parallel 2
```

## Native Workflow Commands

Generate viewer demo data:

```powershell
.\build\mingw-ninja-release\linequadrics.exe demo --quick --samples 500
```

Generate the full demo set:

```powershell
.\build\mingw-ninja-release\linequadrics.exe demo --samples 1000
```

Summarize metrics manually:

```powershell
.\build\mingw-ninja-release\linequadrics.exe summarize-metrics examples\output examples\output\demo_summary.csv
```

Run generated industrial feature validation:

```powershell
.\build\mingw-ninja-release\linequadrics.exe validate-features --ratio 0.20 --n 96 --samples 1000
```

Run external model validation after placing OBJ files under
`examples/external/common_3d_test_models/`:

```powershell
.\build\mingw-ninja-release\linequadrics.exe validate-external --ratio 0.25 --samples 800
```

Expected external file names:

```text
fandisk.obj
rocker-arm.obj or rocker_arm.obj
beetle.obj
cow.obj
suzanne.obj
```
