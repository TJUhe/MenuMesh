# Tessellix 理论包说明

本目录中的理论包用于配合 `line-quadrics-qem-theory-explained.pdf` 阅读。文件名保留英文，正文说明使用中文。该理论包是历史导出资料；当前产品名为 Tessellix，当前源码结构以 `docs/design/source_organization.md` 为准。

## 主笔记

- `line-quadrics-qem-theory-explained.pdf`：解释 QEM、line quadrics、矩阵秩与条件数、边界 quadric、特征曲线 quadric，以及 Tessellix 当前程序如何把这些项解释为候选折叠的能量。

## 随包论文

| 文件 | 作用 |
| --- | --- |
| `garland_heckbert_1997_surface_simplification_qem.pdf` | 原始 QEM 参考。 |
| `liu_rahimzadeh_zordan_2025_line_quadrics.pdf` | line quadrics 主参考。 |
| `garland_heckbert_1998_color_texture_qem.pdf` | 属性 QEM 扩展参考。 |
| `garland_shaffer_2002_efficient_adaptive_simplification_massive_meshes.pdf` | 大模型自适应简化参考。 |
| `hoppe_1996_progressive_meshes.pdf` | progressive mesh 和 edge collapse 框架参考。 |
| `lindstrom_turk_1998_fast_memory_efficient_simplification.pdf` | 局部 placement 与保持约束参考。 |
| `wang_2008_feature_sensitive_metric.pdf` | 特征敏感误差度量参考。 |
| `hussain_2008_feature_preserving_mesh_simplification_vertex_cover.pdf` | 特征保持简化参考。 |
| `jiao_bayyana_2008_identification_c1_c2_discontinuities_surface_meshes_cad.pdf` | CAD 不连续检测参考。 |
| `vidal_wolf_dupont_2011_robust_feature_line_extraction_cad_triangular_meshes.pdf` | CAD 三角网格特征线提取参考。 |
| `tsuchie_higashi_2014_normal_tensor_surface_feature_lines.pdf` | normal tensor 特征检测参考。 |

## 未随包但常被提到的参考

CGAL、OpenMesh、libigl、MeshLab/VCGLib、Yamakawa/Shimada 等在线文档或论文用于工程对照。Tessellix 当前实现不逐字复刻这些库，而是借鉴它们的 cost/legality 分离、约束边和局部过滤思路。
