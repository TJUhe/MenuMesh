# ManuMesh 网格内核架构

ManuMesh 是面向增材制造的 C++ 多边形网格几何内核。当前稳定能力集中在 QEM/line quadrics 简化和 CAD/STL 风格特征检测，代码组织目标是让后续 `repair`、`boolean`、`offset`、`remesh` 等模块能并列加入，而不是继续堆到 QEM 实现里。

算法层面的共同语境见 [`algorithm_essence.md`](algorithm_essence.md)。common 内部基础库的补强规划见 [`common_foundation.md`](common_foundation.md)；新增 `repair`、`remeshing` 等平级模块的落地流程见 [`adding_new_algorithm.md`](adding_new_algorithm.md)。本文只描述架构边界：哪些概念可以公开，哪些必须留在私有实现里，哪些模块可以依赖彼此。

## 分层

```text
include/      安装级公共 SDK 头文件
  core/                         Mesh、PlainMesh、MeshTopology、Status、typed handles
  algorithms/feature_detection/ 特征检测模块入口、选项和结果类型
  algorithms/simplification/    QEM/line quadrics 简化入口、选项、报告和指标
  algorithms/<domain>/          未来平级算法模块，例如 repair/remeshing
  api/                          C ABI，使用 handle 和显式初始化结构体

src/common/detail/              跨算法私有工具，不安装
src/core/                       公共 core 类型的实现
src/feature_detection/          特征检测实现，只依赖 core 和私有 common
src/feature_detection/detail/   特征检测私有类型、策略接口和 helper
src/simplification/             简化实现，可消费 FeatureAnalysis
src/simplification/detail/      简化专属私有状态、policy 分组和策略
src/<domain>/                   未来平级算法实现，例如 repair
src/<domain>/detail/            未来平级算法私有状态和阶段 helper
apps/manumesh/              CLI，按外部用户方式调用 SDK
examples/                       C/C++ SDK 使用示例
tests/                          GoogleTest 和 CTest 回归验证
docs/                           当前设计、指南、论文索引和历史生成资料
```

## 隐私边界

公共头文件只表达稳定 SDK 合约，不承载算法内部状态。当前使用 pimpl 的公共对象包括：

- `MeshTopology`：隐藏拓扑缓存的存储布局。
- `QEMSimplifier`：隐藏单次简化运行状态、队列、动态拓扑和策略对象。
- `FeatureDetector`：隐藏检测器内部配置和后续可能加入的缓存、策略或统计字段。

`SimplifyOptions`、`SimplifyReport`、`FeatureOptions`、`FeatureAnalysis` 仍是公开结构体，因为它们是调用方需要读写的稳定数据交换格式。C++ API 根命名空间为 `manumesh`，核心网格类型和基础工具位于根命名空间；真实功能命名空间按模块拆开：特征检测位于 `manumesh::feature`，QEM/line-quadrics 简化位于 `manumesh::simplification`。功能模块类型不再回灌到根命名空间。其中简化选项、报告和枚举集中在 `SimplificationTypes.h`，不依赖 Eigen 或 `Mesh`。更细的运行时类型，例如候选边、活动面、空间索引、feature graph 追踪辅助结构、edge evidence strategy 和 loop recovery helper，留在 `src/.../detail/` 或 `.cpp` 匿名命名空间中。

特征检测内部按 pipeline 组织：`FeatureDetector.cpp` 只负责 public facade 和阶段编排；`FeatureEvidence.cpp` 负责 boundary、dihedral、non-manifold、normal-tensor 等证据来源；`FeatureGraph.cpp` 负责显式 graph 和 trace graph；`FeatureLoopRecovery.cpp` 只负责恢复阶段调度；`FeatureCycleRecovery.cpp`、`FeatureTraceRecovery.cpp` 和 `FeaturePrimitiveRecovery.cpp` 分别承载 cycle、trace chain 和 primitive component 恢复；`FeatureLoopBuilder.cpp` 负责把 loop 写回 `FeatureAnalysis::vertices`；`FeatureCircularRecovery.cpp` 保留有界圆形 fallback。新增识别能力应优先加入对应私有阶段，而不是继续扩张 `FeatureDetector.cpp`。

简化内部也按 pipeline/strategy 分层：`QEMSimplifier.cpp` 是 public facade；`SimplificationRun.cpp` 负责编排单次运行、队列和状态应用；`SimplificationPolicies.cpp` 把扁平 options 转成内部 policy；`CollapseAttempt.cpp` 负责把 feature、boundary、curve budget 和 legality filters 组合成一次候选坍缩的接受/拒绝结果；`Quadrics.cpp`、`FeatureConstraints.cpp`、`CollapseLegality.cpp` 等模块只表达各自策略。新增 collapse 过滤器优先进入对应 policy/evaluator，而不是继续扩张 `SimplificationRun.cpp`。

CLI 是应用层消费者，不承载算法状态。`apps/manumesh/main.cpp` 只调用 `manumesh::cli::run()`；`CliArguments.cpp` 负责通用参数解析；`ManuMeshCli.cpp` 负责帮助输出和 `run()` 派发；`ManuMeshCommands.cpp` 承载命令 handler 与 command registry。新增 CLI 命令应新增 handler 并注册到 registry，公共算法能力仍优先进入 SDK 层。

## 公共私有层

`src/common/detail/MeshQueries.h` 是库内部公共层，负责多个算法都会用到但暂不应进入 SDK 的网格查询：

- 无向边 key 和面 key。
- 边到相邻面的局部 incidence。
- 面法向、面心、顶点一环邻接。
- 边界顶点标记。

这层解决的是“实现复用”，不是“SDK 暴露”。后续 common 应继续沉淀几何谓词、边界 loop、通用空间索引、索引重映射、mesh 校验和内部诊断结构，避免 `repair`、`remeshing`、`validation` 各自复制基础设施。如果某个能力未来需要外部稳定使用，应优先评估是否提升到 `core/MeshTopology` 或新的公共模块，而不是让外部 include `src/common/detail/...`。

## 算法边界

特征检测是与简化平级的算法模块。它只依赖 core 和私有 common，不依赖 QEM。简化模块可以消费 `FeatureAnalysis`，用于 feature quadrics、placement 投影、曲线预算和硬保护策略。

QEM/line quadrics 只负责候选折叠排序和局部几何优化，工业级安全性由显式过滤器补足：拓扑 link condition、边界策略、法线偏转、三角形质量、局部误差和自交检查。特征图应先成为独立结果，再由简化、验证、修复或未来重网格模块消费。

这个边界来自当前算法本质：标准 QEM 在平坦区域存在切向零空间，line quadrics 补的是候选排序和 placement 正则；特征图补的是制造语义支撑；硬过滤器补的是拓扑和几何安全。把这三件事合在一个类里会让新增模块无法复用 feature graph，也会让后续 envelope、repair、remesh 等能力只能绕着 QEM 转。

当前依赖方向应保持：

```text
core
  -> feature_detection
  -> simplification

core + common/detail 可被 feature_detection 和 simplification 使用；
simplification 可以消费 feature::FeatureAnalysis；
feature_detection 不能反向 include simplification；
未来 repair/remesh/validation 可以消费 feature::FeatureAnalysis。
```

## 数据策略

`Mesh` 仍是轻量交换格式：稠密顶点数组加三角面索引数组。需要重复邻接查询时，算法应构建 `MeshTopology`、私有 common 查询结果，或运行时动态拓扑，而不是在每个模块里重复扫描并复制一套局部工具。

`Mesh` 是 Eigen-backed 便利类型，适合同编译器、同 C++ ABI 的 SDK 消费方。
`PlainMesh` 是 Eigen-free C++ 交换类型；`PlainSimplifier.h` 提供
`simplifyPlainMesh()`，内部转换为 `Mesh` 后复用同一套简化实现。真正跨语言或
严格 ABI 边界仍优先使用 `api/CApi.h`。

未来若加入可编辑半边拓扑，应使用 `VertexId`、`EdgeId`、`HalfedgeId`、`FaceId` 等 typed handle，配合 generation-aware free list 和显式 compaction。属性不要塞进基础顶点结构，应以类型化数组挂在拓扑旁边，方便重映射、导出和 ABI 隔离。

## API 形态

简化主入口：

```cpp
namespace manumesh::simplification {
Mesh simplifyMesh(const manumesh::Mesh& input,
                  const SimplifyOptions& options,
                  SimplifyReport* report = nullptr);
}
```

需要复用配置时使用对象入口：

```cpp
manumesh::simplification::QEMSimplifier simplifier(options);
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

C API 使用 `ManuMeshContext`、`ManuMeshMeshHandle`、`ManuMeshSimplifyOptions`、`ManuMeshSimplifyReport` 和 `ManuMeshMeshStats`。所有公开 C 结构体调用前必须用对应 `*_init` 初始化，避免 ABI 版本和默认值漂移。同一 `MANUMESH_ABI_VERSION` 内，输入结构体允许尾部较短的旧 `struct_size`，库只读取存在的字段，新增尾部字段使用默认值；未初始化或 ABI 版本不匹配仍会被拒绝。
