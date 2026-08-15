# VS Code + Visual Studio 2019 参数调试教程

本文用于在 Visual Studio 16 2019、MSVC v142 和 VS Code 中调试 ManuMesh 的 CLI、特征分析与网格简化。构建参数统一来自 `CMakePresets.json`，调试入口统一来自 `.vscode/launch.json`。

主要代码入口：

- `.vscode/tasks.json`：配置、构建、测试、演示和格式任务。
- `.vscode/launch.json`：MSVC 原生调试入口和默认 CLI 参数。
- `apps/CliOptionBinding.cpp`：CLI 参数到 `SimplifyOptions`、`FeatureOptions` 的绑定。
- `include/algorithms/feature_detection/FeatureOptions.h`：规范的特征分析配置。
- `include/algorithms/feature_detection/FeatureTypes.h`：`FeatureAnalysis` 与特征图结果类型。
- `include/algorithms/simplification/SimplificationTypes.h`：简化配置和报告。
- `src/feature_detection/`：特征证据、图恢复、primitive 拟合和 Normal Tensor。
- `src/simplification/`：策略拆分、候选队列、约束、合法性和坍缩循环。

## 调试前检查

确认 CMake 能读取 VS2019 preset：

```powershell
cmake --version
cmake --list-presets
cmake --preset vs2019-debug
cmake --build --preset vs2019-debug --parallel
```

主要 Debug 产物：

```text
build/vs2019-debug/bin/Debug/manumesh.exe
build/vs2019-debug/bin/Debug/manumesh_tests.exe
```

如果构建目录曾被其他生成器或工具集使用，只删除报错的精确目录后重新 configure。不要复用已经记录了不同 `CMAKE_GENERATOR` 或 `CMAKE_GENERATOR_TOOLSET` 的缓存。

## VS Code 工作流

调试 CLI 特征分析：

1. 在 Run and Debug 中选择 `VS2019 Debug CLI - Feature Report`。
2. 选择输入网格和特征角度。
3. 在 `parseFeatureOptions`、`FeatureDetector::analyze` 或 `detectFeatureCurves` 设置断点。
4. 对照控制台和 `output/vscode_debug_features.csv` 检查结果。

调试特征保持简化：

1. 选择 `VS2019 Debug CLI - Feature Curves`。
2. 在 `parseSimplifyOptions`、`SimplificationPolicies::fromOptions`、`SimplificationRun::analyzeFeatures` 和 `SimplificationRun::tryCollapse` 设置断点。
3. 观察规范 `FeatureOptions`、预计算 `FeatureAnalysis`、候选代价和拒绝原因。
4. 对照输出网格与 metrics CSV，而不是只看最终面数。

调试单元测试：

1. 选择 `VS2019 Debug Unit Tests - Filter`。
2. 在 `gtestFilter` 中输入完整过滤表达式，例如 `ManuMeshFeatureDetection.*`。
3. DebugUtil 快照调用目前已暂时注释；诊断请直接查看 `FeatureAnalysis`、feature report 和 metrics CSV。

所有 launch 配置都使用 `cppvsdbg`，工作目录固定为仓库根目录，并在 F5 前构建相同的 VS2019 Debug preset。

## tasks.json 关键语义

| 写法 | 含义 | 校验方式 |
| --- | --- | --- |
| `cmake --preset vs2019-debug` | 使用 VS2019、x64、v142 和仓库统一开关配置 Debug。 | `CMakeCache.txt` 中生成器为 `Visual Studio 16 2019`，工具集为 `v142`。 |
| `cmake --build --preset ...` | 使用 build preset 中固定的配置和目标。 | Debug 产物在 `bin/Debug`，Release 产物在 `bin/Release`。 |
| `ctest --preset ...` | 使用 preset 固定配置、标签过滤和失败输出。 | 测试输出显示正确的 `Debug` 或 `Release` 配置。 |
| `dependsOn` | 构建前 configure，测试和演示前 build。 | 首次运行任务也能生成完整构建目录。 |
| `$msCompile` | 将 MSBuild 编译诊断映射到 VS Code Problems。 | 编译错误可以跳转到源码。 |
| 独立 build directory | debug、static、performance、SDK、docs 不共享缓存。 | 切换功能开关时不会污染普通构建。 |

## launch.json 关键语义

| 字段 | 当前值 | 含义 |
| --- | --- | --- |
| `type` | `cppvsdbg` | 使用 Microsoft Visual C++ 原生调试器。 |
| `request` | `launch` | 启动新进程。 |
| `program` | `build/vs2019-debug/bin/Debug/...` | 调试带 PDB 的 Debug 产物。 |
| `cwd` | `${workspaceFolder}` | CLI 的相对输入和输出路径从仓库根目录解析。 |
| `console` | `integratedTerminal` | 控制台输出保留在 VS Code 终端中。 |
| `preLaunchTask` | 对应 VS2019 Debug build task | 断点使用的二进制与刚构建的源码一致。 |

## 推荐断点

| 位置 | 观察内容 |
| --- | --- |
| `apps/CliOptionBinding.cpp::parseSimplifyOptions` | CLI 如何形成简化配置和特征配置 override。 |
| `apps/CliOptionBinding.cpp::parseFeatureOptions` | `feature-report` 如何形成独立 `FeatureOptions`。 |
| `src/feature_detection/FeatureDetector.cpp::validateFeatureOptions` | 角度、阈值、尺度和图恢复参数的范围检查。 |
| `src/feature_detection/FeatureDetector.cpp::detectFeatureCurves` | 完整特征分析编排和输出聚合。 |
| `src/feature_detection/FeatureEvidence.cpp` | boundary、dihedral、non-manifold、Normal Tensor 和 smooth-curvature 证据。 |
| `src/feature_detection/NormalTensor.cpp::computeNormalTensorFeatures` | 张量累积、多尺度和平滑结果。 |
| `src/feature_detection/FeatureGraph.cpp` | 特征边如何形成图。 |
| `src/feature_detection/FeatureLoopRecovery.cpp` | loop、trace 和 junction 恢复。 |
| `src/feature_detection/PrimitiveFit.cpp` | 圆、近圆和椭圆拟合判定。 |
| `src/simplification/SimplificationPolicies.cpp::SimplificationPolicies::fromOptions` | 公开 options 如何拆成 target、feature、legality 策略。 |
| `src/simplification/SimplificationRun.cpp::analyzeFeatures` | 简化器如何生成或复用预计算 `FeatureAnalysis`。 |
| `src/simplification/Quadrics.cpp` | plane/line quadrics 与 feature weight。 |
| `src/simplification/FeatureConstraints.cpp` | 特征曲线软约束和硬拒绝。 |
| `src/simplification/CollapseLegality.cpp` | 拓扑、质量、法线、误差和局部相交检查。 |
| `src/simplification/SimplificationRun.cpp::tryCollapse` | 单个候选的最终接受或拒绝。 |

调试时依次确认：输入 option 是否正确、`FeatureAnalysis` 是否属于当前 indexed geometry、策略是否采用预期值、候选为何被接受或拒绝、报告字段是否对应变化。

## DebugUtil

DebugUtil 的实现和私有头文件仍保留，但当前所有生产调用点和主动启用入口均已暂时注释或移除；普通 Debug、ASan、Release、SDK 和文档 preset 均不会编译或运行它。正式验证使用 CTest、feature report、metrics CSV 和输出网格。

## 参数分层

| 层级 | 代表参数 | 主要作用 |
| --- | --- | --- |
| 目标规模 | `--ratio`、`--target-faces` | 决定坍缩停止目标。 |
| 候选排序 | `--method`、`--line-weight`、`--weight-mode`、`--feature-boost` | 改变 QEM/line quadrics 候选代价。 |
| 特征分析 | `--feature-angle-deg`、Normal Tensor、图恢复和 primitive 参数 | 生成可复用的 `FeatureAnalysis`。 |
| 特征保护 | `--preserve-feature-curves`、`--feature-protection-mode`、曲线预算 | 把分析结果转成 placement 引导或硬拒绝。 |
| 合法性 | boundary、triangle quality、normal deviation、local error、intersection | 阻止几何或拓扑不可接受的坍缩。 |

候选代价与合法性必须分开观察。代价降低只改变排序，不保证候选一定被接受；达到较小误差也不代表特征图和 primitive loop 被完整保留。

## 特征分析参数

| CLI 参数 | 对应含义 | 主要观察量 |
| --- | --- | --- |
| `--feature-angle-deg A` | 二面角强特征阈值。 | `dihedral_edges`、`feature_edges`。 |
| `--loop-trace-angle-deg A` | loop/trace 延续角；`-1` 表示复用特征角。 | `traced_edges`、`untraced_edges`、`loops`。 |
| `--circle-fit-threshold E` | 圆和近圆拟合误差阈值。 | circle/near-circle loop 数量和误差。 |
| `--ellipse-fit-threshold E` | 椭圆拟合误差阈值。 | ellipse loop 数量和误差。 |
| `--feature-graph-gap-ratio R` | 特征图小间隙连接尺度。 | graph 分支、component 和 loop 连通性。 |
| `--feature-graph-max-weak-spur-edges N` | 弱短刺清理预算。 | 弱分支数量与 untraced edge。 |
| `--feature-component-min-confidence C` | component 可信度下限。 | 被保留 component、loop 和 primitive。 |
| `--smooth-curvature-features` | 启用光滑 ridge/valley 证据。 | `smooth_curvature_edges`。 |
| `--no-normal-tensor-features` | 禁用张量弱特征证据。 | `normal_tensor_edges` 应为 0。 |

## Normal Tensor 参数

| CLI 参数 | 语义 | 调试重点 |
| --- | --- | --- |
| `--normal-tensor-threshold S` | 特征分数阈值，降低后会接受更多弱边。 | 噪声边与真实 crease 是否同时增加。 |
| `--normal-tensor-edge-alignment A` | 网格边与估计 crease tangent 的最小对齐度，范围 `[0,1]`。 | alignment 提高后候选是否更集中。 |
| `--normal-tensor-smoothing N` | 多尺度评分前的逐顶点张量场一环平滑迭代数；与 normal filter 的面法向预处理独立。 | 平滑是否抑制噪声，同时保住窄小特征。 |
| `--normal-tensor-scales N` | 参与检测的张量尺度数量。 | `mean_normal_tensor_local_scale` 与运行时间。 |
| `--normal-tensor-min-persistent-scales N` | 弱特征至少需要多少尺度支持。 | persistence 提高后孤立噪声是否下降。 |
| `--weight-mode normal-tensor` | 简化权重直接复用分析阶段的张量顶点权重。 | 特征检测和简化排序是否使用同一分析结果。 |

当前实现对单位面法向外积做面积加权累积，以最大特征向量作为主法向、最小特征值方向作为 crease tangent。调试重点不是单看一个 score，而是同时看局部尺度、持久性、双端点 tangent alignment 和最后进入特征图的边。

Normal Tensor 常见对照：

1. 固定网格和特征角，只改变 `threshold`。
2. 固定 threshold，只把 `min-persistent-scales` 从 1 调到 2 或 3。
3. 对同一网格分别运行默认权重和 `normal-tensor` 权重。
4. 检查 `normal_tensor_edges`、`max_normal_tensor_persistent_score`、`mean_normal_tensor_local_scale`、`mean_normal_tensor_persistence` 与最终曲线偏差。

## 简化与保护参数

| CLI 参数 | 语义 | 主要报告字段 |
| --- | --- | --- |
| `--method standard|line` | plane QEM 或加入 line quadrics。 | 误差、fallback 和 collapse 数量。 |
| `--line-weight W` | line quadrics 基础强度。 | placement 漂移与最终误差。 |
| `--weight-mode ...` | line weight 的空间/特征调制。 | 最小、最大 line weight。 |
| `--feature-boost B` | 特征附近的附加权重。 | 特征偏差与候选排序。 |
| `--preserve-feature-curves` | 启用曲线约束。 | `feature_rejected_collapses`、projected placements。 |
| `--feature-protection-mode ...` | 决定哪些特征进入硬保护。 | 终止原因和特征拒绝计数。 |
| `--feature-curve-weight W` | 曲线引导代价。 | 曲线偏差、candidate cost。 |
| `--max-feature-curve-deviation-ratio R` | 曲线允许的相对偏差。 | curve deviation 拒绝。 |
| `--min-circular-feature-loop-vertices N` | 圆/近圆 loop 的顶点预算。 | primitive loop 保真和提前终止。 |

## 合法性参数

| 参数类别 | 作用 | 观察量 |
| --- | --- | --- |
| boundary preservation | 防止开放边界被破坏。 | `boundary_rejected_collapses`。 |
| minimum triangle quality | 阻止细长或退化三角形。 | `quality_rejected_collapses`。 |
| maximum normal deviation | 限制局部法线翻转或偏转。 | normal rejection。 |
| maximum local error | 限制候选局部几何误差。 | `error_rejected_collapses`。 |
| local intersection prevention | 阻止新三角形与邻域相交。 | intersection rejection。 |

达不到目标面数时，先看 `termination_reason` 和各类拒绝计数，再决定是目标过激、特征预算过高，还是合法性阈值过严。不要只通过放宽所有阈值来追求目标面数。

## 输出字段

`feature-report` 的关键字段：

- `feature_edges`：所有证据合并后的边数。
- `boundary_edges`、`dihedral_edges`、`normal_tensor_edges`、`smooth_curvature_edges`、`non_manifold_edges`：各证据来源。
- `traced_edges`、`untraced_edges`：进入或未进入可恢复曲线的边。
- `loops`、primitive 分类和拟合误差：图恢复后的几何结构。
- `normal_tensor_scored_vertices`、`max_normal_tensor_score`、`max_normal_tensor_persistent_score`、`mean_normal_tensor_local_scale`、`mean_normal_tensor_persistence`：张量证据强度与稳定性。

`simplify` 和 metrics CSV 的关键字段：

- 输入/输出面数、`collapsed_edges`、`termination_reason`。
- 总拒绝数及 boundary、feature、quality、error、intersection 等分类。
- `traced_feature_edges`、`untraced_feature_edges` 和 primitive loop 统计。
- Normal Tensor 分析统计，确认简化报告复用了同一份特征分析。
- 距离误差、特征曲线偏差和 projected placement 统计。

## 三个调试实验

### 二面角阈值是否改变特征图

使用 `VS2019 Debug CLI - Feature Report` 和同一个网格，分别把 `launchFeatureAngle` 设为 `15`、`25`、`45`。记录：

- `dihedral_edges` 和总 `feature_edges`。
- graph component、trace 和 loop 数量。
- primitive 分类是否变化。

若原始边数变化但 graph 与 loop 完全不变，继续在 graph cleanup 和 loop recovery 处检查弱分支是否被合并或清理。

### Normal Tensor persistence 是否抑制噪声

在 launch 参数中临时将 `--normal-tensor-min-persistent-scales` 分别设为 1、2、3，保持 threshold、scale count 和 edge alignment 不变。比较：

- `normal_tensor_edges`。
- `max_normal_tensor_persistent_score` 和 `mean_normal_tensor_persistence`。
- 孤立短边、真实 crease 和最终 loop 的变化。

如果边数下降但真实 crease 也消失，检查局部尺度与双端点 tangent alignment，不应只继续降低 threshold。

### 特征保护是软成本还是硬过滤

对同一网格运行 `VS2019 Debug CLI - Feature Curves`，依次比较：

1. 关闭曲线保护。
2. 开启曲线保护但保持较低 curve weight。
3. 提高 curve weight 或收紧 deviation ratio。

在 `Quadrics.cpp` 看排序代价，在 `FeatureConstraints.cpp` 看硬拒绝，在 `SimplificationRun::recordRejectedCollapse` 看报告归因。软成本应主要改变候选顺序，硬约束会增加明确拒绝计数。

## 常见问题

### 断点不命中

确认程序路径位于 `build/vs2019-debug/bin/Debug`，PDB 与 exe 同时更新，launch 的 `preLaunchTask` 与该目录一致。优化过的 Release 构建不适合作为默认源码调试入口。

### 参数改变但结果不变

先在 `CliOptionBinding.cpp` 确认参数被读取，再检查 canonical `FeatureOptions` 是否传入分析。若简化使用预计算 `FeatureAnalysis`，还要确认其 indexed geometry 指纹与当前 mesh 一致，否则应在验证阶段拒绝复用。

### Normal Tensor 识别过多噪声

按顺序尝试提高 persistence 要求、提高 edge alignment、适量提高 threshold，再考虑 tensor smoothing。记录每一步对真实 crease、孤立边和 loop 恢复的影响，避免同时修改所有参数。

### 达不到目标面数

读取 `termination_reason` 和分类拒绝计数。若 feature rejection 占主导，检查 primitive loop 顶点预算和曲线偏差；若 quality/error/intersection 占主导，检查目标比例是否超出当前网格的合法简化空间。

### 缓存指向旧目录

检查精确构建目录：

```powershell
$buildDir = "build/vs2019-debug"
Get-Content "$buildDir/CMakeCache.txt" | Select-String `
  "CMAKE_HOME_DIRECTORY|CMAKE_GENERATOR:|CMAKE_GENERATOR_TOOLSET"
```

确认变量 `$buildDir` 指向当前仓库内的预期目录后，才删除该目录并重新运行 `cmake --preset vs2019-debug`。

## 建议记录格式

每次参数实验至少保存：

```text
commit / worktree state:
configure preset:
launch or task:
input mesh:
changed parameters:
feature report summary:
normal tensor summary:
simplify report summary:
output files:
conclusion:
```

只记录最终 STL 很难区分“特征识别没有变化”“排序变化但被合法性拒绝”或“分析结果没有被正确复用”。参数、报告和输出网格应作为同一次实验保存。
