# 超大网格数据与分区架构（2026-08-18）

本文记录 ManuMesh 当前已经落地的超大网格基础设施、三份 Thingi10K 实测数据，以及后续分区拓扑、特征分析和简化的设计边界。

本文的状态词含义如下：

- **已完成**：代码、测试和当前构建中已经存在，可以直接复现。
- **第一步已完成**：已经有可用的数据层或 CLI 切片，但还没有完成算法语义。
- **计划中**：本文定义了接口和验收方向，当前没有声称实现。

## 结论

当前版本已经能够在受控内存预算下把二进制 STL 流式写入分区数据集，并逐分区读取、校验数量、边界框、校验和、面积和退化面计数。这些完整校验现在由 `src/io/PartitionedMeshValidation.cpp` 实现，CLI 只负责解析参数和格式化报告；库用户可复用同一份 `PartitionedMeshValidationReport` 契约。当前版本还不能在分区数据集上直接运行完整拓扑、FeatureGraph、Normal Tensor 或 QEM 简化。

因此，`PartitionedMeshDataset` 是超大网格的 **I/O 基础层**，不是已经完成的分布式网格算法层。后续实现必须先建立全局实体 ID、owner/ghost 和完整 edge incidence，再把局部计算和全局归并接起来。不能把各分区独立简化后直接拼接，也不能把分区切口的局部单面边误报成真实边界。

## 已完成能力

### 紧凑 MeshTopology

[`MeshTopology`](../../include/core/MeshTopology.h) 的内部存储已经改为连续数组和 CSR 风格 offsets：

- 边端点、边入射面、面角和逐顶点入射索引分开存储；
- 面角使用 1 字节编码，避免每条边拥有一个小 `vector<int>`；
- 构建使用两遍计数和填充，并在末段复用计数缓冲；
- `edgeView()` 和 `vertexView()` 直接引用连续存储，不产生兼容对象；
- 原有 `edges()`、`edge()` 和 `vertex()` API 仍保留，只有旧 vector 视图被调用时才物化；
- `MeshTopology` 的公共计数和 `int` typed handle 契约没有改变。

内部分析、签名体积和 C API 的唯一边导出已经使用紧凑视图。256×256 规则平面网格的回归测试按结构负载估算，CSR 负载约 8.9 MiB，旧的逐实体 vector 对象和等长索引负载约 19.6 MiB；这不是进程 RSS 测量，也不等于 100M 面已经可以放入单个 `Mesh`。

### PartitionedMeshDataset

[`PartitionedMeshDataset.h`](../../include/io/PartitionedMeshDataset.h) 提供当前的三角记录级分区格式和流式读写器：

- `GlobalTriangleId` 和 `MeshPartitionId` 使用 64 位；
- 分区内三角序号使用 32 位；
- 写入器只保留当前记录、I/O 缓冲和当前分区元数据，目录条目在磁盘 sidecar 中暂存；
- 读入器按目录项和分区顺序读取，不在打开数据集时加载全部三角形；
- STL 导入会把声明预算中未被写入器占用的部分用于输入批量读取；预算只够一个记录时回退到单记录路径，
  不为了吞吐隐式扩大 resident 工作集；
- 每个分区记录首个全局三角 ID、数量、payload 偏移/大小、FNV-1a 校验和以及双精度边界框；
- 二进制 STL 导入是事务性的，截断文件、非有限坐标和输入输出别名会被拒绝；
- 当前导入忽略标准二进制 STL 末尾 padding，但不保留 STL 的共享顶点语义。

当前格式保存的是三角记录流，是版本化的 staging 存储契约，而不是完整的分区网格实体模型。它尚未定义全局顶点表、全局边表、属性通道、ghost 记录或跨分区拓扑目录；这些实体不会被后续版本默认塞进现有三角记录格式。

### CLI

[`large-import`](../../apps/ManuMeshLargeMeshCommands.cpp) 和 `large-validate` 已注册到 CLI：

```text
manumesh large-import input.stl output.mmpd \
  --partition-triangles 250000 --memory-mib 1 --io-buffer-mib 1
manumesh large-validate output.mmpd \
  --memory-mib 1 --io-buffer-mib 1
```

默认值是每分区 1,000,000 个三角形、声明的常驻工作集 256 MiB、I/O 缓冲 4 MiB。`--memory-mib` 是该操作向数据层声明的可变工作集预算，不是操作系统进程 RSS 的保证；分配器、运行时和应用自身的常驻内存不包含在这个声明中。

`large-validate` 当前验证以下内容：分区 global triangle range 连续性、目录数量、payload checksum、分区及全局 bounds、面积累加和退化三角形计数。它不验证共享顶点、流形性、绕序、特征边或自交。
这些是公共 `validatePartitionedMeshDataset()` 和 `PartitionedMeshValidationReport` 的同一合同；CLI 不另行实现一套预测逻辑。

## Thingi10K 实测

### 数据来源和复现方式

三份输入由 [`fetch_thingi10k_large.py`](../../tests/support/fetch_thingi10k_large.py) 从 Thingi10K 的公开 metadata 和 mirror 选择，默认条件为至少 2,000,000 面、最多 3 个 STL。模型名称、授权、来源 URL、SHA-256、面数和字节数以本地 [`manifest.json`](../../output/thingi10k_large/manifest.json) 为准；`thingi10k_manifest` CTest 会用 CMake 的版本化 JSON 解析和 `file(SHA256)` 校验 manifest 中的每个文件，分发前仍必须按 manifest 中的模型授权逐项审核。测试说明见 [`tests/data/external/thingi10k_large/README.md`](../../tests/data/external/thingi10k_large/README.md)。

本轮在 Windows Release CLI 上以 `--partition-triangles 250000` 生成了本地 `.mmpd`，再以 1 MiB resident/I/O 声明运行 `large-validate`。下表的 `validate_ms` 是 PowerShell `Stopwatch` 的单次墙钟时间，不是稳定性能基线，也不包含全量网格拓扑或特征处理。`metadata_vertices` 是 Thingi10K 几何 metadata 的顶点数；当前二进制 STL 分区层本身只看三角记录，不建立共享顶点表。

| Thingi10K file ID | 模型 | metadata_vertices | triangles | STL bytes | local MMPD bytes | partitions | closed / edge-manifold metadata | area | degenerate | validate_ms |
| ---: | --- | ---: | ---: | ---: | ---: | ---: | --- | ---: | ---: | ---: |
| 372114 | Venus De Milo Full Scale | 1,577,057 | 3,154,110 | 157,705,584 | 157,708,076 | 13 | true / true | 3,825,094.4808539771 | 0 | 600 |
| 103354 | Sky's Skull 2 | 1,347,290 | 2,714,120 | 135,706,084 | 135,708,192 | 11 | true / true | 214,630.58519974703 | 0 | 518 |
| 120628 | Lucy 3M O10 | 1,264,847 | 2,529,744 | 126,487,284 | 126,489,392 | 11 | true / false | 2,534,353.7514099535 | 58 | 485 |

三次验证的输出均报告 `count_consistency=ok checksum_consistency=ok`。Lucy 的 58 个退化三角形是流式统计结果，不是分区格式自动修复的结果。三份 STL 和 `.mmpd` 默认位于 `output/thingi10k_large/`，该目录不作为仓库源码分发；只提交 manifest 和测试说明。

### 这组数据能证明什么

- 二进制 STL 在不构造 `Mesh` 的情况下可以被切成连续三角 ID 分区；
- 1 MiB 的数据层工作集声明足以完成这三份 2.5M 至 3.15M 面数据的顺序导入/校验；
- 分区目录和 payload checksum 能检测截断、目录错位和有限值破坏；
- 分区数量随配置变化，输入顺序和分区首三角 ID 可以被逐项校验。

### 这组数据不能证明什么

- 没有证明 100M 面的全局共享顶点拓扑可以在单机内存中建立；
- 没有证明分区切口的边界、非流形边和绕序能够被局部结果正确判断；
- 没有运行分区版 Normal Tensor、FeatureGraph 或 QEM；
- 没有给出 OS 级 peak RSS、全局自交认证、Hausdorff 认证或简化质量结论；
- 三个样本都是二进制 STL，不能代表 ASCII STL、OBJ、纹理、属性通道或所有扫描数据分布。

## 目标数据模型

后续算法分区必须把 I/O 分区和算法分区分开。I/O 分区适合顺序读写，算法分区可以依据空间、拓扑和工作集重组。

### 全局和局部 ID

计划定义并持久化以下全局 ID：

```text
GlobalVertexId  = uint64
GlobalEdgeId    = uint64
GlobalFaceId    = uint64
MeshPartitionId = uint64
LocalIndex      = uint32
```

每个实体有一个 owner partition。分区内允许使用 `LocalIndex`，但任何跨分区引用、目录键、日志、归约和结果比较都使用 global ID。owner 负责发布实体属性和拓扑变更；ghost 只读，必须带来源 owner 和版本号。局部索引重排不能改变 global ID。

全局 ID、owner、generation/version 和属性通道必须写入可版本化 manifest。manifest 还需要记录坐标精度、单位、索引宽度、分区 schema、halo schema 和 determinism contract，不能仅依赖文件名或分区顺序推断。

### 完整 edge incidence

每个 owned face 产生三个规范记录：

```text
(canonical_edge_key, global_face_id, local_corner, directed_orientation)
```

其中 `canonical_edge_key` 由排序后的两个 `GlobalVertexId` 构成。记录按 key 和 global face ID 全局归并后，才允许分类为 boundary、manifold interior 或 non-manifold。分区切口上的局部单面边不能直接标为 boundary。

归并结果必须同时支持：

- 两端点的完整 vertex-star；
- 全部入射面和局部角；
- 全局绕序 parity 约束和面组件 DSU；
- edge owner 发布的 boundary/non-manifold/feature evidence；
- 按 global ID 排序的确定性浮点归约。

## owner、ghost 和 halo

### Owner/ghost 契约

- owned 实体可以写入，ghost 实体只读；
- 共享顶点的坐标、法向、权重、特征状态和版本由 owner 发布；
- 跨分区写集必须先锁定，再提交 global operation record；
- 交换后必须检查同一 global ID 的值、版本和来源是否一致；
- owner 丢失或 ghost 版本落后时，阶段应返回 incomplete/defer，而不是用局部值静默降级。

### Halo 不是一个固定层数

拓扑 halo、数值 halo 和空间 halo 必须分开。当前算法的依赖可按下表规划：

| 算法/阶段 | 需要的完整信息 | 分区计划 |
| --- | --- | --- |
| 面法向 | owned face 的三个顶点 | 无额外 ring |
| 顶点法向 | 完整 vertex-star | 由 edge incidence 归并，不用局部截断 star |
| 二面角、边界、非流形 | edge 的全部入射面和局部角 | 全局 edge-key merge 后计算 |
| 法向滤波 | 配置的 manifold face-dual 邻域 | 按迭代次数交换 dual halo |
| Normal Tensor | 多尺度 vertex rings 加最外层完整 face-star | 当前配置可能需要约 16 个 vertex rings；由实际 `smoothing/scales` 配置计算 |
| FeatureGraph component/junction/loop | 全局图连通、junction 分支、环签名 | 不承诺固定 halo，必须全局归并 |
| 局部相交和近距离 bridge | 拓扑写集外的空间邻近三角形 | 单独的空间 halo 或分区 AABB/BVH 索引 |

所谓 halo 完整，必须有可检查的 completeness proof。没有完整 vertex-star、edge incidence 或所需邻域时，结果应标记为 deferred，而不是把分区切口当成真实几何特征。

## 两种正确性合同

### reference-exact

`reference-exact` 允许数据存储和局部执行分区化，但结果语义必须与当前单块实现一致：

- edge-key incidence、boundary/non-manifold、组件和绕序计数一致；
- 共享 global ID 的属性和特征状态一致；
- FeatureGraph 的活动/删除/bridge edge、junction、component、loop signature 和 patch barrier 一致；
- 浮点归约按 global ID 或固定 reduction tree 执行，必要时报告 bitwise 或明确的 ULP 容差；
- QEM 的 global candidate tie-break、collapse trace、placement 和硬过滤结果保持同一提交顺序。

这要求有全局归并和协调器，不能由每个分区独立完成后直接拼接。首个 out-of-core QEM 基线应优先做单一全局提交队列，吞吐优化放在结果合同之后。

### bounded-valid

`bounded-valid` 允许不同的候选顺序和最终三角形，只保证配置的工程约束：

- 无重复、退化和新增非流形面；
- link condition、边界策略、法向翻转、局部质量和局部自交过滤通过；
- Hausdorff/reference-surface、法向和特征漂移在预算内；
- global face count、component 和 seam consistency 满足报告；
- 每轮独立候选的写集和空间 AABB 不相交，轮末同步 owner/ghost、全局计数和索引。

`bounded-valid` 不能宣称与单块 QEM 折叠序列或最终网格 bitwise 相同。跨 seam 候选应 defer 到 seam pass 或后续全局阶段。

## 跨分区算法路线

### 阶段 A：数据和实体目录

1. 为顶点、边、面定义 global ID、owner、generation/version 和可重放 manifest。
2. 将当前三角记录导入扩展为 global vertex dedup/weld、face ownership 和 edge-key side index。
3. 把属性通道按 vertex/edge/face/corner 分域，所有 remap 明确传播策略。

### 阶段 B：全局拓扑和基础归约

1. 归并完整 edge incidence，生成 boundary/non-manifold 和 vertex-star。
2. 以 global face ID 做确定性绕序 parity 和组件 DSU。
3. 提供流式统计、验证、bounds、面积、法向和错误计数，作为后续阶段的低风险基线。

### 阶段 C：halo 驱动的局部分析

1. 先完成 owner-only 面法向、顶点法向和边 evidence。
2. 按配置交换法向 filter、Normal Tensor 所需 halo。
3. 每个局部结果携带 completeness、source owner 和版本；缺 halo 时返回 deferred。
4. 按 global ID 排序归约浮点结果，保证 reference-exact 模式可复现。

### 阶段 D：FeatureGraph 全局合并

1. 以 canonical edge key 去重证据边，按 owner 发布活动状态和 source/sign。
2. 全局合并 component、junction、endpoint、open chain、closed loop 和 loop signature。
3. 对 bridge/consolidation 执行空间 join 后的全局排序和一次性匹配；不要在分区内分别决定同一端点的归属。
4. Feature patch 只在完整 face graph 上分割，分区切口不作为 barrier。

这一步对应当前 FeatureGraph 的全局语义。局部曲率或 normal/tensor 证据可以分区计算，但组件、环、junction 和 patch 不能用固定 halo 近似完成。文献上的局部 quadric 加全局曲线网络分层，可参考 M014、M042、M044；弱特征应先形成连贯支撑再进入简化，可参考 M026。

### 阶段 E：reference-exact out-of-core QEM

1. 由 face owner 生成面 quadric，按 vertex owner 全局归约；boundary quadric 等待完整 edge incidence。
2. 每个分区维护候选子队列，coordinator 取全局最小 `(cost, global_vertex_a, global_vertex_b, version)`。
3. 折叠前锁定动态 2-ring 写集，查询 feature component、边界、参考曲面和空间索引。
4. 提交 global contraction record，更新所有相关 owner、ghost、feature counter 和候选版本。
5. 用局部 BVH 加分区顶层 AABB，避免把所有三角形重新 materialize 成单一全局 BVH。

标准 QEM/line quadrics 仍只是候选排序和 placement 正则；link condition、法向、质量、误差、边界、特征和相交过滤是硬约束。QEM、line quadrics 和大网格 edge-collapse 的文献锚点为 M002、M004、M030、M032、M033，以及近期 QEM 资料中的 085、087。

### 阶段 F：bounded-valid 并行 QEM

在阶段 E 有可验证基线后，再按轮次选择写集和空间包围盒互不相交的候选独立集。每轮结束同步 ghost、全局面数、edge incidence、feature graph 和空间索引；跨 seam 候选延迟处理。阶段 F 允许结果不同，但必须输出完整的拓扑、几何和特征预算报告。

## 验收门槛

每次分区数、输入 chunk 顺序和线程调度变化都应重复验证：

- edge-key incidence、boundary、non-manifold、component 和绕序计数完全一致；
- seam 假边界数为 0，shared global ID 的 owner/ghost 字段一致；
- Normal Tensor 的 valid mask、selected scale 和 evidence key 集一致；
- FeatureGraph 的活动边、junction、component、loop signature 和 patch barrier 一致；
- reference-exact QEM 的 collapse trace 和最终 active face 集一致；
- bounded-valid QEM 无新增裂缝、非流形、自交、退化面，且 Hausdorff、法向、特征漂移在预算内；
- 每个阶段报告 resident budget、磁盘临时空间、吞吐、deferred halo 数和失败清理状态。

## 当前限制和下一步

当前数据层可以处理多百万面顺序三角记录，但以下项目仍属于计划中：

1. 全局顶点去重和 `GlobalVertexId` 持久化；
2. 全局 edge side index、完整 incidence 和 owner/ghost 交换；
3. 分区 Normal Tensor 和 FeatureGraph 合并；
4. reference-exact out-of-core QEM；
5. bounded-valid 并行 QEM、LOD 层次和可恢复任务；
6. ASCII STL/OBJ、纹理和通用属性通道的流式导入。

在这些项目完成前，100M 面仍不能被描述为“ManuMesh 已支持完整处理”。当前可靠的表述是：ManuMesh 已有受内存预算控制的三角记录分区 I/O、紧凑单块拓扑缓存和可复现的大样本流式验证基础。
