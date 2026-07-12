# ManuMesh SDK 集成指南

ManuMesh 当前提供 C++ API 和 C ABI 两条集成路径。C++ API 适合同编译器、同 ABI 的消费工程；C ABI 适合插件、宿主程序、跨语言绑定或 ABI 边界更严格的场景。C++ API 命名空间已统一为 `manumesh`：核心网格类型位于根命名空间，功能入口使用 `manumesh::simplification` 和 `manumesh::feature` 两个功能命名空间，不再提供旧根命名空间别名。当前二进制、include 路径和 C ABI 名称沿用 ManuMesh SDK 约定。

算法和报告字段的理解见 [`../design/algorithm_essence.md`](../design/algorithm_essence.md)。SDK 集成时尤其要注意：QEM/line quadrics 是候选排序，`SimplifyReport` 的拒绝计数来自硬过滤器，不是普通日志噪声。

## 构建 SDK

MinGW + Ninja 的 Release 构建：

```powershell
$buildDir = "build/mingw-ninja-release"
cmake -S . -B $buildDir -G Ninja `
  -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_C_COMPILER=gcc `
  -DCMAKE_CXX_COMPILER=g++ `
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON `
  -DMANUMESH_GOOGLETEST_PROVIDER=auto `
  -DMANUMESH_BUILD_PERFORMANCE_TESTS=OFF
cmake --build $buildDir --parallel
```

如果要生成本地安装布局：

```powershell
$buildDir = "build/mingw-ninja-release"
cmake -S . -B $buildDir -G Ninja `
  -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_C_COMPILER=gcc `
  -DCMAKE_CXX_COMPILER=g++ `
  -DMANUMESH_GOOGLETEST_PROVIDER=auto `
  -DMANUMESH_ENABLE_INSTALL=ON
cmake --build $buildDir --target sdk-install-local --parallel
```

默认本地 SDK 前缀在构建目录的 `sdk/` 下，可通过 `MANUMESH_LOCAL_SDK_PREFIX` 修改。

## C++ API 最小用法

Eigen-backed 便利入口适合同编译器、同 C++ ABI 的消费工程：

```cpp
#include "algorithms/simplification/QEMSimplifier.h"
#include "core/Mesh.h"

manumesh::simplification::SimplifyOptions options;
options.targetRatio = 0.25;
options.useLineQuadrics = true;
options.lineWeight = 1e-3;
options.preserveFeatureCurves = true;

manumesh::simplification::SimplifyReport report;
manumesh::Mesh output = manumesh::simplification::simplifyMesh(input, options, &report);
```

需要复用配置时使用 `manumesh::simplification::QEMSimplifier` 对象。`SimplifyReport` 中的拒绝计数是“每个当前候选第一次被哪个硬过滤拒绝”的诊断信息，不应当当作互斥之外的总失败次数相加解释。

如果使用 normal-tensor 弱特征，`SimplifyOptions::normalTensorMinPersistentScales`
控制最小多尺度支持数；`SimplifyReport` 会返回
`normalTensorScoredVertices`、`maxNormalTensorPersistentScore`、
`meanNormalTensorLocalScale` 和 `meanNormalTensorPersistence`，用于判断弱特征是否形成稳定支持。

特征识别还会输出 cleanup 和 component-level confidence 诊断。`FeatureAnalysis::components`
记录强/弱证据比例、闭合率、junction/endpoint、tensor persistence、primitive residual 和 confidence；`SimplifyReport` 对应返回
`featureComponents`、`weakFeatureComponents`、`highConfidenceFeatureComponents`、
`graphCleanupBridgedGaps`、`graphCleanupRemovedSpurs`、`graphCleanupMergedJunctions`、
`meanFeatureComponentConfidence` 和 `minFeatureComponentConfidence`。QEM 的 feature-curve soft quadric 会按 component confidence 温和缩放。

如果宿主程序不希望自己的 C++ 交换类型暴露 Eigen，使用 `PlainMesh` 入口：

```cpp
#include "algorithms/simplification/PlainSimplifier.h"

manumesh::PlainMesh input;
manumesh::simplification::SimplifyOptions options;
manumesh::simplification::SimplifyReport report;
manumesh::PlainMesh output =
    manumesh::simplification::simplifyPlainMesh(input, options, &report);
```

`SimplificationTypes.h` 包含 `SimplifyOptions`、`SimplifyReport` 和相关枚举，不依赖 Eigen；`QEMSimplifier.h` 仍包含 Eigen-backed `Mesh`。

## 纹理网格与纹理感知简化（C++ API）

带纹理的网格在 SDK 边界使用逐面逐角 UV：

- Eigen-backed 侧：`manumesh::Vec2`（`Eigen::Vector2d`）、`FaceTexCoords`（每面三个 `Vec2` 加 `valid` 标记）、`Mesh::faceTexCoords`。
- Eigen-free 侧：`PlainVec2 { double u, v; }`、`PlainFaceTexCoords`、`PlainMesh::faceTexCoords`；`Mesh` 与 `PlainMesh` 的双向转换以及 compaction/validation/remap 都会保留逐角 UV。

`faceTexCoords` 为空表示无纹理；非空时必须与 `faces` 对齐，个别条目可以 invalid（例如 OBJ 中未贴图的面）。UV 是“角拥有”而不是“顶点拥有”，因为一个几何顶点可能属于多个 UV chart（纹理接缝）。`Mesh::hasTextureCoordinates()` 在至少一个面带有效逐角坐标时返回 true。IO 层的 `loadObj()` / `loadMesh()` 会读取多边形 OBJ、自动三角化并保留每个三角化后角点的 `vt` 索引。

纹理保护是显式 opt-in 的 `SimplifyOptions` 能力：

```cpp
manumesh::simplification::SimplifyOptions options;
options.preserveTexture = true;    // 默认 false，必须显式打开
options.textureWeight = 1.0;       // 只缩放局部标量排序代价
options.textureSeamTolerance = 1e-8;
options.minTextureAreaRatio = 1e-8;
```

启用后，几何 quadric 仍保持 4×4，placement 求解不变；纹理只作为局部 UV 失真标量加入候选排序，并通过局部 chart 配对、UV 定向和有符号面积检查硬性拒绝会破坏接缝或压扁 UV 三角形的坍缩。`SimplifyReport` 返回 `textureProtectedEdges`（初始即无合法中点纹理坍缩的边数）和 `textureRejectedCollapses`（placement 评估后被纹理检查否决的候选数）。注意两点：`preserveTexture = false` 时几何输出与旧无纹理路径完全一致，UV 仍会传播但没有失真/接缝保证；纹理保护启用时可选的固定拓扑质量精修轮（`qualityRefinementIterations`）会被暂时跳过。该能力当前只在 C++ API 提供，CLI `simplify` 与 C ABI 均未暴露纹理选项。设计细节见 [`../design/texture_aware_qem.md`](../design/texture_aware_qem.md)。

独立特征检测入口：

```cpp
#include "algorithms/feature_detection/FeatureDetector.h"

manumesh::feature::FeatureOptions featureOptions;
manumesh::feature::FeatureDetector detector(featureOptions);
manumesh::feature::FeatureAnalysis features = detector.analyze(input);
```

需要检测光滑表面上的 ridge/valley（例如 fillet 中心线、扫描件平缓折痕）时，可以启用确定性 smooth-curvature 弱证据路径（默认关闭）：

```cpp
featureOptions.useSmoothCurvatureFeatures = true;          // 默认 false
featureOptions.smoothCurvatureFeatureThreshold = 0.015;    // 尺度归一化分数阈值
featureOptions.smoothCurvatureMinEdgeAlignment = 0.55;
featureOptions.smoothCurvatureMinTangentConsistency = 0.65;
featureOptions.smoothCurvatureBaseNeighborhoodRings = 2;   // one-ring 拟合对噪声过敏
featureOptions.smoothCurvatureScaleCount = 3;
featureOptions.smoothCurvatureMinPersistentScales = 2;
featureOptions.smoothCurvatureRobustFitIterations = 2;
```

分数无量纲，网格均匀缩放后不需要重新调阈值。`FeatureAnalysis` 对应返回
`smoothCurvatureFeatureEdges`、`smoothCurvatureScoredVertices`、
`maxSmoothCurvatureFeatureScore`、`maxSmoothCurvaturePersistentScore`、
`meanSmoothCurvatureLocalScale` 和 `meanSmoothCurvaturePersistence`；graph edge 带
`smoothCurvature` 来源标记，component 带 `smoothCurvatureEdges` 和
`meanCurvaturePersistence`。该路径 opt-in 的原因是 CAD/STL 硬边与扫描/自由曲面场景需要不同阈值与验证集；不启用时既有硬特征检测和简化行为完全不变。简化侧的 `SimplifyOptions` 当前没有独立的 smooth-curvature 字段，需要时应先用上述 `FeatureOptions` 预计算带 smooth 特征的分析结果，再通过 `QEMSimplifier::simplify(input, features, &report)` 重载交给简化消费。设计细节见 [`../design/smooth_curvature_feature_detection_2026_07_11.md`](../design/smooth_curvature_feature_detection_2026_07_11.md)。

如果有人工或 CAD 导出的 edge labels，可用
`manumesh::feature::benchmarkFeatureEdges(features, labels, junctionLabels)` 计算 edge precision/recall/F1、junction correctness、loop closure rate 和平均 component confidence。

## C ABI 最小流程

1. `manumesh_context_create()` 创建上下文。
2. `manumesh_mesh_create()` 创建输入和输出 mesh handle。
3. 用 `manumesh_load_mesh()` 或 `manumesh_mesh_set_data()` 填充输入。
4. 调用 `manumesh_simplify_options_init()` 初始化 `ManuMeshSimplifyOptions`。
5. 调用 `manumesh_simplify_mesh()`。
6. 用 `manumesh_mesh_copy_vertices()` / `manumesh_mesh_copy_faces()` 取回数据，或用 `manumesh_save_ascii_stl()` 保存。
7. 销毁 mesh handle 和 context。

所有带 `struct_size` / `abi_version` 的结构体都必须先调用对应初始化函数。当前 `MANUMESH_ABI_VERSION` 为 `1`。同一 ABI 版本内，库接受尾部较短的旧 `struct_size`，只读取调用方结构体中实际存在的字段，缺失的新尾部字段使用库默认值；未初始化结构体或 ABI 版本不匹配仍会返回 `MANUMESH_STATUS_INVALID_ARGUMENT`。
`normal_tensor_min_persistent_scales`、feature graph cleanup 选项、component confidence 报告字段都位于 C ABI 结构体尾部，旧调用方保持默认行为。注意：本轮新增的纹理保护选项（`preserveTexture` 等）和 smooth-curvature 特征选项当前只在 C++ API 提供，C ABI 结构体尚未暴露对应字段；跨 ABI 场景暂时无法使用这两项能力。

## CMake config 与 Eigen

安装 SDK 时如果打开 `-DMANUMESH_INSTALL_CMAKE_CONFIG=ON`，下游可以使用：

```cmake
find_package(ManuMesh CONFIG REQUIRED)
target_link_libraries(my_app PRIVATE ManuMesh::manumesh)
manumesh_copy_runtime_dependencies(my_app)
```

`ManuMeshConfig.cmake` 会先为 SDK 内安装的 vendored Eigen 创建 `Eigen3::Eigen` imported target；如果 SDK 内没有 vendored Eigen，则调用 `find_dependency(Eigen3 3.3 NO_MODULE)`。因此包含 `Mesh.h` 的 C++ 消费方可以从导出的目标获得一致的 Eigen include 契约。

## 运行时文件

Windows + MinGW 使用共享库时，应用目录需要：

- `libmanumesh.dll`
- 当前 MinGW 工具链对应的 `libstdc++-6.dll`、`libgcc_s_*.dll`、`libwinpthread-1.dll` 等运行时 DLL

当前 CMake 会在构建目标时把必要 MinGW 运行时复制到构建目录 `bin`。MinGW 下 `MANUMESH_GOOGLETEST_PROVIDER=auto` 默认从仓库源码为当前编译器构建 GoogleTest，测试程序不再依赖仓库历史预编译的 `libgtest.dll` 和 `libgtest_main.dll`。外部分发时仍要确保运行库 DLL 和 exe 放在同一目录，或由部署系统提供。

## 示例工程

仓库包含两个外部消费示例：

| 示例 | 说明 |
| --- | --- |
| `examples/sdk_consumer/sdk_cpp_simplify.cpp` | 使用 C++ SDK 简化网格，包含一个带逐角 UV 平面网格 + `preserveTexture = true` 的纹理保护用例，并检查输出 `faceTexCoords` 与面对齐。 |
| `examples/sdk_consumer/sdk_c_abi_basic.c` | 使用 C ABI 创建、简化和读取网格。 |

启用安装目标后可运行：

```powershell
$buildDir = "build/mingw-ninja-release"
cmake --build $buildDir --target sdk-consumer-test --parallel
```

## 集成边界

- 公共头只从 `include/` 引入；新代码优先使用 `algorithms/feature_detection` 和 `algorithms/simplification`。
- 不要依赖 `src/simplification/detail/`，这些是私有实现。
- 不要依赖 `src/feature_detection/detail/`，primitive fitting、trace/cycle 恢复等 helper 仍是私有实现。
- 当前 ManuMesh SDK 不承诺通用布尔、offset、修复或去噪能力。
- STL/OBJ 文件读写主要服务当前 CLI 和测试；OBJ 读取支持多边形三角化并保留逐角 `vt`，STL 输出仍是 ASCII STL（不携带 UV）。生产系统如需更多格式，应在宿主侧或未来 IO 模块中扩展。
