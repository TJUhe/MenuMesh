# Industrial Dynamic Library Notes

This branch turns the algorithm code into a reusable cross-platform C++ dynamic
library while keeping the existing `linequadrics` CLI as a consumer of that
library.

The repository is organized as an algorithm kernel, not as a web application.
The previous browser viewer path is intentionally not part of the industrial
library boundary. Validation outputs are STL files and CSV metrics; visual
inspection should use an external STL/CAD viewer such as MeshLab, CAD Assistant,
or any lightweight STL viewer available on the target machine.

## Targets

- `line_quadrics_qem`: shared library by default (`.dll`, `.so`, or `.dylib`).
- `linequadrics`: command-line tool linked against the library.
- `linequadrics_basic_simplify`: small external-style example program.
- `linequadrics_c_api_basic`: pure-C example using the binary-stable C ABI.
- `line_quadrics_qem_tests`: GoogleTest test binary.
- `format`: runs `clang-format -i` over public headers, source, examples, and
  tests.
- `check-format`: runs `clang-format --dry-run --Werror`.
- `docs-api`: generates Doxygen HTML into the build tree.

## Source Layout

| Path | Industrial role |
| --- | --- |
| `include/line_quadrics_qem/` | Stable public C++ headers. |
| `src/` | Private implementation plus the CLI entry point. |
| `tests/` | Automated regression coverage. |
| `thirdParty/googletest/` | Vendored GoogleTest source used by the test target. |
| `examples/basic_simplify.cpp` | External-consumer style smoke test. |
| `examples/c_api_basic.c` | C ABI smoke test for non-CMake or plugin-style users. |
| `cmake/` | Installed package configuration. |
| `docs/sdk_integration.md` | CMake, Visual Studio, and C ABI integration guide. |
| `docs/industrial_validation.md` | Command-level validation matrix and performance checks. |
| `examples/input/` | Reproducible sample STL inputs. |
| `examples/output/` | Generated validation outputs, not a required source dependency. |

## Configure And Build

```powershell
cmake -S . -B build/industrial
cmake --build build/industrial --config Release --parallel
cmake -E chdir build/industrial ctest -C Release --output-on-failure
```

For Ninja or Makefile generators, keep using `-DCMAKE_BUILD_TYPE=Release` at
configure time.

Important options:

```text
LQ_BUILD_SHARED_LIBRARY=ON   Build the reusable dynamic library
LQ_BUILD_CLI=ON              Build the linequadrics executable
LQ_BUILD_EXAMPLES=ON         Build example consumers
LQ_BUILD_TESTS=ON            Build GoogleTest tests
LQ_BUILD_DOCS=ON             Add docs-api target
LQ_ENABLE_INSTALL=ON         Install headers, library, and CMake package files
```

## Public API And ABI

Public headers live under:

```text
include/line_quadrics_qem/
```

Most consumers only need:

```cpp
#include "line_quadrics_qem/Mesh.h"
#include "line_quadrics_qem/QEMSimplifier.h"
```

The main entry point is:

```cpp
lq::Mesh simplifyMesh(const lq::Mesh& input,
                      const lq::SimplifyOptions& options,
                      lq::SimplifyReport* report = nullptr);
```

This C++ API is appropriate when the application and library are built by a
compatible compiler, standard library, Eigen version, and runtime setting.

For binary SDK integration, prefer:

```c
#include "line_quadrics_qem/CApi.h"
```

The C ABI uses opaque handles, caller-owned arrays, explicit destroy functions,
and `LqStatus` return values. It does not expose STL, Eigen, or C++ exceptions
across the DLL boundary. See [`sdk_integration.md`](sdk_integration.md).

## External CMake Consumer

After installing:

```powershell
cmake --install build/industrial --config Release --prefix C:/opt/line-quadrics-qem
```

An external project can use:

```cmake
find_package(line_quadrics_qem CONFIG REQUIRED)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE line_quadrics_qem::line_quadrics_qem)
line_quadrics_qem_copy_runtime_dependencies(my_app)
```

The installed package declares Eigen as a dependency. For fully offline
industrial builds, install Eigen 3.3+ or provide `Eigen3_DIR` when configuring
the consuming application.

## Runtime Layout

CMake places runtime binaries in the build tree under `bin/<config>` for Visual
Studio and `bin` for single-config generators. On Windows this keeps
`line_quadrics_qem.dll`, the CLI, examples, and tests in a layout that can run
without manually editing `PATH`.

External CMake consumers should call
`line_quadrics_qem_copy_runtime_dependencies(my_app)` after
`target_link_libraries`. On Windows it adds a post-build copy step from the
imported DLL target to the consuming executable directory. On non-Windows
platforms the function is intentionally a no-op.

Recommended deployment layout:

```text
bin/
  line_quadrics_qem.dll
  linequadrics.exe
include/
  line_quadrics_qem/*.h
lib/
  line_quadrics_qem.lib
  cmake/line_quadrics_qem/*.cmake
share/
  line_quadrics_qem/msvc/line_quadrics_qem.props
```

For Visual Studio projects that are not organized with CMake, import:

```text
<install-prefix>\share\line_quadrics_qem\msvc\line_quadrics_qem.props
```

The property sheet adds include/library paths and copies the runtime DLL next
to the consuming executable after build.

Linux and macOS consumers should set their normal install-time runtime path
policy for `.so` or `.dylib` deployment.

## Testing And Documentation

Run the full test set:

```powershell
cmake -E chdir build/industrial ctest -C Release --output-on-failure
```

Run formatting checks:

```powershell
cmake --build build/industrial --target check-format
```

Generate Doxygen:

```powershell
cmake --build build/industrial --target docs-api
```

Open:

```text
build/industrial/docs/api/html/index.html
```

For performance and feature validation, follow
[`industrial_validation.md`](industrial_validation.md). It maps each claim to a
command, output file, CSV column, and pass criterion.

## Integration Boundaries

The shared library intentionally owns algorithmic functionality only:

- mesh I/O for STL/OBJ,
- mesh generators for tests and demos,
- feature detection,
- QEM and line-quadrics simplification,
- quality metrics.

The CLI remains a separate executable that parses arguments and orchestrates
batch workflows. This keeps the library usable from CAD/CAM applications,
desktop software, services, and test harnesses without bringing in command-line
parsing or filesystem-heavy demo workflows.

The removed web preview stack was a convenience tool, not a kernel dependency.
Its role is replaced by generated STL files for visual inspection and CSV files
for measurable acceptance.
