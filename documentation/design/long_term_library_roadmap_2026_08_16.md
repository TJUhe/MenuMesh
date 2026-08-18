# ManuMesh 长期库化改造路线（2026-08-16）

本文把 ManuMesh 从当前的三角网格分析/简化内核推进到可持续扩展的大型网格库，
并把本轮参数契约收口与后续能力边界固定下来。目标不是一次性复制 Polygonica 的
全部能力，而是先稳定实体模型、错误模型、属性传播和模块依赖，再按功能族增加能力。

## 当前基线

- 构建基线固定为 Visual Studio 16 2019、MSVC v142、x64；CMake 在配置阶段拒绝其他
  generator、toolset 和架构。
- C++ SDK 以 `Mesh`/`PlainMesh` 为交换类型，C ABI 以不透明 handle、状态码、
  `struct_size + abi_version` 为跨语言边界。
- `manumesh::feature`、`manumesh::simplification`、`manumesh::analysis` 是平级算法
  模块；`mesh_edit` 只承载内部可变拓扑和 compaction，不承载算法策略。
- 输入错误、选项错误、C ABI 错误和 IO 错误分别遵循
  [`error_handling_policy.md`](error_handling_policy.md) 的决策表；不会用隐式兜底掩盖失败。
- Doxygen API 与内部源码文档分别由 `docs-api`、`docs-internal` 生成，`check-src-doxygen`
  对 `src` 文件头和块式注释做构建门禁。

## 本轮已落地的契约改造

### 1. 简化参数分组

新代码应使用 `SimplifyConfig`：

```cpp
manumesh::simplification::SimplifyConfig config;
config.target = manumesh::simplification::SimplifyTarget::faceCount(10000);
config.cost.lineQuadrics =
    manumesh::simplification::LineQuadricConfig::adaptive(1e-2);
config.cost.weightMode = manumesh::simplification::WeightMode::NormalTensor;
config.features.enabled = true;
config.features.protectionMode =
    manumesh::simplification::FeatureProtectionMode::PrimitiveCurves;
config.features.detection.featureAngleDeg = 35.0;
config.quality.preserveBoundary = true;
config.texture.preserveTexture = true;

manumesh::simplification::SimplifyReport report;
manumesh::simplification::QEMSimplifier simplifier;
simplifier.setConfig(config);
auto output = simplifier.simplify(input, &report);
```

配置职责固定为五组：目标、排序代价、特征、质量/合法性和纹理；单独的运行日志开关保留在顶层。目标、line-quadric 模式和局部误差都使用单选值，避免同时设置互相竞争或彼此失效的字段。
目标使用
`SimplifyTarget::faceCount()` 或 `SimplifyTarget::ratio()` 明确选择一种单位；line quadrics
使用 `LineQuadricConfig::disabled()`、`uniform(weight)` 或 `adaptive(baseWeight)` 三选一；特征检测只
在 `config.features.detection` 中存一份。`QEMSimplifier::setConfig()` 是有状态调用的
统一入口；`makeSimplifyOptions()` 只作为仍接收旧平面选项的兼容适配点。

原有 `SimplifyOptions` 继续保留在 0.x 版本作为源码兼容适配器。其扁平特征字段只有
在 `featureOptionsOverride` 未设置时才生效；新功能不得再向该平面结构追加第二套同义
字段。达到 1.0 前，新增参数优先进入分组结构和对应的 C ABI 尾部版本字段。

`SimplifyReport` 保留现有扁平字段以维持源码兼容；新代码的常用路径只读取
`report.summary()`。详细字段仍可用于调参和回归，但不再继续添加一组组投影 summary。
未来的逐点误差、事件轨迹和属性输出应使用独立诊断对象、属性通道或显式观察器。
这个方向参考 CGAL visitor、OpenMesh observer、VTK 可选输出数组和 libigl 的明确输出参数。

### 2. CLI 兼容性

CLI 的默认入口先使用 `--profile default|cad|scan|smooth` 构造 `FeatureOptions` 或
`SimplifyConfig`，再只应用命令行中显式提供的字段。这样 profile 与 SDK 使用相同的配置契约，
同时仍允许高级用户精确覆盖任意公开阈值。`--print-resolved-config` 必须显示转换后的有效配置，
不能只回显原始 token。

profile/CLI 的检测默认采用 `FeatureOptions` 的 8 顶点环阈值；直接构造的旧平面
`SimplifyOptions{}`、`SimplifyConfig{}` 和 C ABI 初始化继续保留 16，以避免 0.x SDK 的结果静默漂移。

0.x 保持已有 CLI 语义：`--smooth-curvature-features`、`--feature-normal-filter` 和
`--feature-graph-consolidation` 会自动开启 `--preserve-feature-curves` 对应的简化保护；
`--no-preserve-feature-curves` 是新的显式退出方式。单独给出这些通道的数值调节参数仍不新增
硬错误，但 CLI 必须警告未启用通道的子参数、目标/误差单位冲突、失效的 line-quadric 权重和仅用于
报告的 component-confidence 阈值。未来如需更严格的显式模式，应以新命令或显式版本开关引入，不能改变
已发布命令的默认含义。

### 3. C ABI 边界

v1 ABI 保持二进制兼容，不删除旧符号，也不改变已发布结构的前缀布局。扩展遵循：

1. 新字段只追加到结构尾部，并通过 `struct_size` 读取。
2. 需要改变数组元素布局或所有权语义时，新增 `V2` 结构和入口，不复用旧步长。
3. 输入网格、输出缓冲区、容量和 `*_written` 的契约必须在头文件和 C 测试中同时出现。
4. C++ 异常只能在 C++ 边界内存在；C ABI 统一返回状态码并写入 context last-error。
5. 纹理、属性和大索引能力不通过偷偷复用 v1 字段表达，分别进入后续版本。

## 后续实施顺序

### P2: validation 与 repair（下一阶段）

先新增平级 `validation` 模块，提供只读问题列表和确定性修复计划。非有限坐标、
重复顶点、零面积面、重复面、非流形边、断裂边界和绕序不一致使用稳定的
`ValidationIssueKind` 表达，并由查询函数按类型计数，避免每增加一种问题就扩张主结果。
修复结果必须返回 old-to-new 映射和紧凑摘要，不能在 `Mesh` 构造或简化入口中静默修复。

`repair` 只消费 `validation` 报告和 `mesh_edit` 编辑事务，不依赖 simplification 私有
类型。先做 remove-degenerate、weld、orient-component、remove-duplicate-face 四种
可验证操作，再考虑 hole filling 和局部重建。

### P3: 类型化属性通道

属性系统要服务 remeshing、repair、简化和导出，而不是把字段继续塞进 `Mesh`：

- 先提供内部 `PropertyChannel<T>` + old-to-new remap；
- 属性按 vertex/edge/face/corner 四个域区分；
- UV、法向、材料 ID、来源面 ID 等先作为类型化 channel，不用字符串键驱动核心算法；
- compaction、split、collapse、flip 必须声明每个 channel 的传播策略；
- 稳定后再评估是否进入 C ABI，C 侧优先采用显式 descriptor，而非暴露 C++ 模板。

### P4: 大网格索引与分区数据层（第一步已完成，跨分区算法待实施）

2026-08-18 已完成 P4 的第一步：`MeshTopology` 内部使用紧凑 CSR/连续数组，
`PartitionedMeshDataset` 提供有内存预算的二进制 STL 三角记录流式读写，CLI 提供
`large-import` 和 `large-validate`，并用三份 2.5M 至 3.15M 面 Thingi10K STL 做了外部验证。
这些能力解决的是存储和顺序 I/O，不是全局共享顶点拓扑、分区 FeatureGraph 或 out-of-core QEM。
实测、格式限制和后续合同见
[`large_mesh_architecture_2026_08_18.md`](large_mesh_architecture_2026_08_18.md)。

当前公共 `Face` 和 typed handle 仍使用 `int`，适合当前单块内核和 VS2019 基线，但不是无限
扩展的承诺。下一步必须先定义 `GlobalVertexId`、`GlobalEdgeId`、`GlobalFaceId`、owner/ghost、
完整 edge incidence、halo schema 和可版本化 manifest，再实现分区分析。升级顺序为：

1. 全局 ID 与 32 位 local index 的明确分工；
2. edge-key 全局归并、vertex-star、边界/非流形和绕序 parity；
3. halo-aware 法向、Normal Tensor、Smooth Curvature 和全局 FeatureGraph；
4. `reference-exact` 的全局协调 out-of-core QEM；
5. 满足拓扑/几何/特征预算的 `bounded-valid` 并行 QEM。

禁止在 v1 中把 `int` 静默扩大，也禁止只扩大顶点索引而不定义面、边和报告字段的配套规则。
在跨分区算法完成前，不得把当前三角记录分区格式表述为“完整超大网格处理”。

### P5: remeshing

remeshing 与 simplification 平级，复用 `mesh_edit` 的事务和属性 remap，不复用 QEM
私有策略。第一版只做各向同性 split/collapse/flip + tangential smoothing，随后加入
目标边长场、特征/边界约束和参考曲面投影。每个操作都必须有局部前置条件、事务回滚
和诊断计数；不能用“失败后换另一种操作”作为隐式 fallback。

### P6: C ABI v2 与功能族

当 validation、属性和 remeshing 的 C++ API 稳定后，再发布 v2 C ABI：

- `ManuMeshDocument`/`ManuMeshPropertySet` 作为可选的实体层；
- 算法入口使用 handle + options + report，避免继续增长长参数列表；
- v1 mesh handle 与 v2 document 可通过显式复制函数互转，所有权和生命周期写入文档；
- v2 不改变 v1 的函数行为，两个 ABI 可以在同一 DLL 共存。

## 每阶段验收门槛

每个新模块必须同时通过：

1. include boundary 和 module link boundary；
2. options/default/invalid-input 单测；
3. 至少一个 C++ API 示例和一个 C ABI/安装 SDK consumer（若模块进入 C ABI）；
4. `ctest -LE "performance|external"` 快速回归，以及独立的 external/performance 标签；
5. `docs-api`、`docs-internal` 和 `check-src-doxygen`；
6. VS2019 Debug/Release、x64、/MD 的构建验证。

路线的核心约束是：先稳定共享数据和错误契约，再增加能力；任何新算法都必须通过
平级模块、显式配置、可追踪报告和可验证属性传播接入，而不是把参数、修复和 fallback
继续堆进 simplification 或 CLI。
