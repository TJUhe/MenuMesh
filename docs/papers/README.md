# 论文索引

本目录保存 Tessellix 的论文 PDF 归档。PDF 文件名保留英文，方便和引用链接对应；本索引用中文说明它们在当前实现和后续路线中的作用。

## 核心论文

| 文件 | 在项目中的作用 | 来源 |
| --- | --- | --- |
| `garland_heckbert_1997_surface_simplification_qem.pdf` | 原始 QEM 论文，解释 plane quadric、vertex quadric 和 edge contraction cost。 | https://www.cs.cmu.edu/~garland/Papers/quadrics.pdf |
| `liu_rahimzadeh_zordan_2025_line_quadrics.pdf` | Tessellix 当前复现和扩展的主参考，解释用 point-to-line quadrics 软控制 QEM 简化。 | https://www.dgp.toronto.edu/~hsuehtil/pdf/lineQuadric.pdf |
| `wang_2008_feature_sensitive_metric.pdf` | 支持把位置 QEM 扩展到法线/特征敏感度的思路。 | https://cg.cs.tsinghua.edu.cn/papers/weijin.pdf |
| `vidal_wolf_dupont_2011_robust_feature_line_extraction_cad_triangular_meshes.pdf` | CAD 三角网格特征线提取参考，对当前 feature graph 和 loop 检测有启发。 | https://www.scitepress.org/Papers/2011/33617/33617.pdf |
| `hussain_2008_feature_preserving_mesh_simplification_vertex_cover.pdf` | 特征保持简化参考，用于理解小特征保护和顶点覆盖思路。 | https://www.grahn.cse.bth.se/Papers/cgv2008.pdf |
| `jiao_bayyana_2008_identification_c1_c2_discontinuities_surface_meshes_cad.pdf` | CAD 表面 C1/C2 不连续检测参考。 | https://www.ams.sunysb.edu/~jiao/papers/feature_detect.pdf |
| `tsuchie_higashi_2014_normal_tensor_surface_feature_lines.pdf` | normal tensor 特征评分参考，当前 `normal-tensor` 模式受它启发。 | https://www.cad-journal.net/files/vol_11/CAD_11%282%29_2014_172-181.pdf |
| `lindstrom_turk_1998_fast_memory_efficient_simplification.pdf` | 局部边折叠、内存效率和约束保持参考。 | https://faculty.cc.gatech.edu/~turk/my_papers/memless_vis98.pdf |
| `hoppe_1996_progressive_meshes.pdf` | Progressive Mesh 框架参考，用于理解 collapse workflow 和重建。 | https://hhoppe.com/pm.pdf |
| `garland_heckbert_1998_color_texture_qem.pdf` | QEM 属性扩展参考，说明如何把附加约束并入 quadric 风格误差项。 | https://www.cs.cmu.edu/~garland/Papers/quadric2.pdf |
| `garland_shaffer_2002_efficient_adaptive_simplification_massive_meshes.pdf` | 大模型自适应简化参考。 | https://mgarland.org/papers/massive.pdf |

## 近期 QEM 和网格简化资料

| 文件 | 年份 | 项目用途 |
| --- | ---: | --- |
| `chang_2025_two_round_optimization_qem.pdf` | 2025 | 近期 QEM 变体，可用于比较二轮优化/后处理是否改善质量。 |
| `ha_2025_deep_learning_salient_feature_preserving_mesh_simplification.pdf` | 2025 | 学习式显著特征保护参考；Tessellix 当前未实现学习模型。 |
| `yokota_2024_tracked_qem_temporal_consistency.pdf` | 2024 | 动态序列一致性参考；Tessellix 当前只处理静态网格。 |
| `maruani_2024_ponq_neural_qem_representation.pdf` | 2024 | 神经 QEM 表示参考，不是当前 decimator 的实现基础。 |
| `li_2025_qemesh_qem_based_mesh_generation.pdf` | 2025 | QEM 风格表示在生成任务中的参考，Tessellix 当前未实现生成模型。 |
| `rose_2025_mesh_simplification_edge_collapse_guide.pdf` | 2025 | 边折叠工程清单参考：队列、placement、合法性、边界和误差过滤。 |

## 在线工程参考

| 参考 | 用途 |
| --- | --- |
| CGAL Surface Mesh Simplification | constrained edges、placement 和 stop predicate 的工程参考。 |
| OpenMesh Decimation Framework | cost module + legality module 的架构参考。 |
| libigl `qslim` | 紧凑 QEM 实现参考。 |
| MeshLab / VCGLib | 生产型 decimation filter 和 mesh-quality safeguard 参考。 |
| Yamakawa and Shimada Polygon Crawling | 特征边提取参考。 |

## 与当前实现的关系

Tessellix 当前实现已经落地：QEM、line quadrics、二面角和 normal-tensor 特征证据、圆/近圆/椭圆 loop 拟合、特征曲线保护、边界/拓扑/质量/局部误差/自交过滤。当前没有落地：论文中的完整 edge dihedral plane quadrics、学习式特征评分、时间一致性简化、神经 QEM 表示和通用 CAD/B-Rep 特征恢复。
