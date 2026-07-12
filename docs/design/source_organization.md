# 源码组织说明

ManuMesh 采用小型几何内核布局：公共 SDK 头文件和实现文件分离，跨算法复用工具放在私有 common 层，算法专属 helper 放在各自模块的 `detail/` 目录中。

算法为什么按这些边界拆分，见 [`algorithm_essence.md`](algorithm_essence.md)。common 内部基础库如何持续补强，见 [`common_foundation.md`](common_foundation.md)。新增平级算法模块的完整流程，见 [`adding_new_algorithm.md`](adding_new_algorithm.md)。本文只规定源码目录、include 和扩展规则。

## 目录契约

| 路径 | 角色 | 规则 |
| --- | --- | --- |
| `include/` | 安装级 SDK 根目录 | 只放稳定公共头。 |
| `include/core/` | Mesh、句柄、状态、拓扑缓存 | 外部应用可直接 include。 |
| `include/io/` | STL/OBJ 网格读写 API | 文件格式边界属于 io，不让 `core` 承担解析细节。OBJ 读取支持多边形自动三角化并保留逐角 `vt` 纹理坐标。 |
| `include/algorithms/feature_detection/` | 特征检测 API、结果类型和对象入口 | 与 QEM 简化平级，只依赖 core。 |
| `include/algorithms/simplification/` | QEM 简化选项、报告、指标、对象入口 | 当前主要 decimation 模块。 |
| `include/algorithms/<domain>/` | 未来平级算法 API，例如 `repair`、`remeshing` | 只放稳定 options/result/facade，不放 pipeline 私有状态。 |
| `include/api/` | C ABI | 不暴露 STL、Eigen 或 C++ 异常。 |
| `src/common/` | 跨算法私有实现工具 | 只能被库内部使用，不安装。 |
| `src/common/detail/` | 私有公共头 | 放多个算法共享但尚不稳定的 mesh 查询、key/hash、几何谓词、空间索引、距离索引、边界 loop 等工具。 |
| `src/mesh_edit/` | 跨算法私有编辑内核 | 活动面、动态邻接、compact/remap；供 simplification 和未来 remeshing/repair 复用。 |
| `src/mesh_edit/detail/` | mesh edit 私有头 | 不安装，不包含 QEM 或具体算法策略。 |
| `src/core/` | 基础数据结构实现 | 对应公共 core 头。 |
| `src/io/` | 网格文件读写实现 | 对应 `include/io/`。 |
| `src/feature_detection/` | 特征检测实现 | 对应 `FeatureDetector` 算法模块，不能依赖 simplification。 |
| `src/feature_detection/detail/` | 特征检测私有 helper | primitive fitting、trace graph、loop recovery 等不稳定细节。 |
| `src/simplification/` | 简化算法实现 | 可以消费 `FeatureAnalysis`，但不反向污染 feature detection。 |
| `src/simplification/detail/` | 简化专属私有头 | 只能被简化实现使用，不安装。 |
| `src/debugUtil/` | Debug-only HTML wireframe 辅助工具 | 默认关闭，只给内部实现调试使用。 |
| `src/<domain>/` | 未来平级算法实现，例如 `repair` | 按 public facade、validation、run、stage 拆分。 |
| `src/<domain>/detail/` | 未来平级算法私有头 | 跨模块复用前先判断是否属于 common。 |
| `apps/manumesh/` | CLI | 像外部消费者一样调 SDK，命令实现走 command registry。 |
| `examples/` | SDK 使用示例 | 只 include 公共头。 |
| `tests/` | 回归和验证测试 | `support/` 放公共测试辅助，`unit/` 按功能分单元测试。 |
| `docs/` | 设计、指南、论文、生成笔记 | 必须描述当前代码，不写成愿景幻灯片。 |

## 当前公共私有层

```text
src/common/GeometryPredicates.cpp          跨算法几何谓词实现
src/common/detail/GeometryPredicates.h     三角形质量、点到三角形距离、AABB 距离、三角形包围盒、三角形相交
src/common/MeshDistanceIndex.cpp           跨算法 mesh 表面 BVH 距离查询实现
src/common/detail/MeshDistanceIndex.h      MeshDistanceIndex 私有声明
src/common/MeshQueries.cpp                 跨算法 mesh 查询实现
src/common/detail/MeshQueries.h            无向边 key、面 key、边-面邻接、面法向、顶点邻接、边界顶点
src/common/SpatialIndex.cpp                跨算法 AABB 候选 uniform grid 实现
src/common/detail/SpatialIndex.h           CellCoord、hash、UniformAabbCandidateGrid、queryCandidates、overflow 管理
src/common/detail/MathConstants.h          数学常量
```

这层不是 SDK 合约。外部代码不应 include `src/common/detail/...`；如果某个概念已经足够稳定，应提升到 `include/core/` 或新的公共算法模块。

## 当前简化模块拆分

```text
src/simplification/QEMSimplifier.cpp            公共对象 API、pimpl 和 simplifyMesh 包装
src/simplification/SimplificationRun.cpp        单次运行编排和 collapse loop
src/simplification/SimplificationPolicies.cpp   公开 options 到内部 policy 的转换
src/simplification/CollapseAttempt.cpp          单个候选坍缩的 feature/boundary/budget/legality 评估
src/simplification/Quadrics.cpp                 面 quadric、line quadric、placement 求解
src/simplification/FeatureConstraints.cpp       特征曲线策略、预算和投影
src/simplification/CandidateQueue.cpp           折叠候选优先队列
src/simplification/CollapseTopology.cpp         boundary policy、link condition 与通用动态拓扑的适配
src/simplification/SimplificationValidation.cpp 选项和输入校验
src/simplification/Metrics.cpp                  质量、采样和统计指标；距离查询复用 common MeshDistanceIndex
src/simplification/QualityRefinement.cpp        可选固定拓扑质量精修轮（启用纹理保护时暂时跳过）
src/simplification/CollapseLegality.cpp         拓扑、边界、质量、法线、自交过滤；几何谓词复用 common
src/simplification/SpatialFaceIndex.cpp         面片 AABB 到 common UniformAabbCandidateGrid 的适配器
src/simplification/TextureProtection.cpp        opt-in 纹理保护：局部 UV chart 配对、有符号 UV 面积检查和标量失真代价
src/simplification/detail/TextureProtection.h   纹理保护私有接口，不安装
```

## 当前 mesh_edit 模块拆分

```text
src/mesh_edit/MeshCompaction.cpp             活动编辑状态压缩回 Mesh，并返回 vertex/face remap
src/mesh_edit/DynamicTopology.cpp             通用运行时 face incidence、活动边和 duplicate-face 缓存
src/mesh_edit/detail/MeshEditTypes.h          EditableFace 等稳定索引编辑记录
src/mesh_edit/detail/MeshCompaction.h         compaction 输入输出合约
src/mesh_edit/detail/DynamicTopology.h        动态拓扑私有接口
```

这里是未来 remeshing 的复用入口，不是公共 SDK。操作的几何/拓扑提交属于 `mesh_edit`；QEM cost、feature policy、目标边长、平滑或投影策略分别留在所属算法模块。

新增简化能力时先判断类型：排序/placement 成本进入 `Quadrics.cpp`，公开 options 到内部策略的转换进入 `SimplificationPolicies.cpp`，单个候选是否接受进入 `CollapseAttempt.cpp` 或它调用的 policy/filter，状态应用和队列推进才留在 `SimplificationRun.cpp`。

## 当前特征检测模块拆分

```text
src/feature_detection/FeatureDetector.cpp          FeatureDetector pimpl、公共入口、pipeline stage 编排和 CSV 小工具
src/feature_detection/FeatureEvidence.cpp          boundary/dihedral/non-manifold/normal-tensor/smooth-curvature 边证据策略组合
src/feature_detection/SmoothCurvature.cpp          opt-in 确定性光滑曲率证据：多尺度鲁棒 quadric 拟合、带符号主曲率、方向极值和 persistence
src/feature_detection/FeatureGraph.cpp             FeatureGraph 初始化、trace graph 构建和 junction/shared 标记
src/feature_detection/FeatureLoopRecovery.cpp      cycle/trace/primitive/circular recovery 编排
src/feature_detection/FeatureCycleRecovery.cpp     junction cycle 和小 cycle basis 恢复
src/feature_detection/FeatureTraceRecovery.cpp     feature graph 上的 open chain / closed loop 追踪
src/feature_detection/FeaturePrimitiveRecovery.cpp primitive component 兜底恢复
src/feature_detection/FeatureLoopBuilder.cpp       loop 构造、vertex ownership、切向和 primitive 数据写回
src/feature_detection/FeatureCircularRecovery.cpp  有界圆形顶点簇 fallback 恢复
src/feature_detection/NormalTensor.cpp             normal-tensor 特征评分，复用 common 局部边长尺度
src/feature_detection/PrimitiveFit.cpp             circle/near-circle/ellipse primitive 拟合和误差度量
src/feature_detection/detail/*.h                   feature 检测私有类型、策略接口和 helper 声明
```

特征检测是与 QEM 简化平级的算法模块。它不能反向依赖 `src/simplification/`；简化、验证、修复或未来重网格模块可以消费 `FeatureAnalysis`。

## 当前 CLI 拆分

```text
apps/manumesh/main.cpp             只保留进程入口，调用 manumesh::cli::run()
apps/manumesh/CliArguments.cpp     通用 flag/value 解析和位置参数提取
apps/manumesh/ManuMeshCli.cpp      帮助输出和 run() 派发
apps/manumesh/ManuMeshCommands.cpp 基础简化、参数扫描和 command registry
apps/manumesh/ManuMeshFeatureCommands.cpp 特征报告、特征基准和简化前后特征对比命令
apps/manumesh/ManuMeshWorkflowCommands.cpp demo、特征验证和外部模型验证工作流
apps/manumesh/CliCsv.cpp           CSV 文件写出辅助
apps/manumesh/CliOptionBinding.cpp CLI 参数到算法 options 的绑定
```

新增特征分析命令放入 `ManuMeshFeatureCommands.cpp`，其他命令按所属工作流拆分后注册到 `commandRegistry()`；新增通用参数才改 `CliArguments.cpp`。不要在 `main.cpp` 加分支，也不要让 CLI 直接 include `src/.../detail/...`。

## include 规则

公共代码使用完整 SDK 路径：

```cpp
#include "algorithms/simplification/QEMSimplifier.h"
#include "core/Mesh.h"
```

库内部跨模块私有工具使用 `src` 私有 include 根：

```cpp
#include "common/detail/MeshQueries.h"
#include "common/detail/SpatialIndex.h"
#include "mesh_edit/detail/MeshCompaction.h"
```

算法模块内部 helper 可以使用本模块的 `detail/`：

```cpp
#include "detail/CollapseTopology.h"
```

不要从公共头、示例、CLI 或外部工程 include `src/.../detail/...`。如果外部确实需要某个概念，先判断它是否稳定；稳定则提升为公共类型，不稳定则通过 options、reports 或函数结果暴露行为。

`tests/support/check_include_boundaries.py` 已接入 CTest，通过集中定义的模块依赖表检查 public/apps/examples 不越界，并约束所有 `src` 模块的依赖方向；新增源码模块必须先声明其允许依赖。`include_boundary_checker_tests` 会同时验证这套守卫的关键允许/拒绝场景。

## 扩展规则

模块变大时按职责拆分，而不是按公式片段随意拆文件。新的稳定算法契约进入 `include/algorithms/<domain>/`；长期存在但不稳定的实现 helper 进入所属模块的 `detail/`；无状态几何/查询工具放在 `src/common/detail/`，可变拓扑、编辑状态和 remap 放在 `src/mesh_edit/detail/`。等语义足够稳定后再决定是否提升到公共 `core`。

新增 `repair` 这类平级模块时，先按 [`common_foundation.md`](common_foundation.md) 判断哪些 mesh 基础能力应沉淀到 common，再按 [`adding_new_algorithm.md`](adding_new_algorithm.md) 的 checklist 同步公共头、实现、测试、CLI/C API 和 SDK 验证。

公共对象优先使用 pimpl 隐藏实现存储。只有调用方必须直接读写、且语义稳定的数据交换结构才进入公共头，例如 options、reports 和 analysis results。

新增算法文档应同时说明四件事：要解决的几何现象、核心数学量、当前代码入口和论文出处。缺少其中任意一项时，不应把它写成已稳定能力。
