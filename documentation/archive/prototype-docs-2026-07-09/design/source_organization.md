# 源码组织说明

ManuMesh 采用小型几何内核布局：公共 SDK 头文件和实现文件分离，跨算法复用工具放在私有 common 层，算法专属 helper 放在各自模块的 `detail/` 目录中。

算法为什么要按这些边界拆分，见 [`algorithm_essence.md`](algorithm_essence.md)。common 内部基础库如何持续补强见 [`common_foundation.md`](common_foundation.md)。新增平级算法模块的完整流程见 [`adding_new_algorithm.md`](adding_new_algorithm.md)。本文件只规定源码目录、include 和扩展规则。

## 目录契约

| 路径 | 角色 | 规则 |
| --- | --- | --- |
| `include/` | 安装级 SDK 根目录 | 只放稳定公共头。 |
| `include/core/` | Mesh、句柄、状态、拓扑缓存 | 外部应用可直接 include。 |
| `include/algorithms/feature_detection/` | 特征检测 API、结果类型和对象入口 | 与 QEM 简化平级，只依赖 core。 |
| `include/algorithms/simplification/` | QEM 简化选项、报告、指标、Eigen-backed 入口和 PlainMesh 入口 | 当前主要 decimation 模块；`SimplificationTypes.h` 不依赖 Eigen。 |
| `include/algorithms/<domain>/` | 未来平级算法 API，例如 `repair`、`remeshing` | 只放稳定 options/result/facade，不放 pipeline 私有状态。 |
| `include/api/` | C ABI | 不暴露 STL、Eigen 或 C++ 异常。 |
| `src/common/` | 跨算法私有实现工具 | 只能被库内部使用，不安装。 |
| `src/common/detail/` | 私有公共头 | 放多个算法共享但尚不稳定的 mesh 查询、key、hash、几何谓词、边界 loop、空间索引、索引重映射等工具。 |
| `src/core/` | 基础数据结构实现 | 与公共 core 头对应。 |
| `src/feature_detection/` | 特征检测实现 | 对应 `FeatureDetector` 算法模块。 |
| `src/feature_detection/detail/` | 特征检测私有 helper | primitive fitting 等不稳定实现细节。 |
| `src/simplification/` | 简化算法实现 | 公开薄入口和拆分后的内部模块。 |
| `src/simplification/detail/` | 简化专属私有头 | 只能被简化实现模块使用，不安装。 |
| `src/<domain>/` | 未来平级算法实现，例如 `repair` | 按 public facade、validation、run、stage 拆分。 |
| `src/<domain>/detail/` | 未来平级算法私有头 | 只给该模块内部使用；跨模块复用前先判断是否真属于 common。 |
| `apps/` | CLI | 薄入口加 command registry，像外部消费者一样调用库。 |
| `examples/` | SDK 使用示例 | 只 include 公共头。 |
| `tests/` | 回归和验证测试 | `support/` 放公共测试辅助，`unit/` 按功能分单元测试，`performance/` 放大模型/数据集测试，`data/` 放 fixture。 |
| `documentation/` | 设计、指南、论文、生成笔记 | 必须描述当前代码，不写成愿景幻灯片。 |

## 当前公共私有层

```text
src/common/MeshQueries.cpp                 跨算法 mesh 查询实现
src/common/detail/MathConstants.h          数学常量
src/common/detail/MeshQueries.h            无向边 key、面 key、边-面邻接、面法向、顶点邻接、边界顶点
```

这层不是 SDK 合约。外部代码不得 include `src/common/detail/...`；如果某个概念已经足够稳定，应提升到 `include/core/` 或新的公共算法模块。common 后续应优先沉淀 `GeometryPredicates`、`BoundaryLoops`、`SpatialIndex`、`IndexRemap`、`MeshValidation` 等跨算法基础设施，具体标准见 [`common_foundation.md`](common_foundation.md)。

## 当前简化模块拆分

```text
src/simplification/QEMSimplifier.cpp          公共对象 API、pimpl 和 simplifyMesh 包装
src/simplification/SimplificationRun.cpp      单次运行编排和 collapse loop
src/simplification/SimplificationPolicies.cpp 公开扁平 options 到内部 target/features/legality policy 的转换
src/simplification/CollapseAttempt.cpp        单个候选坍缩的 feature/boundary/budget/legality 评估
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

新增简化能力时先判断类型：排序/placement 成本进入 `Quadrics.cpp`，公开 options 到内部策略的转换进入 `SimplificationPolicies.cpp`，单个候选是否接受进入 `CollapseAttempt.cpp` 或它调用的 policy/filter，状态应用和队列推进才留在 `SimplificationRun.cpp`。

## 当前 CLI 拆分

```text
apps/main.cpp        只保留进程入口，调用 manumesh::cli::run()
apps/CliArguments.cpp 通用 flag/value 解析和位置参数提取
apps/ManuMeshCli.cpp  帮助输出和 run() 派发
apps/ManuMeshCommands.cpp 命令 handler、command registry、CSV 输出和批处理辅助
apps/CliArguments.h   CLI 参数结构和解析 helper 声明
apps/CliCommands.h    command handler 类型和 registry 声明
apps/ManuMeshCli.h   CLI run() 声明
```

新增命令时先在 `ManuMeshCommands.cpp` 新增 `int commandXxx(const Args&)` handler，再注册到 `commandRegistry()`；新增通用参数才改 `CliArguments.cpp`。不要在 `main.cpp` 加分支，也不要让 CLI 直接 include `src/.../detail/...`。

## 当前特征检测模块拆分

```text
src/feature_detection/FeatureDetector.cpp          FeatureDetector pimpl、公开入口、pipeline stage 编排和 CSV 小工具
src/feature_detection/FeatureEvidence.cpp          boundary/dihedral/non-manifold/normal-tensor 边证据策略
src/feature_detection/FeatureGraph.cpp             FeatureGraph 初始化、trace graph 构建和 junction/shared 标记
src/feature_detection/FeatureLoopRecovery.cpp      cycle/trace/primitive/circular recovery 编排
src/feature_detection/FeatureCycleRecovery.cpp     junction cycle 和小 cycle basis 恢复
src/feature_detection/FeatureTraceRecovery.cpp     feature graph 上的 open chain / closed loop 追踪
src/feature_detection/FeaturePrimitiveRecovery.cpp primitive component 兜底恢复
src/feature_detection/FeatureLoopBuilder.cpp       loop 构造、vertex ownership、切向和 primitive 数据写回
src/feature_detection/FeatureCircularRecovery.cpp  有界圆形顶点簇 fallback 恢复
src/feature_detection/NormalTensor.cpp             normal-tensor 特征评分公开函数实现
src/feature_detection/PrimitiveFit.cpp             circle/near-circle/ellipse primitive 拟合和误差度量
src/feature_detection/detail/*.h                   feature 检测私有类型、策略接口和 helper 声明
```

特征检测已经是与 QEM 简化平级的模块。它不能反向依赖 `src/simplification/`；简化、验证、修复或未来重网格模块可以消费 `FeatureAnalysis`。新增特征识别时优先判断它是新的边证据来源、图/loop 恢复阶段，还是 primitive 解释器：边证据进入 `FeatureEvidence.cpp`，junction/cycle 进入 `FeatureCycleRecovery.cpp`，trace chain 进入 `FeatureTraceRecovery.cpp`，component 兜底进入 `FeaturePrimitiveRecovery.cpp` 或专属 recovery 文件，primitive 拟合进入 `PrimitiveFit.cpp`。公共 C++ API 的真实命名空间按功能拆分：`manumesh::feature` 承载检测器、特征选项和分析结果，`manumesh::simplification` 承载 QEM/line-quadrics 简化器、简化选项、报告和指标；根 `manumesh` 只承载 core 类型和基础工具，不再导出功能模块别名。

## include 规则

公共代码使用完整 SDK 路径：

```cpp
#include "algorithms/simplification/QEMSimplifier.h"
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

模块变大时按职责拆分，而不是按公式片段随意拆文件。新的稳定算法契约进入 `include/algorithms/<domain>/`；长期存在但不稳定的实现 helper 进入所属模块的 `detail/`；跨模块工具先放在 `src/common/detail/`，等它的语义足够稳定再决定是否提升到公共 `core`。新增 `repair` 这类平级模块时，先按 [`common_foundation.md`](common_foundation.md) 判断哪些 mesh 基础能力应沉淀到 common，再按 [`adding_new_algorithm.md`](adding_new_algorithm.md) 的 checklist 同步公共头、实现、测试、CLI/C API 和 SDK 验证。

公共对象优先使用 pimpl 隐藏实现存储。只有调用方必须直接读写、且语义稳定的数据交换结构才进入公共头，例如 options、reports 和 analysis results。

新增算法文档应同时说明四件事：要解决的几何现象、核心数学量、当前代码入口和论文出处。缺少其中任意一项时，不应把它写成已稳定能力。
