# common 内部基础库规划

`src/common/` 是 ManuMesh 的内部基础库，不是一个临时杂物目录。它的目标是把
`feature_detection`、`simplification`、未来 `repair`、`remeshing`、`validation`
都会用到的底层 mesh 能力沉淀下来，让新功能不必重复实现边 key、邻接扫描、边界
loop、空间查询、索引重映射和诊断结构。

这层仍然是私有实现层：外部 SDK 不能 include `src/common/detail/...`。当某个概念
稳定到可以成为 SDK 合约时，再提升到 `include/core/` 或新的公共算法模块。

## 当前已有内容

```text
src/common/detail/MathConstants.h   数学常量，例如 kPi
src/common/detail/MeshQueries.h     边 key、面 key、边-面 incidence、面法向、面心、顶点邻接、局部边长尺度、边界顶点
src/common/MeshQueries.cpp          MeshQueries 实现
```

这些能力已经被 feature detection 和 simplification 共用。`computeVertexAverageEdgeLength`
已经用于 normal tensor 的局部尺度归一化，也会继续作为 QEM/feature policy 的采样
密度基础诊断。后续新模块应优先查看 common 是否已有能力，再决定新增私有 helper。

## 应继续沉淀的方向

下面是建议逐步补强的 common 能力。它们不是要求一次性全做，而是在新增 `repair`
等模块时按真实复用需求迁入。

| 方向 | 建议文件 | 典型能力 | 受益模块 |
| --- | --- | --- | --- |
| 几何谓词 | `src/common/detail/GeometryPredicates.h` | triangle quality、point-triangle distance、AABB、triangle intersection、segment distance | simplification、repair、validation、remeshing |
| 边界结构 | `src/common/detail/BoundaryLoops.h` | 从 edge incidence 提取 boundary loops、loop 面积/法向、开边界组件统计 | feature_detection、repair、validation |
| 空间索引 | `src/common/detail/SpatialIndex.h` | CellCoord、hash、uniform grid、AABB query、overflow 管理 | simplification、repair、collision/self-intersection |
| 索引重映射 | `src/common/detail/IndexRemap.h` | 活跃元素压缩、old-to-new 映射、face/edge/attribute remap | simplification、repair、mesh cleanup |
| mesh 校验 | `src/common/detail/MeshValidation.h` | face index 检查、有限坐标、退化三角形扫描、duplicate face key | api、simplification、repair、tests |
| 小型图工具 | `src/common/detail/GraphTraversal.h` | connected components、DFS/BFS、degree counting、path/cycle utility | feature_detection、repair、boundary loop 提取 |
| 诊断结构 | `src/common/detail/Diagnostics.h` | 内部 issue record、severity、element id、stage tag | repair、validation、CLI report |
| 属性/状态数组 | `src/common/detail/AttributeArrays.h` | 按 typed id 管理属性、重映射属性、默认值填充 | repair、remeshing、future editable topology |

其中 `GeometryPredicates` 和 `SpatialIndex` 已经有 simplification 专属雏形：

```text
src/simplification/detail/GeometryPredicates.h
src/simplification/detail/SpatialFaceIndex.h
src/simplification/detail/SimplificationTypes.h 中的 CellCoord/CellCoordHash
```

当 `repair` 或 `validation` 也需要这些能力时，应优先把它们改造成 common 版本，而
不是复制到新模块里。

## 迁入 common 的标准

满足这些条件时，适合迁入 `src/common/detail/`：

- 至少两个算法模块真实需要，或一个新模块马上会和现有模块共用。
- 语义是 mesh/geometry 基础设施，不带某个算法的业务含义。
- API 可以用 core 类型表达，例如 `Mesh`、`Face`、`Vec3`、`std::vector<int>`。
- 不依赖某个模块的私有状态，例如 `VertexState`、`FeatureAnalysisBuilder`。
- 可以用小而确定的单元测试覆盖。

不适合迁入 common 的内容：

- repair 专属的 patch 候选、hole fill 策略评分、issue 聚类状态。
- simplification 专属的 collapse candidate、quadric、active face state。
- feature detection 专属的 trace graph、primitive fit result、loop ownership builder。
- 还不清楚是否会复用的“一次性便利函数”。

## common API 风格

common 头应偏向小函数、小结构、无隐藏全局状态：

```cpp
namespace manumesh::detail {

struct BoundaryLoop {
  std::vector<int> vertices;
  bool closed = false;
};

std::vector<BoundaryLoop> extractBoundaryLoops(const Mesh& mesh);

struct CellCoord {
  int x = 0;
  int y = 0;
  int z = 0;
};

struct CellCoordHash {
  std::size_t operator()(const CellCoord& cell) const;
};

} // namespace manumesh::detail
```

如果需要维护缓存或索引，类也应保持算法无关：

```cpp
class UniformAabbGrid {
public:
  void clear();
  void insert(int itemId, const Vec3& lo, const Vec3& hi);
  std::vector<int> query(const Vec3& lo, const Vec3& hi) const;
};
```

不要把 `SimplifyOptions`、`RepairOptions` 或其他算法 options 传进 common。common
只处理通用数据；阈值、策略和报告解释留在调用模块。

## 与 core 的边界

`core` 是 SDK 稳定层，`common` 是库内部复用层。判断方式：

- 外部用户需要稳定调用，放 `include/core/` 或 `include/algorithms/<domain>/`。
- 只有内部算法需要，放 `src/common/detail/`。
- 只是某个算法内部阶段需要，放 `src/<domain>/detail/`。

例如 boundary loop 提取一开始可以放 common；如果未来 SDK 用户需要直接列出
boundary loops，再设计稳定公共类型并提升到 `include/core/` 或 `include/algorithms/validation/`。

## repair 的 common 优先级

做 `repair` 时，建议优先补这些 common 能力：

1. `MeshValidation`：复用 API 和 simplification 已经在做的 face index、有限坐标、
   退化三角形检查。
2. `BoundaryLoops`：repair 填洞需要 boundary loop，feature detection 也可复用。
3. `IndexRemap`：删除重复点、退化面、孤立点后需要稳定 remap。
4. `GeometryPredicates`：孔洞补面、局部质量和自交检测会复用 simplification 的几何谓词。
5. `SpatialIndex`：局部自交、重复点合并和近邻查询都需要空间结构。

这样 `repair` 的私有实现可以专注策略，而不是重新实现基础 mesh 操作。

## 测试要求

每新增一个 common 组件，应配套小测试文件，例如：

```text
tests/unit/common/mesh_query_tests.cpp
tests/unit/common/boundary_loop_tests.cpp
tests/unit/common/index_remap_tests.cpp
tests/unit/common/spatial_index_tests.cpp
```

测试应覆盖边界情况：空 mesh、退化面、重复面、孤立点、非流形边、开边界和多组件。
common 测试越扎实，后续算法模块越容易只测试策略行为。
