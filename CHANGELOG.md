# 更新日志

## 2026-07-06

### 变更

- 按 CMake 3.18 兼容语义重做测试注册：GoogleTest 用例由
  `gtest_add_tests` 从源码静态注册，不再依赖 `gtest_discover_tests`
  在构建或 CTest 枚举阶段启动测试可执行文件。
- 将 CMake 自定义测试目标、SDK consumer 测试和 VS Code 测试任务统一为
  `cmake -E chdir <builddir> ctest ...`，避免使用较新 CTest 才支持的
  `--test-dir`。
- 将 SDK consumer 清理步骤改为 `cmake -E remove_directory`，避免使用
  CMake 3.18 不支持的 `cmake -E rm -rf`。
- 固定 CMake Tools 默认 MinGW Release 构建目录，并默认关闭性能测试；
  性能测试继续通过独立的 performance 构建目录和 VS Code 任务运行。
- MinGW 下 `MANUMESH_GOOGLETEST_PROVIDER=auto` 不再优先使用预编译
  GoogleTest DLL，改为跳过该 DLL 包并为当前编译器构建 GoogleTest，
  避免 gcc 运行时和预编译 `libgtest*.dll` ABI 不匹配导致测试 exe
  启动时报 `0xc0000139`。
- `validate-features` 复制外部输入前会先删除旧目标文件，保证 VS Code
  验证任务可以重复运行。

### 已验证

- `git pull --ff-only`
- `.vscode/tasks.json`、`.vscode/launch.json`、`.vscode/settings.json`
  JSON 解析通过，69 个 task、8 个 launch 配置和 6 个输入项的引用链完整。
- `cmake -S . -B build/mingw-ninja-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DMANUMESH_GOOGLETEST_PROVIDER=auto -DMANUMESH_BUILD_PERFORMANCE_TESTS=OFF`
- `cmake --build build/mingw-ninja-release --parallel`
- `cmake -E chdir build/mingw-ninja-release ctest -N`
- `cmake -E chdir build/mingw-ninja-release ctest -LE performance --output-on-failure`
- `cmake -S . -B build/mingw-ninja-release-performance -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DMANUMESH_GOOGLETEST_PROVIDER=auto -DMANUMESH_BUILD_PERFORMANCE_TESTS=ON`
- `cmake --build build/mingw-ninja-release-performance --target performance-tests --parallel`
- `cmake -S . -B build/mingw-ninja-release-sdk -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DMANUMESH_GOOGLETEST_PROVIDER=auto -DMANUMESH_BUILD_PERFORMANCE_TESTS=OFF -DMANUMESH_ENABLE_INSTALL=ON -DMANUMESH_INSTALL_CMAKE_CONFIG=ON`
- `cmake --build build/mingw-ninja-release-sdk --target sdk-consumer-test --parallel`
- `cmake --build build/mingw-ninja-release --target docs-api --parallel`
- `.\build\mingw-ninja-release\bin\manumesh.exe validate-features --ratio 0.20 --samples 64 --output-dir tests/output/feature_curve_validation`

## 2026-07-05

### 新增

- 增加 `FeatureProtectionMode`，用于特征曲线简化策略：
  `none`、`circular-only`、`primitive-curves` 和 `all-feature-edges`。
  默认 `primitive-curves` 只硬保护圆、近圆和椭圆原语；普通多边形折线和二面角锐边继续作为软性的 line-quadric 代价，并由拓扑、法向、质量和局部误差过滤器约束。
- CLI 增加 `--feature-protection-mode`，C ABI 增加
  严格保护所有特征边请使用正式的 `feature_protection_mode = all-feature-edges`。
- C++ 和 C 报告中增加原语/普通特征拒绝计数，便于验证新策略是否减少普通特征的硬锁定。
- 测试辅助代码增加共享的简化报告计数不变量，每个 `simplifyWithReport`
  fixture 都会检查拒绝总数以及原语/普通特征拒绝子计数。
- 增加 `docs/design/feature_protection_roadmap.md`，记录外部模型探测结果，以及已实现的 CGAL/OpenMesh 风格策略拆分：原语曲线硬保护加普通锐边软约束。
- 在 `docs/generated/notes/` 下的生成 HTML 说明中加入共享的
  “Feature Protection Roadmap” 部分，使浏览器可读文档和 Markdown 设计文档保持同一算法方向。

### 变更

- 调整 `solverFallbacks` / `solver_fallbacks` 计数，只统计简化循环中实际处理的当前折叠候选。惰性队列插入仍会为排序计算临时位置，但不再抬高公开诊断计数。
- 将特征检测中的圆形顶点簇 fallback 限制为 32768 次确定性的三点圆扫描，避免破碎 CAD/STL 特征图耗时失控，同时保留已有图环和原语拟合路径。
- 扩展特征检测 API 与算法注释，说明 CAD/STL 图路径、张量弱特征路径和有界圆形修复 fallback 的适用范围与失败模式。
- 扩展简化 SDK 和 C ABI 注释，在 API 边界说明目标选择、line-quadric 排序代价、特征检测阈值、硬合法性过滤器、特征保护策略和拒绝计数。
- 重做特征曲线折叠策略：默认保护模式下，多边形/普通锐边顶点不再自动拒绝；严格保护模式仍可通过 `all-feature-edges` 使用。
- `validate-features` 默认改用完成态外部 STL fixture：Thingi10K spindle、NASA antenna azimuth track、Thingi10K mini pulley 和 OpenFOAM flange。旧的程序生成轴/联轴器/滑轮验证路径不再作为默认工业特征测试。
- 更新特征验证文档和生成 HTML 结果，报告新的原语/普通策略拆分，以及 `primitive-curves` 在破碎工业 STL 特征图上减少普通硬锁定的前后探测结果。
- 刷新文档，使用户命令、生成 HTML 说明和算法解释跟随当前 C++ 实现，而不是旧实验路径。
- 记录 Windows MinGW/Ninja 配置要求：需要同时指定
  `-DCMAKE_C_COMPILER=gcc` 和 `-DCMAKE_CXX_COMPILER=g++`；只指定 C++ 编译器可能让 CMake 混用 `cl.exe` 和 MinGW，并在构建前失败。
- 更新特征曲线文档和实践结果 HTML，说明当前 line/curve 验证输出、圆/椭圆/多边形特征策略、投影计数、曲线预算拒绝，以及曲线保护改进几何但不一定改进所有匹配数量的情况。
- 修正生成算法说明，使其使用当前 `collapseRejectReason(...)` 合法性路径，包括 link-condition 拓扑、三角形质量、法向偏差、局部误差和可选局部自相交保护。
- 澄清来源边界：Garland-Heckbert QEM、Liu/Rahimzadeh/Zordan line quadrics、Tsuchie-Higashi normal tensor feature lines 是来源思想；文档现在明确区分这些思想和仓库当前实现。
- 澄清工业验证文档和 SDK 头文件中 `solver_fallbacks` 的含义：它是执行期位置 fallback 指标，不是队列预排序统计。

### 已验证

- 删除 `build/` 后，使用
  `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DMANUMESH_ENABLE_INSTALL=ON -DMANUMESH_GOOGLETEST_PROVIDER=prebuilt -DMANUMESH_EIGEN_PROVIDER=vendored`
  配置。
- `cmake --build build --parallel`
- `cmake -E chdir build ctest -C Release --output-on-failure`
- `.\build\bin\manumesh.exe validate-features --ratio 0.20 --samples 1000 --input-dir tests\output\generated_inputs --output-dir tests\output\feature_curve_validation`
- `tests/output/feature_policy_validation/` 下的策略验证：
  `primitive-curves` 在 `nasa_mars2020_wheel` 上以 9066 个面达到目标，特征拒绝 31 次、普通特征拒绝 0 次；`all-feature-edges` 停在 10974 个面，特征拒绝 468702 次。
  在 `thingi10k_37880_functional_differential_gear_system` 上，
  `primitive-curves` 以 1236 个面达到目标且特征拒绝 0 次，而
  `all-feature-edges` 停在 2662 个面，特征拒绝 68993 次。
  在 `fandisk_2014` 上两者均达到目标，但普通特征拒绝从 513 降到 0。
- 对 `nasa_cubesat_middle`、`nasa_mars2020_wheel`、`casting_aimshape_2014`、
  `fandisk_2014`、`thingi10k_37880_functional_differential_gear_system` 和
  `large/rocker_arm.stl` 做外部探测；输出位于
  `tests/output/new_model_validation/`，用于暴露当前特征策略过度保护风险。
- `cmake -S . -B build/doccheck -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ -DMANUMESH_BUILD_DOCS=OFF`
- `cmake --build build/doccheck --parallel`
- `cmake -E chdir build/doccheck ctest --output-on-failure`
- `.\build\doccheck\bin\manumesh.exe --help`
- `.\build\doccheck\bin\manumesh.exe feature-report tests\data\feature_fixtures\coaxial_hole_plate.obj --feature-angle-deg 25 --circle-fit-threshold 0.04 --ellipse-fit-threshold 0.05 --normal-tensor-threshold 0.06 --normal-tensor-edge-alignment 0.2 --csv output\doccheck\features.csv`
- `.\build\doccheck\bin\manumesh.exe simplify tests\data\feature_fixtures\coaxial_hole_plate.obj output\doccheck\simplified.stl --method line --ratio 0.50 --line-weight 1e-3 --weight-mode dihedral --feature-boost 0.08 --feature-angle-deg 25 --preserve-feature-curves --feature-curve-weight 0.08 --max-feature-curve-deviation-ratio 0.05 --circle-fit-threshold 0.04 --ellipse-fit-threshold 0.05 --min-circular-feature-loop-vertices 12 --samples 128 --metrics-csv output\doccheck\metrics.csv`
- `.\build\doccheck\bin\manumesh.exe validate-features --ratio 0.20 --samples 64 --output-dir output\doccheck\feature_validation`
- `.\build\doccheck\bin\manumesh.exe validate-external --ratio 0.25 --samples 64 --output-dir output\doccheck\external_validation`
- `.\build\doccheck\bin\manumesh.exe demo --quick --samples 64 --output-dir output\doccheck\demo --input-dir output\doccheck\demo_input`

## 2026-07-03

### 新增

- 增加边折叠的局部几何容差保护，包括 `maxLocalError`、
  `maxLocalErrorRatio`，以及超过局部漂移预算时的拒绝计数。
- 增加显式特征图层，用于特征环、共享顶点、交汇点、多特征归属和曲线感知折叠策略。
- 为圆形和椭圆特征环增加逐环特征曲线预算；圆形投影和重采样导向保护替代旧的仅依赖固定 `minFeatureLoopVertices` 的行为。
- 增加多尺度、局部归一化 normal-tensor 特征检测参数和报告，使弱特征可以和仅二面角检测进行比较。
- 增加数据集级验证覆盖：特征召回、曲线漂移、采样距离、拓扑、三角形质量和拒绝计数一致性。
- 增加 VS Code 演示任务，覆盖选定网格、算法预设、比例扫描、特征报告和算法对比。

### 变更

- 简化流程现在组合 QEM 排序、折叠前合法性和容差保护，不再只依赖事后采样距离。
- C API 报告暴露新的拒绝计数，包括曲线预算和局部误差拒绝。
- VS Code 工作流收敛到两条受支持的 Ninja 链路：主路径 `mingw+ninja` 和备用/调试路径 `msvc+ninja`。Ninja 任务移除固定 `--parallel 2` 限制，让 CMake/Ninja 使用可用并行度。
- 文档补充演示可用网格和参数示例，用于算法选择、特征保护、normal-tensor 检测和保守工业安全简化。

### 已验证

- `cmake -E chdir build\mingw-ninja-debug ctest --output-on-failure`
- `cmake --build build\mingw-ninja-release --target manumesh --parallel`
- `manumesh feature-report tests\data\feature_fixtures\coaxial_hole_plate.obj --feature-angle-deg 25 --circle-fit-threshold 0.04 --ellipse-fit-threshold 0.05 --normal-tensor-threshold 0.06 --normal-tensor-edge-alignment 0.2 --csv examples\output\vscode_demo\coaxial_hole_plate\feature_report\features.csv`
- `manumesh simplify tests\data\feature_fixtures\coaxial_hole_plate.obj examples\output\vscode_demo\coaxial_hole_plate\feature-curves_r0_50\simplified.stl --method line --ratio 0.50 --line-weight 1e-3 --weight-mode dihedral --feature-boost 0.08 --feature-angle-deg 25 --preserve-feature-curves --feature-curve-weight 0.08 --max-feature-curve-deviation-ratio 0.05 --circle-fit-threshold 0.04 --ellipse-fit-threshold 0.05 --min-circular-feature-loop-vertices 12 --samples 128 --metrics-csv examples\output\vscode_demo\coaxial_hole_plate\feature-curves_r0_50\metrics.csv`

## 2026-07-01

### 新增

- 增加 `Status`、`Result`、类型化拓扑 handle 和 `MeshTopology`，作为网格内核方向的第一层可复用拓扑缓存。
- 增加 `docs/design/architecture.md`，说明 Polygonica 风格工业网格内核目标和模块边界。
- 增加公开的 Fandisk 与 AIM@SHAPE Casting STL fixture，用于本地复现 Tsuchie 和 Higashi 2014 CAD 模型实验。
- 增加 10 个超过 10k 面的公开大型 STL 验证网格，并加入
  `docs/design/large_model_validation.md` 记录 90% 和 50% line-quadrics 批处理结果。
- 增加 `docs/design/industrial_validation.md`，记录命令级验证覆盖、输出位置、指标和通过标准。
- 在 `thirdParty/googletest` 下加入 vendored GoogleTest，支持离线测试构建。

### 变更

- 调整 CMake 和 VS Code 工作流以适配 CMake 3.18.6 环境：移除 preset 命令，改用显式构建目录和生成器。
- 围绕 C++ 几何内核工作流重构仓库文档：CLI 生成的 STL/CSV 输出、CTest/API smoke 检查和外部 STL/CAD 查看器，替代此前以浏览器预览优先的路径。
- 更新 VS Code launch/tasks 和用户命令示例，以匹配库构建生成的 `bin/manumesh.exe` 运行时布局。
- 扩展工业库说明，补充源码布局边界、验证期望，以及将生成输出视为检查产物而不是源码依赖的指导。
- 刷新 flange、pipe coupling、pulley 和 stepped-shaft 场景的特征曲线验证 STL/CSV 输出。
- 网格 metric 中的边、边界和非流形计算改经 `MeshTopology`，不再在 `Metrics.cpp` 中重复构建临时 edge map。
- 为 QEM 边折叠增加 link-condition 拓扑合法性过滤器，避免闭合二流形输入被简化成意外孔洞或非流形边。
- 将 QEM 折叠验证和更新中的重复全量面扫描替换为增量 incident-face 拓扑，显著改善大型网格简化运行时间。

### 删除

- 移除 Vite/Node 浏览器查看器工作流。
- 移除 `CMakePresets.json`；CMake 3.18.6 不支持 presets。

## 2026-06-30

### 新增

- 增加跨平台 `manumesh` 共享库目标，公共头位于
  `include/manumesh`。
- 增加 Windows DLL export/import 处理，并为共享库构建设置默认符号可见性。
- 增加 install/export 规则，使外部 CMake 工程可以使用
  `find_package(ManuMesh CONFIG REQUIRED)`。
- 增加 `manumesh_copy_runtime_dependencies(target)`，供外部 Windows CMake consumer 将运行时 DLL 复制到可执行文件旁。
- 增加库消费示例程序 `examples/basic_simplify.cpp`。
- 增加 GoogleTest 覆盖和 CTest discovery，用于简化、特征检测和网格指标。
- 增加 clang-format 配置以及 `format`、`check-format` 目标。
- 增加 Doxygen 配置和 `docs-api` 目标。
- 增加 `docs/design/industrial_library.md`，记录集成、安装、运行时和工具说明。

### 变更

- CLI 改为链接新的可复用库目标，不再直接把算法源码编进可执行文件。
- 公共 API 声明移动到可安装头文件，同时保留旧 `src/*.h` 头作为兼容转发头。
- 使用新的 clang-format 配置格式化已有 C++ 源码。

### 已验证

- `cmake --build build\codex-industrial --config Debug --parallel`
- `cmake -E chdir build\codex-industrial ctest -C Debug --output-on-failure`
- `cmake --build build\codex-industrial --config Debug --target check-format`
- `cmake --build build\codex-industrial --config Debug --target docs-api`
- `cmake --install build\codex-industrial --config Debug --prefix build\codex-industrial\stage-copy-helper-3`
