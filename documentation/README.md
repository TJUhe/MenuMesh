# ManuMesh 文档入口

> `documentation/` 保存人工维护的设计记录、指南、论文资料和历史导出笔记。
> `docs/` 专用于 `docs-api` 目标生成的 Doxygen 内容，不再存放手工文档。

本文档目录已经拆分为“交付文档”和“历史/研发材料”两层。面向客户、SDK 集成、商务交付和内核评审时，请优先使用交付文档。

## 推荐入口

| 文档 | 用途 |
| --- | --- |
| [`delivery/manumesh_kernel_developer_guide.html`](delivery/manumesh_kernel_developer_guide.html) | 商用内核交付级开发者手册，包含定位、架构、模块、API、C ABI、构建、验证、扩展边界和交付清单。 |
| [`generated/notes/manumesh-feature-recognition-pipeline.html`](generated/notes/manumesh-feature-recognition-pipeline.html) | 当前特征识别源码级权威 HTML：9 阶段 pipeline、MathJax 公式推导、normal-filter 实际数据流、normal tensor、三次 Monge ridge/valley、graph cleanup/consolidation、junction/patch、Taubin/Halíř-Flusser primitive、component confidence、伪代码、数值算例和可复现 CLI 实验。 |
| [`design/feature_recognition_system_upgrade_2026_07_15.md`](design/feature_recognition_system_upgrade_2026_07_15.md) | 本轮系统增强的设计与实现对照：模块职责、默认兼容策略、C++/CLI/C ABI、benchmark 标签、论文/开源程序依据和仍未实现的边界。 |
| [`generated/notes/manumesh-loop-construction.html`](generated/notes/manumesh-loop-construction.html) | FeatureLoop 五段恢复顺序、宽松 trace 与严格 recovered cycle 的差异、各 fallback 上限、primitive fitting 和 vertex ownership 源码对照。 |
| [`guide/debug_util_usage.md`](guide/debug_util_usage.md) | 内部 Debug-only HTML wireframe 工具使用教程，包含开启方式、常用宏、颜色约定、插入位置和截图预览。 |
| [`design/feature_detection_upgrade_2026_07_09.md`](design/feature_detection_upgrade_2026_07_09.md) | 本次特征识别升级记录，覆盖 loop trace 阈值、traced/untraced 诊断、common 局部尺度、normal-tensor persistence、QEM 联动、gtest 保护和后续算法计划。 |
| [`design/mesh_edit_foundation.md`](design/mesh_edit_foundation.md) | 可供 simplification 与未来 remeshing/repair 复用的内部编辑层，说明动态拓扑、compact/remap 和扩展边界。 |
| [`design/texture_aware_qem.md`](design/texture_aware_qem.md) | 纹理感知 4×4 QEM 的权威设计文档：逐角 UV 数据模型、局部 chart 配对与拒绝规则、标量失真代价 `E_uv_local`、选项与诊断、复杂度与文献定位。 |
| [`design/smooth_curvature_feature_detection_2026_07_11.md`](design/smooth_curvature_feature_detection_2026_07_11.md) | 确定性光滑曲率特征检测的权威设计文档：双证据路径理念、多尺度 quadric 拟合算法八步、`FeatureOptions` 新参数与默认值、诊断字段和开源/文献对照。 |
| [`design/architecture_v2_2026_07_12.md`](design/architecture_v2_2026_07_12.md) | 架构升级蓝图 v2：R1-R7 改进项与实施状态（第一至三批已落地），包含 `manumesh::analysis` 模块、CLI 选项表、C ABI 加固等本轮架构改动的立项论证。 |
| [`design/error_handling_policy.md`](design/error_handling_policy.md) | 错误处理策略一页决策表：数据错误用 Status/Result、编程错误用异常、C 边界用状态码、IO 渐进迁移 `Result<Mesh>`；新增公共入口前先查本表。 |
| [`design/algorithm_extension_protocol.md`](design/algorithm_extension_protocol.md) | 算法扩展协议：新增算法模块的 7 步机械化路径、`validateOptions` 协议与诊断字段命名规范。 |
| [`design/testing_strategy.md`](design/testing_strategy.md) | 测试体系与策略：unit/analytic/perf-guard/external/performance 五层划分、解析真值 fixture 设计理念（真值访问器 + 推导断言界）、确定性测试、快速/全量套件命令与规模、新增测试的注册方式。 |
| [`archive/prototype-docs-2026-07-09/`](archive/prototype-docs-2026-07-09/) | 2026-07-09 归档的阶段性设计、指南和生成笔记，用作历史备份。 |
| [`papers/`](papers/) | 论文 PDF 资料库。该目录没有复制进归档目录，以避免重复大文件。 |

## 当前文档策略

- `delivery/` 是对外交付和内部正式评审入口。
- `archive/` 保存阶段性材料，不再作为当前产品能力说明的主入口；其中的旧参数边界不会随当前程序回写。论文 PDF 和带日期的下载快照同样保持原始内容，不因当前实现而重写。
- `design/` 和 `guide/` 可继续作为研发工作区使用，但如果内容与交付文档冲突，以 `delivery/` 为准。
- `generated/notes/` 多数文件属于历史导出资料；其中 `manumesh-feature-recognition-pipeline.html`、`manumesh-loop-construction.html` 和本轮修订到的程序/QEM说明已同步 2026-07-15 源码。对外交付总边界仍以 `delivery/` 为准。
- 交付文档已同步纹理感知 QEM、简化内置的光滑曲率特征检测、CLI/C ABI 尾字段、公共绕向感知二面角、面积加权退化面 point quadric、共享拓扑感知的局部相交检查，以及 OBJ 凹多边形 ear clipping。PDF 二进制未重写，PDF 与同名 HTML 冲突时以当前 HTML/Markdown 为准。
- 新增商业能力时，应先更新交付文档的能力边界、API、验证方法和限制说明，再补充研发细节。

## 交付定位

ManuMesh 当前定位为面向增材制造和三角网格处理的 C++17 mesh geometry kernel。现阶段核心交付能力包括：

- triangle mesh 数据结构、基础拓扑查询和 SDK/C ABI 边界；
- 跨算法网格分析模块 `manumesh::analysis`（`computeMeshStats` / `compareMeshesBySampledDistance`）与特征 loop 匹配公共入口 `manumesh::feature::matchCircularLoops`；
- feature evidence、feature graph、loop recovery 和 primitive fitting；
- QEM / line-quadrics edge-collapse simplification；
- 纹理感知 4×4 QEM 简化（opt-in）：逐角 UV chart 保护与标量 UV 失真排序代价，几何 quadric 保持 4×4 齐次形式，当前仅通过 C++ `SimplifyOptions` 暴露；
- 确定性光滑曲率特征检测与保护（opt-in）：多尺度 quadric 拟合的 ridge/valley 弱证据路径，经显式 `FeatureGraph` 与硬证据汇合；feature-analysis CLI 和 `simplify` 均支持 `--smooth-curvature-*`，`simplify --smooth-curvature-features` 自动开启特征曲线策略；C++/C ABI 均有对应尾字段；
- feature、boundary、topology、normal、triangle quality、local error 和 local intersection 过滤；相交检查覆盖新一环内部与附近活动面，但不声称全局无自交认证；
- STL/OBJ IO；OBJ 严格凸面保持 fan 顺序，凹面使用投影 ear clipping，并拒绝重复、退化或自交 polygon；
- CLI、examples、CTest/GoogleTest 和外部 STL/OBJ 验证路径。
- Debug-only HTML wireframe 辅助工具只属于内部算法排查手段，不属于 SDK/API 或交付 viewer。

明确不在当前交付范围内的能力：

- 完整 B-Rep CAD kernel；
- 通用 Boolean、offset/thickening、shape healing；
- 从 STL/OBJ 自动恢复完整 CAD feature tree；
- 全局 Hausdorff/envelope 形式化认证；
- 直接制造公差合规保证。
