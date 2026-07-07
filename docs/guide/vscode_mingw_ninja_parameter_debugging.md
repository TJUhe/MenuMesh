# VS Code + MinGW + Ninja 参数调试教程

本文用于在 VS Code 中用 MinGW、Ninja 和 GDB 调试 ManuMesh，并逐项校验每个参数的意义。它不是另一份构建清单，而是一份“参数实验手册”：每改一个参数，都要知道它进入了哪个字段、影响了管线的哪一层、应该在哪个断点观察、最后应该看哪些输出指标。

本文以当前仓库为准，核心来源是：

- `.vscode/tasks.json`：VS Code 任务、CMake 参数、CTest 参数。
- `.vscode/launch.json`：GDB 调试入口和默认 CLI 参数。
- `apps/manumesh/main.cpp`：CLI 参数解析、命令入口、CSV 输出。
- `include/manumesh/algorithms/simplification/SimplificationTypes.h`：`SimplifyOptions` 和 `SimplifyReport`。
- `include/manumesh/algorithms/feature_detection/FeatureTypes.h`：`FeatureOptions` 和 `FeatureAnalysis`。
- `src/simplification/` 与 `src/feature_detection/`：参数真正改变算法行为的位置。

## 调试前检查

先确认 VS Code、CMake、Ninja、MinGW 和 GDB 使用的是同一套工具链：

```powershell
where cmake
where ninja
where gcc
where g++
where gdb
```

如果 `where g++` 与 `where gdb` 指向不同 MinGW 安装目录，调试时可能出现断点不命中、运行时 DLL 不匹配或测试 exe 启动失败。当前 VS Code 配置刻意使用 `gcc`、`g++`、`gdb.exe` 这些 PATH 名称，而不是写死某台机器的绝对路径。

构建目录使用：

```text
build/${workspaceFolderBasename}/mingw-ninja-debug
build/${workspaceFolderBasename}/mingw-ninja-release
build/${workspaceFolderBasename}/mingw-ninja-debug-performance
build/${workspaceFolderBasename}/mingw-ninja-release-performance
```

`${workspaceFolderBasename}` 会随当前文件夹名变化。例如仓库文件夹叫 `ManuMesh` 时，Debug 目录就是 `build/ManuMesh/mingw-ninja-debug`。这能避免项目改名后仍复用旧目录里的 `CMakeCache.txt`。

如果遇到：

```text
CMakeCache.txt directory ... is different than the directory ... where CMakeCache.txt was created
The source ... does not match the source ... used to generate cache
```

先检查缓存记录的源目录：

```powershell
$buildDir = "build/$(Split-Path -Leaf (Get-Location))/mingw-ninja-debug"
Get-Content "$buildDir\CMakeCache.txt" | Select-String "CMAKE_HOME_DIRECTORY|CMAKE_CACHEFILE_DIR"
```

确认 `$buildDir` 指向当前仓库内的预期目录后，再删除这个具体构建目录并重新配置：

```powershell
Remove-Item -LiteralPath $buildDir -Recurse -Force
cmake -S . -B $buildDir -G Ninja `
  -DCMAKE_BUILD_TYPE=Debug `
  -DCMAKE_C_COMPILER=gcc `
  -DCMAKE_CXX_COMPILER=g++ `
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON `
  -DMANUMESH_GOOGLETEST_PROVIDER=auto `
  -DMANUMESH_BUILD_PERFORMANCE_TESTS=OFF
```

不要在 `tasks.json` 中调用外部脚本。当前仓库约定是：VS Code task 直接调用 `cmake`、`ctest` 或 `manumesh.exe`。

## VS Code 工作流

用于参数调试的主路径是：

1. 运行 `Terminal > Run Task... > build: mingw+ninja debug`。
2. 打开 Run and Debug，选择 `(MinGW Ninja) Debug CLI Feature Curves` 或 `(MinGW Ninja) Debug CLI Feature Report`。
3. 在 VS Code 弹出的输入框中选择 `launchMesh`、`launchRatio`、`launchSamples`、`launchFeatureAngle`。
4. 命中断点后观察 `args`、`options`、`featureAnalysis_`、`report_` 和输出 CSV。

如果要调单元测试：

1. 选择 `(MinGW Ninja) Debug Unit Tests Filter`。
2. 在 `gtestFilter` 输入类似 `ManuMeshParameters.*`、`ManuMeshFeatureDetection.*` 或 `ManuMeshSimplifier.*`。
3. 断点可以直接打在被测函数或 `tests/unit/...` 中的断言附近。

如果要调性能测试：

1. 选择 `(MinGW Ninja) Debug Performance Tests Filter`。
2. 该配置使用 `build/${workspaceFolderBasename}/mingw-ninja-debug-performance`。
3. 它会先运行 `build: mingw+ninja debug performance tests`，也就是 configure 时带 `-DMANUMESH_BUILD_PERFORMANCE_TESTS=ON`。

## tasks.json 参数

这些参数控制“工程如何被生成、构建和测试”，不会直接改变算法结果，但会改变调试可用性、测试覆盖和运行时环境。

| 参数 | 出现位置 | 含义 | 校验方式 |
| --- | --- | --- | --- |
| `type: process` | 所有 task | VS Code 直接启动一个进程，不经过 shell 脚本包装。 | 任务输出里应直接显示 `cmake`、`ctest` 或 `manumesh.exe`。 |
| `command: cmake` | configure/build/test/docs task | 统一用 CMake 驱动生成、构建、CTest 和文档目标。 | 任务开始行应是 `cmake ...`。 |
| `-S .` | configure task | 源码目录是当前工作区根目录。 | CMakeCache 中 `CMAKE_HOME_DIRECTORY` 应指向当前仓库。 |
| `-B build/${workspaceFolderBasename}/...` | configure task | 构建目录随项目文件夹名变化，避免旧仓库名缓存混用。 | 看实际生成目录是否带当前文件夹名。 |
| `-G Ninja` | MinGW configure task | 使用 Ninja 单配置生成器。 | `CMAKE_GENERATOR:INTERNAL=Ninja`。 |
| `-DCMAKE_BUILD_TYPE=Debug` | Debug configure task | 生成带调试信息、优化较低的构建。 | GDB 能进入源码，变量较容易观察。 |
| `-DCMAKE_BUILD_TYPE=Release` | Release configure task | 生成优化构建，用于演示、指标和发布验证。 | STL/CSV 性能更接近实际使用。 |
| `-DCMAKE_C_COMPILER=gcc` | MinGW configure task | C 编译器来自 PATH。 | `CMAKE_C_COMPILER` 应指向目标 MinGW。 |
| `-DCMAKE_CXX_COMPILER=g++` | MinGW configure task | C++ 编译器来自 PATH。 | `CMAKE_CXX_COMPILER` 应与 `gdb.exe` 属于同一套 MinGW。 |
| `-DCMAKE_EXPORT_COMPILE_COMMANDS=ON` | configure task | 生成 `compile_commands.json`，供 C/C++ 扩展和静态分析使用。 | 构建目录下存在 `compile_commands.json`。 |
| `-DMANUMESH_GOOGLETEST_PROVIDER=auto` | MinGW configure task | 让 CMake 为当前 MinGW 选择合适 GoogleTest，避免旧预编译 DLL 不匹配。 | 测试 exe 启动不应报 `0xc0000139`。 |
| `-DMANUMESH_BUILD_PERFORMANCE_TESTS=ON/OFF` | performance configure task | 是否生成性能测试目标。 | `manumesh_performance_tests.exe` 是否存在。 |
| `-DMANUMESH_ENABLE_INSTALL=ON` | release sdk task | 打开安装和 SDK 本地验证目标。 | `sdk-consumer-test` 可构建。 |
| `-DMANUMESH_INSTALL_CMAKE_CONFIG=ON` | release sdk task | 安装 `ManuMeshConfig.cmake`，用于下游 `find_package`。 | SDK 目录下有 `lib/cmake/ManuMesh`。 |
| `cmake --build <dir> --target manumesh --parallel` | build task | 只构建 CLI 和库所需目标。 | `bin/manumesh.exe` 更新。 |
| `--target manumesh_tests` | test build task | 只构建单元测试 exe。 | `bin/manumesh_tests.exe` 更新。 |
| `--target manumesh_performance_tests` | performance build task | 只构建性能测试 exe。 | `bin/manumesh_performance_tests.exe` 更新。 |
| `cmake -E chdir <dir> ctest` | test task | 在构建目录内运行 CTest，不依赖脚本。 | CTest 能找到测试注册文件。 |
| `ctest -LE performance` | 非性能测试 task | 排除 `performance` 标签。 | 快速回归，不跑大模型性能测试。 |
| `ctest -L performance` | 性能测试 task | 只运行 `performance` 标签。 | 用于大模型和耗时指标。 |
| `--output-on-failure` | CTest task | 失败时打印测试输出。 | 失败定位时能看到 CLI/stdout。 |
| `dependsOn` | build/test/demo task | 确保运行前自动 configure 或 build。 | 第一次运行 task 会先生成构建目录。 |
| `dependsOrder: sequence` | full/demo task | 多个依赖按顺序执行。 | Release full 会依次跑测试、SDK 和文档。 |
| `problemMatcher: "$gcc"` | MinGW build task | 将 GCC 编译错误映射到 VS Code Problems。 | 编译错误可点击跳转源码。 |
| `inputs` | demo/debug task | VS Code 运行时弹出的参数选择。 | 改 `demoRatio` 或 `launchRatio` 会改变传给 CLI 的 `--ratio`。 |

## launch.json 参数

这些参数控制“调试器如何启动程序”。它们不改变算法实现，但决定断点、命令行参数和当前目录。

| 字段 | MinGW 配置中的值 | 含义 | 校验方式 |
| --- | --- | --- | --- |
| `type` | `cppdbg` | 使用 Microsoft C/C++ 扩展的 GDB/MI 调试器。 | 调试控制台显示 GDB/MI 启动信息。 |
| `request` | `launch` | 启动新进程，而不是附加到已有进程。 | 每次 F5 都会新启动 `manumesh.exe`。 |
| `program` | `build/${workspaceFolderBasename}/mingw-ninja-debug/bin/manumesh.exe` | 被调试的 Debug CLI。 | 文件存在且时间戳随 Debug 构建更新。 |
| `args` | CLI 参数数组 | 传给 `manumesh.exe` 的真实命令行。 | 在 `apps/manumesh/main.cpp::main` 观察 `argv`。 |
| `cwd` | `${workspaceFolder}` | 程序运行目录是仓库根目录。 | 相对路径如 `tests/data/...` 能解析。 |
| `stopAtEntry` | `false` | 不在进程入口自动暂停。 | 需要自己打断点。 |
| `externalConsole` | `false` | 使用 VS Code 内部调试控制台。 | 不弹独立控制台窗口。 |
| `MIMode` | `gdb` | 使用 GDB。 | 需要 `gdb.exe` 在 PATH。 |
| `miDebuggerPath` | `gdb.exe` | 调试器可执行文件名。 | `where gdb` 应能找到。 |
| `preLaunchTask` | `build: mingw+ninja debug` 等 | 调试前自动构建对应目标。 | F5 前会先跑 task。 |

`launch.json` 中最有用的两个 CLI 调试配置是：

- `(MinGW Ninja) Debug CLI Feature Report`：只跑特征检测，适合验证 `--feature-angle-deg`、primitive 拟合和 normal tensor 参数。
- `(MinGW Ninja) Debug CLI Feature Curves`：跑完整简化，适合验证 QEM/line quadrics、特征保护和合法性过滤。

## 推荐断点

| 断点位置 | 用来观察什么 |
| --- | --- |
| `apps/manumesh/main.cpp::main` | 命令名、原始 `argv`、未知参数拒绝。 |
| `apps/manumesh/main.cpp::parseSimplifyOptions` | CLI 参数如何进入 `SimplifyOptions`。 |
| `apps/manumesh/main.cpp::parseFeatureOptions` | `feature-report` 参数如何进入 `FeatureOptions`。 |
| `src/simplification/SimplificationValidation.cpp` | 参数范围是否合法，例如角度、比例、质量阈值。 |
| `src/simplification/SimplificationPolicies.cpp::makePolicies` | 公开 options 如何拆成 target、features、legality policy。 |
| `src/feature_detection/FeatureDetector.cpp` | boundary、dihedral、normal-tensor edge 如何进入 feature graph。 |
| `src/feature_detection/PrimitiveFit.cpp` | 圆、近圆、椭圆、折线 loop 的拟合判定。 |
| `src/feature_detection/NormalTensor.cpp` | normal tensor 多尺度和 smoothing 后的 feature score。 |
| `src/simplification/Quadrics.cpp` | plane quadrics、line quadrics、weight mode 和 feature boost 的实际权重。 |
| `src/simplification/FeatureConstraints.cpp` | `FeatureProtectionMode` 如何产生硬拒绝。 |
| `src/simplification/CollapseLegality.cpp` | 三角形质量、法线偏转、局部误差、自交检查。 |
| `src/simplification/SimplificationRun.cpp` | collapse 主循环、候选接受/拒绝、`SimplifyReport` 计数。 |

调试时不要只看最后 STL。参数意义通常体现在三个层面：

1. `Options` 字段是否变了。
2. 队列排序或硬过滤是否因此改变。
3. `SimplifyReport`、feature report CSV 或 metrics CSV 是否出现对应变化。

## 算法分层

ManuMesh 当前简化管线可以看成四层：

| 层级 | 代表参数 | 数学含义 | 典型输出 |
| --- | --- | --- | --- |
| 目标规模 | `--ratio`、`--target-faces`、`--ratios`、`--faces` | 决定边坍缩停止条件。 | `faces`、`collapsed_edges`、`termination_reason`。 |
| 候选排序 | `--method`、`--line-weight`、`--weight-mode`、`--feature-boost`、`--adaptive-scale` | QEM 和 line quadrics 给每个候选 placement 排序。line quadrics 主要抑制平坦区域切向漂移。 | `min_line_weight`、`max_line_weight`、`solver_fallbacks`。 |
| 特征图与曲线保护 | `--feature-angle-deg`、`--preserve-feature-curves`、`--feature-protection-mode`、primitive 拟合参数、normal tensor 参数 | 从三角网格推断 boundary、crease、弱特征和 primitive loop，再决定软约束或硬拒绝。 | `feature_edges`、`loops`、`feature_rejected_collapses`、`projected_feature_placements`。 |
| 合法性过滤 | `--preserve-boundary`、`--min-triangle-quality`、`--max-normal-deviation-deg`、`--max-local-error*`、`--prevent-local-intersections`、`--industrial-safe` | QEM 是排序，不是安全证明。硬过滤器负责阻止拓扑、法线、质量、局部误差和自交问题。 | `boundary_rejected_collapses`、`quality_rejected_collapses`、`error_rejected_collapses` 等。 |

文献上，QEM/line quadrics 更像候选代价，不能替代合法性检查。特征保护也不是“把所有特征点锁死”这么简单；过强硬锁会保住曲线但损伤三角形质量，所以当前默认 `primitive-curves` 只硬保护拟合出的圆、近圆和椭圆 loop，普通 crease 更多作为软成本和过滤依据。

## CLI 参数总表

### 通用命令参数

| 参数 | 适用命令 | 进入位置 | 意义 | 校验方法 |
| --- | --- | --- | --- | --- |
| `--samples N` | `compare`、`simplify`、`sweep`、`ratio-sweep`、`face-sweep`、`demo`、`validate-*` | `apps/manumesh/main.cpp` | 采样距离误差的采样数量，影响误差统计耗时和稳定性，不改变简化本身。 | 调大后 `mean_orig_to_simp`、`max_orig_to_simp` 更稳定但运行更慢。 |
| `--csv path` | `feature-report`、`feature-compare` | `apps/manumesh/main.cpp` | 写出特征报告或特征比较 CSV。 | 文件中应有 `feature_edges`、`loops` 等字段。 |
| `--metrics-csv path` | `simplify` | `apps/manumesh/main.cpp` | 写出一行简化指标 CSV。 | 文件中应有 `collapsed_edges`、`termination_reason`、拒绝计数。 |
| `--input-dir dir` | `demo`、`validate-features`、`validate-external` | `apps/manumesh/main.cpp` | 指定批量验证输入目录。 | 输出日志应读取该目录下模型。 |
| `--output-dir dir` | `demo`、`validate-features`、`validate-external` | `apps/manumesh/main.cpp` | 指定批量验证输出目录。 | STL/CSV 写到该目录。 |
| `--quick` | `demo` | `apps/manumesh/main.cpp` | 快速 demo 模式，默认减少采样数。 | `samples` 默认从 `1000` 降为 `500`。 |
| `--verbose` | `simplify` 系列解析 | `SimplifyOptions::verbose` | 当前作为诊断开关保留，主要用于后续扩展详细日志。 | 在 `parseSimplifyOptions` 确认字段变为 `true`。 |

### 生成与扫描参数

| 参数 | 适用命令 | 进入位置 | 意义 | 校验方法 |
| --- | --- | --- | --- | --- |
| `--type clustered-plane` | `generate` | `commandGenerate` | 选择内置生成网格类型。当前公开示例是 `clustered-plane`。 | 输出 STL 形状应为聚簇平面测试网格。 |
| `--n N` | `generate` | `commandGenerate` | 控制内置生成网格的分辨率或规模。 | N 越大，输出 faces/vertices 越多。 |
| `--out path` | `generate` | `commandGenerate` | 指定生成 STL 路径。 | 文件应写到该路径。 |
| `--weights list` | `sweep` | `commandSweep` | 对一组 line weight 做扫描。当前是低频调试入口，VS Code demo 优先使用 `ratio-sweep`。 | `metrics.csv` 中 `line_weight` 多行变化。 |
| `--ratios list` | `ratio-sweep` | `commandRatioSweep` | 对一组输出比例做扫描。 | 输出多个 `*_r_*` STL 和 `metrics.csv`。 |
| `--faces list` | `face-sweep` | `commandFaceSweep` | 对一组绝对目标面数做扫描。 | 输出多个 `*_f_*` STL 和 `target_faces` CSV 列。 |
| `--spindle-input path` | `validate-features` | `commandValidateFeatures` | 替换默认 spindle/shaft 外部验证模型。 | 输出文件名和日志应使用自定义输入。 |
| `--ring-input path` | `validate-features` | `commandValidateFeatures` | 替换默认 ring/track 外部验证模型。 | 同上。 |
| `--pulley-input path` | `validate-features` | `commandValidateFeatures` | 替换默认 pulley 外部验证模型。 | 同上。 |
| `--flange-input path` | `demo`、`validate-features` | `commandDemo`、`commandValidateFeatures` | 替换默认 flange 外部验证模型。 | 同上。 |

### 目标规模参数

| 参数 | 进入字段 | 数学/工程意义 | 断点 | 输出观察 |
| --- | --- | --- | --- | --- |
| `--ratio R` | `SimplifyOptions::targetRatio` | 输出面数约为输入面数的 R 倍，范围 `(0, 1]`。 | `SimplificationPolicies.cpp::resolveTargetFaces` | `faces`、`collapsed_edges`、`termination_reason`。 |
| `--target-faces N` | `SimplifyOptions::targetFaces` | 绝对目标面数。大于 0 时覆盖 `--ratio`。 | `SimplificationPolicies.cpp::resolveTargetFaces` | 输出 `faces` 更接近 N。 |

调试建议：同一模型上先只改 `--ratio 0.80/0.50/0.25/0.10`。如果 `termination_reason` 不是 `reached-target`，说明某类硬过滤器把可行坍缩耗尽了，需要看拒绝计数。

### QEM 与 line quadrics 参数

| 参数 | 进入字段 | 数学/工程意义 | 断点 | 输出观察 |
| --- | --- | --- | --- | --- |
| `--method standard` | `useLineQuadrics=false`、`lineWeight=0` | 只使用标准平面 QEM。 | `parseSimplifyOptions`、`Quadrics.cpp` | `min_line_weight=max_line_weight=0`。 |
| `--method line` | `useLineQuadrics=true` | 在 QEM 基础上加入 line quadrics，改善平面或弱约束区域沿切向漂移。 | `Quadrics.cpp` | 与 standard 对比 `edge_length_cv`、距离误差和 STL 目检。 |
| `--line-weight W` | `SimplifyOptions::lineWeight` | line quadrics 基础权重。过小接近 standard，过大可能压过平面 QEM。 | `Quadrics.cpp` | `min_line_weight`、`max_line_weight` 改变。 |
| `--weight-mode uniform` | `WeightMode::Uniform` | 所有顶点/区域使用相同 line weight。 | `Quadrics.cpp::featureScoreForVertex` | `min_line_weight` 与 `max_line_weight` 接近。 |
| `--weight-mode dihedral` | `WeightMode::Dihedral` | 用二面角特征分数提高硬边附近权重。适合干净 CAD/STL crease。 | `Quadrics.cpp`、`FeatureDetector.cpp` | `max_line_weight` 高于 `min_line_weight`，硬边更稳定。 |
| `--weight-mode normal-tensor` | `WeightMode::NormalTensor` | 用 normal tensor 弱特征分数提高权重。适合二面角不明显的弱特征。 | `NormalTensor.cpp`、`Quadrics.cpp` | `normal_tensor_feature_edges` 或 tensor score 变化。 |
| `--weight-mode height` | `WeightMode::Height` | 按高度归一化分布权重，主要用于调试空间变化权重。 | `Quadrics.cpp` | 高度方向不同区域权重不同。 |
| `--weight-mode xband` | `WeightMode::XBand` | 按 x 方向带状区域分配权重，主要用于调试空间变化权重。 | `Quadrics.cpp` | x 区域权重变化。 |
| `--feature-boost W` | `SimplifyOptions::featureBoost` | 在非 uniform mode 下给特征证据额外加权，属于软成本，不是硬锁。 | `Quadrics.cpp` | `max_line_weight` 增大，特征附近坍缩排序后移。 |
| `--adaptive-scale` | `SimplifyOptions::adaptiveScale=true` | 根据局部网格尺度调整 line weight，使权重不完全依赖全局固定数值。 | `Quadrics.cpp` | 不同网格尺度下 `min/max_line_weight` 更可比。 |
| `--adaptive-base-line-weight W` | `SimplifyOptions::adaptiveBaseLineWeight` | `--adaptive-scale` 的基础权重。当前不在 help 中主推，但代码会解析。 | `parseSimplifyOptions`、`Quadrics.cpp` | 只有配合 `--adaptive-scale` 才应明显影响应用权重。 |
| `--boundary-weight W` | `SimplifyOptions::boundaryWeight` | 给边界附近添加软约束。它影响排序，不等同于硬保护边界。 | `Quadrics.cpp` | 边界漂移减少，但不一定产生 `boundary_rejected_collapses`。 |

调试建议：先比较 `--method standard` 和 `--method line --line-weight 1e-3`。如果 STL 形状差异不明显，再切换 `--weight-mode dihedral` 或 `normal-tensor`，观察 `max_line_weight` 是否被拉高。

### 特征检测参数

这些参数同时影响 `feature-report` 和 `simplify --preserve-feature-curves`。如果只想验证识别结果，优先用 `(MinGW Ninja) Debug CLI Feature Report`，它不会引入 QEM 坍缩的干扰。

| 参数 | 进入字段 | 数学/工程意义 | 断点 | 输出观察 |
| --- | --- | --- | --- | --- |
| `--feature-angle-deg A` | `featureAngleDeg` | 二面角阈值。相邻三角面法向夹角超过阈值时更容易被认为是 crease。 | `FeatureDetector.cpp` | `dihedral_edges`、`convex_edges`、`concave_edges`。 |
| `--circle-fit-threshold R` | `circleFitRelativeThreshold` | 圆 loop 的相对拟合误差阈值。越小越严格。 | `PrimitiveFit.cpp` | `circle_loops`、`near_circle_loops`、`rmsRadialError`。 |
| `--ellipse-fit-threshold R` | `ellipseFitRelativeThreshold` | 椭圆 loop 的相对拟合误差阈值。 | `PrimitiveFit.cpp` | `ellipse_loops`、`rmsEllipseError`。 |
| `--near-circle-axis-ratio-tolerance R` | `nearCircleAxisRatioTolerance` | 椭圆长短轴比接近 1 时可归为 near-circle 的容差。 | `PrimitiveFit.cpp` | `near_circle_loops` 与 `ellipse_loops` 的分配。 |
| `--min-feature-loop-vertices N` | `minFeatureLoopVertices` | 普通 feature loop 至少需要的顶点数，避免短碎片被当作稳定曲线。 | `FeatureDetector.cpp` | `loops` 数量和 polygonal loop 数。 |
| `--min-circular-feature-loop-vertices N` | `minCircularFeatureLoopVertices` | 简化中圆/近圆 loop 的最低保留顶点预算。 | `FeatureConstraints.cpp` | 过小会圆形变粗糙，过大增加 `feature_rejected_collapses`。 |
| `--normal-tensor-threshold S` | `normalTensorFeatureThreshold` | normal tensor feature score 阈值。越低越容易接受弱特征，也越可能引入噪声。 | `FeatureDetector.cpp::normalTensorEdgeCandidate` | `normal_tensor_edges`、`max_normal_tensor_score`。 |
| `--normal-tensor-edge-alignment A` | `normalTensorMinEdgeAlignment` | 边方向与 tensor crease tangent 的最小对齐要求，范围 `[0,1]`。 | `FeatureDetector.cpp` | alignment 变严格时 `normal_tensor_edges` 减少。 |
| `--normal-tensor-smoothing N` | `normalTensorSmoothingIterations` | tensor 评分前的法向平滑迭代数。 | `NormalTensor.cpp` | 噪声敏感性下降，但弱小特征可能被抹平。 |
| `--normal-tensor-scales N` | `normalTensorScaleCount` | tensor 多尺度数量。 | `NormalTensor.cpp` | 多尺度能提高稳定性，但耗时增加。 |
| `--no-normal-tensor-features` | `useNormalTensorFeatures=false` | 关闭 tensor 弱特征候选，只保留 boundary/dihedral/non-manifold 证据。 | `FeatureDetector.cpp` | `normal_tensor_edges=0`。 |

调试建议：在 `boss_pocket_plate.obj` 上设置 `--feature-angle-deg 179 --weight-mode normal-tensor --normal-tensor-threshold 0.06 --normal-tensor-edge-alignment 0.2`，可把二面角通道基本关掉，专门观察 tensor 弱特征。

### 特征曲线保护参数

| 参数 | 进入字段 | 数学/工程意义 | 断点 | 输出观察 |
| --- | --- | --- | --- | --- |
| `--preserve-feature-curves` | `preserveFeatureCurves=true` | 启用特征检测、曲线 quadric、projection 和硬保护策略。 | `SimplificationRun.cpp`、`FeatureConstraints.cpp` | 额外打印 `feature_loops`、`feature_vertices`。 |
| `--feature-protection-mode none` | `FeatureProtectionMode::None` | 不做硬特征拒绝，但软成本仍可能影响排序。 | `FeatureConstraints.cpp` | `feature_rejected_collapses` 低或为 0。 |
| `--feature-protection-mode circular-only` | `CircularOnly` | 只硬保护圆和近圆 loop。 | `FeatureConstraints.cpp` | `primitive_feature_rejected_collapses` 主要来自圆。 |
| `--feature-protection-mode primitive-curves` | `PrimitiveCurves` | 默认策略。硬保护圆、近圆、椭圆 loop，普通 crease 保持软约束。 | `FeatureConstraints.cpp` | 圆孔/椭圆孔更稳定，质量损伤较小。 |
| `--feature-protection-mode all-feature-edges` | `AllFeatureEdges` | 严格模式，所有检测到的 feature edge 都参与硬保护。 | `FeatureConstraints.cpp` | `generic_feature_rejected_collapses` 增加，可能更难达到目标面数。 |
| `--feature-curve-weight W` | `featureCurveWeight` | 给特征 loop 的切向 line quadric 权重，属于软排序成本。 | `FeatureConstraints.cpp`、`Quadrics.cpp` | `projected_feature_placements` 和曲线漂移变化。 |
| `--max-feature-curve-deviation-ratio R` | `maxFeatureCurveDeviationRatio` | 原始 placement 离特征曲线太远时拒绝，阈值为 `R * bbox_diag`。0 表示关闭。 | `SimplificationRun.cpp` | `curve_budget_rejected_collapses`。 |

调试建议：在 `coaxial_hole_plate.obj` 上比较：

```powershell
--preserve-feature-curves --feature-protection-mode none
--preserve-feature-curves --feature-protection-mode primitive-curves
--preserve-feature-curves --feature-protection-mode all-feature-edges
```

如果 `all-feature-edges` 明显增加拒绝计数但输出三角形质量下降，说明特征硬锁已经从“保护语义”变成“阻碍重采样”。

### 硬合法性过滤参数

| 参数 | 进入字段 | 数学/工程意义 | 断点 | 输出观察 |
| --- | --- | --- | --- | --- |
| `--preserve-boundary` | `preserveBoundary=true` | 拒绝破坏开放边界拓扑的坍缩。 | `CollapseLegality.cpp`、`SimplificationRun.cpp` | `boundary_rejected_collapses`。 |
| `--min-triangle-quality Q` | `minTriangleQuality` | 坍缩后局部三角形质量低于 Q 时拒绝。范围 `[0,1]`。 | `CollapseLegality.cpp` | `quality_rejected_collapses`、`min_triangle_quality`。 |
| `--max-normal-deviation-deg A` | `maxNormalDeviationDeg` | 坍缩后局部面法向偏转超过 A 度时拒绝。 | `CollapseLegality.cpp` | `normal_flip_rejected_collapses`。 |
| `--max-local-error D` | `maxLocalError` | 用绝对距离限制局部坍缩漂移。0 表示关闭。 | `CollapseLegality.cpp` | `error_rejected_collapses`。 |
| `--max-local-error-ratio R` | `maxLocalErrorRatio` | 用 `R * bbox_diag` 限制局部坍缩漂移。0 表示关闭。 | `SimplificationPolicies.cpp`、`CollapseLegality.cpp` | `error_rejected_collapses`。 |
| `--prevent-local-intersections` | `preventLocalIntersections=true` | 用局部三角形相交检测拒绝可能引入自交的坍缩。 | `CollapseLegality.cpp` | `self_intersection_rejected_collapses`。 |
| `--industrial-safe` | 多字段组合 | 保守预设：打开边界保护、自交检查，提高最低质量要求，并设置局部误差比例下限。 | `parseSimplifyOptions` | 多类拒绝计数上升，目标可能更难达到。 |

`--industrial-safe` 当前展开为：

```text
preserveBoundary = true
minTriangleQuality = max(current, 1e-4)
maxNormalDeviationDeg = min(current, 75)
maxLocalErrorRatio = max(current, 0.02)
preventLocalIntersections = true
```

注意 `maxLocalErrorRatio = max(current, 0.02)` 的含义是给未设置局部误差预算的情况补上一个保守上限；如果你手动设置比 `0.02` 更大的值，`industrial-safe` 不会把它降回 `0.02`。

## 输出字段怎么读

### feature-report 输出

`feature-report` 首行摘要字段：

| 字段 | 含义 |
| --- | --- |
| `feature_edges` | 进入 feature graph 的总边数。 |
| `boundary_edges` | 开放边界产生的特征边。 |
| `dihedral_edges` | 二面角阈值产生的特征边。 |
| `normal_tensor_edges` | normal tensor 弱特征产生的边。 |
| `non_manifold_edges` | 非流形结构产生的边。 |
| `convex_edges`、`concave_edges` | 通过符号二面角区分的凸/凹边。 |
| `unknown_signed_edges` | 无法稳定判断凸凹的特征边。 |
| `max_normal_tensor_score` | 当前模型中最大的 tensor 特征分数。 |
| `loops` | 恢复出的特征曲线/环数量。 |
| `circle_loops`、`near_circle_loops`、`ellipse_loops`、`polygonal_loops` | primitive 拟合分类。 |

如果 `--feature-angle-deg` 从 `15` 调到 `45` 后 `dihedral_edges` 下降，这是正常现象；阈值越大，只有更尖锐的折痕会被接受。

### simplify 控制台输出

`simplify` 会打印：

| 字段 | 含义 |
| --- | --- |
| `collapsed` | 实际接受的边坍缩数量。 |
| `rejected` | 当前候选被硬过滤拒绝的总次数。 |
| `feature_rejected` | 特征策略拒绝。 |
| `boundary_rejected` | 边界保护拒绝。 |
| `topology_rejected` | 拓扑合法性拒绝。 |
| `normal_flip_rejected` | 法线偏转/翻面风险拒绝。 |
| `quality_rejected` | 三角形质量拒绝。 |
| `self_intersection_rejected` | 局部自交拒绝。 |
| `curve_budget_rejected` | 特征曲线漂移预算拒绝。 |
| `error_rejected` | 局部误差预算拒绝。 |
| `solver_fallbacks` | placement 线性求解退化到端点/中点候选集。 |
| `termination` | 停止原因。 |
| `line_weight_range` | 实际应用的 line weight 范围。 |

启用 `--preserve-feature-curves` 后还会打印：

| 字段 | 含义 |
| --- | --- |
| `feature_loops` | 简化开始时识别出的 loop 数。 |
| `circular_feature_loops` | 圆/近圆 loop 数。 |
| `feature_vertices` | 被 feature graph 标记的顶点数。 |
| `feature_protection_mode` | 本次使用的硬保护模式。 |
| `normal_tensor_feature_edges` | tensor 弱特征边数。 |
| `primitive_feature_rejected` | primitive loop 保护产生的拒绝。 |
| `generic_feature_rejected` | 普通 feature edge 保护产生的拒绝。 |
| `projected_feature_placements` | placement 被投影回 primitive 曲线的次数。 |

### metrics CSV 字段

`--metrics-csv` 会把网格质量、距离误差和报告计数写到一行。常用字段：

| 字段 | 看什么 |
| --- | --- |
| `faces`、`vertices` | 目标规模是否达成。 |
| `mean_triangle_quality`、`min_triangle_quality` | 三角形质量是否被保护。 |
| `edge_length_cv` | 采样是否均匀。 |
| `mean_orig_to_simp`、`max_orig_to_simp` | 输入到输出的采样距离误差。 |
| `non_manifold_edges` | 输出是否产生非流形风险。 |
| `collapsed_edges`、`rejected_collapses` | 候选接受与拒绝数量。 |
| `feature_loops`、`circular_feature_loops`、`feature_vertices` | 特征识别进入简化的规模。 |
| `normal_tensor_feature_edges` | tensor 通道是否有效。 |
| `feature_protection_mode` | 确认本次硬保护策略。 |
| `*_rejected_collapses` | 定位哪个过滤器限制了简化。 |
| `projected_feature_placements` | 曲线 projection 是否实际发生。 |
| `termination_reason` | 是否达到目标，或因候选耗尽/拒绝过多停止。 |
| `min_line_weight`、`max_line_weight` | line quadrics 实际权重范围。 |

## 三个调试实验

### 实验一：二面角阈值是否改变特征图

使用 `(MinGW Ninja) Debug CLI Feature Report`，网格选择 `tests/data/external/fandisk_2014.stl`。

分别运行：

```text
--feature-angle-deg 15
--feature-angle-deg 25
--feature-angle-deg 45
```

观察：

- `parseFeatureOptions` 中 `options.featureAngleDeg` 是否改变。
- `FeatureDetector.cpp` 中 dihedral candidate 数量是否改变。
- 输出里的 `dihedral_edges` 是否随阈值增大而减少。

这验证的是“特征图输入改变”，还没有进入 QEM。

### 实验二：line quadrics 是否改变候选排序

使用 `(MinGW Ninja) Debug CLI Feature Curves`，网格选择 `tests/data/external/fandisk_2014.stl`。

先运行：

```text
--method standard --ratio 0.25
```

再运行：

```text
--method line --line-weight 1e-3 --weight-mode dihedral --feature-boost 0.08 --ratio 0.25
```

观察：

- `parseSimplifyOptions` 中 `useLineQuadrics`、`lineWeight`、`weightMode`。
- `Quadrics.cpp` 中 `appliedWeight` 是否在特征附近变大。
- metrics CSV 中 `min_line_weight`、`max_line_weight`。
- STL 中硬边附近是否比 standard 更稳定。

这验证的是“候选代价改变”。如果硬过滤参数不变，但输出路径和质量变化，说明排序确实改变了坍缩顺序。

### 实验三：特征保护是软成本还是硬过滤

使用 `tests/data/feature_fixtures/coaxial_hole_plate.obj`。

先运行：

```text
--method line --weight-mode dihedral --feature-angle-deg 25 --ratio 0.25
```

再运行：

```text
--method line --weight-mode dihedral --feature-angle-deg 25 --ratio 0.25 `
--preserve-feature-curves --feature-protection-mode primitive-curves `
--feature-curve-weight 0.08 --max-feature-curve-deviation-ratio 0.05
```

观察：

- `FeatureDetector.cpp` 是否识别圆 loop。
- `FeatureConstraints.cpp` 是否根据 primitive loop 拒绝坍缩。
- `SimplificationRun.cpp` 中 `projectedFeaturePlacements` 是否增加。
- 输出里的 `primitive_feature_rejected`、`curve_budget_rejected` 是否变化。

这验证的是“特征保护同时有软成本、projection 和硬过滤”。如果只看到 `featureCurveWeight` 变化但拒绝计数不变，说明当前变化更多体现在排序而不是硬保护。

## 常见问题

### 断点不命中

确认调试配置使用 Debug exe：

```text
build/${workspaceFolderBasename}/mingw-ninja-debug/bin/manumesh.exe
```

确认 `preLaunchTask` 是 `build: mingw+ninja debug`，并且 `where gdb` 能找到当前 MinGW 的 GDB。

### 参数改了但输出没变

先在 `parseSimplifyOptions` 或 `parseFeatureOptions` 看字段是否变了。如果字段变了但输出没变，说明参数所在层级没有成为当前模型的主导因素。例如：

- `--feature-boost` 需要配合非 `uniform` 的 `--weight-mode` 才容易看到权重范围变化。
- `--max-feature-curve-deviation-ratio` 只有启用 `--preserve-feature-curves` 且存在 feature curve 时才有意义。
- `--no-normal-tensor-features` 对只有强二面角特征的模型影响可能很小。
- `--boundary-weight` 是软成本，想要硬保护开放边界应使用 `--preserve-boundary`。

### 达不到目标面数

看 `termination_reason` 和拒绝计数：

- `feature_rejected_collapses` 高：特征保护过强，可比较 `primitive-curves` 与 `all-feature-edges`。
- `quality_rejected_collapses` 高：`--min-triangle-quality` 太严格。
- `error_rejected_collapses` 高：`--max-local-error*` 太严格。
- `boundary_rejected_collapses` 高：开放边界保护限制了可坍缩区域。
- `self_intersection_rejected_collapses` 高：模型局部几何复杂或自交过滤过保守。

### normal tensor 识别太多噪声

提高：

```text
--normal-tensor-threshold
--normal-tensor-edge-alignment
```

或增加：

```text
--normal-tensor-smoothing
--normal-tensor-scales
```

然后用 `feature-report` 比较 `normal_tensor_edges` 和 `max_normal_tensor_score`。对于干净 CAD/STL，二面角和 primitive loop 通常比 tensor 更直接；tensor 更适合作为弱特征补充。

### CMakeCache 指向旧文件夹

这是构建目录复用了旧仓库路径。用本文开头的方法检查 `CMAKE_HOME_DIRECTORY`，删除对应的当前构建子目录，再重新跑 configure。不要把旧绝对路径写进 `.vscode/tasks.json`。

## 建议记录格式

每次调参建议记录这几项：

```text
mesh:
command:
changed parameter:
expected layer: target / ranking / feature graph / feature protection / legality / diagnostics
breakpoint observation:
feature-report summary:
metrics csv:
stl visual observation:
conclusion:
```

这样能避免把“参数确实生效”和“这次模型刚好看不出差异”混在一起。ManuMesh 的参数调试，本质上是在验证一条受约束边坍缩管线：QEM/line quadrics 负责候选排序，feature graph 负责识别要保护的几何语义，硬过滤器负责把不可接受的坍缩拦下。
