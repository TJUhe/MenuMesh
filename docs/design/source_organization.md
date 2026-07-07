# 源码组织说明

ManuMesh 采用小型几何内核布局：公共 SDK 头文件和实现文件分离，跨算法复用工具放在私有 common 层，算法专属 helper 放在各自模块的 `detail/` 目录中。

算法为什么要按这些边界拆分，见 [`algorithm_essence.md`](algorithm_essence.md)。本文件只规定源码目录、include 和扩展规则。

## 目录契约

| 路径 | 角色 | 规则 |
| --- | --- | --- |
| `include/manumesh/` | 安装级 SDK 根目录 | 只放稳定公共头。 |
| `include/manumesh/core/` | Mesh、句柄、状态、拓扑缓存 | 外部应用可直接 include。 |
| `include/manumesh/algorithms/feature_detection/` | 特征检测 API、结果类型和对象入口 | 与 QEM 简化平级，只依赖 core。 |
| `include/manumesh/algorithms/simplification/` | QEM 简化选项、报告、指标、Eigen-backed 入口和 PlainMesh 入口 | 当前主要 decimation 模块；`SimplificationTypes.h` 不依赖 Eigen。 |
| `include/manumesh/api/` | C ABI | 不暴露 STL、Eigen 或 C++ 异常。 |
| `src/common/` | 跨算法私有实现工具 | 只能被库内部使用，不安装。 |
| `src/common/detail/` | 私有公共头 | 放多个算法共享但尚不稳定的 mesh 查询、key、hash 等工具。 |
| `src/core/` | 基础数据结构实现 | 与公共 core 头对应。 |
| `src/feature_detection/` | 特征检测实现 | 对应 `FeatureDetector` 算法模块。 |
| `src/feature_detection/detail/` | 特征检测私有 helper | primitive fitting 等不稳定实现细节。 |
| `src/simplification/` | 简化算法实现 | 公开薄入口和拆分后的内部模块。 |
| `src/simplification/detail/` | 简化专属私有头 | 只能被简化实现模块使用，不安装。 |
| `apps/manumesh/` | CLI | 像外部消费者一样调用库。 |
| `examples/` | SDK 使用示例 | 只 include 公共头。 |
| `tests/` | 回归和验证测试 | `support/` 放公共测试辅助，`unit/` 按功能分单元测试，`performance/` 放大模型/数据集测试，`data/` 放 fixture。 |
| `docs/` | 设计、指南、论文、生成笔记 | 必须描述当前代码，不写成愿景幻灯片。 |

## 当前公共私有层

```text
src/common/MeshQueries.cpp                 跨算法 mesh 查询实现
src/common/detail/MeshQueries.h            无向边 key、面 key、边-面邻接、面法向、顶点邻接、边界顶点
```

这层不是 SDK 合约。外部代码不得 include `src/common/detail/...`；如果某个概念已经足够稳定，应提升到 `include/manumesh/core/` 或新的公共算法模块。

## 当前简化模块拆分

```text
src/simplification/QEMSimplifier.cpp          公共对象 API、pimpl 和 simplifyMesh 包装
src/simplification/SimplificationRun.cpp      单次运行编排和 collapse loop
src/simplification/SimplificationPolicies.cpp 公开扁平 options 到内部 target/features/legality policy 的转换
src/simplification/Quadrics.cpp               面 quadric、line quadric、placement 求解
src/simplification/FeatureConstraints.cpp     特征曲线策略、预算和投影
src/simplification/CandidateQueue.cpp         折叠候选优先队列
src/simplification/ResultBuilder.cpp          活跃状态压缩回 Mesh
src/simplification/SimplificationValidation.cpp 选项和输入校验
src/simplification/Metrics.cpp                质量、距离和统计指标
src/simplification/CollapseLegality.cpp       拓扑、边界、质量、法线、自交过滤
src/simplification/DynamicTopology.cpp        运行时增量邻接
src/simplification/GeometryPredicates.cpp     局部几何谓词
src/simplification/SpatialFaceIndex.cpp       局部自交查询的空间哈希
```

## 当前特征检测模块拆分

```text
src/feature_detection/FeatureDetector.cpp     FeatureDetector pimpl、特征边收集、feature graph 和 loop/cycle 恢复主流程
src/feature_detection/NormalTensor.cpp        normal-tensor 特征评分公开函数实现
src/feature_detection/PrimitiveFit.cpp        circle/near-circle/ellipse primitive 拟合和误差度量
src/feature_detection/detail/PrimitiveFit.h   primitive fitting 私有数据结构和 helper 声明
```

特征检测已经是与 QEM 简化平级的模块。它不能反向依赖 `src/simplification/`；简化、验证、修复或未来重网格模块可以消费 `FeatureAnalysis`。公共 C++ API 的真实命名空间按功能拆分：`manumesh::feature` 承载检测器、特征选项和分析结果，`manumesh::simplification` 承载 QEM/line-quadrics 简化器、简化选项、报告和指标；根 `manumesh` 只承载 core 类型和基础工具，不再导出功能模块别名。

## include 规则

公共代码使用完整 SDK 路径：

```cpp
#include "manumesh/algorithms/simplification/QEMSimplifier.h"
```

库内部跨模块私有工具使用 `src` 私有 include 根：

```cpp
#include "common/detail/MeshQueries.h"
```

单一算法模块内部可以使用当前模块私有头：

```cpp
#include "detail/DynamicTopology.h"
```

不要从公共头、示例、CLI 或外部工程 include `src/.../detail/...`。如果外部确实需要某个概念，先判断它是否稳定；稳定则提升为公共类型，不稳定则通过 options、reports 或函数结果暴露行为。

## 扩展规则

模块变大时按职责拆分，而不是按公式片段随意拆文件。新的稳定算法契约进入 `include/manumesh/algorithms/<domain>/`；长期存在但不稳定的实现 helper 进入所属模块的 `detail/`；跨模块工具先放在 `src/common/detail/`，等它的语义足够稳定再决定是否提升到公共 `core`。

公共对象优先使用 pimpl 隐藏实现存储。只有调用方必须直接读写、且语义稳定的数据交换结构才进入公共头，例如 options、reports 和 analysis results。

新增算法文档应同时说明四件事：要解决的几何现象、核心数学量、当前代码入口和论文出处。缺少其中任意一项时，不应把它写成已稳定能力。
