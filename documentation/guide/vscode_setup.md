# VS Code + Visual Studio 2019 构建与调试说明

ManuMesh 的 Windows 开发基线是 Visual Studio 16 2019、MSVC v142 和 x64。VS Code 任务、CMake Tools 和手工命令统一读取根目录的 `CMakePresets.json`，不再在编辑器配置中重复维护生成器、工具集或功能开关。

逐项调试 CLI、特征识别、Normal Tensor 和简化参数时，请看 [`vscode_vs2019_parameter_debugging.md`](vscode_vs2019_parameter_debugging.md)。

## 环境要求

Visual Studio 2019 需要安装以下组件：

- “使用 C++ 的桌面开发”工作负载。
- MSVC v142 x64/x86 生成工具。
- Windows 10 SDK。
- CMake 3.20 或更高版本；Visual Studio 2019 16.11 自带版本可直接使用。

VS Code 推荐安装 `.vscode/extensions.json` 中列出的扩展：

- C/C++：`ms-vscode.cpptools`。
- CMake Tools：`ms-vscode.cmake-tools`。
- Vim：`vscodevim.vim`，可选。

在 PowerShell 中先验证 CMake 能识别仓库 preset：

```powershell
cmake --version
cmake --list-presets
```

需要确认 Visual Studio 安装时，可以使用安装器自带的 `vswhere.exe`：

```powershell
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
& $vswhere -latest -products * -version "[16.0,17.0)" `
  -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
  -property installationPath
```

使用 Visual Studio 生成器时，普通 PowerShell 即可配置。使用 `vs2019-ninja-*` preset 时，应从 Visual Studio 2019 x64 Developer PowerShell 启动 VS Code，并确保 `cl` 与 `ninja` 可用；两组 preset 都使用 v142 编译器。

## CMake preset

VS Code 的日常任务使用下列 Visual Studio 2019 preset：

| Configure preset | 构建目录 | 用途 |
| --- | --- | --- |
| `vs2019-debug` | `build/vs2019-debug` | 日常 Debug、CLI、示例和快速测试。 |
| `vs2019-asan` | `build/vs2019-asan` | VS2019 AddressSanitizer 内存安全回归。 |
| `vs2019-release` | `build/vs2019-release` | 优化共享库、CLI、示例和回归测试。 |
| `vs2019-release-static` | `build/vs2019-release-static` | 静态库及静态 consumer 验证。 |
| `vs2019-release-performance` | `build/vs2019-release-performance` | 性能测试。 |
| `vs2019-release-sdk` | `build/vs2019-release-sdk` | 安装包、C/C++ SDK consumer 和 CMake package 验证。 |
| `vs2019-release-docs` | `build/vs2019-release-docs` | API 与内部 Doxygen 文档。 |

根目录还提供同名的 `vs2019-ninja-*` 变体，供已经进入 VS2019 v142 开发者环境的 Ninja Multi-Config 用户使用。不要让同一个构建目录在两种生成器之间复用。

## 常用命令

Debug 构建与快速测试：

```powershell
cmake --preset vs2019-debug
cmake --build --preset vs2019-debug --parallel
ctest --preset vs2019-debug-unit
```

AddressSanitizer 内存安全回归：

```powershell
cmake --preset vs2019-asan
cmake --build --preset vs2019-asan -- /m:4
ctest --preset vs2019-asan-unit
```

该测试 preset 保留单元、架构和所有权生命周期压力测试，排除外部数据集以及受插桩开销影响的性能阈值测试；真实 ASan 错误仍会立即终止测试。
VS2019 v142 的 Windows AddressSanitizer 不支持 `detect_leaks=1`；泄漏回归由
`ownership_lifetime_stress` 在 64 轮 C/C++ API 生命周期中统计未释放的 C++ 分配，
ASan 负责检查越界、释放后使用和重复释放。

Release 全量回归：

```powershell
cmake --preset vs2019-release
cmake --build --preset vs2019-release --parallel
ctest --preset vs2019-release-full
```

静态库、性能和 SDK 各自使用隔离目录：

```powershell
cmake --preset vs2019-release-static
cmake --build --preset vs2019-release-static --parallel
ctest --preset vs2019-release-static-unit

cmake --preset vs2019-release-performance
cmake --build --preset vs2019-release-performance --parallel
ctest --preset vs2019-release-performance

cmake --preset vs2019-release-sdk
cmake --build --preset vs2019-release-sdk --parallel
ctest --preset vs2019-release-sdk
```

生成文档和检查格式：

```powershell
cmake --preset vs2019-release-docs
cmake --build --preset vs2019-release-docs --parallel

cmake --preset vs2019-debug
cmake --build --preset vs2019-debug --target check-format --parallel
```

Visual Studio 多配置生成器的主要可执行文件位于：

```text
build/vs2019-debug/bin/Debug/manumesh.exe
build/vs2019-debug/bin/Debug/manumesh_tests.exe
build/vs2019-release/bin/Release/manumesh.exe
build/vs2019-release-performance/bin/Release/manumesh_performance_tests.exe
```

## VS Code 任务

在 `Terminal > Run Task...` 中使用以下入口：

- `build: vs2019 debug`
- `build: vs2019 asan`
- `build: vs2019 release`
- `build: vs2019 release static`
- `build: vs2019 release performance`
- `build: vs2019 release SDK`
- `build: vs2019 release docs`
- `test: vs2019 debug`
- `test: vs2019 debug external`
- `test: vs2019 asan`
- `test: vs2019 release`
- `test: vs2019 release full`
- `test: vs2019 release static`
- `test: vs2019 release performance`
- `test: vs2019 release SDK`
- `verify: vs2019 release matrix`
- `demo: simplify selected mesh`
- `demo: feature report selected mesh`
- `demo: feature benchmark selected mesh`
- `demo: ratio sweep selected mesh`
- `run: feature validation`
- `run: external validation`
- `check: format`
- `format`

构建和测试任务会先执行对应的 configure preset。`verify: vs2019 release matrix` 顺序覆盖 Release 动态库、静态库、性能、SDK、文档和格式检查，适合发布前使用。

## F5 调试

`.vscode/launch.json` 使用 Microsoft C++ 原生调试器，并提供六个入口：

- `VS2019 Debug CLI - Feature Curves`
- `VS2019 Debug CLI - Feature Report`
- `VS2019 Debug Unit Tests - Filter`
- `VS2019 Debug + debugUtil Unit Tests - Filter`
- `VS2019 ASan Unit Tests - Filter`
- `VS2019 ASan Ownership Lifetime Stress`

CLI 配置会提示选择输入网格、简化比例、采样数量和特征角度。测试配置通过 `gtestFilter` 接受完整过滤表达式，例如：

```text
ManuMeshParameters.*
ManuMeshFeatureDetection.*
ManuMeshSimplifier.*
```

调试前任务会自动配置并构建对应的 Debug preset。ASan 入口设置为发现首个内存错误即终止；普通单元测试入口不带插桩。工作目录固定为仓库根目录，因此 `tests/data/...` 和 `output/...` 相对路径可直接使用。

## IntelliSense

`.vscode/settings.json` 将 CMake Tools 作为 C/C++ 配置提供者。首次打开仓库时：

1. 执行 `CMake: Select Configure Preset`，选择 `vs2019-debug`。
2. 执行 `CMake: Configure`。
3. 如果包含路径仍未刷新，执行 `C/C++: Reset IntelliSense Database`。

不要手工写入某台机器的编译器绝对路径。头文件搜索路径、编译定义和语言标准应来自已选择的 CMake preset。

## 常见问题

### CMake 找不到 Visual Studio 2019

先运行前面的 `vswhere.exe` 命令。若没有输出，检查 Visual Studio 2019 的 C++ 工作负载和 v142 组件；若有输出但 `cmake --preset vs2019-debug` 仍失败，确认当前 `cmake.exe` 版本至少为 3.20。

### generator 或 toolset mismatch

构建目录保存了生成器和工具集。只删除报错的精确目录后重新运行 preset，例如：

```powershell
$buildDir = Resolve-Path "build/vs2019-debug" -ErrorAction Stop
Remove-Item -LiteralPath $buildDir -Recurse -Force
cmake --preset vs2019-debug
```

不要删除整个仓库根目录，也不要让其他 preset 共用这个目录。

### 断点不命中

确认选择的是 Debug launch 配置，程序路径包含 `bin/Debug`，并且 `preLaunchTask` 构建的是同一个 preset。若源文件或 PDB 时间戳不一致，重新运行 `build: vs2019 debug`。

### SDK consumer 失败

运行 `build: vs2019 release SDK`。该 build preset 会构建安装与 consumer 验证目标；随后运行 `test: vs2019 release SDK` 完成该配置下的回归测试。不要从普通 Release 目录手工拼装 SDK。
