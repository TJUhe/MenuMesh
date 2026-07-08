# ManuMesh 文档入口

本文档目录已经拆分为“交付文档”和“历史/研发材料”两层。面向客户、SDK 集成、商务交付和内核评审时，请优先使用交付文档。

## 推荐入口

| 文档 | 用途 |
| --- | --- |
| [`delivery/manumesh_kernel_developer_guide.html`](delivery/manumesh_kernel_developer_guide.html) | 商用内核交付级开发者手册，包含定位、架构、模块、API、C ABI、构建、验证、扩展边界和交付清单。 |
| [`design/feature_detection_upgrade_2026_07_09.md`](design/feature_detection_upgrade_2026_07_09.md) | 本次特征识别升级记录，覆盖 loop trace 阈值、traced/untraced 诊断、common 局部尺度、normal-tensor persistence、QEM 联动、gtest 保护和后续算法计划。 |
| [`archive/prototype-docs-2026-07-09/`](archive/prototype-docs-2026-07-09/) | 2026-07-09 归档的阶段性设计、指南和生成笔记，用作历史备份。 |
| [`papers/`](papers/) | 论文 PDF 资料库。该目录没有复制进归档目录，以避免重复大文件。 |

## 当前文档策略

- `delivery/` 是对外交付和内部正式评审入口。
- `archive/` 保存阶段性材料，不再作为当前产品能力说明的主入口。
- `design/` 和 `guide/` 可继续作为研发工作区使用，但如果内容与交付文档冲突，以 `delivery/` 为准。
- `generated/notes/` 属于历史导出资料，适合追溯思路，不适合作为商用交付主文档。
- 新增商业能力时，应先更新交付文档的能力边界、API、验证方法和限制说明，再补充研发细节。

## 交付定位

ManuMesh 当前定位为面向增材制造和三角网格处理的 C++17 mesh geometry kernel。现阶段核心交付能力包括：

- triangle mesh 数据结构、基础拓扑查询和 SDK/C ABI 边界；
- feature evidence、feature graph、loop recovery 和 primitive fitting；
- QEM / line-quadrics edge-collapse simplification；
- feature、boundary、topology、normal、triangle quality、local error 和 local intersection 过滤；
- CLI、examples、CTest/GoogleTest 和外部 STL/OBJ 验证路径。

明确不在当前交付范围内的能力：

- 完整 B-Rep CAD kernel；
- 通用 Boolean、offset/thickening、shape healing；
- 从 STL/OBJ 自动恢复完整 CAD feature tree；
- 全局 Hausdorff/envelope 形式化认证；
- 直接制造公差合规保证。
