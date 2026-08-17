# ManuMesh 网格内核架构

ManuMesh 是面向增材制造的 C++ 多边形网格几何内核。当前稳定能力集中在 QEM/line quadrics 简化和 CAD/STL 风格特征检测，代码组织目标是让后续 `repair`、`boolean`、`offset`、`remesh` 等模块能并列加入，而不是继续堆到 QEM 实现里。

算法层面的共同语境见 [`algorithm_essence.md`](algorithm_essence.md)。common 内部基础库的补强规划见 [`common_foundation.md`](common_foundation.md)；新增 `repair`、`remeshing` 等平级模块的落地流程见 [`adding_new_algorithm.md`](adding_new_algorithm.md)。本文只描述架构边界：哪些概念可以公开，哪些必须留在私有实现里，哪些模块可以依赖彼此。

## 分层

```text
include/      安装级公共 SDK 头文件
  core/                         Mesh、PlainMesh、MeshTopology、Status、typed handles、Tolerances、MathConstants
  io/                           STL/OBJ 网格读写 API
  algorithms/analysis/          通用网格统计与双 mesh 比较（MeshAnalysis.h）
  algorithms/feature_detection/ 特征检测模块入口、选项和结果类型（含 FeatureComparison.h loop 匹配）
  algorithms/simplification/    QEM/line quadrics 简化入口、选项、报告（Metrics.h 已为弃用转发头）
  algorithms/<domain>/          未来平级算法模块，例如 repair/remeshing
  api/                          C ABI，使用 handle 和显式初始化结构体

src/common/detail/              跨算法私有工具（namespace manumesh::common），不安装
src/mesh_edit/                  可编辑 mesh 状态、动态邻接和 compact/remap
src/mesh_edit/detail/           mesh edit 私有类型和 helper，不安装
src/core/                       公共 core 类型的实现
src/io/                         STL/OBJ 读写实现；OBJ 读取支持多边形三角化并保留逐角 vt
src/analysis/                   通用统计与比较实现（namespace manumesh::analysis）
src/feature_detection/          特征检测实现，只依赖 core 和私有 common
src/feature_detection/detail/   特征检测私有类型、策略接口和 helper
src/simplification/             简化实现，可消费 FeatureAnalysis
src/simplification/detail/      简化专属私有状态、policy 分组和策略
src/<domain>/                   未来平级算法实现，例如 repair
src/<domain>/detail/            未来平级算法私有状态和阶段 helper
apps/              CLI，按外部用户方式调用 SDK
examples/                       C/C++ SDK 使用示例
tests/                          GoogleTest 和 CTest 回归验证
documentation/                           当前设计、指南、论文索引和历史生成资料
```

## 隐私边界

公共头文件只表达稳定 SDK 合约，不承载算法内部状态。当前使用 pimpl 的公共对象包括：

- `MeshTopology`：隐藏拓扑缓存的存储布局。
- `QEMSimplifier`：隐藏单次简化运行状态、队列、QEM 属性和策略对象。
- `FeatureDetector`：隐藏检测器内部配置和后续可能加入的缓存、策略或统计字段。

`SimplifyConfig`、`SimplifyOptions`、`SimplifyReport`、`FeatureOptions`、`FeatureAnalysis` 仍是公开数据交换类型。新代码以按职责分组的 `SimplifyConfig` 为规范入口，目标通过 `SimplifyTarget` 明确选择面数或比例，`SimplifyOptions` 仅作为 0.x 源码兼容适配器保留。`SimplifyReport` 的扁平字段也保留用于兼容和完整诊断；常规调用只需 `SimplifySummary`，不应把每个内部计数都当成主流程输出，也不再为每组计数建立镜像 summary 类型。`FeatureAnalysis` 的 graph branches、components、patches 和 benchmark labels 是公开结果语义；候选 bridge、trace state、compatibility candidate 等运行时细节仍留在私有层。`FeatureAnalysis` 不是可以任意移植到另一份网格的标签包：其 `source` 绑定生成分析时的精确 indexed geometry，包括顶点/面数量、面与角点索引顺序以及按顶点顺序排列的坐标；UV 不参与该身份指纹。C++ API 根命名空间为 `manumesh`，核心网格类型和基础工具位于根命名空间；特征检测位于 `manumesh::feature`，QEM/line-quadrics 简化位于 `manumesh::simplification`。

特征检测内部按九阶段 pipeline 组织：evidence、退化证据过滤、graph、cleanup、component consolidation、loop recovery、component summary、graph finalize、patch segmentation。`FeatureNormalFilter.cpp` 只稳定 evidence normals；`FeatureGraphCompatibility.cpp` 统一 cleanup/consolidation 的方向/source/sign 规则；`FeatureGraph.cpp` 生成 junction branches/continuation pairs；`FeatureBenchmark.cpp` 和 `FeatureSegmentation.cpp` 分别承载扩展 benchmark 与 face partition。`FeatureDetector.cpp` 只负责 public facade、校验和阶段编排。新增识别能力应优先加入职责明确的 translation unit，而不是继续扩张 facade。

简化内部也按 pipeline/strategy 分层：`QEMSimplifier.cpp` 是 public facade；`SimplificationRun.cpp` 负责编排单次运行、队列和状态应用；`SimplificationPolicies.cpp` 接收 `SimplifyConfig` 经 `makeSimplifyOptions()` 生成的显式 `featureOptionsOverride`，或处理旧扁平字段适配，随后归一化成内部 policy；`CollapseAttempt.cpp` 负责把 feature、boundary、curve budget 和 legality filters 组合成一次候选坍缩的接受/拒绝结果；`Quadrics.cpp`、`Placement.cpp`（placement 策略单元，含 Lindstrom-Turk 边界守恒投影）、`FeatureConstraints.cpp`、`CollapseTopology.cpp`（含与 `preserveBoundary` 无关的边界弦 pinch 拒绝）、`CollapseLegality.cpp` 等模块只表达各自策略；`TextureProtection.cpp`（配套 `detail/TextureProtection.h`）承载 opt-in 的纹理感知策略——局部 UV chart 配对、有符号 UV 面积检查和标量失真代价，几何 quadric 保持 4×4，不做属性扩维。新增 collapse 过滤器优先进入对应 policy/evaluator，而不是继续扩张 `SimplificationRun.cpp`。

CLI 是应用层消费者，不承载算法状态。`apps/main.cpp` 只调用 `manumesh::cli::run()`；`CliArguments.cpp` 用共享 `OptionSpec` 选项表驱动帮助生成（`optionsHelpText()`）和逐命令参数校验（`validateArgsForCommand()`，拼错或属于其他命令的选项在入口统一报错）；`ManuMeshCli.cpp` 负责帮助输出和 `run()` 派发；`ManuMeshCommands.cpp` 承载命令 handler 与 command registry；`CliCsv.cpp` 承担 CSV 表现层（含自 simplification 移入的网格统计 CSV 拼装）。新增 CLI 命令应新增 handler 并注册到 registry，公共算法能力仍优先进入 SDK 层。

## 命名空间约定

约定：目录名 = 模块名 = 内部命名空间；公共命名空间允许短于目录名，但必须在下表登记，
新增模块默认不再引入新的错位。

| 目录 / 模块 | 命名空间 | 状态 |
| --- | --- | --- |
| `src/common` | `manumesh::common` | 2026-07 由 `manumesh::detail` 改名；过渡别名已于 2026-08 移除 |
| `src/mesh_edit` | `manumesh::mesh_edit` | 一致 |
| `src/analysis` | `manumesh::analysis` | 一致 |
| `src/feature_detection` | `manumesh::feature` | 接受错位：公共 API 已发布（头、C API、示例全量引用 `manumesh::feature`），改名是破坏性变更；`feature` 作为 API 词面更短更好 |
| `src/simplification` | `manumesh::simplification` | 一致 |
| 公共头前缀 `algorithms/` | 无对应命名空间层级 | 接受：`algorithms/` 只是 SDK 安装目录的分组手段（对齐 pmp 的 `pmp/algorithms/` 前缀），不承载语义层级 |

各模块自己的私有层继续用 `src/<module>/detail/` 目录表达，符号留在模块命名空间内；
"detail" 一词只表示目录私有性，不再作为命名空间使用。

## 公共私有层

`src/common/detail/` 是库内部公共层，负责多个算法都会用到但暂不应进入 SDK 的网格与几何基础设施：

- 无向边 key 和面 key。
- 边到相邻面的局部 incidence。
- 面法向、面心、顶点一环邻接。
- 边界顶点标记。
- 三角形质量、点到三角形距离、AABB 距离、三角形包围盒和三角形相交谓词。
- AABB uniform grid、cell key/hash、overflow 候选管理。

这层解决的是“实现复用”，不是“SDK 暴露”。后续 common 应继续沉淀边界 loop、mesh 校验和内部诊断结构；可变拓扑、活动状态和索引重映射进入 `mesh_edit`，避免 `repair`、`remeshing`、`simplification` 各自复制编辑内核。如果某个能力未来需要外部稳定使用，应优先评估是否提升到 `core/MeshTopology` 或新的公共模块，而不是让外部 include `src/common/detail/...`。

## 可编辑网格基础层

`src/mesh_edit/` 是算法之间共享的内部编辑层。当前提供活动三角面、增量 face incidence、活动边查询、duplicate-face 跟踪，以及返回 vertex/face old-to-new 映射的 Mesh compaction。详细边界见 [`mesh_edit_foundation.md`](mesh_edit_foundation.md)。

它不包含 QEM cost、feature policy 或 remeshing 调度。简化模块只通过 position/activity/face 状态调用 compaction，并在自己的 `CollapseTopology.cpp` 中保留 boundary 和 link-condition 策略。未来 remeshing 应复用同一编辑层，而不是 include `simplification/detail/...`。

## 算法边界

特征检测是与简化平级的算法模块。它只依赖 core 和私有 common，不依赖 QEM。简化模块可以消费 `FeatureAnalysis`，用于 feature quadrics、placement 投影、曲线预算和硬保护策略。

应用级数据依赖明确保持为：

```text
Mesh -> FeatureAnalysis -> simplification
```

第一条边由特征检测建立，第二条边是经过验证的只读消费关系。所有同时接收输入 `Mesh` 与对应预计算分析的公共入口都会先校验 `FeatureAnalysis::source` 与该网格的精确 indexed geometry，并检查公开 graph/loop/component/patch 索引及图连接关系；来源不匹配或结构损坏会以参数错误拒绝，而不是在简化内部猜测或重新解释。

QEM/line quadrics 只负责候选折叠排序和局部几何优化，工业级安全性由显式过滤器补足：拓扑 link condition、边界策略、法线偏转、三角形质量、局部误差和自交检查；输入带 UV 且显式打开 `preserveTexture` 时，还会加入局部 chart 配对和有符号 UV 面积过滤。特征图应先成为独立结果，再由简化、验证、修复或未来重网格模块消费。

这个边界来自当前算法本质：标准 QEM 在平坦区域存在切向零空间，line quadrics 补的是候选排序和 placement 正则；特征图补的是制造语义支撑；硬过滤器补的是拓扑和几何安全。把这三件事合在一个类里会让新增模块无法复用 feature graph，也会让后续 envelope、repair、remesh 等能力只能绕着 QEM 转。

当前依赖方向应保持：

```text
core
  -> common
  -> mesh_edit
  -> analysis / feature_detection
  -> simplification / future remeshing / future repair

core + common/detail 可被 analysis、feature_detection、mesh_edit 和算法模块使用；
mesh_edit 只能依赖 core/common；
analysis（通用网格统计与双 mesh 距离比较）只依赖 core/common，可被
simplification、api 与未来 repair/remeshing 消费；
simplification 可以消费 feature::FeatureAnalysis 与 analysis 统计；
feature_detection 不能反向 include simplification；
未来 repair/remesh/validation 可以消费 feature::FeatureAnalysis。
```

## 数据策略

`Mesh` 仍是轻量交换格式：稠密顶点数组加三角面索引数组。带纹理的输入额外携带 `Mesh::faceTexCoords`（`FaceTexCoords`，每面三个 `Vec2` 逐角 UV 加 `valid` 标记）：为空表示无纹理，非空时与 `Mesh::faces` 对齐，个别条目可以 invalid（例如 OBJ 中未贴图的面）。UV 采用“角拥有”而不是“顶点拥有”，因为一个几何顶点可能属于多个 UV chart（纹理接缝）；`Mesh::hasTextureCoordinates()` 在至少一个面带有效逐角坐标时为 true。`FeatureAnalysis` 的来源身份只覆盖 indexed geometry，因此修改 UV 不会使已有分析失效；修改顶点顺序、坐标、面顺序、面角点顺序或索引则会失效。需要重复邻接查询时，算法应构建 `MeshTopology`、私有 common 查询结果，或运行时动态拓扑，而不是在每个模块里重复扫描并复制一套局部工具。

`Mesh` 是 Eigen-backed 便利类型，适合同编译器、同 C++ ABI 的 SDK 消费方。
pre-1.0 C++ SDK 只保证有明确的源码迁移路径，不承诺跨 SDK 版本直接复用旧二进制；
`SimplifyConfig` 按职责嵌套组织新配置，`SimplifyOptions` 保留为扁平的 0.x 源码兼容适配器；
升级 SDK 后消费工程必须重新编译。
`PlainMesh` 是 Eigen-free C++ 交换类型，提供对应的 `PlainVec2`、`PlainFaceTexCoords` 和
`PlainMesh::faceTexCoords` 字段，与 `Mesh` 的双向转换、compaction/validation/remap 都会保留逐角 UV；`PlainSimplifier.h` 提供
`simplifyPlainMesh()`，内部转换为 `Mesh` 后复用同一套简化实现。真正跨语言或
严格或跨版本 ABI 边界使用 `api/CApi.h`；纹理保护通过 `ManuMeshSimplifyOptions` 的尾部字段和 `ManuMeshSimplifyReport` 的尾部诊断字段提供，并保持较短旧结构前缀兼容。

当前 `mesh_edit` 仍是稳定索引的最小编辑层。未来升级为可编辑半边拓扑时，应使用 `VertexId`、`EdgeId`、`HalfedgeId`、`FaceId` 等 typed handle，配合 generation-aware free list 和显式 compaction。属性不要塞进基础顶点结构，应以类型化数组挂在拓扑旁边，方便重映射、导出和 ABI 隔离。

## API 形态

简化主入口：

```cpp
namespace manumesh {
namespace simplification {
SimplifyOptions makeSimplifyOptions(const SimplifyConfig& config);
Mesh simplifyMesh(const manumesh::Mesh& input,
                  const SimplifyOptions& options,
                  SimplifyReport* report = nullptr);
} // namespace simplification
} // namespace manumesh
```

新代码优先使用 `SimplifyConfig` 的 `target`、`cost`、`features`、`quality` 和
`texture` 五个分组，运行日志由顶层 `verbose` 开关控制。目标使用
`SimplifyTarget::faceCount()` 或 `SimplifyTarget::ratio()` 明确选择一种单位；
line quadrics 使用 `LineQuadricConfig::disabled()`、`uniform(weight)` 或
`adaptive(baseWeight)` 明确选择一种模式；
局部误差使用 `SimplifyErrorLimit::absolute()` 或
`SimplifyErrorLimit::boundingBoxRatio()` 明确选择一种单位；
有状态调用使用 `QEMSimplifier::setConfig()`。
`makeSimplifyOptions()` 只把规范配置转为现有热循环
使用的已归一化选项；`features.detection` 是唯一的特征检测配置来源，不会与旧扁平字段
隐式合并。旧 `SimplifyOptions` 只作为兼容适配器保留，详见
[`long_term_library_roadmap_2026_08_16.md`](long_term_library_roadmap_2026_08_16.md)。

需要复用配置时使用对象入口：

```cpp
manumesh::simplification::SimplifyConfig config;
manumesh::simplification::QEMSimplifier simplifier;
simplifier.setConfig(config);
manumesh::Mesh output = simplifier.simplify(input, &report);
```

不希望在 C++ 交换类型里暴露 Eigen 时使用：

```cpp
manumesh::PlainMesh output =
    manumesh::simplification::simplifyPlainMesh(inputPlain, options, &report);
```

特征检测提供平级对象入口：

```cpp
manumesh::feature::FeatureDetector detector(featureOptions);
manumesh::feature::FeatureAnalysis features = detector.analyze(mesh);
```

预计算结果通过显式数据流进入简化：

```cpp
manumesh::simplification::SimplifyConfig config;
config.features.enabled = true;
config.features.detection = featureOptions; // 规范特征检测配置
manumesh::simplification::QEMSimplifier simplifier;
simplifier.setConfig(config);
manumesh::Mesh output = simplifier.simplify(mesh, features, &report);
```

新代码使用按职责分组的 `SimplifyConfig`；`features.detection` 是唯一的检测配置来源，转换到内部热循环只发生一次。旧的 `SimplifyOptions` 扁平特征检测字段只保留为源码兼容适配器；设置 `featureOptionsOverride` 后，override 对应配置优先。已验证的预计算 `FeatureAnalysis` 在启用 Normal Tensor 证据时还携带按检测时阈值和尺度解析好的逐顶点权重；简化器把这组权重视为规范证据直接复用，不会用另一组简化选项重新阈值化。显式传入预计算分析并选择 `weightMode=NormalTensor` 时，权重数组必须覆盖全部输入顶点；缺失权重会抛出 `std::invalid_argument`，只有未传入预计算分析的普通简化入口才会按当前有效配置计算权重。

C API 使用 `ManuMeshContext`、`ManuMeshMeshHandle`、`ManuMeshSimplifyOptions`、`ManuMeshSimplifyReport` 和 `ManuMeshMeshStats`。输入 options 调用前必须用对应 `*_init` 初始化，避免 ABI 版本和默认值漂移；同一 `MANUMESH_ABI_VERSION` 内允许尾部较短的旧 `struct_size`，库只读取存在的字段，新增尾字段使用默认值。`ManuMeshSimplifyOptions::feature_options` 是调用期借用的 `ManuMeshFeatureOptions` 指针，库不保留所有权；非空时覆盖全部旧的扁平特征检测字段。外层和嵌套 options 都独立按各自 `struct_size` 做 size-aware 读取，嵌套结构缺失的尾字段同样使用默认值。report/stats 是纯输出，current 源码 alias 与显式 `*_with_size` 入口以调用方容量为唯一写入边界，不读取 output 原有字节；旧 ABI 符号固定写首发 v1 前缀。
