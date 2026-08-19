# 项目导览：主流程、大网格与 oneTBB

本文面向第一次接触 ManuMesh 源码的开发者。它只描述当前实现，帮助先建立正确的
心智模型，再进入具体算法文件。

最重要的结论只有两条：

1. ManuMesh 的主产品是内存中的三角表面网格处理内核：读入 Mesh，做特征检测、分析或
   受约束简化，再导出结果。
2. MMPD 大网格模块和 oneTBB 是两条独立支线。前者解决二进制 STL 三角记录的有界内存
   暂存与校验；后者只并行内存算法中的独立范围计算。二者目前没有组合成跨分块、
   out-of-core 的特征检测或 QEM 简化器。

## 先看全局图

~~~text
常规内存处理链

 STL / OBJ
     |
     v
 MeshIo -> Mesh -----------------------------------+
     |                                             |
     +-> MeshTopology（按需构建的只读拓扑快照）     +-> MeshAnalysis -> 统计 / 距离 / CSV
     |
     +-> FeatureDetector -> FeatureAnalysis --+
     |                                         |
     +-> QEMSimplifier <-----------------------+
                 |
                 v
          Mesh -> STL / OBJ / CSV

独立的大网格存储支线

 binary STL -> large-import -> MMPD 文件 -> Reader / large-validate
                                      |
                                      +-> 当前不构建 Mesh，也不进入特征检测或简化
~~~

Mesh 是核心几何对象。MeshTopology 是按需从 Mesh 构建、独立拥有稠密缓存的只读快照；
它不会由 I/O 自动创建，也不会随之后的 Mesh 修改自动更新。FeatureAnalysis 是可选的特征
结果，能被简化器消费；特征检测本身不依赖简化器。完整模块依赖方向见
[架构与模块边界](../design/architecture.md)，算法步骤见
[当前算法管线](../design/algorithms.md)。

## 模块地图

| 你想理解的事 | 从这里开始 | 接着读 |
| --- | --- | --- |
| 网格数据、拓扑和校验 | [Mesh.h](../../include/core/Mesh.h)、[MeshTopology.h](../../include/core/MeshTopology.h) | [src/core](../../src/core)、[公共契约](../design/contracts.md) |
| STL / OBJ 的读写 | [MeshIo.h](../../include/io/MeshIo.h) | [MeshIo.cpp](../../src/io/MeshIo.cpp) |
| 特征检测的公开入口 | [FeatureDetector.h](../../include/algorithms/feature_detection/FeatureDetector.h) | [FeatureDetector.cpp](../../src/feature_detection/FeatureDetector.cpp) |
| 特征检测的内部阶段 | [FeatureEvidence.cpp](../../src/feature_detection/FeatureEvidence.cpp) | graph、loop、primitive 与 segmentation 相关文件 |
| QEM / line quadrics 简化 | [QEMSimplifier.h](../../include/algorithms/simplification/QEMSimplifier.h) | [SimplificationRun.cpp](../../src/simplification/SimplificationRun.cpp) |
| 候选和坍缩合法性 | [CandidateQueue.cpp](../../src/simplification/CandidateQueue.cpp) | [CollapseLegality.cpp](../../src/simplification/CollapseLegality.cpp)、[CollapseTopology.cpp](../../src/simplification/CollapseTopology.cpp) |
| 分析与度量 | [MeshAnalysis.h](../../include/algorithms/analysis/MeshAnalysis.h) | [MeshAnalysis.cpp](../../src/analysis/MeshAnalysis.cpp) |
| CLI 如何拼接工作流 | [ManuMeshCommands.cpp](../../apps/ManuMeshCommands.cpp) | [CLI 使用说明](cli.md) |
| C ABI | [CApi.h](../../include/api/CApi.h) | [CApi.cpp](../../src/api/CApi.cpp) |

公共 SDK 都在 include；src/common、src/mesh_edit 和各模块的 detail 目录是私有实现。
从应用或外部集成视角先读公共头；从算法改造视角再沿着相应的 cpp 进入私有细节，能少走
很多弯路。

## 如何使用这份导览

如果只想先建立项目印象，依次读全局图、核心数据模型、特征检测、简化、大型网格和 oneTBB
六节即可。它们描述当前运行时真正存在的主链与两条独立支线。

如果要开始改代码，按这个节奏推进：

1. 先选一个实际 CLI 命令或 SDK 调用，沿公开头和实现反向跟踪。
2. 再读对应模块的阶段编排文件，而不是从一个局部数学函数开始。
3. 最后读同模块的单元测试，确认输入边界、确定性和失败语义。

本文使用几个约定：

- include 表示对 SDK consumer 可见的公共头；
- src/common、src/mesh_edit 和每个 detail 目录表示私有实现；
- 流程图中的箭头表示数据或调用依赖，不表示线程、文件格式或对象所有权；
- 描述为当前没有的能力时，表示不能从现有实现、测试或 API 推导该能力。

## 核心数据模型与契约

### Mesh、PlainMesh 与拓扑快照

主算法输入是 [Mesh.h](../../include/core/Mesh.h) 中的 Mesh。它直接拥有双精度顶点、
三角面和可选逐面逐角 UV；面内索引顺序决定法向方向，库不会静默统一绕序。UV 归属于面角，
同一几何顶点可在不同图表中拥有不同纹理坐标。

[PlainMesh.h](../../include/core/PlainMesh.h) 提供不暴露 Eigen 的纯 C++ 交换容器。它不是第二套
算法数据模型，而是通过 toMesh 和 toPlainMesh 做显式深拷贝转换；算法内部仍使用 Mesh。

[MeshTopology.h](../../include/core/MeshTopology.h) 则是从某一时刻的 Mesh 按需构建的只读快照。
它独立拥有无向边与入射关系缓存，不持有或观察原始 Mesh。修改 Mesh 后，已有拓扑快照不会
自动更新，必须重新构建。

~~~text
Mesh M0 -- MeshTopology::build --> Topology snapshot T0
  |
  +-- 编辑为 M1 后，T0 仍描述 M0；为 M1 重新构建 T1
~~~

Topology 的连通分量按共享完整边计算，不是只共享顶点。TopologyIndexView、TopologyEdgeView
和 VertexTopologyView 都借用 MeshTopology 的连续存储，不能超过其生命周期。

简化器使用的动态编辑拓扑是私有的
[DynamicTopology.h](../../src/mesh_edit/detail/DynamicTopology.h)：它在坍缩期间标记不活动
顶点和面，最后才压缩为新的稠密 Mesh。不要把它当作 MeshTopology 的可变 SDK 版本。

### 输入校验、退化面与错误模型

| 场景 | 当前契约 | 主要入口 |
| --- | --- | --- |
| 索引检查 | 每个面索引必须指向已有顶点 | validateMeshIndices |
| 严格几何 | 拒绝非有限坐标、重复顶点索引、无效 UV 和零面积面 | validateMeshGeometry；STL 写出使用它 |
| 分析和简化 | 仍拒绝无效索引、非有限值、重复索引和无效 UV，但允许零面积面 | validateMeshGeometryLenient |
| 常规 STL / OBJ I/O | 返回 bool 和可选诊断，失败不替换输出 Mesh 或截断已有目标文件 | [MeshIo.h](../../include/io/MeshIo.h) |
| 值型拓扑和大网格操作 | 返回稳定 Status 或 Result | [Status.h](../../include/core/Status.h) |
| C++ 程序员错误 | 无效配置、来源身份不匹配等会抛标准异常 | 各公开头的前置条件 |
| C ABI | 异常不会穿过 ABI，失败转为 ManuMeshStatus | [CApi.h](../../include/api/CApi.h) |

宽松入口允许零面积面，是为了让分析和简化能报告脏输入而不是完全失去诊断机会；这不表示退化面
拥有正常几何语义。特征检测会跳过不可用法向的贡献，简化报告会记录退化输入并使用合法性过滤
阻止退化继续扩散。严格 STL 导出不能写出这样的网格。

### FeatureAnalysis 是带来源身份的结果

特征检测产生的 [FeatureAnalysis](../../include/algorithms/feature_detection/FeatureTypes.h) 包含逐顶点
特征归属、显式图、曲线或环、组件、诊断和可选面片分区。它不是可以随意搬到相似网格上的
提示对象：

~~~text
顶点坐标 + 顶点/面数量 + 面和面角的索引顺序
                     |
                     v
         FeatureAnalysisSource 的确定性指纹
                     |
                     v
简化器复用分析前验证精确 indexed geometry
~~~

重排顶点或面、改变任一顶点坐标、替换连接关系，即使视觉形状看起来等价，都要求重新检测。
逐角 UV 有意不参与来源指纹，但纹理保护会另行检查图表和局部面积。FeatureAnalysis 的
普通只读消费者应使用 [FeatureAnalysisViews.h](../../include/algorithms/feature_detection/FeatureAnalysisViews.h)
中按证据、曲线、分区和诊断分组的借用 view。

还有两个很重要的细节：

- featureEdges 只统计原始局部证据边；graph.edges 还可能包含 cleanup 或 consolidation 产生的
  合成桥接边，因此两个数量并不必然相等；
- 手工修改 FeatureAnalysis 的公开字段后，不能假定它仍能安全交给简化器。
  [FeatureAnalysisValidation.cpp](../../src/feature_detection/FeatureAnalysisValidation.cpp) 会校验
  来源、图、loop、component、patch 和 tensor 权重等内部一致性。

### 四条对外调用边界

| 边界 | 适合谁 | 关键规则 |
| --- | --- | --- |
| C++ SDK | 同编译器、同 C++ ABI 的应用 | 链接 ManuMesh::manumesh，只 include include 下的头 |
| PlainMesh | 不希望在宿主 API 暴露 Eigen 的 C++ 应用 | 用纯数据容器显式转换，算法内部仍用 Mesh |
| C ABI v1 | 跨语言、插件或稳定二进制边界 | 结构体带 abi_version 和 struct_size，变长输出先查询容量 |
| CLI | 批处理、实验和人工诊断 | 参数校验、报告和工作流在 apps，算法仍在库中 |

C ABI 的 context 和 mesh handle 必须区分。context 只保存最近错误文本和后续算法调用使用的
ExecutionOptions；它不拥有 mesh handle。每个 handle 独立创建、独立销毁，并持有自己的
mutex。context 不保证线程安全；需要跨线程使用时应提供外部同步、为每个线程使用独立 context，
或在不需要错误文本时传入 null。销毁 context 或 handle 前，调用方必须等待针对该对象的在途
调用完成。

ABI 的可变长数组采用先查询容量、后复制的模式，容量不足时不会部分写入；带大小的 options、
report 和 stats 可按兼容前缀读写。C++ 详细集成、C ABI 生命周期和安装后 consumer 见
[SDK 集成指南](sdk.md)。

## 主流程应怎样读

建议按下面的顺序阅读，而不是直接跳入最复杂的 QEM 文件。

1. 先读根 [README](../../README.md) 和 [架构与模块边界](../design/architecture.md)，确认
   模块职责和依赖方向。
2. 阅读 [Mesh.h](../../include/core/Mesh.h)、[PlainMesh.h](../../include/core/PlainMesh.h) 与
   [MeshTopology.h](../../include/core/MeshTopology.h)，理解顶点、三角面、拓扑 view 和
   输入校验的边界。
3. 从 [ManuMeshCommands.cpp](../../apps/ManuMeshCommands.cpp) 选择一个 CLI 命令，反向跟到
   公共 API。这样能看到真正对用户生效的参数和报告，而不是只看到内部工具。
4. 读 [FeatureDetector.cpp](../../src/feature_detection/FeatureDetector.cpp) 的阶段编排，再按需
   深入证据、图清理、环恢复和 primitive 拟合。
5. 读 [QEMSimplifier.cpp](../../src/simplification/QEMSimplifier.cpp) 与
   [SimplificationRun.cpp](../../src/simplification/SimplificationRun.cpp)，然后再进入 quadric、
   placement、候选队列和坍缩合法性。
6. 最后用对应单元测试确认边界。测试既说明预期结果，也说明哪些失败被视为正常拒绝。

简化的核心分工尤其值得记住：quadric 和 placement 决定“优先尝试哪个候选、放在哪里”；
legality 和动态拓扑决定“这个拓扑改动能否提交”。因此目标面数或比例未达到时，应先看
termination reason 与拒绝计数，而不是把它直接理解为排序器失效。

## 特征检测：从局部证据到可复用语义

特征检测的公开门面是
[FeatureDetector.h](../../include/algorithms/feature_detection/FeatureDetector.h)。它独立于简化器：
输入一个 Mesh，输出 FeatureAnalysis；简化器可以消费该结果，但特征模块不反向依赖简化。

### 运行阶段

[FeatureDetector.cpp](../../src/feature_detection/FeatureDetector.cpp) 是最适合建立整体印象的入口。
一次完整检测按固定顺序执行：

~~~text
输入宽松校验
  -> 建立边信息、面法向、局部尺度和邻接缓存
  -> 可选 FeatureNormalFilter 稳定面法向
  -> 收集 boundary / non-manifold / 有向二面角 / Normal Tensor 证据
  -> 初始化显式 FeatureGraph
  -> 清理弱 spur、桥接小间隙、可选 component consolidation
  -> trace、cycle、loop 恢复
  -> 对独立 component 拟合 circle / near-circle / ellipse / polygon
  -> 汇总 component、junction、诊断
  -> 可选用真实 mesh-edge 屏障切分 surface patch
~~~

每一步的输入和输出并不等价：

| 阶段 | 主要产物 | 应读的实现 |
| --- | --- | --- |
| 法向和局部缓存 | 面法向、边表、邻接、局部尺度 | FeatureDetectionCache、FeatureNormalFilter |
| 证据收集 | boundary、non-manifold、二面角和 tensor 标记 | [FeatureEvidence.cpp](../../src/feature_detection/FeatureEvidence.cpp) |
| 图处理 | FeatureGraph、弱证据清理、兼容桥接 | FeatureGraphCleanup、FeatureGraphConsolidation |
| 曲线恢复 | trace、cycle、loop、junction 分支 | FeatureLoopRecovery、FeatureTraceRecovery |
| 基元恢复 | 圆、近圆、椭圆或 polygon 拟合 | FeaturePrimitiveRecovery、PrimitiveFit |
| 分区 | FeaturePatch 及邻接 | FeatureSegmentation |

FeatureNormalFilter 只改变本次检测使用的面法向，不移动 Mesh 顶点，也不改变输入拓扑。
Normal Tensor 是局部、多尺度的弱证据通道，不是通用曲率、ridge 或 valley 实现。输入中的
退化面可被记录，但没有可靠法向时不会贡献逐面证据。

### 怎样理解输出

FeatureAnalysis 可以按五层阅读：

1. 局部证据：FeatureGraphEdge 标记离散边界、非流形、二面角或 Normal Tensor 来源。
2. 显式图：FeatureGraph 保存边、顶点、端点、连接点和分支连续配对。
3. 曲线与基元：FeatureLoop 保存拓扑序列、是否闭合以及圆、近圆、椭圆等拟合参数和残差。
4. 组件与分区：FeatureComponent 汇总置信度和证据构成；可选 FeaturePatch 表达特征边分隔的
   面区域及 patch 邻接。
5. 诊断与来源：退化面数、清理行为、tensor 统计、绕序问题、来源指纹和逐顶点 tensor 权重。

cleanup 或 consolidation 产生的桥接边可以存在于图中，却不一定是输入 Mesh 的真实边。它们帮助
恢复和解释曲线，不应自动被理解为可用于任意拓扑约束的几何边；surface patch 只以相应的真实
mesh-edge 屏障建立。

### 配置与典型读法

[FeatureOptions.h](../../include/algorithms/feature_detection/FeatureOptions.h) 按证据、清理和可选
阶段组织配置。FeatureProfile 的 Default、Cad 和 NoisyScan 只是为已有证据路径选择起点，
不会锁定字段；返回配置后仍可逐项覆盖。Cad 强调离散二面角和环恢复，NoisyScan 打开法向
稳定与多尺度 tensor 路径。参数含义、上限和默认值应以头文件为准。

排查一次误检或漏检时，建议按以下顺序断点：

1. FeatureEvidence：某条边为何成为或没有成为证据；
2. FeatureGraphCleanup / FeatureGraphConsolidation：弱片段是否被裁剪或桥接；
3. FeatureLoopRecovery / FeaturePrimitiveRecovery：图如何变成 loop 和基元；
4. FeatureSegmentation：是否被真实特征边用作 patch 障碍；
5. FeatureAnalysis 的 diagnostics：问题是输入退化、绕序、阈值还是恢复上限。

覆盖该契约的主要测试在 tests/unit/feature_detection；并行等价测试见
[feature_detection_parallel_tests.cpp](../../tests/unit/feature_detection/feature_detection_parallel_tests.cpp)。
算法语义的简明版本见 [当前算法管线](../design/algorithms.md)。

## 简化：QEM 排序与硬约束提交

简化入口是 [QEMSimplifier.h](../../include/algorithms/simplification/QEMSimplifier.h)。它保持输入
Mesh 不变，并返回一个重新压缩过的新 Mesh；有状态对象会保存最近一次 SimplifyReport。

### 配置不等于一组权重

新代码应优先使用 [SimplificationTypes.h](../../include/algorithms/simplification/SimplificationTypes.h)
中的 SimplifyConfig，它把配置分成五组：

| 配置组 | 决定什么 |
| --- | --- |
| target | 以面数或输入面数比例表达目标 |
| cost | 标准 QEM、可选 line quadrics、候选排序权重 |
| features | 特征检测、曲线保护、投影和约束策略 |
| quality | 边界、三角质量、法向、局部误差、自交和固定拓扑精修 |
| texture | 可选逐角 UV 图表、面积和局部失真保护 |

旧的 SimplifyOptions 是兼容的平面选项结构；它通过 makeSimplifyOptions 适配为规范分组配置。
如果没有特殊兼容需求，不要在新代码中同时维护两套字段来源。

有一个刻意保留的兼容差异：直接构造 `SimplifyConfig{}` 时，最小特征环阈值仍为 16；
`makeSimplifyConfig(FeatureProfile::Default)` 使用当前特征检测默认的 8。CLI 的 profile 路径走后者。
因此复现 CLI 结果或切换到新配置 API 时，先确认自己选择的是哪条初始化路径。

QEM 和 line quadrics 只负责候选的代价和放置优先级。它们不保证候选一定能提交，也不替代
拓扑、边界、特征、质量、局部误差、自交或纹理约束。

### 一次简化怎样运行

[SimplificationRun.cpp](../../src/simplification/SimplificationRun.cpp) 调度整个生命周期：

~~~text
初始化报告和输入降级信息
  -> 可选地检测特征，或验证调用方传入的预计算 FeatureAnalysis
  -> 累积面平面、边界和可选特征 quadrics
  -> 初始化顶点状态、面状态和私有 DynamicTopology
  -> 收集活动边并构建按代价排序的候选队列
  -> 反复弹出当前最低代价候选
       -> 跳过过期候选
       -> 尝试多个 placement
       -> 通过硬合法性检查后提交局部坍缩
       -> 更新邻接、版本和受影响候选
  -> 到达目标、没有候选或达到拒绝上限时停止
  -> 可选固定拓扑质量精修
  -> 压缩 inactive 顶点和面，输出新的稠密 Mesh
~~~

候选队列的 tie-break 固定为 cost 后再按规范端点对排序；端点版本号使拓扑变化前的候选失效。
这解释了为什么简化不是一次全局最小化，而是一个有确定性协调顺序的局部编辑过程。

硬合法性主要覆盖：

- link condition、重复面和局部拓扑一致性；
- boundary 策略与 placement 投影；
- 特征曲线、基元约束和曲线偏离预算；
- 法向翻转、最小三角形质量和可选局部误差；
- 可选局部自交检测；
- 显式开启纹理保护时的 UV seam、图表、带符号面积和局部失真。

质量精修发生在边坍缩结束后的固定拓扑阶段；当前纹理保护激活时该精修会跳过，报告会说明原因。
纹理保护默认关闭，CLI 当前也没有启用它的开关。

### 预计算特征、报告与终止

当 feature 策略启用时，简化器可以自己检测，也可以复用精确对应输入的 FeatureAnalysis：

~~~text
Mesh -> FeatureDetector -> FeatureAnalysis -> simplify(input, features, report)
~~~

复用的价值是避免重复检测，并让多个简化策略共享同一份特征判断。代价是必须接受来源验证；
输入顶点、面及其顺序或坐标发生变化后，旧分析会被拒绝。Normal Tensor 权重模式还要求预计算
结果带有与输入顶点数对齐的 normalTensorVertexWeights。

SimplifyReport 应优先读三个部分：

1. 初始与最终顶点、面、坍缩数、队列重建和 solver fallback；
2. 运行开始时的特征摘要，帮助区分特征保护是否真的参与了运行；
3. terminationReason 和按首次硬过滤器归因的拒绝计数。

目标比例或目标面数不是无条件承诺。没有合法候选时会以 NoCandidates 停止；连续候选被拒绝到
上限时会以 RejectionLimit 停止。拒绝分类是每次失败的第一个归因，不应把所有字段机械相加
当成独立失败总数。

深入简化时，推荐按 Quadrics -> Placement -> CandidateQueue -> CollapseLegality /
CollapseAttempt -> CollapseTopology -> QualityRefinement 的顺序阅读。对应串并行等价测试是
[simplification_parallel_tests.cpp](../../tests/unit/simplification/simplification_parallel_tests.cpp)；
更广泛的策略、合法性和报告测试位于 tests/unit/simplification。

## 大型网格：MMPD 现在是什么

MMPD v1 的公共入口是
[PartitionedMeshDataset.h](../../include/io/PartitionedMeshDataset.h)。它是有界内存的、
顺序三角记录暂存格式，不是空间分区网格，也不是分块算法框架。

### 它实际保存了什么

导入器顺序读取二进制 STL 的固定 50-byte 三角记录，按输入顺序每 N 条记录关闭一个
partition。每个 partition 的目录元数据包括：

- 64 位全局三角形 ID 范围和 64 位 partition ID；
- 32 位范围内的分块局部索引；
- payload 的文件偏移、字节数和 FNV-1a checksum；
- 分块包围盒。

文件由数据集头、多个 partition block 和末尾目录组成；Writer 把目录暂存到磁盘 sidecar，
而不是把全部目录留在内存中。写入完成前目标文件不会被替换；只有 finish 成功后才发布
新文件。实现细节从 [PartitionedMeshDataset.cpp](../../src/io/PartitionedMeshDataset.cpp)
开始读，完整流式校验在
[PartitionedMeshValidation.cpp](../../src/io/PartitionedMeshValidation.cpp)。

### 文件格式、可寻址性与内存模型

MMPD v1 使用小端编码，文件布局固定为：

~~~text
[64-byte dataset header]
  repeated P times:
    [96-byte partition header][N * 50-byte binary-STL triangle records]
[16-byte directory header][P * 96-byte directory records]
~~~

每条 payload 记录保留 binary STL 的 normal、三个 float 顶点和 uint16 attribute；它不是经由
Mesh 去重后的顶点或边实体。提交完成的文件大小严格满足：

~~~text
80 + 50 * triangleCount + 192 * partitionCount bytes
~~~

全局 triangle ID 与 partition ID 都是 64 位，单个 partition 内的本地索引范围受 32 位限制。
源 binary STL 的标准三角形计数字段仍是 32 位；MMPD 的 64 位 ID 不会让该导入路径突破 binary
STL 输入格式本身的计数边界。

分区可以按 index 定位，但不是任意三角形随机访问索引。Reader 打开文件时会顺序扫描目录和
每个 partition 头来验证结构，所以打开成本随 partition 数量增长；它不会把全部目录常驻内存。
之后 partitionMetadata 按需读取单条目录记录，beginPartition 选择一个分块，readNextTriangle
顺序产出记录。

~~~text
空间复杂度：主要随 I/O buffer 变化，而不是随 T 或 P 线性增长
打开时间：需要 O(P) 的目录和 block-header 结构检查
读取语义：一个 partition 必须完整消费，才能切换到另一个
~~~

最后一条让 checksum 成为完整读完成的契约，而不是可选的尽力检查。普通 Reader 的 open 会检查
版本、尺寸、目录连续性、payload 范围和 block-header 一致性；实际 payload 的非有限值和
checksum 在完整顺序消费该 partition 时确认。large-validate 则完整读取全部 partition。

### 写入事务和校验分层

Writer 写入临时 dataset 文件与磁盘目录 sidecar。它在 finish 成功前不会替换请求的目标；成功时
追加目录、核对计数并发布新文件，失败或析构时清理临时产物。这个事务保证保护目标文件，
不等价于在掉电或文件系统不支持所需替换语义时提供数据库级别持久性保证。

| 检查层 | 能确认什么 | 不能确认什么 |
| --- | --- | --- |
| Reader::open | 格式版本、目录结构、范围、分块头一致性 | 每条 payload 的完整 checksum，除非把分块读完 |
| 读完整个 partition | 该 payload 的有限数值与 FNV-1a checksum | 全局流形、共享顶点、绕序或自交 |
| large-validate | 全体分块的上述检查，加计数、bounds、面积和退化三角形统计 | 拓扑有效性、连通性、法向语义或制造精度 |

FNV-1a checksum 的用途是普通传输或存储损坏检测，不是密码学防篡改承诺。已提交 MMPD 的尾随
字节会被拒绝；与之不同，binary STL 输入允许忽略尾随 padding。

默认预算和分块大小是：

| 参数 | 默认值 | 含义 |
| --- | --- | --- |
| maxResidentBytes | 256 MiB | 一个 Reader 或 Writer 的可变大小工作集预算 |
| ioBufferBytes | 4 MiB | 复用的顺序 I/O buffer |
| trianglesPerPartition | 1,000,000 | 自动关闭一个 partition 前最多的三角记录数 |

这里的有界内存是 API 管理的可变大小工作集约束，不等价于整个进程 RSS 的严格上限；文件流、
运行时与固定大小对象仍会占用内存。

配置会保证 I/O buffer 至少能容纳一个 50-byte 记录、不会超过声明预算，并检查本机的 size_t 和
streamsize 表示范围。导入时的输入 buffer 也受 Writer buffer 余量限制；预算很小时退回固定的
单条记录栈缓冲。因此这里应称为算法自有的可变大小工作缓冲预算，不能承诺进程 RSS 严格小于
memory-mib。

### 读写和校验的生命周期

~~~text
binary STL
  -> 流式解码记录
  -> Writer 按顺序写入 payload，目录写入 sidecar
  -> finish：追加目录、核对计数、替换目标 MMPD
  -> Reader：逐项读取目录，选择一个 partition
  -> 必须读完整个已选 partition，完成 checksum 后才能切换
~~~

最后一条是有意的完整性契约：Reader 不允许半读一个分块后跳到另一个分块，因为 checksum
必须覆盖完整的 payload 才有意义。

CLI 只提供两个与此格式对应的命令：

~~~powershell
manumesh large-import input.stl output.mmpd --memory-mib 256 --io-buffer-mib 4 --partition-triangles 1000000
manumesh large-validate output.mmpd --memory-mib 256 --io-buffer-mib 4
~~~

large-import 只接收二进制 STL，流式导入时不构造 Mesh。large-validate 会完整扫描所有分块，
复核目录、区间、block metadata、有限数值、checksum、包围盒、面积和退化三角形计数。
它是存储完整性与基础几何统计校验，不是流形、邻接、绕序或全局拓扑有效性校验。命令的参数
和默认值在 [ManuMeshLargeMeshCommands.cpp](../../apps/ManuMeshLargeMeshCommands.cpp)，完整
CLI 边界见 [CLI 使用说明](cli.md)。

large-import 支持 partition-triangles；large-validate 不接受它。两个命令都不注册 threads，
也不接收 ExecutionOptions，因此它们不会因为 Release 启用 oneTBB 而自动变成并行导入或并行
校验。MMPD 当前没有 C ABI 入口，只有 C++ API 与 CLI。

用于实际大文件操作时，建议采用以下闭环：

1. 对原始 binary STL 使用 large-import，并为机器内存和后续寻址习惯选择 memory-mib 与
   partition-triangles；
2. 立即运行 large-validate，确认完整发布后的文件、checksum 和基础统计；
3. 将 MMPD 视为可流式存取和再校验的 staging artifact，而不是已经可交给 simplify 或
   feature-report 的 Mesh；
4. 需要常规算法时，先确认数据规模能够安全进入内存 Mesh 路径，或另行实现真正的 out-of-core
   算法数据模型。

### 不应从 MMPD 推导出的能力

| 当前没有的能力 | 原因 |
| --- | --- |
| 空间分块、负载均衡分块 | partition 仅按输入记录顺序切分 |
| 全局 vertex / edge 表与跨块顶点去重 | v1 只保存独立三角记录 |
| owner / ghost / halo | 没有跨块拓扑实体 |
| 跨块 FeatureGraph、环恢复或 patch | 特征检测仍以完整内存 Mesh 为对象 |
| out-of-core QEM、坍缩或动态拓扑编辑 | 简化器尚未消费 MMPD |
| MMPD 与 oneTBB 的联合执行 | 导入、读取、校验当前是串行流式路径 |

若要建设真正的拓扑型 out-of-core 网格模块，应新建版本化实体 schema、邻接和跨块一致性
契约，而不能重新解释 MMPD v1 的三角记录。

一个合格的新版本至少要明确全局 vertex 和 edge ID、跨块去重、owner/ghost、halo、空间或负载
分区策略、跨块一致性更新、错误恢复和算法级验收。只有在这些实体和契约存在后，跨块
FeatureGraph、out-of-core QEM 或并行分区调度才有可实现的落点。

大网格阅读路径：

1. [PartitionedMeshDataset.h](../../include/io/PartitionedMeshDataset.h)：先读能力与限制。
2. [PartitionedMeshDataset.cpp](../../src/io/PartitionedMeshDataset.cpp)：格式、Writer、Reader 与
   binary STL 导入。
3. [PartitionedMeshValidation.cpp](../../src/io/PartitionedMeshValidation.cpp)：验证器真正检查什么。
4. [partitioned_mesh_dataset_tests.cpp](../../tests/unit/io/partitioned_mesh_dataset_tests.cpp)：小 buffer、
   多 partition、损坏文件和事务行为。
5. [partitioned_mesh_dataset_external_tests.cpp](../../tests/unit/io/partitioned_mesh_dataset_external_tests.cpp)：
   可选的大型外部数据集用例。

## oneTBB：并行化到哪里为止

公共并发契约定义在 [ExecutionOptions.h](../../include/core/ExecutionOptions.h)，内部适配层在
[ParallelExecution.h](../../src/common/detail/ParallelExecution.h) 和
[ParallelExecution.cpp](../../src/common/ParallelExecution.cpp)。

~~~text
FeatureDetector / QEMSimplifier / C API
                |
                v
        ExecutionOptions
                |
                v
 common::parallel::forEachRange
                |
                +-- oneTBB 可用：tbb::parallel_for + blocked_range + static_partitioner
                |
                +-- oneTBB 不可用或串行要求：同一回调串行执行
~~~

### 公共契约

| 字段 | 默认 / 语义 |
| --- | --- |
| mode | Serial；调用方必须显式请求 Parallel |
| maxConcurrency | 0 交给后端选择；1 等价于串行调度；正数限制本次调用的 worker 数 |
| minItemsPerTask | 默认 4096，用于避免小网格生成过多任务 |

公共 API 不暴露 oneTBB 类型，也不拥有线程池。内部实现只对不重叠的半开区间调用回调；回调
只能写入自己的输出槽，或由调用方自行同步。

Release preset 通过 vendored oneTBB 启用后端；Debug 与 ASan preset 默认没有该后端，适合作为
串行基线和调试环境。构建配置见 [CMakePresets.json](../../CMakePresets.json)，运行时可用
isParallelExecutionAvailable 和 parallelExecutionBackendName 查询已编译后端。

这里要区分库 API 与 CLI：

- C++ / C API 中，即使请求 Parallel 而构建没有 oneTBB，内部 range adapter 仍按相同算法
  契约串行执行。
- CLI 的 --threads 0 或大于 1 在没有 oneTBB 的构建中会直接拒绝；--threads 1 可作为显式
  串行运行。

### 哪些阶段可并行

| 模块 | 已并行的独立工作 | 必须保持串行协调的工作 |
| --- | --- | --- |
| 特征检测 | 法向滤波中的面积与双缓冲面法向更新、Normal Tensor 的逐顶点步骤、互不依赖的 primitive fit | MeshEdgeInfo 和边证据构建、FeatureGraph 清理和整合、trace / cycle / loop 恢复、全局排序与 patch 协调 |
| 简化 | 顶点和面的状态初始化、初始候选的独立 placement 与纹理检查 | 候选堆构建和提交、动态拓扑坍缩、邻接更新、接受顺序、质量精修 |

### 后端调用链和构建语义

全仓库只有 [ParallelExecution.cpp](../../src/common/ParallelExecution.cpp) 直接包含并调用 oneTBB；
算法模块只依赖内部范围适配接口：

~~~text
FeatureDetector / QEMSimplifier / C ABI context
                    |
                    v
             ExecutionOptions
                    |
                    v
 makeRangeExecutionOptions
                    |
                    v
 common::parallel::forEachRange
                    |
                    +-- 有 MANUMESH_HAS_ONETBB：
                    |     tbb::parallel_for(blocked_range, static_partitioner)
                    |
                    +-- 无后端、Serial 或 maxConcurrency = 1：
                          一次串行回调整个半开区间
~~~

当 maxConcurrency 大于 1 时，适配层会为这一次范围调用使用局部 task_arena 限制 worker 数；
零让 oneTBB 自己选择资源。minItemsPerTask 控制调度粒度，避免小网格生成过多任务。适配层
要求每个回调只写自己的不重叠输出区间，或由调用方自己同步共享状态。

构建时，根 CMakeLists 默认关闭 MANUMESH_ENABLE_ONETBB；普通 Release 和 Ninja Release 基础
preset 通过 vendored oneTBB 打开它，Debug、ASan 和 workspace 默认保持串行。oneTBB 是私有
实现依赖：公开头不包含 TBB 类型，SDK 调用方也无需以 TBB API 编写算法。运行时可用
isParallelExecutionAvailable 和 parallelExecutionBackendName 查询实际编译进库的后端。

库 API 和 CLI 的后端缺失行为不同：

| 入口 | 请求 Parallel 但构建不含 oneTBB |
| --- | --- |
| C++ 和 C ABI | 相同算法契约下串行回退 |
| CLI | threads 为 0 或大于 1 会拒绝；threads 为 1 允许显式串行 |

CLI 的 threads 只绑定 simplify、sweep、ratio-sweep、face-sweep、feature-report、
feature-benchmark 和 feature-compare；compare 和 large-import / large-validate 没有这个选项。

### 不只是“哪些模块并行”，而是哪些循环并行

特征检测中，很多看似可分的工作仍刻意保持串行：

- MeshEdgeInfo、面绕序协调、顶点邻接、局部尺度和边证据收集需要固定顺序或共享图状态；
- FeatureNormalFilter 的初始面法向和最终统计归约串行，但面积、绕序翻转和每轮双缓冲面法向
  更新可以分区写入；
- Normal Tensor 的每个面向三个共享顶点累积 tensor 保持串行，以避免竞争和浮点归约顺序变化；
  归一化、双缓冲平滑、逐尺度分解、持久性计数和最终逐顶点评分可以并行；
- primitive component 的只读拟合可并行写各自结果槽，但 loop ID 分配、loop 发布和顶点归属
  仍按原 component 顺序串行提交。

简化中，初始面到顶点的 quadric 累积目前也是串行，因为多个面会写入同一顶点 quadric。
随后逐顶点状态初始化、逐面状态复制，以及对已经收集的独立活动边计算 placement 或纹理检查
可以并行。collectActiveEdges、候选堆构建、弹出最低代价候选、tryCollapse、局部拓扑更新、
候选版本更新、网格压缩和质量精修都保持串行。

这意味着 oneTBB 的加速上限受实际网格、证据配置和坍缩比例影响很大。候选准备占比较高时更可能
受益；以动态坍缩和全局图协调为主的运行会更接近串行。项目不把固定加速比写成正确性或性能承诺。

static_partitioner 固定逻辑分块，不代表整个算法天然确定。浮点归约、排序、图清理和坍缩
接受顺序仍由各算法明确维持，因此不要把共享容器写入、全局归约或拓扑突变直接塞进
forEachRange。

从调用侧，CLI 的 simplify、扫描和部分特征命令提供 --threads N；0 表示让后端决定并发度。
C++ 可把
ExecutionOptions 传给 FeatureDetector 的带执行选项重载，或传给 QEMSimplifier 的
setExecutionOptions。C ABI 则通过 ManuMeshExecutionOptions 初始化并设置到 context；接口
见 [CApi.h](../../include/api/CApi.h)。

### 并行阅读与验证路径

1. [ExecutionOptions.h](../../include/core/ExecutionOptions.h)：先掌握稳定的用户可见契约。
2. [ParallelExecution.cpp](../../src/common/ParallelExecution.cpp)：理解后端选择、grain size、
   task arena 与串行回退。
3. [FeatureDetector.cpp](../../src/feature_detection/FeatureDetector.cpp)、
   [FeatureNormalFilter.cpp](../../src/feature_detection/FeatureNormalFilter.cpp)、
   [NormalTensor.cpp](../../src/feature_detection/NormalTensor.cpp)：
   对照特征检测的并行点和串行阶段。
4. [SimplificationRun.cpp](../../src/simplification/SimplificationRun.cpp)：
   看候选初始化和动态坍缩的边界；随后读 CandidateQueue 与 CollapseTopology。
5. [parallel_execution_tests.cpp](../../tests/unit/common/parallel_execution_tests.cpp)、
   [feature_detection_parallel_tests.cpp](../../tests/unit/feature_detection/feature_detection_parallel_tests.cpp)、
   [simplification_parallel_tests.cpp](../../tests/unit/simplification/simplification_parallel_tests.cpp)：
   确认串并行结果等价的测试口径。
6. [c_api_execution_tests.cpp](../../tests/unit/api/c_api_execution_tests.cpp) 与
   [parallel_pipeline_benchmark.cpp](../../tests/performance/parallel_pipeline_benchmark.cpp)：
   分别看 ABI 接入和性能实验方式。

## CLI：从用户命令回到库调用

CLI 是应用层，不是另一套算法实现。它的真实调用链是：

~~~text
apps/main.cpp
  -> ManuMeshCli::run
  -> CliArguments::validateArgsForCommand
  -> ManuMeshCommands 的 commandRegistry
  -> 具体 command handler
  -> 公共 C++ SDK
~~~

顶层入口负责 help、version、错误退出和命令分派；参数白名单在
[CliArguments.cpp](../../apps/CliArguments.cpp)；业务工作流分别分布在
[ManuMeshCommands.cpp](../../apps/ManuMeshCommands.cpp)、
ManuMeshFeatureCommands、[ManuMeshLargeMeshCommands.cpp](../../apps/ManuMeshLargeMeshCommands.cpp)
和 ManuMeshWorkflowCommands。

| 命令族 | 命令 | 主要用途 |
| --- | --- | --- |
| 生成与单次处理 | generate、simplify、compare | 构造 fixture、简化、统计和采样距离 |
| 特征分析 | feature-report、feature-benchmark、feature-compare | 报告、标签基准和特征环比较 |
| 参数实验 | sweep、ratio-sweep、face-sweep | 扫描 line weight、目标比例或目标面数 |
| 大网格存储 | large-import、large-validate | MMPD 导入与完整性校验 |
| 仓库工作流 | demo、summarize-metrics、validate-features、validate-external | 演示、汇总和验证编排 |

CLI 会拒绝未知选项、缺少值和属于其他命令的选项，因此命令帮助不是宽松脚本接口。
manumesh --help 是当前命令和选项的权威清单；本导览只解释工作流边界。

常见诊断出口也有不同职责：

| 输出或选项 | 用途 | 注意事项 |
| --- | --- | --- |
| print-resolved-config | 展示 profile 与显式覆盖合并后的有效配置 | 用它确认实际 feature、simplify 和 execution 设置 |
| metrics-csv | 简化输出的质量、距离和报告摘要 | 用 termination reason 和拒绝分类解释未达目标 |
| csv | 特征报告、基准或比较结果 | 字段以命令当前表头为准 |
| performance-csv | 一次调用的阶段墙钟记录 | 适用于指定命令，不是固定性能承诺 |
| verbose | 支持该命令时输出更多运行诊断 | large-import 和 large-validate 当前不消费它 |

完整示例和参数说明见 [CLI 使用说明](cli.md)。要修改命令行为，应同时读
ManuMeshCli、CliArguments、对应 handler 和 apps 下的 CLI CTest；不要只改 help 文本或单独的
参数解析器。

## 构建、目标与交付

### 支持基线和 CMake 层次

根 [CMakeLists.txt](../../CMakeLists.txt) 强制项目当前的工具链边界：Visual Studio 2019、
MSVC v142、x64、CMake 3.20 或更高，以及 C++14。Debug 和 Release 都使用 DLL CRT，也就是
/MD；构建系统不支持把 /MDd 或静态 CRT 混入同一交付。

生产实现按照模块先编译，再汇为物理核心库。外部程序集成时不应链接内部 object target 或
manumesh_internal：

~~~text
src 的 geometry / common / mesh_edit / analysis / io /
feature_detection / simplification / c_api 实现模块
                    |
                    v
             manumesh_core 物理库
                    |
                    +-- ManuMesh::manumesh：C++ SDK
                    +-- ManuMesh::c_api：纯 C ABI SDK
                    +-- manumesh_internal：仅测试白盒边界
~~~

公共 C++ target 和纯 C target 都指向同一交付能力；C API target 刻意不把 Eigen 或 C++ 编译要求
传播给纯 C consumer。具体目标装配在 [src/CMakeLists.txt](../../src/CMakeLists.txt)，安装、
格式、Doxygen 和 SDK 验证辅助目标在 [adm/CMakeLists.txt](../../adm/CMakeLists.txt)。

### 按目的选择 preset

不要靠名字猜构建模式。以下是 CMakePresets 中最常用的用途；对应 Ninja 多配置变体使用同样
语义，仍必须从 VS2019 x64 开发环境运行。

| 目标 | 推荐 preset | 说明 |
| --- | --- | --- |
| 快速读源码和调试 CLI | vs2019-debug | 精简核心与 CLI，关闭测试、示例和开发辅助目标 |
| 完整串行 Debug 回归 | vs2019-debug-full | 完整 Debug 测试基线 |
| 内存安全检查 | vs2019-asan | AddressSanitizer，关闭 CLI 和示例 |
| 正式运行与并行 | vs2019-release | Release，启用 vendored oneTBB |
| 静态库交付 | vs2019-release-static | 静态库变体 |
| 性能实验 | vs2019-release-performance | 单独启用性能测试 |
| 安装后的 SDK 验证 | vs2019-release-sdk 或 vs2019-release-static-sdk | 安装并构建独立 consumer |
| Doxygen | vs2019-release-docs | 只生成文档目标，不构建运行时工作流 |

精确 configure、build 和 test preset 见 [CMakePresets.json](../../CMakePresets.json)；
可复制的命令见 [构建、测试与文档](build.md)。Release 只是 oneTBB 后端通常开启的构建，并不让
每个 CLI 命令或算法阶段自动并行。

### 文档与安装不是同一件事

docs-api 只面向公开 API 和示例，docs-internal 才包含 include 与 src 的私有调用关系；二者都
先运行 check-src-doxygen。生成站点放在 docs 目录且不应提交。SDK 安装验证则从
examples/sdk_consumer 重新配置一个下游工程，只使用安装前缀中的头、库、运行时和 CMake package。
它验证的不是工作树里恰好能 include 的私有路径。

## 测试与验收：从改动反推验证范围

测试按边界而不是按文件数量组织。测试总数动态发现，会随源码变化；用 ctest -N 查询当前清单，
不要把某个计数当成项目状态。

| 层次 | 主要内容 | 代表入口 |
| --- | --- | --- |
| unit | core、common、mesh_edit、io、feature、simplification、api 和 apps 的白盒行为 | tests/unit |
| external | 外部 STL / OBJ、大模型和可选 Thingi10K fixture | tests/unit 下的 external tests 与 external 标签 |
| CLI 黑盒 | help、参数拒绝、Unicode 路径、命令烟测、扫描输出和 MMPD 合同 | apps/CMakeLists 与 CheckCliContracts 等脚本 |
| ABI / 安装 | C 和 C++ consumer、旧结构前缀、导出与运行时布局 | tests/unit/api、tests/support、examples/sdk_consumer |
| architecture | include 依赖边界和模块 link boundary | tests/support/check_include_boundaries.py |
| memory | 生命周期压力和所有权回归 | ownership_lifetime_stress |
| performance | 公开 SDK 上的耗时和真实数据集门禁 | tests/performance，默认独立构建 |

性能测试以结果一致性和测量记录为目的，不断言一台机器上必须获得固定加速比。特别是
[parallel_pipeline_benchmark.cpp](../../tests/performance/parallel_pipeline_benchmark.cpp) 会比较多个
线程宽度下的结果 fingerprint，再记录墙钟时间；吞吐差异应结合 Release、输入规模、CPU 和
并行阶段占比解释。

| 你改了什么 | 至少应阅读或运行什么 |
| --- | --- |
| Mesh、拓扑或 I/O | 对应 tests/unit/core 或 io，加相关 external fixture |
| FeatureDetection | tests/unit/feature_detection；涉及范围并行时加 parallel tests |
| QEM、合法性或动态拓扑 | tests/unit/simplification；必要时真实数据集或性能套件 |
| ExecutionOptions / oneTBB | common、feature、simplification 和 C API 的 parallel tests；Release benchmark |
| MMPD | partitioned_mesh_dataset_tests、CheckLargeMeshCommands 和可选 external 测试 |
| CLI 参数或报告 | CliArguments unit test 与 apps CTest 脚本 |
| C ABI、CMake、安装布局 | C API tests、ABI stress、release-sdk consumer 验证 |
| 公共头或模块边界 | include-boundary 与 Doxygen 检查 |

日常命令、标签含义和最小验证闭环见 [测试与验收](../design/testing.md)。修改算法时，比较
Serial 与 Parallel 的结构化结果比只看墙钟更重要；修改 ABI 时，旧前缀、容量不足和失败时
不替换输出都属于必测行为。

## 选择阅读入口

| 目标 | 最短阅读组合 |
| --- | --- |
| 想调用库 | [SDK 集成指南](sdk.md) + Mesh / FeatureDetector / QEMSimplifier 公共头 |
| 想改特征检测 | FeatureDetector.cpp + [当前算法管线](../design/algorithms.md) + feature_detection 单元测试 |
| 想改简化策略 | QEMSimplifier.h + SimplificationRun.cpp + CandidateQueue / CollapseLegality 测试 |
| 想处理超大二进制 STL | PartitionedMeshDataset.h + large-import / large-validate + MMPD 测试 |
| 想评估并行收益或确定性 | ExecutionOptions.h + ParallelExecution.cpp + parallel tests / benchmark |
| 想定位行为差异 | [调试指南](debugging.md) + CLI 的 --print-resolved-config、--verbose 和 CSV 报告 |
| 想理解 CLI 从参数到算法 | apps/main.cpp + ManuMeshCli.cpp + CliArguments.cpp + 对应 command handler |
| 想给外部程序集成 SDK | [SDK 集成指南](sdk.md) + examples/basic_simplify.cpp 或 examples/c_api_basic.c |
| 想修改 C ABI | CApi.h + CApi.cpp + tests/unit/api + ABI stress 与 sdk consumer |
| 想选择构建或发布方式 | CMakePresets.json + CMakeLists.txt + [构建、测试与文档](build.md) |
| 想确认某项能力是否有验收 | [测试与验收](../design/testing.md) + tests/CMakeLists.txt + 相应模块测试 |
| 想新增跨模块能力 | [新增功能流程](../design/extension.md) + architecture / include-boundary 测试 |

## 建议的阅读计划

### 第一次进入仓库：三十分钟

1. 读本页的全局图、核心数据模型和模块地图。
2. 从 README 的一个 simplify 命令，跳到 ManuMeshCommands，再跳到 QEMSimplifier。
3. 快速浏览 [Mesh.h](../../include/core/Mesh.h)、[FeatureDetector.h](../../include/algorithms/feature_detection/FeatureDetector.h)
   和 [QEMSimplifier.h](../../include/algorithms/simplification/QEMSimplifier.h) 的公开契约。
4. 根据兴趣选择 FeatureDetector.cpp、SimplificationRun.cpp、PartitionedMeshDataset.cpp 或
   ParallelExecution.cpp 中的一个继续下钻。

目标不是记住每个参数，而是清楚区分四件事：内存 Mesh 主算法、MMPD 流式存储、范围并行后端、
以及把它们组合给用户的 CLI 和 SDK。

### 想理解一次真实简化：半天

1. 用 generate 生成一个小 fixture，或者挑一个现有 STL。
2. 用 simplify 的 print-resolved-config 记录实际 profile、目标、特征和线程设置。
3. 在 SimplificationRun::execute、rebuildQueue、tryCollapse 和 MeshCompaction 打断点。
4. 同时观察 SimplifyReport 的 terminationReason、collapsedEdges 和拒绝分类。
5. 再打开一个 simplification 单元测试，对比代码中的预期边界。

这样读能把 Quadric 的数学、候选队列、局部拓扑和报告诊断联系起来，而不是只看到一段
矩阵计算。

### 想做大网格或并行设计：先回答这些问题

| 问题 | 当前答案 | 设计含义 |
| --- | --- | --- |
| 数据是否已经有全局顶点和邻接？ | 否，MMPD 只有独立三角记录 | 不能直接写跨块 QEM 或 FeatureGraph |
| 分块是否按空间或连通性？ | 否，按输入记录顺序 | 不能假设同块局部性或负载均衡 |
| 分块 I/O 是否由 oneTBB 调度？ | 否，MMPD 是串行流式路径 | 不要从 Release 后端推导并行导入 |
| 算法是否都可按面或边 parallel_for？ | 否 | 共享归约、排序和动态拓扑必须先定义确定性与同步模型 |
| FeatureAnalysis 能否跨网格复用？ | 仅限精确 indexed geometry | 分块重编号或拼接前要先设计来源身份 |
| C ABI 是否能直接消费 MMPD？ | 否 | 跨语言大网格需求需要单独设计 ABI 和资源模型 |

若答案要求改变其中任一行，工作不应从给现有循环套 parallel_for 开始，而应先写数据模型、所有权、
失败恢复、跨块一致性和可验证的端到端契约。

## 容易形成的错误心智模型

1. MeshIo 只产生 Mesh；MeshTopology 不会自动创建或随 Mesh 修改更新。
2. MeshTopology 是只读分析快照；简化器的可变编辑结构是私有 DynamicTopology。
3. PlainMesh 是 C++ 的 Eigen-free 数据边界，不是纯 C ABI，也不是第二套算法内核。
4. FeatureAnalysis 不能因为两个网格视觉上相似就跨网格复用；面和顶点顺序也属于来源身份。
5. FeatureGraph 中的合成桥接边不必是输入 Mesh 的真实几何边。
6. 特征检测与简化不是双向依赖；数据方向始终是 Mesh 到 FeatureAnalysis 到 simplification。
7. targetRatio 或 targetFaces 是请求目标，不是覆盖硬约束的强制结果。
8. oneTBB 不是全局网格分区调度器，也不并行动态坍缩和拓扑提交。
9. MMPD partition 不是空间块、拓扑块或负载均衡块。
10. C ABI context 不拥有 mesh handle；两者的线程安全与销毁责任不同。

## 维护一项能力的闭环

对任何新增能力，先确认它属于哪一条链：内存 Mesh 算法、MMPD 存储、范围并行后端、SDK / ABI，
还是 CLI 工作流。然后按下面顺序推进：

~~~text
公开契约和非目标
  -> 模块落点与依赖方向
  -> 实现及确定性 / 线程模型
  -> C++、C ABI 或 CLI 入口
  -> 单元、外部、安装或性能测试
  -> Doxygen 与本目录文档
~~~

跨越现有边界的改动，例如让简化器消费 MMPD，不能只增加一个适配函数。必须先补数据模型、
错误与一致性契约、资源与生命周期规则，再补可复现的端到端测试。
