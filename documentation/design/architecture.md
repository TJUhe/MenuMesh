# 架构与模块边界

本文只描述当前源码中存在的模块。未来的 repair、boolean、offset 或 remeshing 不属于当前
公共 API，不能据此推断已经实现。

## 依赖方向

```text
dependency edges point from a module to the modules it uses:

geometry (src/core)
common (src/common)             -> geometry
mesh_edit (src/mesh_edit)       -> common, geometry
analysis (src/analysis)         -> common, geometry
io (src/io)                     -> geometry
debug_util (src/debugUtil)      -> geometry                       [optional, no active product call sites]
feature_detection (src/feature_detection)
                                -> common, geometry (+ debug_util)
simplification (src/simplification)
                                -> analysis, common, geometry,
                                   feature_detection, mesh_edit
c_api (src/api)                 -> analysis, feature_detection,
                                   geometry, io, simplification

apps/     --> ManuMesh::manumesh (public C++ interface)
examples/ --> public SDK interfaces
tests/    --> manumesh_internal for white-box tests;
             selected ABI/performance checks use public targets
```

公共头在 `include/`，实现按目录拆分：

| 目录 | 当前职责 |
| --- | --- |
| `include/core`, `src/core`（CMake `geometry`） | `Mesh`、`PlainMesh`、状态、容差、生成器和基础几何 |
| `include/io`, `src/io` | STL/OBJ 和 `PartitionedMeshDataset` |
| `src/common` | 多模块复用的私有几何谓词、查询、距离索引、空间索引和并行适配 |
| `src/mesh_edit` | 活动面、动态入射关系和 compact/remap；不决定算法策略 |
| `include/algorithms/analysis`, `src/analysis` | 统计和采样距离 |
| `include/algorithms/feature_detection`, `src/feature_detection` | 特征证据到图、环、primitive、component、patch |
| `include/algorithms/simplification`, `src/simplification` | QEM/line-quadrics 候选、placement、合法性、提交和精修 |
| `include/api`, `src/api` | handle、状态码和 size-aware C ABI |
| `src/debugUtil` | Debug-only HTML 线框诊断；当前 instrumentation 调用点暂时停用，不属于产品流程 |
| `apps` | CLI 解析、命令分派、CSV 和工作流 |

`src/*/detail` 和 `src/common` 是私有实现，不安装、不导出。测试为覆盖私有边界而链接
`manumesh_internal`，该目标不是 SDK consumer 的公共入口。`manumesh::feature` 是已发布
的公共命名空间，目录名 `feature_detection` 不应作为破坏性改名理由。

`Mesh` 是核心公共几何对象，`PlainMesh` 是不暴露 Eigen 的交换容器；仓库没有独立的
B-Rep/逻辑实体层。边界、非流形和闭合性语义由 `MeshTopology` 按三角面共享边计算，而不是
由一个更高层的几何对象维护。

## 数据流

静态算法的数据流是：

```text
STL/OBJ -> Mesh -> (可选) FeatureAnalysis -> Simplify -> Mesh -> STL/OBJ/CSV
                         \-> MeshAnalysis / benchmark / surface patches
```

`FeatureAnalysis::source` 绑定顶点坐标、面顺序和面角索引；简化消费前会校验来源和公开图
索引。UV 不参与该 indexed-geometry 身份指纹，但会在纹理保护策略中单独验证。

## 并发边界

公共 `ExecutionOptions` 只暴露 `Serial`/`Parallel`、最大并发度和任务粒度，不暴露 oneTBB
类型。可并行的是法向滤波的独立面更新、Normal Tensor 的逐顶点步骤、独立 primitive 拟合，
以及简化中的状态初始化和初始候选 placement；共享图构建、图排序、cleanup、环发布、
动态坍缩、拓扑提交和固定归约保持确定性协调。未编译后端或默认模式会串行执行。并行不能
改变图排序、浮点归约或 collapse 顺序。

## 超大网格边界

`PartitionedMeshDataset`/`large-import`/`large-validate` 是二进制 STL 三角记录的分区存储和
完整性校验，包含目录、64 位三角 ID、checksum 和包围盒。当前没有全局顶点/边表、owner/ghost、
halo、跨分区 FeatureGraph 或 out-of-core QEM；不要把 MMPD 称为分区算法执行器。

## 构建边界

顶层 `CMakeLists.txt` 负责工具链、全局选项和目录装配；`src/CMakeLists.txt` 集中维护生产
模块的 source/header 列表、依赖和库目标，`apps/CMakeLists.txt`、`tests/CMakeLists.txt` 和
`adm/CMakeLists.txt` 分别维护应用、测试和开发辅助目标。公共 C++ 目标是
`ManuMesh::manumesh`，C-only 目标是 `ManuMesh::c_api`。
