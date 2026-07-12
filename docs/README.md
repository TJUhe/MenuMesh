# ManuMesh 文档入口

本文档目录已经拆分为“交付文档”和“历史/研发材料”两层。面向客户、SDK 集成、商务交付和内核评审时，请优先使用交付文档。

## 推荐入口

| 文档 | 用途 |
| --- | --- |
| [`delivery/manumesh_kernel_developer_guide.html`](delivery/manumesh_kernel_developer_guide.html) | 商用内核交付级开发者手册，包含定位、架构、模块、API、C ABI、构建、验证、扩展边界和交付清单。 |
| [`guide/debug_util_usage.md`](guide/debug_util_usage.md) | 内部 Debug-only HTML wireframe 工具使用教程，包含开启方式、常用宏、颜色约定、插入位置和截图预览。 |
| [`design/feature_detection_upgrade_2026_07_09.md`](design/feature_detection_upgrade_2026_07_09.md) | 本次特征识别升级记录，覆盖 loop trace 阈值、traced/untraced 诊断、common 局部尺度、normal-tensor persistence、QEM 联动、gtest 保护和后续算法计划。 |
| [`design/mesh_edit_foundation.md`](design/mesh_edit_foundation.md) | 可供 simplification 与未来 remeshing/repair 复用的内部编辑层，说明动态拓扑、compact/remap 和扩展边界。 |
| [`design/texture_aware_qem.md`](design/texture_aware_qem.md) | 纹理感知 4×4 QEM 的权威设计文档：逐角 UV 数据模型、局部 chart 配对与拒绝规则、标量失真代价 `E_uv_local`、选项与诊断、复杂度与文献定位。 |
| [`design/smooth_curvature_feature_detection_2026_07_11.md`](design/smooth_curvature_feature_detection_2026_07_11.md) | 确定性光滑曲率特征检测的权威设计文档：双证据路径理念、多尺度 quadric 拟合算法八步、`FeatureOptions` 新参数与默认值、诊断字段和开源/文献对照。 |
| [`archive/prototype-docs-2026-07-09/`](archive/prototype-docs-2026-07-09/) | 2026-07-09 归档的阶段性设计、指南和生成笔记，用作历史备份。 |
| [`papers/`](papers/) | 论文 PDF 资料库。该目录没有复制进归档目录，以避免重复大文件。 |

## 当前文档策略

- `delivery/` 是对外交付和内部正式评审入口。
- `archive/` 保存阶段性材料，不再作为当前产品能力说明的主入口。
- `design/` 和 `guide/` 可继续作为研发工作区使用，但如果内容与交付文档冲突，以 `delivery/` 为准。
- `generated/notes/` 属于历史导出资料，适合追溯思路，不适合作为商用交付主文档。
- 交付文档本轮已同步纹理感知 QEM 与光滑曲率特征检测说明；`generated/notes/*.html` 此前补充的 `io/` 与 Debug-only `debugUtil/` 布局说明保持不变，PDF 二进制未重写。
- 新增商业能力时，应先更新交付文档的能力边界、API、验证方法和限制说明，再补充研发细节。

## 交付定位

ManuMesh 当前定位为面向增材制造和三角网格处理的 C++17 mesh geometry kernel。现阶段核心交付能力包括：

- triangle mesh 数据结构、基础拓扑查询和 SDK/C ABI 边界；
- feature evidence、feature graph、loop recovery 和 primitive fitting；
- QEM / line-quadrics edge-collapse simplification；
- 纹理感知 4×4 QEM 简化（opt-in）：逐角 UV chart 保护与标量 UV 失真排序代价，几何 quadric 保持 4×4 齐次形式，当前仅通过 C++ `SimplifyOptions` 暴露；
- 确定性光滑曲率特征检测（opt-in）：多尺度 quadric 拟合的 ridge/valley 弱证据路径，经显式 `FeatureGraph` 与硬证据汇合，`feature-report`/`feature-benchmark`/`feature-compare` CLI 已暴露 `--smooth-curvature-*` 参数；
- feature、boundary、topology、normal、triangle quality、local error 和 local intersection 过滤；
- CLI、examples、CTest/GoogleTest 和外部 STL/OBJ 验证路径。
- Debug-only HTML wireframe 辅助工具只属于内部算法排查手段，不属于 SDK/API 或交付 viewer。

明确不在当前交付范围内的能力：

- 完整 B-Rep CAD kernel；
- 通用 Boolean、offset/thickening、shape healing；
- 从 STL/OBJ 自动恢复完整 CAD feature tree；
- 全局 Hausdorff/envelope 形式化认证；
- 直接制造公差合规保证。
