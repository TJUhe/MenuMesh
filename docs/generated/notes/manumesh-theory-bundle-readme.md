# ManuMesh 理论包说明

本目录中的理论包用于配合 `manumesh-theory-explained.pdf` 阅读。文件名保留英文，正文说明使用中文。该理论包是历史导出资料；当前产品名为 ManuMesh，当前源码结构以 `docs/design/source_organization.md` 为准。

## 主笔记

- `manumesh-theory-explained.pdf`：解释 QEM、line quadrics、矩阵秩与条件数、边界 quadric、特征曲线 quadric，以及 ManuMesh 当前程序如何把这些项解释为候选折叠的能量。PDF 为历史导出，未随 2026-07-12 的算法更新重写；同名 HTML（`manumesh-theory-explained.html`）已同步当日源码基线：有向二面角证据、Taubin 圆拟合与 Halíř-Flusser 椭圆拟合（Kåsa 为回退）、smooth-curvature 分支的三次 Monge patch 与边零交叉极值数学、GH97 三级 placement 回退链、Lindstrom-Turk 边界 placement、adaptiveScale 模式下 featureBoost 的队列优先级解耦（Wang 2008）。以 HTML 版为准。

## 随包论文

| 文件 | 作用 |
| --- | --- |
| `docs/papers/qem/garland_heckbert_1997_surface_simplification_qem.pdf` | 原始 QEM 参考。 |
| `docs/papers/line_quadrics/liu_rahimzadeh_zordan_2025_line_quadrics.pdf` | line quadrics 主参考。 |
| `docs/papers/qem/garland_heckbert_1998_color_texture_qem.pdf` | 属性 QEM 扩展参考。 |
| `docs/papers/edge_collapse/garland_shaffer_2002_efficient_adaptive_simplification_massive_meshes.pdf` | 大模型自适应简化参考。 |
| `docs/papers/edge_collapse/hoppe_1996_progressive_meshes.pdf` | progressive mesh 和 edge collapse 框架参考。 |
| `docs/papers/edge_collapse/lindstrom_turk_1998_fast_memory_efficient_simplification.pdf` | 局部 placement 与保持约束参考。 |
| `docs/papers/feature_preserving_simplification/wang_2008_feature_sensitive_metric.pdf` | 特征敏感误差度量参考。 |
| `docs/papers/feature_preserving_simplification/hussain_2008_feature_preserving_mesh_simplification_vertex_cover.pdf` | 特征保持简化参考。 |
| `docs/papers/feature_detection/jiao_bayyana_2008_identification_c1_c2_discontinuities_surface_meshes_cad.pdf` | CAD 不连续检测参考。 |
| `docs/papers/feature_detection/vidal_wolf_dupont_2011_robust_feature_line_extraction_cad_triangular_meshes.pdf` | CAD 三角网格特征线提取参考。 |
| `docs/papers/feature_detection/tsuchie_higashi_2014_normal_tensor_surface_feature_lines.pdf` | normal tensor 特征检测参考。 |

## 未随包但常被提到的参考

CGAL、OpenMesh、libigl、MeshLab/VCGLib、Yamakawa/Shimada 等在线文档或论文用于工程对照。ManuMesh 当前实现不逐字复刻这些库，而是借鉴它们的 cost/legality 分离、约束边和局部过滤思路。2026-07-12 起的 primitive 拟合还引用了三个未随包的方法锚点：Taubin 1991（代数圆拟合，一阶无偏，当前圆拟合主路径）、Halíř-Flusser 1998（Fitzgibbon 约束 4ac−b²=1 的数值稳定直接最小二乘椭圆拟合，当前椭圆拟合主路径）、Chernov《Circular and Linear Regression》2010（Taubin 特征方程的 Newton 求解形式）；Kåsa 最小二乘保留为确定性回退。
