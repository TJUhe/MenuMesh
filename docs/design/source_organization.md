# 源码组织说明

Tessellix 采用小型几何内核布局：公共 SDK 头文件和实现文件分离，跨算法复用工具放在私有 common 层，算法专属 helper 放在各自模块的 `detail/` 目录中。

## 目录契约

| 路径 | 角色 | 规则 |
| --- | --- | --- |
| `include/line_quadrics_qem/` | 安装级 SDK 根目录 | 只放稳定公共头。 |
| `include/line_quadrics_qem/core/` | Mesh、句柄、状态、拓扑缓存 | 外部应用可直接 include。 |
| `include/line_quadrics_qem/algorithms/feature_detection/` | 特征检测 API、结果类型和对象入口 | 与 QEM 简化平级，只依赖 core。 |
| `include/line_quadrics_qem/features/` | 旧特征检测 include 兼容层 | 保留给旧调用方，新代码不要继续使用。 |
| `include/line_quadrics_qem/algorithms/simplification/` | QEM 简化选项、报告、指标和入口 | 当前主要 decimation 模块。 |
| `include/line_quadrics_qem/api/` | C ABI | 不暴露 STL、Eigen 或 C++ 异常。 |
| `src/common/` | 跨算法私有实现工具 | 只能被库内部使用，不安装。 |
| `src/common/detail/` | 私有公共头 | 放多个算法共享但尚不稳定的 mesh 查询、key、hash 等工具。 |
| `src/core/` | 基础数据结构实现 | 与公共 core 头对应。 |
| `src/feature_detection/` | 特征检测实现 | 对应 `FeatureDetector` 算法模块。 |
| `src/simplification/` | 简化算法实现 | 公开薄入口和拆分后的内部模块。 |
| `src/simplification/detail/` | 简化专属私有头 | 只能被简化实现模块使用，不安装。 |
| `apps/linequadrics/` | CLI | 像外部消费者一样调用库。 |
| `examples/` | SDK 使用示例 | 只 include 公共头。 |
| `tests/` | 回归和验证测试 | 优先走公共 API，少量白盒测试可用内部细节。 |
| `docs/` | 设计、指南、论文、生成笔记 | 必须描述当前代码，不写成愿景幻灯片。 |

## 当前公共私有层

```text
src/common/MeshQueries.cpp                 跨算法 mesh 查询实现
src/common/detail/MeshQueries.h            无向边 key、面 key、边-面邻接、面法向、顶点邻接、边界顶点
```

这层不是 SDK 合约。外部代码不得 include `src/common/detail/...`；如果某个概念已经足够稳定，应提升到 `include/line_quadrics_qem/core/` 或新的公共算法模块。

## 当前简化模块拆分

```text
src/simplification/QEMSimplifier.cpp          公共对象 API、pimpl 和 simplifyMesh 包装
src/simplification/SimplificationRun.cpp      单次运行编排和 collapse loop
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
src/feature_detection/FeatureDetector.cpp     FeatureDetector pimpl、便捷函数、特征图追踪和 primitive 拟合
```

特征检测已经是与 QEM 简化平级的模块。它不能反向依赖 `src/simplification/`；简化、验证、修复或未来重网格模块可以消费 `FeatureAnalysis`。

## include 规则

公共代码使用完整 SDK 路径：

```cpp
#include "line_quadrics_qem/algorithms/simplification/QEMSimplifier.h"
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

模块变大时按职责拆分，而不是按公式片段随意拆文件。新的稳定算法契约进入 `include/line_quadrics_qem/algorithms/<domain>/`；长期存在但不稳定的实现 helper 进入所属模块的 `detail/`；跨模块工具先放在 `src/common/detail/`，等它的语义足够稳定再决定是否提升到公共 `core`。

公共对象优先使用 pimpl 隐藏实现存储。只有调用方必须直接读写、且语义稳定的数据交换结构才进入公共头，例如 options、reports 和 analysis results。
