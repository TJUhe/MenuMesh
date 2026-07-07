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
构建目录：build/${workspaceFolderBasename}/mingw-ninja-release
编译数据库：build/${workspaceFolderBasename}/mingw-ninja-release/compile_commands.json
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
| `build/${workspaceFolderBasename}/mingw-ninja-debug` | Ninja | MinGW `g++` | 调试、单元测试、格式检查 |
| `build/${workspaceFolderBasename}/mingw-ninja-release` | Ninja | MinGW `g++` | 发布构建、演示、Release 测试 |
| `build/${workspaceFolderBasename}/mingw-ninja-debug-performance` | Ninja | MinGW `g++` | 性能测试 |
| `build/${workspaceFolderBasename}/mingw-ninja-release-performance` | Ninja | MinGW `g++` | Release 性能测试 |
| `build/${workspaceFolderBasename}/mingw-ninja-release-sdk` | Ninja | MinGW `g++` | 本地安装和 SDK consumer 测试 |
| `build/${workspaceFolderBasename}/msvc-vs2022` | Visual Studio 17 2022 | MSVC | VS2022 调试和发布构建 |
| `build/${workspaceFolderBasename}/msvc-vs2022-performance` | Visual Studio 17 2022 | MSVC | VS2022 性能测试 |
| `build/${workspaceFolderBasename}/msvc-vs2019` | Visual Studio 16 2019 | MSVC | VS2019 调试和发布构建 |
| `build/${workspaceFolderBasename}/msvc-vs2019-performance` | Visual Studio 16 2019 | MSVC | VS2019 性能测试 |

MinGW + Ninja 的 Release 配置命令：

```powershell
$buildDir = "build/$(Split-Path -Leaf (Get-Location))/mingw-ninja-release"
cmake -S . -B $buildDir -G Ninja `
  -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_C_COMPILER=gcc `
  -DCMAKE_CXX_COMPILER=g++ `
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON `
  -DMANUMESH_GOOGLETEST_PROVIDER=auto `
  -DMANUMESH_BUILD_PERFORMANCE_TESTS=OFF
```

构建全部目标：

```powershell
$buildDir = "build/$(Split-Path -Leaf (Get-Location))/mingw-ninja-release"
cmake --build $buildDir --parallel
```

运行非性能测试：

```powershell
$buildDir = "build/$(Split-Path -Leaf (Get-Location))/mingw-ninja-release"
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
$buildDir = "build/$(Split-Path -Leaf (Get-Location))/mingw-ninja-release"
Get-ChildItem "$buildDir\bin\*.dll"
```

然后删除对应构建目录或重新运行 configure，再构建：

```powershell
$buildDir = "build/$(Split-Path -Leaf (Get-Location))/mingw-ninja-release"
cmake -S . -B $buildDir -G Ninja `
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
- `build: mingw+ninja debug tests`
- `build: mingw+ninja debug all`
- `build: mingw+ninja release`
- `build: mingw+ninja release all`
- `test: mingw+ninja debug`
- `test: mingw+ninja release`
- `build: mingw+ninja debug performance tests`
- `test: mingw+ninja debug performance`
- `build: mingw+ninja release performance tests`
- `test: mingw+ninja release performance`
- `test: mingw+ninja release sdk consumer`
- `test: mingw+ninja release full`
- `build: msvc+vs2022 debug`
- `build: msvc+vs2022 debug tests`
- `build: msvc+vs2022 debug all`
- `build: msvc+vs2022 release`
- `build: msvc+vs2022 release tests`
- `build: msvc+vs2022 release all`
- `test: msvc+vs2022 debug`
- `test: msvc+vs2022 release`
- `test: msvc+vs2022 debug performance`
- `test: msvc+vs2022 release performance`
- `test: msvc+vs2022 full`
- `build: msvc+vs2019 debug`
- `build: msvc+vs2019 debug tests`
- `build: msvc+vs2019 debug all`
- `build: msvc+vs2019 release`
- `build: msvc+vs2019 release tests`
- `build: msvc+vs2019 release all`
- `test: msvc+vs2019 debug`
- `test: msvc+vs2019 release`
- `test: msvc+vs2019 debug performance`
- `test: msvc+vs2019 release performance`
- `test: msvc+vs2019 full`
- `build: msvc custom debug`
- `build: msvc custom debug tests`
- `build: msvc custom debug all`
- `build: msvc custom release`
- `build: msvc custom release tests`
- `build: msvc custom release all`
- `test: msvc custom debug`
- `test: msvc custom release`
- `check: format`
- `format`
- `build: docs-api`

`test: mingw+ninja release full` 会按顺序运行 Release 非性能回归、Release 性能测试、SDK consumer 测试和 API 文档生成。MSVC 的 `full` 任务分别覆盖 Debug/Release 的非性能测试和性能测试；需要本机安装对应版本的 Visual Studio 生成器。

演示和验证任务：

- `demo: simplify standard selected mesh`
- `demo: simplify feature curves selected mesh`
- `demo: simplify normal tensor selected mesh`
- `demo: feature report selected mesh`
- `demo: ratio sweep selected mesh`
- `run: feature validation`
- `run: external validation`
- `open: vscode demo output`

`demo:*` 任务会询问网格、简化比例、特征角度和采样数量。输出写入 `output/vscode_demo/`。

## VS Code 调试配置

Run and Debug 面板中的配置：

- `(MinGW Ninja) Debug CLI Feature Curves`
- `(MinGW Ninja) Debug CLI Feature Report`
- `(MinGW Ninja) Debug Unit Tests Filter`
- `(MinGW Ninja) Debug Performance Tests Filter`
- `(MSVC VS2022) Debug CLI Feature Curves`
- `(MSVC VS2022) Debug Unit Tests Filter`
- `(MSVC VS2019) Debug CLI Feature Curves`
- `(MSVC VS2019) Debug Unit Tests Filter`

MinGW 调试需要 `gdb.exe` 在 `PATH` 中。MSVC 调试配置使用 `cppvsdbg`，需要对应版本的 Visual Studio 调试组件。

推荐断点：

- `src/simplification/SimplificationRun.cpp`：collapse loop、候选接受/拒绝、报告计数器递增。
- `src/simplification/SimplificationPolicies.cpp`：公开 `SimplifyOptions` 到内部 target/features/legality policy 的转换。
- `src/feature_detection/FeatureDetector.cpp`：特征边收集、特征图遍历和 loop/cycle 恢复主流程。
- `src/feature_detection/PrimitiveFit.cpp`：圆、近圆和椭圆 primitive 拟合。
- `src/feature_detection/NormalTensor.cpp`：normal-tensor 特征评分。

## 常用命令示例

生成特征报告：

```powershell
$exe = "build/$(Split-Path -Leaf (Get-Location))/mingw-ninja-release/bin/manumesh.exe"
& $exe feature-report `
  tests\data\feature_fixtures\coaxial_hole_plate.obj `
  --feature-angle-deg 25 `
  --circle-fit-threshold 0.04 `
  --ellipse-fit-threshold 0.05 `
  --csv output\vscode_demo\features.csv
```

带特征曲线保护的简化：

```powershell
$exe = "build/$(Split-Path -Leaf (Get-Location))/mingw-ninja-release/bin/manumesh.exe"
& $exe simplify `
  tests\data\feature_fixtures\coaxial_hole_plate.obj `
  output\vscode_demo\feature_curves.stl `
  --method line `
  --line-weight 1e-3 `
  --weight-mode dihedral `
  --feature-boost 0.08 `
  --feature-angle-deg 25 `
  --preserve-feature-curves `
  --feature-curve-weight 0.08 `
  --max-feature-curve-deviation-ratio 0.05 `
  --circle-fit-threshold 0.04 `
  --ellipse-fit-threshold 0.05 `
  --min-circular-feature-loop-vertices 12 `
  --ratio 0.25 `
  --samples 512 `
  --metrics-csv output\vscode_demo\feature_curves_metrics.csv
```

比例扫描：

```powershell
$exe = "build/$(Split-Path -Leaf (Get-Location))/mingw-ninja-release/bin/manumesh.exe"
& $exe ratio-sweep `
  tests\data\external\fandisk_2014.stl `
  output\vscode_demo\ratio_sweep `
  --method line `
  --line-weight 1e-3 `
  --weight-mode dihedral `
  --feature-boost 0.08 `
  --feature-angle-deg 25 `
  --ratios "0.8,0.5,0.25,0.1" `
  --samples 512
```

外部验证：

```powershell
$exe = "build/$(Split-Path -Leaf (Get-Location))/mingw-ninja-release/bin/manumesh.exe"
& $exe validate-features --ratio 0.20 --samples 1000
& $exe validate-external --ratio 0.25 --samples 800
```

## 演示用例建议

| 目的 | 网格 | 参数建议 | 讲解重点 |
| --- | --- | --- | --- |
| 特征环保护 | `tests/data/feature_fixtures/coaxial_hole_plate.obj` | `feature curves`，比例 `0.50` 或 `0.25` | 四个圆形环、曲线投影、`projected_feature_placements` |
| 椭圆和圆的预算权衡 | `tests/data/feature_fixtures/elliptical_hole_plate.obj` | `feature curves` | 每个环独立预算优于只有全局最小顶点数 |
| 硬边线二次误差 | `tests/data/external/fandisk_2014.stl` | `dihedral`，特征角 `15/25/45` | 二面角特征权重如何影响保持效果和三角形质量 |
| 保守工业模型简化 | `tests/data/external/casting_aimshape_2014.stl` | 严格局部误差和质量保护 | 更强保护会增加拒绝折叠次数和耗时 |
| 弱特征实验 | `tests/data/feature_fixtures/boss_pocket_plate.obj` | `normal tensor` | normal-tensor 作为二面角特征的补充 |
