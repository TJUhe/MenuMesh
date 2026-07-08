# 新增算法模块指南

本文说明后续如何给 ManuMesh 增加新的稳定功能模块。示例以 `repair`
为主，但同一套规则也适用于 `remeshing`、`validation`、`offset` 等模块。

ManuMesh 的扩展原则是：新能力作为平级算法加入，不反向塞进
`simplification` 或 `feature_detection`。公共 API 先定义清楚，内部实现再按
pipeline 拆分；能复用的底层 mesh 查询进入 `src/common/detail/`，但不要过早把
算法私有状态放到 common。

新增模块前应先读 [`common_foundation.md`](common_foundation.md)。如果新功能需要
边界 loop、几何谓词、空间索引、索引重映射或 mesh 校验，应优先把这些基础能力做成
common 组件，再在算法模块里组合策略。

## 何时新建模块

满足下面任一条件时，应新建 `include/algorithms/<domain>/` 和
`src/<domain>/`：

- 调用方会独立使用这项能力，而不是只作为某个算法的内部阶段。
- 它有自己的 options/report/result 类型。
- 它需要被 CLI、C API、SDK 示例或其他算法复用。
- 它的实现 pipeline 足够长，继续放进现有 `.cpp` 会让职责混在一起。

例如 `repair` 不应该藏在 `simplification` 里。简化可以消费 repair 的结果，
repair 也可以消费 `FeatureAnalysis` 或 `MeshTopology`，但它自身应是
`manumesh::repair` 模块。

## 推荐目录

以 `repair` 为例：

```text
include/algorithms/repair/Repairer.h
include/algorithms/repair/RepairTypes.h

src/repair/Repairer.cpp
src/repair/RepairValidation.cpp
src/repair/RepairRun.cpp
src/repair/HoleFill.cpp
src/repair/DuplicateRemoval.cpp
src/repair/NonManifoldRepair.cpp
src/repair/detail/RepairTypes.h
src/repair/detail/RepairValidation.h
src/repair/detail/RepairRun.h
src/repair/detail/HoleFill.h
src/repair/detail/DuplicateRemoval.h
src/repair/detail/NonManifoldRepair.h

tests/unit/repair/RepairTestSupport.h
tests/unit/repair/repair_api_tests.cpp
tests/unit/repair/repair_validation_tests.cpp
tests/unit/repair/repair_hole_fill_tests.cpp
tests/unit/repair/repair_non_manifold_tests.cpp
tests/unit/repair/repair_fixture_tests.cpp
```

公共头只表达 SDK 合约；`src/repair/detail/` 承载可变实现细节。不要从公共头
include `src/...`，不要让外部示例依赖 `detail`。

## 公共 API 形态

优先采用和现有模块一致的结构：

```cpp
namespace manumesh::repair {

struct RepairOptions {
  bool removeDuplicateVertices = true;
  bool fillSmallHoles = true;
  bool splitNonManifoldEdges = true;
  double mergeTolerance = 1e-8;
};

enum class RepairIssueKind {
  DegenerateFace,
  DuplicateVertex,
  BoundaryLoop,
  NonManifoldEdge,
};

struct RepairReport {
  int removedDegenerateFaces = 0;
  int mergedVertices = 0;
  int filledHoles = 0;
  int splitNonManifoldEdges = 0;
};

struct RepairResult {
  Mesh mesh;
  RepairReport report;
};

class MANUMESH_API Repairer {
public:
  Repairer();
  explicit Repairer(RepairOptions options);
  RepairResult repair(const Mesh& input) const;
};

RepairResult repairMesh(const Mesh& input, const RepairOptions& options);

} // namespace manumesh::repair
```

如果算法需要跨 DLL 边界长期稳定使用，公共对象用 pimpl。公开结构体只放调用方
确实需要读写的扁平数据；临时图、队列、候选集、空间索引、访问标记等全部放
`src/<domain>/detail/` 或 `.cpp` 匿名命名空间。

如果需要 Eigen-free 入口，另加 `PlainRepairer.h` 或 `repairPlainMesh()`，
内部转换到 `Mesh` 后复用同一套实现。不要复制一套 repair pipeline。

## 实现拆分

新模块一开始就按阶段拆，避免形成新的“大文件”：

- `Repairer.cpp`：public facade、pimpl、薄函数包装。
- `RepairValidation.cpp`：options 和输入 mesh 校验。
- `RepairRun.cpp`：单次运行编排、阶段顺序和 report 汇总。
- `HoleFill.cpp`：边界 loop 提取、小孔判定、补面策略。
- `DuplicateRemoval.cpp`：容差合并、退化面剔除、索引重映射。
- `NonManifoldRepair.cpp`：非流形边/点的检测和局部处理策略。

如果某个 helper 被 `feature_detection`、`simplification`、`repair` 至少两个模块
稳定复用，并且语义不是 repair 专属，再考虑放到 `src/common/detail/`。例如：

- 适合 common：边 key、边-面 incidence、面法向、顶点一环、边界顶点标记。
- 不适合 common：repair 的 hole patch 候选、修复 issue 聚类、阶段私有状态。

对 `repair` 来说，应优先补强 common 的 `MeshValidation`、`BoundaryLoops`、
`IndexRemap`、`GeometryPredicates` 和 `SpatialIndex`，让 repair 只负责“怎么修”，
不负责重复实现所有 mesh 基础设施。

## 模块依赖

推荐依赖方向：

```text
core
  -> feature_detection
  -> repair
  -> simplification
```

这不是强制线性链，但要避免环：

- `repair` 可以使用 `MeshTopology`、`src/common/detail/MeshQueries.h`。
- `repair` 可以选择消费 `feature::FeatureAnalysis` 来保护孔边界或制造特征。
- `simplification` 可以在未来消费 `repair::RepairReport` 或修复后的 mesh。
- `feature_detection` 不应 include `repair` 或 `simplification`。

当新模块需要另一个模块的结果时，优先通过公共结果类型传递，例如
`FeatureAnalysis`、`RepairReport`、`MeshTopology`，不要 include 对方的
`detail`。

## CMake 接入

新增模块时同步更新根 `CMakeLists.txt`：

- `MANUMESH_PUBLIC_HEADERS` 加入公共头。
- `MANUMESH_LIBRARY_SOURCES` 加入 `.cpp`。
- 新建 `MANUMESH_REPAIR_PRIVATE_HEADERS`，再加入 `MANUMESH_PRIVATE_HEADERS`。
- 增加对应 `source_group()`，保持 IDE 分组清楚。
- 如公共头依赖 Eigen，确认这是有意的；能放在 `RepairTypes.h` 的扁平结构尽量
  不依赖 Eigen。

测试文件加入 `tests/CMakeLists.txt` 的 `MANUMESH_UNIT_TEST_SOURCES`。测试数量一
开始就按功能拆，不要先堆一个 `repair_tests.cpp`。

## 测试规划

按行为边界拆测试文件：

- `repair_api_tests.cpp`：命名空间、对象复制/移动、options round-trip。
- `repair_validation_tests.cpp`：非法 face、非有限坐标、空 mesh、容差边界。
- `repair_hole_fill_tests.cpp`：简单孔、小孔阈值、不可补洞。
- `repair_non_manifold_tests.cpp`：非流形边、重复面、T-junction 风险。
- `repair_fixture_tests.cpp`：真实 fixture 和和外部 STL。

共享 fixture 构造函数放在 `RepairTestSupport.h`。测试名保持行为化，例如
`Repair.FillsSmallBoundaryLoop`，而不是按实现类命名。

## CLI 和 C API

只有当 SDK API 稳定且测试覆盖足够后，再加 CLI：

- `apps/manumesh/ManuMeshCommands.cpp` 新增 `commandRepair()`。
- 在 `commandRegistry()` 注册 `repair`。
- CLI 参数只表达公共 `RepairOptions`，不要暴露内部 stage 开关。
- 若需要 CSV，字段对应 `RepairReport`。

C API 更晚加入。加入时要：

- 在 `include/api/CApi.h` 增加 ABI struct，带 `struct_size` 和 `abi_version`。
- 提供 `manumesh_repair_options_init()` 和 `manumesh_repair_report_init()`。
- 在 `src/api/CApi.cpp` 做 C/C++ 类型转换和异常屏蔽。
- 增加 `tests/unit/api/` 下的 C API 测试文件。

## 文档和示例

新增模块至少更新：

- `README.md` 的核心交付物或能力列表。
- `docs/README.md` 的当前代码事实。
- `docs/design/architecture.md` 的模块依赖图。
- `docs/design/source_organization.md` 的目录契约。
- 必要时新增 `docs/guide/<domain>_usage.md`。
- 若对 SDK 用户开放，新增或更新 `examples/` 和 `examples/sdk_consumer/`。

不要更新 `docs/generated/notes/` 当作唯一说明；那里是导出笔记或历史材料。若内容受
影响，要么重新生成，要么明确标注为历史资料。

## 完成检查清单

新增功能合入前至少确认：

- 公共头只使用 `#include "core/..."`、`#include "algorithms/..."`、`#include "api/..."`。
- 没有公共头 include `src/...`。
- 新模块没有和已有模块形成 include 环。
- `cmake --build <build> --parallel` 通过。
- `ctest --test-dir <build> -C Release -LE performance --output-on-failure` 通过。
- `cmake --build <build> --target check-format` 通过。
- 如果新增/改动 SDK 公开头，`sdk-consumer-test` 通过。

这个 checklist 通过后，再考虑是否补性能测试、大模型 fixture、CLI 批处理或 C API。
