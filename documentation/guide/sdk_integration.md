# ManuMesh SDK 集成指南

ManuMesh 当前提供 C++ API 和 C ABI 两条集成路径。C++ API 适合同编译器、同 ABI 的消费工程；pre-1.0 阶段保证源码迁移路径，但不承诺不同 SDK 版本间直接复用旧 C++ 二进制，升级后必须重新编译。C ABI v1 才是插件、宿主程序、跨语言绑定与跨版本场景的稳定二进制边界。C++ API 命名空间已统一为 `manumesh`：核心网格类型位于根命名空间，功能入口使用 `manumesh::simplification`、`manumesh::feature` 和 `manumesh::analysis` 三个功能命名空间，不再提供旧根命名空间别名。当前二进制、include 路径和 C ABI 名称沿用 ManuMesh SDK 约定。

算法和报告字段的理解见 [`../design/algorithm_essence.md`](../design/algorithm_essence.md)。SDK 集成时尤其要注意：QEM/line quadrics 是候选排序，`SimplifyReport` 的拒绝计数来自硬过滤器，不是普通日志噪声。

## 构建 SDK

Visual Studio 16 2019 / MSVC v142 的 Release SDK 构建：

```powershell
cmake --preset vs2019-release-sdk
cmake --build --preset vs2019-release-sdk --parallel
```

如果要生成本地安装布局：

```powershell
cmake --preset vs2019-release-sdk
cmake --build --preset vs2019-release-sdk --target sdk-install-local --parallel
```

默认本地 SDK 前缀在构建目录的 `sdk/` 下，可通过 `MANUMESH_LOCAL_SDK_PREFIX` 修改。
`sdk-consumer-test` 会先用只包含 SDK `bin/` 和 Windows 系统目录的隔离 `PATH` 启动
`manumesh --version`，再编译并运行安装后 consumer，避免开发机工具链目录掩盖缺失 DLL。
ManuMesh 固定使用 DLL CRT：Debug 和 Release 均为 `/MD`，Debug 不使用 `/MDd`。
仓库内示例、测试、安装后 CMake consumer 和 `ManuMesh.props` 都使用同一设置；
`ManuMesh.props` 还会显式校验 Visual Studio 16.0、v142、x64 和 C++14。

## C++ API 最小用法

Eigen-backed 便利入口适合同编译器、同 C++ ABI 的消费工程：

```cpp
#include "algorithms/simplification/QEMSimplifier.h"
#include "core/Mesh.h"

manumesh::simplification::SimplifyConfig config;
config.target = manumesh::simplification::SimplifyTarget::ratio(0.25);
config.cost.lineQuadrics =
    manumesh::simplification::LineQuadricConfig::uniform(1e-3);
config.features.enabled = true;

manumesh::feature::FeatureOptions featureOptions;
featureOptions.featureAngleDeg = 30.0;
featureOptions.normalTensorScaleCount = 3;
featureOptions.normalTensorMinPersistentScales = 2;
config.features.detection = featureOptions;

manumesh::simplification::SimplifyReport report;
manumesh::simplification::QEMSimplifier simplifier;
simplifier.setConfig(config);
manumesh::Mesh output = simplifier.simplify(input, &report);
const manumesh::simplification::SimplifySummary summary = report.summary();
```

新代码使用 `SimplifyConfig` 的 `target`、`cost`、`features`、`quality` 和 `texture` 五组，运行日志由顶层 `verbose` 开关控制。目标必须通过 `SimplifyTarget::faceCount()` 或 `SimplifyTarget::ratio()` 明确选择一种单位；局部误差同样通过 `SimplifyErrorLimit::absolute()` 或 `SimplifyErrorLimit::boundingBoxRatio()` 二选一。`features.detection` 是唯一的特征检测配置来源。`features.enabled` 只控制特征曲线约束、投影和保护；Dihedral 或 NormalTensor 权重模式仍会读取相应检测参数来计算排序代价。有状态调用直接使用 `QEMSimplifier::setConfig()`；`makeSimplifyOptions()` 供仍接收旧 `SimplifyOptions` 的自由函数使用。`SimplifyOptions` 及其扁平字段继续作为 0.x 源码兼容入口。

`SimplifyReport` 保留完整诊断字段；普通日志和 UI 可使用 `report.summary()` 获取输入/输出规模、坍缩数和终止原因。特征分析、拒绝原因、精修和纹理诊断仍按命名分组直接保存在报告中，只有需要调参或排查问题的调用方才需要读取。

`SimplifyReport` 中的拒绝计数是“每个当前候选第一次被哪个硬过滤拒绝”的诊断信息，不应当当作互斥之外的总失败次数相加解释。

如果使用 normal-tensor 弱特征，规范 `FeatureOptions::normalTensorMinPersistentScales`
控制最小多尺度支持数；`SimplifyReport` 会返回
`normalTensorScoredVertices`、`maxNormalTensorPersistentScore`、
`meanNormalTensorLocalScale` 和 `meanNormalTensorPersistence`，用于判断弱特征是否形成稳定支持。这里的 `localScale` 是所选尺度的名义高斯核半径，即局部平均边长乘尺度倍率；它不是顺序平滑多轮扩散后的精确支持域半径。

特征识别还会输出 cleanup 和 component-level confidence 诊断。`FeatureAnalysis::components`
记录强/弱证据比例、闭合率、junction/endpoint、tensor persistence、primitive residual 和 confidence；`SimplifyReport` 对应返回
`featureComponents`、`weakFeatureComponents`、`highConfidenceFeatureComponents`、
`graphCleanupBridgedGaps`、`graphCleanupRemovedSpurs`、`graphCleanupMergedJunctions`、
`meanFeatureComponentConfidence` 和 `minFeatureComponentConfidence`。QEM 的 feature-curve soft quadric 会按 component confidence 温和缩放。

如果宿主程序不希望自己的 C++ 交换类型暴露 Eigen，使用 `PlainMesh` 入口：

```cpp
#include "algorithms/simplification/PlainSimplifier.h"

manumesh::PlainMesh input;
manumesh::simplification::SimplifyConfig config;
manumesh::simplification::SimplifyReport report;
manumesh::PlainMesh output =
    manumesh::simplification::simplifyPlainMesh(
        input,
        manumesh::simplification::makeSimplifyOptions(config),
        &report
    );
```

`SimplificationTypes.h` 包含 `SimplifyConfig`、兼容用的 `SimplifyOptions`、`SimplifyReport` 和相关枚举，不依赖 Eigen；`QEMSimplifier.h` 仍包含 Eigen-backed `Mesh`。

## 网格统计与比较（manumesh::analysis）

通用网格统计已从 simplification 提升为跨算法公共模块 `manumesh::analysis`：

```cpp
#include "algorithms/analysis/MeshAnalysis.h"

manumesh::analysis::MeshStats stats =
    manumesh::analysis::computeMeshStats(output);
manumesh::analysis::DistanceStats distance =
    manumesh::analysis::compareMeshesBySampledDistance(input, output, 800);
```

- `MeshStats` 提供顶点/面/边计数、边界与非流形边、面积、三角形质量、边长统计；`DistanceStats` 是确定性采样的双向距离摘要。
- 旧头 `algorithms/simplification/Metrics.h` 保留类型别名和旧函数符号作为一个迁移周期的兼容包装；新代码请直接 include `algorithms/analysis/MeshAnalysis.h`。
- CSV 拼装函数 `statsHeaderCsv` / `statsRowCsv` 属表现层，主实现已移入 CLI（`apps/CliCsv.h`）；旧库函数只为迁移保留，宿主程序的新代码应自行格式化 `MeshStats`。
- 两个入口对空 mesh 或拓扑破损输入退化为零值字段而不抛异常，且为纯函数、可并发调用（见 `documentation/design/error_handling_policy.md`）。

## 特征 loop 匹配（FeatureComparison.h）

需要比较简化前后圆形特征保留情况时，使用新公共头 `algorithms/feature_detection/FeatureComparison.h`：

```cpp
#include "algorithms/feature_detection/FeatureComparison.h"

manumesh::feature::LoopMatchOptions matchOptions; // 默认阈值 = 原 feature-compare CLI 硬编码值
manumesh::feature::LoopMatchReport loopReport =
    manumesh::feature::matchCircularLoops(
        originalFeatures, simplifiedFeatures, simplifiedMesh, matchOptions);
```

每个原始圆环被分类为 `Matched` / `WeakMatch` / `Missing`（三级中心距/半径/法向阈值，`referenceDiagonal` 用于归一化中心误差）。CLI 的 `feature-compare` 命令现在只是这个 API 的薄封装（load → detect → match → 格式化）。

## 纹理网格与纹理感知简化（C++ 与 C ABI）

带纹理的网格在 SDK 边界使用逐面逐角 UV：

- Eigen-backed 侧：`manumesh::Vec2`（`Eigen::Vector2d`）、`FaceTexCoords`（每面三个 `Vec2` 加 `valid` 标记）、`Mesh::faceTexCoords`。
- Eigen-free 侧：`PlainVec2 { double u, v; }`、`PlainFaceTexCoords`、`PlainMesh::faceTexCoords`；`Mesh` 与 `PlainMesh` 的双向转换以及 compaction/validation/remap 都会保留逐角 UV。

`faceTexCoords` 为空表示无纹理；非空时必须与 `faces` 对齐，个别条目可以 invalid（例如 OBJ 中未贴图的面）。UV 是“角拥有”而不是“顶点拥有”，因为一个几何顶点可能属于多个 UV chart（纹理接缝）。`Mesh::hasTextureCoordinates()` 在至少一个面带有效逐角坐标时返回 true。IO 层的 `loadObj()` / `loadMesh()` 会读取多边形 OBJ：严格凸面保持确定性 fan 三角化，凹面使用主轴投影 ear clipping，并保留每个三角化后角点的 `vt` 索引；重复、退化、自交 polygon 以及同一面混用有/无纹理角会返回错误。

纹理保护是显式 opt-in 的 `SimplifyConfig::texture` 能力：

```cpp
manumesh::simplification::SimplifyConfig config;
config.texture.preserveTexture = true;    // 默认 false，必须显式打开
config.texture.weight = 1.0;              // 只缩放局部标量排序代价
config.texture.seamTolerance = 1e-8;
config.texture.minAreaRatio = 1e-8;
```

启用后，几何 quadric 仍保持 4×4，placement 求解不变；纹理只作为局部 UV 失真标量加入候选排序，并通过局部 chart 配对、UV 定向和有符号面积检查硬性拒绝会破坏接缝或压扁 UV 三角形的坍缩。`SimplifyReport` 返回 `textureProtectedEdges`（初始即无合法中点纹理坍缩的边数）、`textureRejectedCollapses`（placement 评估后被纹理检查否决的候选数）和 `textureApplyFailures`（接受坍缩后无法应用 UV 更新计划的内部一致性诊断）。注意两点：`preserveTexture = false` 时几何输出与旧无纹理路径完全一致，UV 仍会传播但没有失真/接缝保证；纹理保护启用时可选的固定拓扑质量精修轮（`quality.refinementIterations`）会被暂时跳过。C++ API、C ABI 均可显式启用该能力；CLI `simplify` 仍使用默认关闭状态。设计细节见 [`../design/texture_aware_qem.md`](../design/texture_aware_qem.md)。

独立特征检测入口：

```cpp
#include "algorithms/feature_detection/FeatureDetector.h"

manumesh::feature::FeatureOptions featureOptions;
manumesh::feature::FeatureDetector detector(featureOptions);
manumesh::feature::FeatureAnalysis features = detector.analyze(input);
```

依赖和数据流保持为 `Mesh -> FeatureAnalysis -> simplification`。每个 `FeatureAnalysis` 都通过 `source` 绑定生成它的精确 indexed geometry：顶点坐标及顺序、面及其角点索引顺序必须相同；UV 被明确排除在指纹之外。`QEMSimplifier::simplify(input, features, ...)` 以及其他预计算分析消费入口会先验证来源、公开索引和图连接关系，来源不匹配或结构损坏会抛出 `std::invalid_argument`。

`FeatureAnalysis` 保留公开字段以兼容既有调用方。新的只读消费代码可以用窄视图明确依赖边界，避免把局部证据、曲线恢复、面分区和运行诊断绑定成一个隐式契约：

```cpp
#include "algorithms/feature_detection/FeatureAnalysisViews.h"

const auto evidence = manumesh::feature::viewFeatureEvidence(features);
const auto curves = manumesh::feature::viewFeatureCurves(features);
const auto segmentation = manumesh::feature::viewFeatureSegmentation(features);
const auto diagnostics = manumesh::feature::viewFeatureDiagnostics(features);

for (const auto& edge : evidence.graphEdges()) {
    // graphEdges() 包含恢复阶段合成的边；只接受原始证据时检查 edge.synthetic()。
}
for (const auto& loop : curves.loops()) {
    // 消费已恢复曲线，不依赖检测器的诊断计数。
}
```

这些视图不复制数据、不能修改分析结果，也不拥有生命周期；因此必须在对应 `FeatureAnalysis` 存活且不被并发修改期间使用。

简化侧可直接启用同一检测器：

```cpp
manumesh::simplification::SimplifyConfig simplifyConfig;
simplifyConfig.features.enabled = true;
simplifyConfig.features.detection = featureOptions;
```

`SimplifyReport` 会返回 normal-tensor、normal-filter iterations/changed faces/preserved edges/angular change、consolidation bridge/cap，以及既有 winding/cleanup/recovery 诊断。需要让多个流程共享同一分析时，先计算 `FeatureAnalysis`，再调用 `QEMSimplifier::simplify(input, features, &report)`。当 `weightMode=NormalTensor` 时，预计算分析中的 `normalTensorVertexWeights` 是按检测时阈值和尺度解析好的规范证据；验证通过后直接复用，即使当前 options 带有不同的 Normal Tensor 参数也不会重新阈值化。显式传入的分析若未启用 Normal Tensor、因而权重数组为空，该调用会抛出 `std::invalid_argument`；应改用包含该证据的分析，或改走不传预计算分析的普通入口以按当前有效配置计算。

独立计算 Normal Tensor 时，`smoothingIterations` 控制逐顶点张量场的邻域平滑，`normalFilter` 则在构建张量前预处理面法向，二者互相独立。该入口与完整特征检测、`filterFeatureNormals()` 共享 normal-filter 参数校验：

```cpp
manumesh::feature::NormalTensorOptions tensorOptions;
tensorOptions.smoothingIterations = 1;
tensorOptions.scaleCount = 3;
tensorOptions.normalFilter = featureOptions.normalFilter;
auto tensor = manumesh::feature::computeNormalTensorFeatures(input, tensorOptions, 0.04);
```

独立 feature analysis 还可以启用 face partition：

```cpp
featureOptions.normalFilter.enabled = true;
featureOptions.graphConsolidation.enabled = true;
featureOptions.surfacePatches.enabled = true;
featureOptions.surfacePatches.includeWeakEvidence = false; // strong barriers only
auto analysis = manumesh::feature::detectFeatureCurves(input, featureOptions);
// analysis.facePatchIds / patches / patchAdjacencies
```

surface patches 不进入 QEM collapse；它们是验证、后续 region processing 和 benchmark 的输出。非 mesh-edge recovery bridge 不会切开 faces，并计入 `segmentationIgnoredRecoveryEdges`。

如果有人工或 CAD 导出的标签，可用兼容入口
`benchmarkFeatureEdges()` 评估 edge/junction，或用 `benchmarkFeatureAnalysis(mesh, features, labels)` 同时评估 edge、junction、continuation branch pair 和 face-patch adjacency。

## C ABI 网格与算法流程

1. `manumesh_context_create()` 创建上下文。
2. `manumesh_mesh_create()` 创建输入和输出 mesh handle。
3. 用 `manumesh_load_mesh()`、`manumesh_mesh_set_data()` 或带逐角 UV 的 `manumesh_mesh_set_data_with_texcoords()` 填充输入。
4. 调用 `manumesh_simplify_options_init()` 初始化 `ManuMeshSimplifyOptions`。当前头文件会把该调用透明转发到带容量的安全入口。
5. 调用 `manumesh_simplify_mesh()`。
6. 用 `manumesh_mesh_copy_vertices()` / `manumesh_mesh_copy_faces()` 取回数据，或用 `manumesh_save_binary_stl()` / `manumesh_save_obj()` 保存。
7. 销毁 mesh handle 和 context。

所有公开 C 函数和 size-aware 内联别名固定使用 `__cdecl`，公共结构体在头文件中强制使用 pack-8；消费方可以在自己的编译单元启用 `/Gv` 或 `/Zp1`，无需改变 ABI。句柄内部带互斥锁，读写同一个 mesh handle 可以并发调用；`copy` / `append` 以无死锁方式同时锁定两个不同句柄，同句柄自拷贝、自追加和原地简化也受支持。销毁句柄不参与内部同步，调用方必须先等待全部使用该对象的在途调用结束；context 本身仍不是线程安全的，并发调用应传 null context 或为每个线程使用独立 context。

基础几何入口包括：

| 能力 | 入口 |
| --- | --- |
| 批量几何与 UV 输入 | `manumesh_mesh_set_data()`、`manumesh_mesh_set_data_with_texcoords()`；后者在同一事务内复制并校验逐角 UV |
| 深拷贝、逐顶点/逐面读写 | `manumesh_mesh_copy()`、`manumesh_mesh_get_vertex()`、`manumesh_mesh_set_vertex()`、`manumesh_mesh_get_face()`、`manumesh_mesh_set_face()` |
| 变换和包围盒 | `manumesh_mesh_get_bounds()`、`manumesh_mesh_translate()`、`manumesh_mesh_transform()` |
| 压缩和校验 | `manumesh_mesh_compact()`、`manumesh_mesh_validate()`、`manumesh_mesh_remove_degenerate_faces()` |
| 面积、质心、法向和体积 | `manumesh_mesh_copy_face_areas()`、`manumesh_mesh_get_surface_area()`、`manumesh_mesh_get_surface_centroid()`、`manumesh_mesh_copy_face_centroids()`、`manumesh_mesh_copy_face_normals()`、`manumesh_mesh_copy_vertex_normals()`、`manumesh_mesh_get_signed_volume()` |
| 拓扑 | `manumesh_mesh_copy_unique_edges()` 返回端点、入射面数、边界和非流形标志；`manumesh_mesh_get_topology_summary()` 返回边计数、共享边连通分量、闭合性与绕序一致性 |
| UV 与绕序 | `manumesh_mesh_get/set/copy_face_texcoords()`、`manumesh_mesh_has_texture_coordinates()`、`manumesh_mesh_reverse_winding()` |
| 拼接 | `manumesh_mesh_append()`，支持索引偏移、自追加和 UV 对齐 |
| 文件输出 | `manumesh_save_ascii_stl()`、`manumesh_save_binary_stl()`、`manumesh_save_obj()`；OBJ 会保留逐角 UV |

所有 `copy_*` 批量接口都支持容量查询：先传 `capacity == 0`，读取 `*_written`，再分配足够数组重新调用。容量不足时返回 `MANUMESH_STATUS_BUFFER_TOO_SMALL`，不会写入任何元素；即使算法在计算后发现错误，也会先在临时数组中完成结果，调用方缓冲区不会得到半截输出。`*_written` 在成功时是写入数，在容量不足时是所需数。

`manumesh_mesh_transform()` 接收行主序 16 元素齐次矩阵，按 `(x,y,z,1)` 计算并除以 `w`；矩阵、`w` 和结果必须有限。所有修改型入口都先在候选网格上校验，再一次性提交，失败时原句柄保持不变。

逐角 UV 使用 `ManuMeshFaceTexCoords`。没有 UV 的面读取为 `valid == 0` 且坐标全零；设置 `valid == 0` 会存储同样的确定性零值。设置有效 UV 时三个角的分量都必须有限。`manumesh_mesh_set_data_with_texcoords()` 接收与输入面数组同长的可选 UV 数组，并在一次校验/提交中替换全部数据。反转面绕序会同步交换第二、第三角 UV；追加时只要任一输入有 UV，输出就会为所有面建立对齐数组，未纹理化的面标记为 invalid。`manumesh_save_obj()` 会写出每个有效面角的 UV，因此可与 `manumesh_load_mesh()` 完成几何和 UV 往返。

`manumesh_mesh_get_signed_volume()` 只接受严格有效、非空、封闭二流形且共享边绕序相反的网格；外向绕序通常产生正体积，`manumesh_mesh_reverse_winding()` 会翻转其符号。对开放、非流形或绕序冲突网格，应先使用 `manumesh_mesh_get_topology_summary()` 获取诊断，而不是把三角形求和当作物理体积。

`ManuMeshSimplifyOptions` 是输入结构体，调用前必须初始化；同一 `MANUMESH_ABI_VERSION` 内，库接受尾部较短的旧 `struct_size`，只读取实际存在的字段，缺失尾字段使用库默认值，未初始化或 ABI 版本不匹配会返回 `MANUMESH_STATUS_INVALID_ARGUMENT`。`ManuMeshSimplifyReport` 和 `ManuMeshMeshStats` 是纯输出结构体：当前头文件会把普通调用转到显式容量入口，输出内存无需预初始化，库按调用方容量有界清零、写入 ABI 头和完整存在的字段。

规范的 C ABI 特征配置通过调用期借用指针传入：

```c
ManuMeshFeatureOptions feature_options;
manumesh_feature_options_init(&feature_options);
feature_options.feature_angle_deg = 30.0;
feature_options.normal_tensor_scale_count = 3;
feature_options.normal_tensor_min_persistent_scales = 2;

ManuMeshSimplifyOptions options;
manumesh_simplify_options_init(&options);
options.preserve_feature_curves = 1;
options.feature_options = &feature_options;
options.preserve_texture = 1;
options.texture_weight = 1.0;
options.texture_seam_tolerance = 1e-8;
options.min_texture_area_ratio = 1e-8;

ManuMeshSimplifyReport report;
ManuMeshStatus status = manumesh_simplify_mesh(
    context, input, &options, output, &report);
```

`feature_options` 只需在这次简化调用返回前保持有效，库不会保存或释放该指针。非 NULL 时，它覆盖 `ManuMeshSimplifyOptions` 中全部旧的扁平特征检测字段；`preserve_feature_curves`、`feature_curve_weight`、`feature_protection_mode`、`min_circular_feature_loop_vertices` 等简化专属字段仍独立生效。外层 `ManuMeshSimplifyOptions` 与嵌套 `ManuMeshFeatureOptions` 都按各自 `struct_size` 独立做 size-aware 读取，因此嵌套旧结构缺失的尾字段同样使用库默认值。

新代码可以继续写 `manumesh_simplify_options_init(&options)`、`manumesh_simplify_mesh(..., &report)` 和 `manumesh_compute_mesh_stats(..., &stats)`。公开头通过名称 alias 把直接调用、全局限定调用和函数地址都转到 current size-aware inline wrapper。绑定层或 FFI 可以直接调用 `*_init_with_size(pointer, capacity)`、`manumesh_simplify_mesh_with_report_size(..., report, report_capacity)` 和 `manumesh_compute_mesh_stats_with_size(..., stats, stats_capacity)`：非空输出容量小于 `abi_version` 字段末尾时返回 `MANUMESH_STATUS_INVALID_ARGUMENT` 且不写 output；容量大于当前结构体时只写库已知尺寸，未知尾部保持不变。simplify 的 report 可为 null，mesh stats 的 stats 不可为 null。report/stats 初始化函数仍可用于在调用前取得独立的零值结构体，但不再是 current output 调用的前置条件。

旧的无容量 init 符号以及旧 `manumesh_simplify_mesh` / `manumesh_compute_mesh_stats` 符号仍导出。它们不读取 report/stats 的原有字节，并始终只写首次发布的 v1 历史尺寸，因而兼容首发时允许未初始化 output 的调用方式，也不会覆盖旧调用方较小的栈对象。这个选择无法保留所有后加尾字段的语义：已经编译且依赖 `loop_trace_angle_deg`、cleanup、quality refinement 或新增 report 尾字段的中间版本客户端，换用新 DLL 后必须重新编译或迁移到 size-aware 入口，否则这些字段会按首发容量被忽略。新源码确实需要直接访问旧符号时，可在包含 `api/CApi.h` 前定义 `MANUMESH_DISABLE_SIZE_AWARE_ALIASES`；旧名称 `MANUMESH_DISABLE_SIZE_AWARE_INIT_MACROS` 仍作为兼容开关。库的 C API object target 使用 `MANUMESH_C_API_IMPLEMENTATION` 禁用 alias。

normal-tensor、cleanup、feature normal filter、graph consolidation 和对应 report 字段都位于 C ABI 结构体尾部；纹理选项追加在 `feature_options` 指针之后，纹理报告追加在 `quality_refinement_skipped_for_texture` 之后。较早、较短的同版本调用方继续使用库默认行为（纹理保护关闭）。surface patch 结果是可变长 C++ 容器，当前没有加入 v1 C ABI。启用纹理保护时，可从 `ManuMeshSimplifyReport` 读取 `texture_rejected_collapses`、`texture_protected_edges` 和 `texture_apply_failures`。

错误路径本轮加固：异常映射新增 `std::bad_alloc` → `MANUMESH_STATUS_OUT_OF_MEMORY`（内存耗尽不再归入通用错误码）；网格索引/几何不满足操作契约时返回 `MANUMESH_STATUS_INVALID_MESH`；未知文件扩展名返回 `MANUMESH_STATUS_UNSUPPORTED_FORMAT`；数值参数会做 finite 校验，例如 `merge_relative_epsilon` 必须有限且非负，否则返回 `MANUMESH_STATUS_INVALID_ARGUMENT`。C 客户端应把 OOM、网格错误、格式错误与参数错误作为可区分的状态处理。

## C ABI 特征边识别

如果你的程序已有两个扁平数组：

```cpp
std::vector<double> vertexData = {
    x0, y0, z0,
    x1, y1, z1,
    // ...
};
std::vector<int> faceData = {
    i0, j0, k0,
    i1, j1, k1,
    // ...
};
```

C ABI 不接收 `std::vector` 本身。先把每三个 `double` 转成一个
`ManuMeshVec3`，每三个索引转成一个 `ManuMeshFace`：

```cpp
if (vertexData.size() % 3 != 0 || faceData.size() % 3 != 0) {
    throw std::runtime_error("vertexData/faceData must contain triples");
}

std::vector<ManuMeshVec3> vertices(vertexData.size() / 3);
for (std::size_t i = 0; i < vertices.size(); ++i) {
    vertices[i].x = vertexData[3 * i + 0];
    vertices[i].y = vertexData[3 * i + 1];
    vertices[i].z = vertexData[3 * i + 2];
}

std::vector<ManuMeshFace> faces(faceData.size() / 3);
for (std::size_t i = 0; i < faces.size(); ++i) {
    faces[i].v[0] = faceData[3 * i + 0];
    faces[i].v[1] = faceData[3 * i + 1];
    faces[i].v[2] = faceData[3 * i + 2];
}

ManuMeshStatus status = manumesh_mesh_set_data(
    context, mesh,
    vertices.empty() ? nullptr : vertices.data(), vertices.size(),
    faces.empty() ? nullptr : faces.data(), faces.size());
```

面索引从零开始，且每个面必须是三角形；所有索引必须小于顶点数量。
`manumesh_mesh_set_data` 会复制数组，函数返回后可以修改或释放原始 `vector`。

然后初始化特征选项并采用“两次调用”取得边：

```cpp
ManuMeshFeatureOptions featureOptions;
manumesh_feature_options_init(&featureOptions);
featureOptions.feature_angle_deg = 25.0;
featureOptions.use_normal_tensor_features = 0;

std::size_t edgeCount = 0;
status = manumesh_detect_feature_edges_v2(
    context, mesh, &featureOptions, nullptr, 0, &edgeCount);
// 第一次调用通常返回 MANUMESH_STATUS_BUFFER_TOO_SMALL，edgeCount 仍然有效。
if (status != MANUMESH_STATUS_OK &&
    status != MANUMESH_STATUS_BUFFER_TOO_SMALL) {
    throw std::runtime_error(manumesh_context_last_error(context));
}

std::vector<ManuMeshFeatureEdgeV2> featureEdges(edgeCount);
std::size_t written = 0;
status = manumesh_detect_feature_edges_v2(
    context, mesh, &featureOptions,
    featureEdges.empty() ? nullptr : featureEdges.data(), featureEdges.size(), &written);
if (status != MANUMESH_STATUS_OK) {
    throw std::runtime_error(manumesh_context_last_error(context));
}
featureEdges.resize(written);
```

每个 `ManuMeshFeatureEdgeV2` 的 `a`、`b` 是升序输入顶点索引。
`feature_edge_index` 是当前 v2 结果按端点和来源确定性排序后的特征边序号；
`input_edge_index` 是输入网格唯一边按 `(a,b)` 字典序排列后的稳定序号。
`boundary`、
`dihedral`、`normal_tensor`、`non_manifold` 表示证据来源，
可能同时为 1；`signed_kind` 大于 0 表示凸，小于 0 表示凹，0 表示无可靠符号。
`cleanup_bridge` 和 `consolidation_bridge` 是图恢复阶段补出的桥接边。
非拓扑桥返回 `input_edge_index == MANUMESH_INVALID_EDGE_INDEX`、`synthetic != 0`
和 `geometric_constraint == 0`，它只表达图连续性，不能直接作为 QEM 投影线段。
接口只返回当前“活动特征图边”；
`removed_by_cleanup` 为 1 的候选边已被过滤，因此当前返回元素的该字段总为 0。
旧的 `manumesh_detect_feature_edges`/`ManuMeshFeatureEdge` 保留 ABI-v1 兼容，
但只提供端点和来源标志，不提供稳定输入边编号。

如果调用方只需要与输入面数组相同风格的扁平索引，可以转换为
`[a0,b0,a1,b1,...]`：

```cpp
std::vector<int> featureEdgeIndices;
featureEdgeIndices.reserve(featureEdges.size() * 2);
for (const ManuMeshFeatureEdgeV2& edge : featureEdges) {
    featureEdgeIndices.push_back(edge.a);
    featureEdgeIndices.push_back(edge.b);
}
```

输出数组由调用方管理。若容量不足，函数返回
`MANUMESH_STATUS_BUFFER_TOO_SMALL`，并通过 `edges_written` 返回完整所需数量；
其他参数或算法错误会把有效的 `edges_written` 置为 0。
这套入口使用纯 C 数据结构和 C 链接名，调用方可以是 C、C++14 或其他 FFI；
不要求消费工程启用 C++。库内部以 C++14 编译，这是库的实现要求，与消费者
语言标准相互独立。完整示例见 `examples/sdk_consumer/sdk_cpp_c_api_feature_edges.cpp`。
查询数量和复制数据会各执行一次完整特征检测；若同一个大网格频繁调用，应在应用层
缓存最终边数组，而不是反复查询。

CMake 工程应链接 C ABI 专用目标，它不会传播 C++ 编译特性或 Eigen 使用要求：

```cmake
find_package(ManuMesh CONFIG REQUIRED COMPONENTS CApi)
target_compile_features(my_cpp14_app PRIVATE cxx_std_14)
target_link_libraries(my_cpp14_app PRIVATE ManuMesh::c_api)
manumesh_configure_msvc_runtime(my_cpp14_app)
manumesh_copy_runtime_dependencies(my_cpp14_app)
```

只请求 `CApi` 组件时，包配置不会查找 Eigen；该目标只传播 `CApi.h` 所需的
include、DLL import 定义和 ManuMesh 二进制库。

只有直接包含 `Mesh.h`、`FeatureDetector.h` 等 C++ API 头文件时，才应链接
`ManuMesh::manumesh` 并以 C++14 编译。

## CMake config 与 Eigen

安装 SDK 时如果打开 `-DMANUMESH_INSTALL_CMAKE_CONFIG=ON`，下游可以使用：

```cmake
find_package(ManuMesh CONFIG REQUIRED)
target_link_libraries(my_app PRIVATE ManuMesh::manumesh)
manumesh_configure_msvc_runtime(my_app)
manumesh_copy_runtime_dependencies(my_app)
```

无组件调用保持原有 C++ SDK 行为，也可以显式请求 `COMPONENTS CXX`。
这两种方式都会先为 SDK 内安装的 vendored Eigen 创建 `Eigen3::Eigen` imported
target；如果 SDK 内没有 vendored Eigen，则调用
`find_dependency(Eigen3 3.3 NO_MODULE)`。因此包含 `Mesh.h` 的 C++ 消费方可以从
导出的目标获得一致的 Eigen include 契约和 C++14 编译要求。

## 运行时文件

共享库消费程序需要 `manumesh.dll`，并需要 Visual Studio 2019 v142 对应的
VC/UCRT 运行时。安装型 SDK 会把 ManuMesh DLL 和可部署的 MSVC 运行时放入 SDK
`bin/`；安装后的 CMake helper 与 MSVC props 会把该目录中的运行时文件复制到消费程序
旁。静态 ManuMesh 库不需要 `manumesh.dll`，但消费程序仍必须使用兼容的 v142 C/C++
运行时设置。CMake consumer 应对每个可执行文件或动态库调用
`manumesh_configure_msvc_runtime(target)`；ManuMesh 的全部配置统一使用 `/MD`，不要改成
`/MDd`、`/MT` 或 `/MTd`，
也不要混用不同工具集、架构或 Debug/Release 二进制。

Windows CLI、C++ `std::string` 路径参数和 C ABI `const char*` 路径参数都使用 UTF-8。
消费方不要把本地 ANSI code page 字节串传给路径接口。

## 示例工程

仓库包含两个外部消费示例：

| 示例 | 说明 |
| --- | --- |
| `examples/sdk_consumer/sdk_cpp_simplify.cpp` | 使用 C++ SDK 简化网格，包含一个带逐角 UV 平面网格 + `preserveTexture = true` 的纹理保护用例，并检查输出 `faceTexCoords` 与面对齐。 |
| `examples/sdk_consumer/sdk_c_abi_basic.c` | 使用 C ABI 创建、简化和读取网格。 |
| `examples/sdk_consumer/sdk_cpp_c_api_feature_edges.cpp` | C++14 调用 C ABI，将扁平点/面数组转换为网格并输出每条特征边及证据标志。 |

启用安装目标后可运行：

```powershell
cmake --preset vs2019-release-sdk
cmake --build --preset vs2019-release-sdk --target sdk-consumer-test --parallel
ctest --preset vs2019-release-sdk
```

## 集成边界

- 公共头只从 `include/` 引入；新代码优先使用 `algorithms/feature_detection`、`algorithms/simplification` 和 `algorithms/analysis`。公共工具头还包括 `core/Tolerances.h`（统一退化三角形容差族）与 `core/MathConstants.h`（`kPi`）；`core/MeshGenerators.h` 提供闭流形共享顶点立方体 `generateClosedCubeGrid(n, size)` 等测试/演示网格生成器。
- 不要依赖 `src/simplification/detail/`，这些是私有实现。
- 不要依赖 `src/feature_detection/detail/`，primitive fitting、trace/cycle 恢复等 helper 仍是私有实现。
- 内部实现命名空间已由 `manumesh::detail` 改名为 `manumesh::common`，旧过渡别名已经移除；这是内部层调整，不影响任何公共 API。
- 当前 ManuMesh SDK 不承诺通用布尔、offset、修复或去噪能力。
- STL/OBJ 文件读写主要服务当前 CLI 和测试；OBJ 读取支持凸面 fan、凹面 ear clipping、逐角 `vt`，并拒绝重复/退化/自交 polygon；CLI 默认输出标准 little-endian 二进制 STL（不携带 UV），SDK 同时保留 `saveAsciiStl()` / `manumesh_save_ascii_stl()` 兼容接口。STL/OBJ 解析器使用固定 C locale 的 `_strtod_l`、有界整数解析与缓冲扫描，内部自动探测 ASCII/二进制 STL 并预读三角形数，解析失败的错误信息更明确。生产系统如需更多格式，应在宿主侧或未来 IO 模块中扩展。
