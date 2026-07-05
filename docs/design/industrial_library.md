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
| `include/line_quadrics_qem/` | Stable public SDK headers, grouped under `core/`, `features/`, `algorithms/`, and `api/`. |
| `src/<domain>/*.cpp` | Private library implementation entry points grouped by the same responsibilities. |
| `src/<domain>/detail/*.h` | Private implementation headers used only by library sources; not installed as SDK headers. |
| `apps/linequadrics/` | CLI application that consumes the library. |
| `tests/` | Automated regression coverage. |
| `thirdParty/eigen/` | Eigen header bundle used by the C++ API and installed with the SDK. |
| `thirdParty/googletest/` | Prebuilt GoogleTest binary bundle used only by the repository test target. |
| `examples/basic_simplify.cpp` | External-consumer style smoke test. |
| `examples/c_api_basic.c` | C ABI smoke test for non-CMake or plugin-style users. |
| `examples/sdk_consumer/` | Independent downstream project that builds only from an installed SDK root. |
| `adm/templates/` | Administrative SDK install templates; normal library compilation does not depend on this directory. |
| `docs/guide/sdk_integration.md` | CMake, Visual Studio, and C ABI integration guide. |
| `docs/design/industrial_validation.md` | Command-level validation matrix and performance checks. |
| `output/demo_input/` | Generated demo STL inputs. |
| `output/` | Demo and manual experiment outputs, not a required source dependency. |
| `tests/output/` | Generated validation outputs, ignored by git. |

The layout intentionally keeps a compact `include` / `src` split for the current
project size, while keeping an OpenCascade-style discipline that public headers,
private implementation modules, tests, tools, and documentation have separate
ownership. See
[`source_organization.md`](source_organization.md) for the detailed policy.

## Third-Party Dependency Policy

Runtime SDK dependencies should be explicit and small. Public C++ headers use
Eigen types, so C++ consumers need Eigen headers available at compile time. The
C ABI remains the preferred binary-stable surface when consumers should not see
Eigen, STL, or C++ exceptions.

GoogleTest is a test-only dependency. It may be supplied by:

- the repository prebuilt binary bundle under `thirdParty/googletest/prebuilt/`,
- a company-provided prebuilt static or dynamic library bundle,
- a system GTest installation discoverable by CMake,
- FetchContent as a developer fallback.

Eigen is primarily a header-only template library. Normal Eigen integration is
therefore an installed include tree, not a `.lib`, `.a`, `.dll`, or `.so`.
The SDK installs Eigen headers under
`share/line_quadrics_qem/thirdParty/eigen/include` so Visual Studio consumers
do not need a separate Eigen installation unless they deliberately override it.

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
LQ_ENABLE_INSTALL=OFF        Enable SDK install rules when set to ON
LQ_INSTALL_CMAKE_CONFIG=OFF  Also install optional CMake config files when set to ON
LQ_EIGEN_PROVIDER=auto       Prefer system Eigen, then the vendored header bundle, then fetch
LQ_GOOGLETEST_PROVIDER=auto  Prefer prebuilt GoogleTest, then system GTest, then fetch
```

When `LQ_ENABLE_INSTALL=ON`, two SDK-specific targets are available:

```text
sdk-install-local   Install the SDK to build/sdk by default.
sdk-consumer-test   Install the SDK, build examples/sdk_consumer against it, and run those downstream examples.
```

## Public API And ABI

Public headers live under:

```text
include/line_quadrics_qem/
```

Most consumers only need:

```cpp
#include "line_quadrics_qem/core/Mesh.h"
#include "line_quadrics_qem/algorithms/simplification/QEMSimplifier.h"
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
#include "line_quadrics_qem/api/CApi.h"
```

The C ABI uses opaque handles, caller-owned arrays, explicit destroy functions,
and `LqStatus` return values. It does not expose STL, Eigen, or C++ exceptions
across the DLL boundary. See [`../guide/sdk_integration.md`](../guide/sdk_integration.md).

## Visual Studio Consumer

The main SDK integration path for Visual Studio is the generated property sheet
plus the installed `.lib` and optional `.dll`.

```text
<install-prefix>\share\line_quadrics_qem\msvc\line_quadrics_qem.props
```

The property sheet adds the SDK include directory, the SDK library directory,
`line_quadrics_qem.lib`, and copies `line_quadrics_qem.dll` next to the
consumer executable when the DLL exists. Static-library SDK builds use the same
library name and do not need a runtime copy.

## Optional CMake Consumer

After installing with `LQ_INSTALL_CMAKE_CONFIG=ON`:

```powershell
cmake -S . -B build/industrial -DLQ_ENABLE_INSTALL=ON -DLQ_INSTALL_CMAKE_CONFIG=ON
cmake --build build/industrial --config Release --parallel
cmake --install build/industrial --config Release --prefix C:/opt/line-quadrics-qem
```

An external project can use:

```cmake
find_package(line_quadrics_qem CONFIG REQUIRED)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE line_quadrics_qem::line_quadrics_qem)
line_quadrics_qem_copy_runtime_dependencies(my_app)
```

The installed SDK target carries the Eigen include path, so CMake consumers do
not need a separate `find_package(Eigen3)` for the default SDK layout. Visual
Studio property-sheet consumers get the same Eigen include path through
`LQEigenIncludeDir`.

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
  line_quadrics_qem/api/*.h
  line_quadrics_qem/algorithms/simplification/*.h
  line_quadrics_qem/core/*.h
  line_quadrics_qem/features/*.h
lib/
  line_quadrics_qem.lib
share/
  line_quadrics_qem/thirdParty/eigen/include/Eigen/...
  line_quadrics_qem/msvc/line_quadrics_qem.props
  line_quadrics_qem/samples/*.c*
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
