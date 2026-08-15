# common 内部基础库规划

`src/common/` 是 ManuMesh 的库内复用层，不是临时杂物目录，也不是稳定 SDK。它的目标是沉淀多个算法模块都会用到的 mesh/geometry 基础设施，让 `feature_detection`、`simplification`、未来的 `repair`、`remeshing`、`validation` 不再各自复制边 key、邻接扫描、几何谓词、空间查询、距离索引和诊断结构。

这层仍然是私有实现层：外部 SDK 不能 include `src/common/detail/...`，common 里的符号也不应该用 `MANUMESH_API` 导出。当某个概念稳定到可以成为 SDK 合约时，再提升到 `include/core/` 或新的公共算法模块。

## 当前内容

```text
src/common/detail/GeometryPredicates.h  triangle quality、点到三角形距离、AABB 距离、三角形包围盒、普通/共享拓扑感知三角形相交
src/common/GeometryPredicates.cpp       GeometryPredicates 实现
src/common/detail/MeshDistanceIndex.h   面向三角网格表面的 BVH 距离查询
src/common/MeshDistanceIndex.cpp        MeshDistanceIndex 实现
src/common/detail/MeshQueries.h         边 key、面 key、边-面 incidence、面法向、面心、顶点邻接、局部边长、边界顶点、绕向协调与有向二面角
src/common/MeshQueries.cpp              MeshQueries 实现
src/common/detail/SpatialIndex.h        CellCoord、hash、UniformAabbCandidateGrid、AABB 候选查询和 overflow 管理
src/common/SpatialIndex.cpp             SpatialIndex 实现
```

这些能力已经被 feature detection 和 simplification 共享，或已经从 simplification 的私有实现迁入 common。`computeOrientedDihedralAngle` 统一 feature evidence 与 `WeightMode::Dihedral` 的反折角定义；`trianglesIntersectBeyondSharedTopology` 允许仅限声明共享顶点/边的接触，同时拒绝越过该共享拓扑的重叠。`SpatialFaceIndex` 现在只负责把 simplification 的 `FaceState`/`VertexState` 转成 AABB，真正的候选网格由 `UniformAabbCandidateGrid` 维护。通用网格统计与双 mesh 采样距离比较已上浮为 `manumesh::analysis` 模块（`src/analysis/MeshAnalysis.cpp`），点到 mesh 表面的 BVH 距离查询由 `MeshDistanceIndex` 承担。数学常量的正典位置是 `include/core/MathConstants.h`（`kPi`），`src/common/detail/MathConstants.h` 只保留常量转发。common 的命名空间是 `manumesh::common`；旧 `manumesh::detail` 过渡别名已经移除。

后续新增模块应优先检查 common 是否已有能力，再决定是否新增私有 helper。common 测试越扎实，算法模块越容易只测试策略行为。

## 迁入标准

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
- 还不清楚是否会复用的一次性便利函数。

## API 风格

common 头文件偏向小函数、小结构、无隐藏全局状态：

```cpp
namespace manumesh {
namespace common {

double triangleQuality(const Vec3& a, const Vec3& b, const Vec3& c);
std::pair<Vec3, Vec3> triangleAabb(const std::array<Vec3, 3>& tri, double padding = 0.0);

class MeshDistanceIndex {
public:
    explicit MeshDistanceIndex(const Mesh& mesh);
    bool empty() const;
    double distanceSquared(const Vec3& point) const;
};

class UniformAabbCandidateGrid {
public:
    void clear();
    void reset(const Vec3& lo, const Vec3& hi, int expectedItems);
    void insert(int itemId, const Vec3& lo, const Vec3& hi);
    std::vector<int> queryCandidates(const Vec3& lo, const Vec3& hi) const;
};

} // namespace common
} // namespace manumesh
```

不要把 `SimplifyOptions`、`RepairOptions` 或其他算法 options 传进 common。common 只处理通用数据；阈值、策略和报告解释留在调用模块。

## 与 core 的边界

`core` 是 SDK 稳定层，`common` 是库内复用层。判断方式：

- 外部用户需要稳定调用，放 `include/core/` 或 `include/algorithms/<domain>/`。
- 只有内部算法需要，放 `src/common/detail/`。
- 只是某个算法内部阶段需要，放 `src/<domain>/detail/`。

例如 boundary loop 提取一开始可以放 common；如果未来 SDK 用户需要直接列出 boundary loops，再设计稳定公共类型并提升到 `include/core/` 或 `include/algorithms/validation/`。

## 应继续沉淀的方向

| 方向 | 建议文件 | 典型能力 | 受益模块 |
| --- | --- | --- | --- |
| 几何谓词扩展 | `src/common/detail/GeometryPredicates.h` | segment distance、更鲁棒的 orientation/overlap 谓词 | simplification、repair、validation、remeshing |
| 边界结构 | `src/common/detail/BoundaryLoops.h` | 从 edge incidence 提取 boundary loops、loop 面积/法向、开边界组件统计 | feature_detection、repair、validation |
| 空间索引扩展 | `src/common/detail/SpatialIndex.h` | 近邻点查询、半径查询、多 payload 策略 | simplification、repair、collision/self-intersection |
| 索引重映射 | `src/common/detail/IndexRemap.h` | 活跃元素压缩、old-to-new 映射、face/edge/attribute remap | simplification、repair、mesh cleanup |
| mesh 校验 | `src/common/detail/MeshValidation.h` | face index 检查、有限坐标、退化三角形扫描、duplicate face key | api、simplification、repair、tests |
| 小型图工具 | `src/common/detail/GraphTraversal.h` | connected components、DFS/BFS、degree counting、path/cycle utility | feature_detection、repair、boundary loop 提取 |
| 诊断结构 | `src/common/detail/Diagnostics.h` | 内部 issue record、severity、element id、stage tag | repair、validation、CLI report |

## 自动化守卫

`tests/support/check_include_boundaries.py` 会作为 CTest 的 `include_boundaries` 运行。脚本用一张集中定义的模块依赖表约束 `core`、`common`、`io`、`mesh_edit`、`analysis`、`feature_detection`、`simplification`、`api` 和 `debugUtil`，并守住这些底线：

- `include/`、`apps/`、`examples/` 不能 include 私有 `src/.../detail` 或 `common/detail`。
- `src/common/` 不能依赖 feature detection 或 simplification。
- `src/feature_detection/` 不能反向依赖 simplification。
- 新增 `src/<module>/` 时必须先在依赖表中声明允许方向。

`include_boundary_checker_tests` 会用最小临时源码验证允许和禁止的方向。这个检查不是替代代码审查，而是防止边界规则和规则实现本身在日常增量里悄悄失效。

## 测试要求

每新增一个 common 组件，应配套小测试文件，例如：

```text
tests/unit/common/geometry_predicates_tests.cpp
tests/unit/common/mesh_distance_index_tests.cpp
tests/unit/common/mesh_query_tests.cpp
tests/unit/common/spatial_index_tests.cpp
```

测试应覆盖边界情况：空 mesh、退化面、重复面、孤立点、非流形边、开边界、多组件、超大 AABB 查询和禁用状态。common 测试越扎实，后续算法模块越容易保持简洁。
