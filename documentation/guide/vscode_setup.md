# VS Code 构建与调试说明

ManuMesh 以 VS Code 任务、显式 CMake 命令和 CMake Tools 为主要工作流。旧的根目录 PowerShell 脚本已经移除；常用构建、测试、演示和验证流程都在 `.vscode/tasks.json` 中维护。

当前仓库按 C++ 几何内核维护。浏览器预览任务已经移除，生成的 STL 文件请用外部 STL/CAD 查看器打开，CSV 文件作为可度量的验证记录。

如果目标是逐项验证 VS Code、MinGW、Ninja、GDB 和 CLI 参数到底改变了什么，请看 [`vscode_mingw_ninja_parameter_debugging.md`](vscode_mingw_ninja_parameter_debugging.md)。本文继续作为工具链安装、任务入口和常用命令说明。

## 推荐扩展

安装 `.vscode/extensions.json` 中列出的扩展：

- C/C++：`ms-vscode.cpptools`
- CMake Tools：`ms-vscode.cmake-tools`
- Vim：`vscodevim.vim`，可选键位扩展

如果使用 VS Code 1.70.2 或离线机器，仓库在 `adm/vscode-extensions/` 中提供了兼容的 VSIX：

- CMake Tools：`ms-vscode.cmake-tools` 1.19.52，`engines.vscode: ^1.67.0`
- Vim：`vscodevim.vim` 1.24.3，`engines.vscode: ^1.67.0`

从仓库根目录安装：

```powershell
code --install-extension adm\vscode-extensions\ms-vscode.cmake-tools-1.19.52.vsix
code --install-extension adm\vscode-extensions\vscodevim.vim-1.24.3.vsix
```

`adm/vscode-extensions/` 中保留的 clangd VSIX 只是历史备用包。当前工作区设置使用 Microsoft C/C++ 扩展，而不是 clangd。

## 工具链原则

主要编辑器配置优先使用 MinGW + Ninja：

```text
编译器：PATH 中的 gcc/g++
构建目录：build/mingw-ninja-release
编译数据库：build/mingw-ninja-release/compile_commands.json
```

仓库设置刻意避免写死机器相关的 MinGW 绝对路径。配置前请确保目标 MinGW 的 `bin` 目录在 `PATH` 中：

```powershell
where g++
where gcc
where ninja
where cmake
```

如果 C/C++ 扩展提示找不到 `Eigen/Dense` 或标准库头文件，先确认 `where g++` 指向预期 MinGW，再重新生成 `compile_commands.json`，然后在 VS Code 命令面板执行 `C/C++: Reset IntelliSense Database`。

临时 PowerShell 会话可以这样加入 MinGW：

```powershell
$env:PATH = "D:\path\to\mingw64\bin;$env:PATH"
```

关键规则：CMake、VS Code 任务和 C/C++ IntelliSense 必须使用同一套 MinGW 工具链和同一个 `compile_commands.json`。

## CMake 工作流

ManuMesh 当前支持 CMake 3.18.6 及以上版本，不依赖 `CMakePresets.json`。VS Code 任务直接调用 `cmake -S ... -B ...`，所以同样命令也可以在普通终端中运行。

常用构建目录：

| 构建目录 | 生成器 | 编译器 | 用途 |
| --- | --- | --- | --- |
| `build/mingw-ninja-debug` | Ninja | MinGW `g++` | 调试、单元测试、格式检查 |
| `build/mingw-ninja-release` | Ninja | MinGW `g++` | 发布构建、演示、Release 测试 |
| `build/mingw-ninja-debug-performance` | Ninja | MinGW `g++` | 性能测试 |
| `build/mingw-ninja-release-performance` | Ninja | MinGW `g++` | Release 性能测试 |
| `build/mingw-ninja-release-sdk` | Ninja | MinGW `g++` | 本地安装和 SDK consumer 测试 |
| `build/msvc-v143` | Visual Studio 17 2022 | MSVC v143 | VS2022 调试和发布构建 |
| `build/msvc-v143-performance` | Visual Studio 17 2022 | MSVC v143 | VS2022 性能测试 |
| `build/msvc-v142` | Visual Studio 17 2022 + `-T v142` | MSVC v142 | VS2019 工具集兼容构建 |
| `build/msvc-v142-performance` | Visual Studio 17 2022 + `-T v142` | MSVC v142 | VS2019 工具集性能测试 |

MSVC 任务运行时会询问 `msvcToolset`：选择 `v143` 使用 MSVC 2022，选择 `v142` 使用 MSVC 2019。两者都使用 Visual Studio 2022 generator 和 x64 平台，但写入独立构建目录，避免切换工具集时发生 CMake 缓存冲突。选择 `v142` 前，必须在 Visual Studio Installer 的“单个组件”中安装 `MSVC v142 - VS 2019 C++ x64/x86 build tools`。

安装入口：[Visual Studio 2022 Build Tools](https://aka.ms/vs/17/release/vs_BuildTools.exe)；需要完整旧版安装器时使用 [Visual Studio 旧版本下载页](https://visualstudio.microsoft.com/vs/older-downloads/)。

MSVC 任务使用 `MANUMESH_GOOGLETEST_PROVIDER=source`，从仓库已有的 GoogleTest 源码在构建目录中生成测试库。这样不会向 `thirdParty` 新增文件，也不会依赖预编译 Debug 库缺失的 PDB。

MinGW + Ninja 的 Release 配置命令：

```powershell
$buildDir = "build/mingw-ninja-release"
cmake -S . -B $buildDir -G Ninja `
  -DCMAKE_MAKE_PROGRAM=ninja `
  -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_C_COMPILER=gcc `
  -DCMAKE_CXX_COMPILER=g++ `
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON `
  -DMANUMESH_GOOGLETEST_PROVIDER=auto `
  -DMANUMESH_BUILD_PERFORMANCE_TESTS=OFF
```

构建全部目标：

```powershell
$buildDir = "build/mingw-ninja-release"
cmake --build $buildDir --parallel
```

运行非性能测试：

```powershell
$buildDir = "build/mingw-ninja-release"
cmake -E chdir $buildDir ctest -LE performance --output-on-failure
```

## Windows MinGW 运行时说明

Release 全量构建会生成测试程序。当前 CMake 配置使用 `gtest_add_tests` 从源码静态注册 GoogleTest 用例，不再使用需要启动测试 exe 的 `gtest_discover_tests`。这样 CTest 运行单个 CLI 或示例测试时，不会因为另一个 GoogleTest exe 的运行时 DLL 问题而提前报错。

仓库的 CMake 仍会把当前 `CMAKE_CXX_COMPILER` 所在目录中的 MinGW 运行时 DLL 复制到构建目录的 `bin` 下。MinGW 的 GoogleTest 默认不再使用仓库里的预编译 `libgtest.dll`、`libgtest_main.dll`；`MANUMESH_GOOGLETEST_PROVIDER=auto` 会跳过这些 DLL，并为当前 MinGW 编译器构建 GoogleTest。这样可以避免外部机器 gcc 版本较旧时，测试 exe 启动阶段因为 GoogleTest DLL 依赖了另一套 MinGW 运行时符号而报 `0xc0000139`。

如果另一台机器仍然出现 `0xc0000139`，优先检查：

```powershell
where g++
where cmake
where ninja
$buildDir = "build/mingw-ninja-release"
Get-ChildItem "$buildDir\bin\*.dll"
```

然后删除对应构建目录或重新运行 configure，再构建：

```powershell
$buildDir = "build/mingw-ninja-release"
cmake -S . -B $buildDir -G Ninja `
  -DCMAKE_MAKE_PROGRAM=ninja `
  -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_C_COMPILER=gcc `
  -DCMAKE_CXX_COMPILER=g++ `
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON `
  -DMANUMESH_GOOGLETEST_PROVIDER=auto `
  -DMANUMESH_BUILD_PERFORMANCE_TESTS=OFF
cmake --build $buildDir --parallel
```

## VS Code 任务

在 `Terminal > Run Task...` 中可用的核心任务：

- `build: mingw+ninja debug`
- `build: mingw+ninja release`
- `test: mingw+ninja debug`
- `test: mingw+ninja release`
- `build: mingw+ninja debug performance`
- `test: mingw+ninja debug performance`
- `build: mingw+ninja release performance`
- `test: mingw+ninja release performance`
- `test: mingw+ninja release sdk consumer`
- `test: mingw+ninja release full`
- `build: msvc selected debug`
- `build: msvc selected release`
- `build: msvc selected debug performance`
- `build: msvc selected release performance`
- `test: msvc selected debug`
- `test: msvc selected release`
- `test: msvc selected debug performance`
- `test: msvc selected release performance`
- `build: docs-api`
- `build: docs-internal`

`build: docs-api` 与 `build: docs-internal` 优先使用仓库内 `thirdParty/doxygen`
与 `thirdParty/graphviz`；只有 vendored 工具缺失时才回退到系统 PATH。

