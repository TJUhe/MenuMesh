# 开源表面网格库地图

快照日期：2026-08-16。本地图仅覆盖三角形/多边形表面网格处理。某个库可以间接读取 CAD 交换格式，
但这不意味着 B-Rep 建模属于 ManuMesh。

许可证说明是事实性的仓库元数据；本地图的主要目的是算法比较。

| 库 | 与此处相关的表面网格能力 | 许可证信息 | 在 ManuMesh 中的用途 |
| --- | --- | --- | --- |
| [OpenMesh](https://www.graphics.rwth-aachen.de/software/openmesh/) | 半边连通性、状态标志、collapse/split/flip 基本操作、decimater 框架 | BSD 3-Clause | 将网格内核/状态/垃圾回收与算法策略分离的主要架构参考。 |
| [CGAL Polygon Mesh Processing](https://github.com/CGAL/cgal) | 各向同性重网格、锐边约束、修复、距离/相交谓词 | 特定包 GPL/LGPL | 约束重网格测试的行为和 API 参考。 |
| [VTK](https://github.com/Kitware/VTK) | 数据流过滤器、网格属性数组、简化、平滑和可视化 | BSD 3-Clause | 借鉴小型运行摘要，以及把逐点/逐单元诊断作为可选属性输出的做法。 |
| [pmp-library](https://github.com/pmp-library/pmp-library) | 紧凑表面网格、各向同性/自适应重网格、曲率和特征边 | MIT | 实用局部算子重网格循环的最佳小型 C++ 参考。 |
| [libigl](https://github.com/libigl/libigl) | 几何算子、曲率、邻接、谓词、参数化及可选 copyleft 模块 | 核心 MPL 2.0；可选模块/依赖各异 | 聚焦几何操作的参考和测试预言器，不是可变网格内核替代品。 |
| [geometry-central](https://github.com/nmwsharp/geometry-central) | 半边表面网格、内在三角化、切线数据和鲁棒几何算子 | MIT | 内在量和拓扑感知数据归属的参考。 |
| [Geogram](https://github.com/BrunoLevy/geogram) | 受限 Voronoi 图、CVT、网格修复和表面处理 | BSD 3-Clause | 全局采样/CVT 重网格和空间加速参考。 |
| [VCGlib](https://github.com/cnr-isti-vclab/vcglib) | 三角网格清理、重网格、简化和面向特征的过滤器 | GPL 3.0 | 算法比较和外部验证参考。 |
| [MeshLab](https://github.com/cnr-isti-vclab/meshlab) | 主要由 VCGlib 支持的终端用户过滤器，包括重网格和清理 | GPL 3.0 | 手动/结果比较工具，不是 SDK 依赖。 |
| [MMG](https://github.com/MmgTools/mmg) | 通过 MMGS 进行度量驱动的各向异性表面重网格 | LGPL 3.0-or-later | 各向异性度量行为的参考或可选外部进程基线。 |
| [Instant Meshes](https://github.com/wjakob/instant-meshes) | 方向场对齐的三角/四边形网格生成 | BSD 风格 3-Clause 条款 | 远期方向场对齐基准；与各向同性三角 remesh MVP 分开。 |
| [MeshLib](https://github.com/MeshInspector/MeshLib) | 广泛的网格处理，以及面向法向/分割的去噪示例 | 仓库特定；GitHub API 未报告 SPDX 声明，复用前需核实 | 面向特征的法向过滤行为和实际扫描处理比较。 |
| [L0Denoising](https://github.com/tatsy/L0Denoising) | 使用 L0 优化的网格去噪参考实现 | MIT | 全局稀疏/特征保持去噪比较；明显重于 ManuMesh 仅检测的法向过滤器。 |
| [NLLR](https://github.com/nini-lxz/NLLR) | 用于网格去噪的非局部低秩法向过滤 | 未报告许可证元数据；除非澄清，否则仅作研究比较 | 非局部扫描去噪基线和失败模式比较。 |
| [LSD](https://github.com/xnowbzhao/lsd) | 用于几何和特征保持网格去噪的 Local Surface Descriptor | 未报告许可证元数据；除非澄清，否则仅作研究比较 | 描述子引导的去噪和弱特征保持参考。 |

## 建议学习顺序

1. OpenMesh：学习编辑内核职责以及状态/压缩语义。
2. pmp-library：学习最小且易读的 split/collapse/flip/smooth 重网格循环。
3. CGAL PMP：学习约束重网格契约和生产级前置条件。
4. VTK：学习算法结果、逐元素属性和数据流对象之间的边界。
5. Geogram 和 M039：学习 Voronoi/CVT 或度量驱动采样。
6. MMG：学习各向异性度量场；Instant Meshes：学习方向场对齐拓扑生成。
7. 评估轻量法向域过滤器是否足以处理噪声扫描时，参考 MeshLib/L0Denoising/NLLR/LSD。这些是比较项目，不是计划中的运行时依赖。

## 应用于 ManuMesh 的 API 与诊断模式

- OpenMesh：内部约束、代价和观察器按模块分离；公共 ABI 暂不暴露整套模块类。
- CGAL：主入口保持短，停止条件和扩展点使用小策略；不照搬重模板 named-parameter 系统。
- VTK：常规结果只返回少量全局状态，逐顶点、逐边和逐面结果进入可选属性或独立诊断对象。
- pmp-library：普通分析报告保持小型；不延续其逐渐增长的多标量参数入口。
- geometry-central：配置、局部操作结果和高级编辑上下文分开，局部操作返回 handle、布尔值或计数。
- libigl：借鉴 pre/post callback 生命周期；不向回调暴露一长串算法内部矩阵和队列状态。

因此新简化代码使用按职责分组的 `SimplifyConfig`，由 `SimplifyTarget` 表达互斥目标，
由 `LineQuadricConfig` 表达关闭、均匀和自适应三种互斥 line-quadric 模式，
再由 `makeSimplifyOptions()` 进入兼容层；有状态调用直接使用 `QEMSimplifier::setConfig()`。
既有 `SimplifyReport` 为保持源码和二进制契约继续保留完整字段，常规调用只读取
`SimplifySummary`。以后新增的逐元素误差、拒绝边集合和事件轨迹应进入属性、诊断对象
或显式观察器，不能继续扩张所有调用方都必须看到的总报告，也不要再添加镜像 summary
结构来包住同一批字段。

## 应用于 ManuMesh 的特征检测模式

- OpenMesh：将特征策略置于拓扑内核之外，并把结果存为显式的边/顶点属性或分析对象。
- CGAL PMP：在分割或重网格消费检测结果前，将锐利或平滑曲线视为受约束的边图。
- pmp-library：使用显式的邻域、边界和平滑选择计算曲率；测试已知解析形状，而不只依赖可视化输出。
- libigl：在 k-ring/半径邻域中使用切线框架 quadric 拟合，并暴露主值和方向。
- geometry-central：将微分量附加到拥有它们的网格元素上，并在拓扑编辑后有意使其失效/重新计算。

ManuMesh 遵循这些模式，但不导入外部网格内核。当前平滑通道增加了鲁棒重加权、无量纲局部尺度归一化、带符号方向
极值、跨尺度持久性和显式特征图归属。

可选的 `FeatureNormalFilter` 有意小于 L0、非局部或描述子驱动的去噪器：它为证据提取稳定面法向，保持顶点/拓扑，
并暴露角度变化诊断，而不声称重建了几何体。图清理和组件整合共享一个用于方向、证据来源及 ridge/valley 符号的
兼容性 helper；junction 暴露分支延续配对；patch 分割忽略不是真实网格边的恢复桥。

同样的分离现在也适用于简化器：`SimplifyConfig::features.detection` 是新代码的统一入口，
`makeSimplifyOptions()` 将它映射到兼容 `SimplifyOptions`；坍缩循环只消费归一化后的策略和
`FeatureGuidance`。预计算分析重载仍可用于在算法间共享一个特征结果的工作流。

对于多边形 IO，ManuMesh 遵循实用表面网格加载器使用的常规投影 ear-clipping 模式：严格凸的 OBJ 面保留旧的扇形
顺序，凹面沿主多边形法向投影后执行 ear-clipping，重复/退化/自相交多边形则被拒绝。这是内部实现，不增加外部
三角化依赖。

## ManuMesh 的边界

- 复用这些库的概念并据其验证行为；不要照搬它们的公共 API。
- 让 `mesh_edit` 负责可变拓扑状态、索引映射和压缩。
- 让 `remeshing` 负责目标长度/度量场、操作调度、特征和边界策略、投影、质量目标及停止条件。
- 让特征检测负责证据、图/环归属、几何基元或平滑曲线拟合及置信度。重网格通过窄适配器消费其结果。
- 不要将 B-Rep 实体、NURBS 拓扑、实体布尔、CAD 特征树或 STEP 拓扑加入表面网格内核。
