# ManuMesh 网格内核架构

ManuMesh 是面向增材制造的 C++ 多边形网格几何内核。当前稳定能力集中在 QEM/line quadrics 简化和 CAD/STL 风格特征检测，代码组织目标是让后续 `repair`、`boolean`、`offset`、`remesh` 等模块能并列加入，而不是继续堆到 QEM 实现里。

## 分层

```text
include/line_quadrics_qem/      安装级公共 SDK 头文件
  core/                         Mesh、PlainMesh、MeshTopology、Status、typed handles
  algorithms/feature_detection/ 特征检测模块入口、选项和结果类型
  algorithms/simplification/    QEM/line quadrics 简化入口、选项、报告和指标
  api/                          C ABI，使用 handle 和显式初始化结构体

src/common/detail/              跨算法私有工具，不安装
src/core/                       公共 core 类型的实现
src/feature_detection/          特征检测实现，只依赖 core 和私有 common
src/simplification/             简化实现，可消费 FeatureAnalysis
src/simplification/detail/      简化专属私有状态和策略
apps/linequadrics/              CLI，按外部用户方式调用 SDK
examples/                       C/C++ SDK 使用示例
tests/                          GoogleTest 和 CTest 回归验证
docs/                           当前设计、指南、论文索引和历史生成资料
```

## 隐私边界

公共头文件只表达稳定 SDK 合约，不承载算法内部状态。当前使用 pimpl 的公共对象包括：

- `MeshTopology`：隐藏拓扑缓存的存储布局。
- `QEMSimplifier`：隐藏单次简化运行状态、队列、动态拓扑和策略对象。
- `FeatureDetector`：隐藏检测器内部配置和后续可能加入的缓存、策略或统计字段。

`SimplifyOptions`、`SimplifyReport`、`FeatureOptions`、`FeatureAnalysis` 仍是公开结构体，因为它们是调用方需要读写的稳定数据交换格式。更细的运行时类型，例如候选边、活动面、空间索引、feature graph 追踪辅助结构，留在 `src/.../detail/` 或 `.cpp` 匿名命名空间中。

## 公共私有层

`src/common/detail/MeshQueries.h` 是库内部公共层，负责多个算法都会用到但暂不应进入 SDK 的网格查询：

- 无向边 key 和面 key。
- 边到相邻面的局部 incidence。
- 面法向、面心、顶点一环邻接。
- 边界顶点标记。

这层解决的是“实现复用”，不是“SDK 暴露”。如果某个能力未来需要外部稳定使用，应优先评估是否提升到 `core/MeshTopology` 或新的公共模块，而不是让外部 include `src/common/detail/...`。

## 算法边界

特征检测是与简化平级的算法模块。它只依赖 core 和私有 common，不依赖 QEM。简化模块可以消费 `FeatureAnalysis`，用于 feature quadrics、placement 投影、曲线预算和硬保护策略。

QEM/line quadrics 只负责候选折叠排序和局部几何优化，工业级安全性由显式过滤器补足：拓扑 link condition、边界策略、法线偏转、三角形质量、局部误差和自交检查。特征图应先成为独立结果，再由简化、验证、修复或未来重网格模块消费。

## 数据策略

`Mesh` 仍是轻量交换格式：稠密顶点数组加三角面索引数组。需要重复邻接查询时，算法应构建 `MeshTopology`、私有 common 查询结果，或运行时动态拓扑，而不是在每个模块里重复扫描并复制一套局部工具。

未来若加入可编辑半边拓扑，应使用 `VertexId`、`EdgeId`、`HalfedgeId`、`FaceId` 等 typed handle，配合 generation-aware free list 和显式 compaction。属性不要塞进基础顶点结构，应以类型化数组挂在拓扑旁边，方便重映射、导出和 ABI 隔离。

## API 形态

简化主入口：

```cpp
lq::Mesh simplifyMesh(const lq::Mesh& input,
                      const lq::SimplifyOptions& options,
                      lq::SimplifyReport* report = nullptr);
```

需要复用配置时使用对象入口：

```cpp
lq::QEMSimplifier simplifier(options);
lq::Mesh output = simplifier.simplify(input, &report);
```

特征检测提供平级对象入口：

```cpp
lq::FeatureDetector detector(featureOptions);
lq::FeatureAnalysis features = detector.analyze(mesh);
```

C API 使用 `LqContext`、`LqMeshHandle`、`LqSimplifyOptions`、`LqSimplifyReport` 和 `LqMeshStats`。所有公开 C 结构体调用前必须用对应 `*_init` 初始化，避免 ABI 版本和默认值漂移。
