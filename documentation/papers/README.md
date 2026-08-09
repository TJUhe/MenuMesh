# 论文索引

本目录保存 ManuMesh 的论文 PDF 归档。PDF 按算法或研究方向分组，文件名保留英文，便于和 DOI、论文标题及外部引用链接对应。

M001-M036 的引用数量来自 2026-07-09 的 OpenAlex `cited_by_count` 快照；M037-M044
来自 2026-07-11 的补充查询。该数值可能与 Google Scholar、出版商统计或后续查询结果不同。

如需理解当前实现和路线图，请先读 [`../design/algorithm_essence.md`](../design/algorithm_essence.md)，再按本文分组选择论文。

## 目录分组

| 目录 | 算法方向 | 用途 |
| --- | --- | --- |
| `qem/` | 标准 QEM、QEM 属性扩展和近期 QEM 变体 | ManuMesh 当前 decimation 代价函数的基础。 |
| `line_quadrics/` | line quadrics 控制 QEM | ManuMesh 当前复现和扩展的主参考。 |
| `feature_detection/` | CAD/STL 特征线、normal tensor、normal voting、crest/ridge 和神经线框提取 | 支撑特征检测模块、feature graph 和后续识别路线。 |
| `segmentation/` | 工程对象分割和解析面恢复 | 支撑后续 patch/analytic surface recovery 路线。 |
| `weak_features/` | 弱特征整合 | 支撑后续弱特征保护和高质量简化路线。 |
| `feature_preserving_simplification/` | 特征保持简化、特征敏感度和学习式显著特征保护 | 支撑特征保护策略和后续显著性评分路线。 |
| `edge_collapse/` | 边折叠框架、progressive mesh、局部 placement 和大模型简化 | 支撑 collapse workflow、队列和合法性检查。 |
| `neural_and_temporal_qem/` | 神经 QEM 表示和时间一致性 QEM | 后续研究参考，当前未实现。 |
| `mesh_generation/` | QEM 风格网格生成 | 后续生成式网格任务参考，当前未实现。 |
| `remeshing/` | 三角表面网格的各向同性、自适应、Voronoi/CVT 和方向场重网格化 | 支撑未来 remesh 算法和 `mesh_edit` 扩展。 |

## QEM 基础与变体

| ID | 论文标题与引用数量 | 本地 PDF | DOI / ID | 在 ManuMesh 中的作用 |
| --- | --- | --- | --- | --- |
| M001 | Two-Round Optimization Algorithm Based on Quadric Error Metrics (OpenAlex citations: 3) | `qem/chang_2025_two_round_optimization_qem.pdf` | `10.1109/ACCESS.2025.3541436` | 近期二轮 QEM 优化参考，可用于后处理和 refinement 路线。 |
| M002 | Surface Simplification Using Quadric Error Metrics (OpenAlex citations: 3386) | `qem/garland_heckbert_1997_surface_simplification_qem.pdf` | `10.1145/258734.258849` | 原始 QEM 论文，解释 plane quadric、vertex quadric 和 edge contraction cost。ManuMesh 的正常面按面积/3 累加；退化面仅加 `1e-6 * representativeArea * pointQuadric`，保持与正常 QEM 相同的 L^4 缩放量纲。 |
| M003 | Simplifying Surfaces with Color and Texture Using Quadric Error Metrics (OpenAlex citations: 246) | `qem/garland_heckbert_1998_color_texture_qem.pdf` | `10.1109/VISUAL.1998.745312` | QEM 属性扩展参考，说明如何把颜色、纹理等属性并入 quadric 风格误差项。 |

## Line Quadrics（线二次型）

| ID | 论文标题与引用数量 | 本地 PDF | DOI / ID | 在 ManuMesh 中的作用 |
| --- | --- | --- | --- | --- |
| M004 | Controlling Quadric Error Simplification with Line Quadrics (OpenAlex citations: 1) | `line_quadrics/liu_rahimzadeh_zordan_2025_line_quadrics.pdf` | `10.1111/cgf.70184` | ManuMesh 当前复现和扩展的主参考，用于控制 QEM 平坦区切向漂移。 |

## 特征检测与线框提取

| ID | 论文标题与引用数量 | 本地 PDF | DOI / ID | 在 ManuMesh 中的作用 |
| --- | --- | --- | --- | --- |
| M005 | A Variational Approach for Detecting Feature Lines on Meshes (OpenAlex citations: 7) | `feature_detection/benhabiles_2011_variational_feature_lines_meshes.pdf` | `10.4208/jcm.1510-m4510` | 变分特征线参考，用于替换脆弱局部启发式的长期路线。 |
| M006 | D-FRAME: Direction-Field-Based Wireframe Extraction for Complex CAD Models (OpenAlex citations: 0) | `feature_detection/feng_2025_dframe_direction_field_wireframe_extraction_cad.pdf` | `10.1109/TVCG.2025.3609350` | 复杂 CAD 方向场线框提取参考，用于后续图级特征恢复。 |
| M007 | Identification of C1 and C2 Discontinuities for Surface Meshes in CAD (OpenAlex citations: 26) | `feature_detection/jiao_bayyana_2008_identification_c1_c2_discontinuities_surface_meshes_cad.pdf` | `10.1016/j.cad.2007.10.005` | CAD 表面 C1/C2 不连续检测参考；其"junction = ridge valence ≥ 3 的图分支点"判据已在 `FeatureGraph.cpp` 的 `finalizeFeatureGraphMarkers` 落地为图级 junction 报告（2026-07-13，替代按 loop 共享标记的旧语义，chamfer box junction precision 0.29→1.0）。 |
| M008 | PC2WF: 3D Wireframe Reconstruction from Raw Point Clouds (OpenAlex citations: 3) | `feature_detection/liu_2021_pc2wf_wireframe_reconstruction_raw_point_clouds.pdf` | `10.48550/arxiv.2103.02766` | 原始点云到线框重建参考，用于 feature graph 目标设计。 |
| M009 | Multi-Scale Creases Detection on Noisy Meshes (OpenAlex citations: 1) | `feature_detection/luo_zha_2008_multiscale_creases_detection_noisy_meshes.pdf` | `10.1109/ICIP.2008.4712166` | 噪声网格多尺度 crease 检测参考。 |
| M010 | DEF: Deep Estimation of Sharp Geometric Features in 3D Shapes (OpenAlex citations: 41) | `feature_detection/matveev_2022_def_deep_estimation_sharp_geometric_features.pdf` | `10.1145/3528223.3530140` | 尖锐几何特征估计和基准参考，当前不作为核心依赖。 |
| M011 | Ridge-Valley Lines on Meshes via Implicit Surface Fitting (OpenAlex citations: 382) | `feature_detection/ohtake_2004_ridge_valley_lines_implicit_surface_fitting.pdf` | `10.1145/1015706.1015768` | smooth ridge/valley 特征线参考；其边零交叉极值判据、一阶极大测试和反比插值归属已在 `SmoothCurvature.cpp` 落地（2026-07-12）。 |
| M012 | Robust Crease Detection and Curvature Estimation of Piecewise Smooth Surfaces from Triangle Mesh Approximations Using Normal Voting (OpenAlex citations: 60) | `feature_detection/page_koschan_sun_paik_abidi_2001_robust_crease_detection_normal_voting.pdf` | `10.1109/CVPR.2001.990471` | piecewise-smooth 三角网格 normal voting crease 检测参考。 |
| M013 | Normal Vector Voting: Crease Detection and Curvature Estimation on Large, Noisy Meshes (OpenAlex citations: 121) | `feature_detection/page_sun_koschan_paik_abidi_2002_normal_vector_voting_crease_detection_curvature_estimation.pdf` | `10.1006/gmod.2002.0574` | 大型噪声网格 normal vector voting 参考。 |
| M014 | Estimating Curvatures and Their Derivatives on Triangle Meshes (OpenAlex citations: 294) | `feature_detection/rusinkiewicz_2004_estimating_curvatures_derivatives_triangle_meshes.pdf` | `10.1109/TDPVT.2004.1335277` | 曲率和曲率导数估计基线，用于 ridge/valley 和 soft feature 路线。 |
| M015 | Extraction of Surface-Feature Lines on Meshes Using Normal Tensor Framework (OpenAlex citations: 3) | `feature_detection/tsuchie_higashi_2014_normal_tensor_surface_feature_lines.pdf` | `10.1080/16864360.2014.846088` | normal tensor 特征评分参考，当前 `normal-tensor` 模式受它启发。 |
| M016 | Robust Feature Line Extraction on CAD Triangular Meshes (OpenAlex citations: 11) | `feature_detection/vidal_wolf_dupont_2011_robust_feature_line_extraction_cad_triangular_meshes.pdf` | `10.5220/0003361701620167` | CAD 三角网格特征线提取参考，对当前 feature graph 和 loop 检测有启发。 |
| M017 | PIE-NET: Parametric Inference of Point Cloud Edges (OpenAlex citations: 43) | `feature_detection/wang_2020_pienet_parametric_inference_point_cloud_edges.pdf` | `10.48550/arxiv.2007.04883` | 点云参数化边推理参考，用于后续 primitive fitting 和点云路线。 |
| M018 | Polygon Crawling: Feature-Edge Extraction from a General Polygonal Surface for Mesh Generation (OpenAlex citations: 12) | `feature_detection/yamakawa_2005_polygon_crawling_feature_edge_extraction.pdf` | `10.1007/3-540-29090-7_15` | CAD/STL polygon crawling 特征边提取参考。 |
| M019 | Polygon Crawling: Feature Edge Extraction from a General Polygonal Surface for Mesh Generation (OpenAlex citations: 7) | `feature_detection/yamakawa_shimada_2009_polygon_crawling_feature_edge_extraction.pdf` | `10.1007/s00366-009-0165-y` | polygon crawling 期刊扩展，适合非均匀 CAD facet surface。 |
| M020 | NEF: Neural Edge Fields for 3D Parametric Curve Reconstruction from Multi-View Images (OpenAlex citations: 26) | `feature_detection/ye_2023_nef_neural_edge_fields_curve_reconstruction.pdf` | `10.1109/CVPR52729.2023.00820` | 多视图神经边场曲线重建参考，当前不作为核心依赖。 |
| M021 | Fast and Robust Detection of Crest Lines on Meshes (OpenAlex citations: 5) | `feature_detection/yoshizawa_2005_fast_robust_detection_crest_lines.pdf` | `10.1145/1060244.1060270` | crest-line 提取参考；其三次拟合解析 extremality、组件级曲线强度 T = (∫ds)·(∫strength ds) 过滤和 gap 桥接角度规则已分别在 `SmoothCurvature.cpp` 与 `FeatureGraphCleanup.cpp` 落地（2026-07-12）；其 Eq.5-6 cyclideness 已进一步落地为零交叉门控 `kMinCrossingCyclidenessRatio = 0.15`（无量纲 mean|e|/κ² 比值，消除环面等 Dupin cyclide 上的伪 ridge/valley，2026-07-13）。 |
| M022 | EC-Net: An Edge-Aware Point Set Consolidation Network (OpenAlex citations: 302) | `feature_detection/yu_2018_ecnet_edge_aware_point_set_consolidation.pdf` | `10.1007/978-3-030-01234-2_24` | 边感知点集 consolidation 参考，用于扫描件预处理和去噪路线。 |
| M023 | NerVE: Neural Volumetric Edges for Parametric Curve Extraction from Point Cloud (OpenAlex citations: 31) | `feature_detection/zhu_2023_nerve_neural_volumetric_edges.pdf` | `10.1109/CVPR52729.2023.01307` | 点云参数曲线连续性和 junction 参考，当前不作为核心依赖。 |

## 分割与解析面恢复

| ID | 论文标题与引用数量 | 本地 PDF | DOI / ID | 在 ManuMesh 中的作用 |
| --- | --- | --- | --- | --- |
| M024 | An Edge-Based Mesh Segmentation Method for Engineering Objects (OpenAlex citations: 1) | `segmentation/liu_ramani_2010_edge_based_mesh_segmentation_engineering_objects.pdf` | `10.1109/MACE.2010.5536720` | 工程对象 edge-based segmentation 参考，用于闭合 feature loop 和 C1/C2 处理。 |
| M025 | Segmentation of Scanned Mesh into Analytic Surfaces Based on Robust Curvature Estimation and Region Growing (OpenAlex citations: 19) | `segmentation/mizoguchi_2006_scanned_mesh_analytic_surfaces_region_growing.pdf` | `10.1007/11802914_52` | 扫描网格解析面分割参考，用于 robust curvature 和 region growing 路线。 |

## 弱特征整合

| ID | 论文标题与引用数量 | 本地 PDF | DOI / ID | 在 ManuMesh 中的作用 |
| --- | --- | --- | --- | --- |
| M026 | CWF: Consolidating Weak Features in High-quality Mesh Simplification (OpenAlex citations: 18) | `weak_features/xu_2024_cwf_consolidating_weak_features_mesh_simplification.pdf` | `10.1145/3658159` | 弱特征整合参考，用于高质量简化前的 coherent feature support 路线。 |

## 特征保持简化

| ID | 论文标题与引用数量 | 本地 PDF | DOI / ID | 在 ManuMesh 中的作用 |
| --- | --- | --- | --- | --- |
| M027 | A Deep Learning-Based Salient Feature-Preserving Algorithm for Mesh Simplification (OpenAlex citations: 1) | `feature_preserving_simplification/ha_2025_deep_learning_salient_feature_preserving_mesh_simplification.pdf` | `10.32604/cmc.2025.060260` | 学习式显著特征保护参考，ManuMesh 当前未实现学习模型。 |
| M028 | Feature-Preserving Mesh Simplification: A Vertex Cover Approach (OpenAlex citations: 6) | `feature_preserving_simplification/hussain_2008_feature_preserving_mesh_simplification_vertex_cover.pdf` | N/A | 顶点覆盖式特征保持简化参考，用于理解小特征保护。 |
| M029 | Feature Preserving Mesh Simplification Using Feature Sensitive Metric (OpenAlex citations: 29) | `feature_preserving_simplification/wang_2008_feature_sensitive_metric.pdf` | `10.1007/s11390-010-9331-7` | feature-sensitive metric 参考；其"blow-up 权重只进队列优先级、不进 placement 求解"的解耦模式已在 `adaptiveScale` 的 `priorityScale` 通道落地（2026-07-12），6D 扩维本身未采用。 |

## 边折叠与大模型简化

| ID | 论文标题与引用数量 | 本地 PDF | DOI / ID | 在 ManuMesh 中的作用 |
| --- | --- | --- | --- | --- |
| M030 | Efficient Adaptive Simplification of Massive Meshes (OpenAlex citations: 84) | `edge_collapse/garland_shaffer_2002_efficient_adaptive_simplification_massive_meshes.pdf` | `10.1109/VISUAL.2001.964503` | 大模型自适应简化参考。 |
| M031 | Progressive Meshes (OpenAlex citations: 2813) | `edge_collapse/hoppe_1996_progressive_meshes.pdf` | `10.1145/237170.237216` | Progressive Mesh 框架参考，用于理解 collapse workflow 和重建；开边界"虚拟顶点"扩展 link condition（边界弦 pinch 拒绝）已在 `CollapseTopology.cpp` 落地（2026-07-12）。 |
| M032 | Fast and Memory Efficient Polygonal Simplification (OpenAlex citations: 151) | `edge_collapse/lindstrom_turk_1998_fast_memory_efficient_simplification.pdf` | `10.1109/VISUAL.1998.745314` | 局部边折叠、内存效率和约束保持参考；其边界守恒约束（§4.2.2）已作为边界边折叠 placement 在 `Placement.cpp` 落地（2026-07-12）。 |
| M033 | A Comprehensive Guide to Mesh Simplification using Edge Collapse (OpenAlex citations: 0) | `edge_collapse/rose_2025_mesh_simplification_edge_collapse_guide.pdf` | `10.48550/arXiv.2512.19959` | 边折叠工程清单参考：队列、placement、合法性、边界和误差过滤。当前局部相交 guard 检查新一环内部及附近活动面，并通过共享拓扑感知谓词允许合法共享顶点/边接触；它不是全局无自交认证。 |

## 神经与时间一致性 QEM

| ID | 论文标题与引用数量 | 本地 PDF | DOI / ID | 在 ManuMesh 中的作用 |
| --- | --- | --- | --- | --- |
| M034 | PoNQ: a Neural QEM-based Mesh Representation (OpenAlex citations: 13) | `neural_and_temporal_qem/maruani_2024_ponq_neural_qem_representation.pdf` | `10.1109/CVPR52733.2024.00350` | 神经 QEM 表示参考，不是当前 decimator 的实现基础。 |
| M035 | Tracked QEM Algorithm: Adding Temporal Consistency to Dynamic Mesh Simplification Based on Mesh Registration (OpenAlex citations: 0) | `neural_and_temporal_qem/yokota_2024_tracked_qem_temporal_consistency.pdf` | `10.3169/mta.12.175` | 动态序列一致性参考；ManuMesh 当前只处理静态网格。 |

## QEM 风格网格生成

| ID | 论文标题与引用数量 | 本地 PDF | DOI / ID | 在 ManuMesh 中的作用 |
| --- | --- | --- | --- | --- |
| M036 | QEMesh: Employing A Quadric Error Metrics-Based Representation for 3D Mesh Generation (OpenAlex citations: 0) | `mesh_generation/li_2025_qemesh_qem_based_mesh_generation.pdf` | `10.48550/arXiv.2504.05720` | QEM 风格表示在生成任务中的参考，ManuMesh 当前未实现生成模型。 |

## 三角表面重网格化

| ID | 论文标题与引用数量 | 本地 PDF | DOI / ID | 在 ManuMesh 中的作用 |
| --- | --- | --- | --- | --- |
| M037 | Isotropic Remeshing of Surfaces: a Local Parameterization Approach (OpenAlex citations: 127) | `remeshing/surazhsky_2003_isotropic_remeshing_local_parameterization.pdf` | `inria-00071612` | 局部参数化各向同性 remeshing 基线，强调采样、投影和局部三角化。 |
| M038 | A Remeshing Approach to Multiresolution Modeling (OpenAlex citations: 260) | `remeshing/botsch_kobbelt_2004_remeshing_multiresolution_modeling.pdf` | `10.1145/1057432.1057457` | split/collapse/flip/smooth 局部操作和特征约束的核心工程参考。 |
| M039 | Generic Remeshing of 3D Triangular Meshes with Metric-Dependent Discrete Voronoi Diagrams (OpenAlex citations: 218) | `remeshing/valette_2008_generic_metric_voronoi_remeshing.pdf` | `10.1109/TVCG.2007.70430` | metric-dependent discrete Voronoi remeshing，用于理解各向异性度量和采样密度控制。 |
| M040 | Adaptive Remeshing for Real-Time Mesh Deformation (OpenAlex citations: 40) | `remeshing/dunyach_2013_adaptive_remeshing_realtime_deformation.pdf` | `10.2312/conf/eg2013/short/029-032` | 实时局部自适应 remeshing，适合设计增量更新、长度场和局部操作调度。 |
| M041 | Instant Field-Aligned Meshes (OpenAlex citations: 335) | `remeshing/jakob_2015_instant_field_aligned_meshes.pdf` | `10.1145/2816795.2818078` | 方向场对齐的三角/四边网格生成参考；当前只作为远期方向场路线，不属于 B-Rep 重建。 |

## 表面网格特征检测补充

| ID | 论文标题与引用数量 | 本地 PDF | DOI / ID | 在 ManuMesh 中的作用 |
| --- | --- | --- | --- | --- |
| M042 | Smooth Feature Lines on Surface Meshes (OpenAlex citations: 122) | `feature_detection/hildebrandt_2005_smooth_feature_lines_surface_meshes.pdf` | `10.2312/SGP.SGP05.085-090` | 光滑曲率特征线参考，用于补足二面角之外的 ridge/valley/crest 路线。 |
| M043 | An Image Processing Approach to Detection of Ridges and Ravines on Polygonal Surfaces (OpenAlex citations: 15) | `feature_detection/belyaev_ohtake_2000_ridges_ravines_polygonal_surfaces.pdf` | `10.2312/egs.20001016` | 直接面向 polygonal surface 的 ridge/ravine 检测参考。 |
| M044 | Feature Curve Network Extraction via Quadric Surface Fitting (OpenAlex citations: 14) | `feature_detection/lu_2019_feature_curve_network_quadric_surface_fitting.pdf` | `10.2312/pg.20191338` | quadric surface fitting、曲线连续性和 junction/network 组织参考。 |

## 与当前实现的关系

ManuMesh 当前实现已经落地：QEM、line quadrics、有向二面角、normal-tensor、opt-in smooth-curvature 与 stable-scale、独立 loop trace、法线域 evidence stabilization、cleanup + compatible component consolidation、逐 junction branch pairing、圆/近圆/椭圆 loop、component confidence、feature-induced surface patches，以及边界/拓扑/质量/局部误差/自交过滤。关键算法落点包括 `FeatureNormalFilter.cpp`、`SmoothCurvature.cpp`、`FeatureGraphCompatibility.cpp`、`FeatureGraphConsolidation.cpp`、`FeatureSegmentation.cpp` 和 `FeatureBenchmark.cpp`。完整 2026-07-15 对照见 [`../design/feature_recognition_system_upgrade_2026_07_15.md`](../design/feature_recognition_system_upgrade_2026_07_15.md)。

当前没有落地：完整 edge dihedral plane quadrics、Rusinkiewicz per-face 张量曲率估计器、Page 全投票场与三路相对分类、Vidal graph-cut 链化、会移动顶点的 variational/L0/non-local 扫描去噪、analytic surface fitting/patch merge、全局 Hough/winding-number recovery、学习式特征评分、时间一致性简化、神经 QEM 和完整 remeshing。ManuMesh 的目标是三角表面网格处理，不把 B-Rep/CAD feature-tree 重建作为本库范围。

### 补充论文与开源对照（未收录本地 PDF）

| 方向 | 参考 | 对当前实现的意义 |
| --- | --- | --- |
| Graph spectral feature-preserving denoising | DOI `10.1109/TVCG.2018.2802926` | 说明全局频谱/特征保持去噪比当前局部 normal relaxation 更完整，也更重。 |
| Dynamic adaptive mesh denoising | DOI `10.1016/j.gmod.2020.101065` | 动态尺度与噪声强度自适应参考。 |
| Segmentation-driven denoising | DOI `10.1007/s00371-023-03161-w` | 将 patch/region 语义反向用于去噪；当前 ManuMesh 只输出 connectivity patches。 |
| Winding-number feature line | DOI `10.1016/j.gmod.2025.101296` | 局部曲线碎裂时的全局 recovery 对照；未读取全文前不推断公式细节。 |
| 开源实现 | MeshLib、L0Denoising、NLLR、LSD，见 [`open_source_mesh_libraries.md`](open_source_mesh_libraries.md) | 用于 scan preprocessing 结果和失败模式对比，不作为当前运行时依赖。 |

## 按问题阅读

| 想理解的问题 | 推荐阅读 | 读完应回到的代码 |
| --- | --- | --- |
| 为什么标准 QEM 在平面区域会排序退化？ | M002，然后读 M004。 | `src/simplification/Quadrics.cpp` |
| line quadrics 为什么是正则项而不是替代 QEM？ | M004，配合 `documentation/generated/notes/qem-line-quadrics-notes.html`。 | `lineQuadric()`、`computeInitialQuadrics()` |
| 为什么不能只调大特征权重？ | M026、M028、M029、M033。 | `FeatureConstraints.cpp`、`CollapseLegality.cpp` |
| CAD/STL 特征边为什么优先用二面角、边界和 loop tracing？ | M007、M016、M018、M019。 | `FeatureEvidence.cpp`、`FeatureGraph.cpp`、`FeatureLoopRecovery.cpp` |
| normal tensor 的特征值怎么解释？ | M012、M013、M015。 | `NormalTensor.cpp` |
| 轻中度 noisy normals 如何稳定？ | M009、M012、M013、M015，并与 MeshLib/L0Denoising/NLLR/LSD 对照。 | `FeatureNormalFilter.cpp`；注意它不移动顶点 |
| 弱 component 为什么以及如何连接？ | M026、M044。 | `FeatureGraphCompatibility.cpp`、`FeatureGraphConsolidation.cpp` |
| feature edge 如何变成 surface patches？ | M024、M025。 | `FeatureSegmentation.cpp`；当前只做 connectivity partition |
| 圆/椭圆 loop 为什么要拟合 primitive？ | M016、M024、M025，配合当前 feature fixture；拟合核的出处是 Taubin 1991 与 Halíř-Flusser 1998（经典文献，无本地 PDF）。 | `PrimitiveFit.cpp` |
| 如果要补 ridge/valley/crest line 应看什么？ | M005、M011、M014、M021、M042。 | `src/feature_detection/SmoothCurvature.cpp`（三次拟合 + 零交叉已落地；M014 张量估计器仍是路线图） |
| 为什么需要 topology/quality/error/self-intersection filters？ | M031、M032、M033。 | `CollapseLegality.cpp`、`src/common/GeometryPredicates.cpp`、`src/common/SpatialIndex.cpp` |
| 下一步如果做全局误差 envelope 应看什么？ | M030、M033、M026。 | 未来 `validation` 或 `simplification/detail` 中独立 envelope filter |
| remesh 的局部编辑循环如何组织？ | M038、M040，然后对照 OpenMesh 和 pmp-library。 | `src/mesh_edit/` 与未来 `src/remeshing/` |
| 如何设计各向同性或度量驱动的目标长度场？ | M037、M039。 | 未来 remeshing sampling/metric policy |
| 如何让重网格化保留当前 feature graph？ | M038、M042-M044。 | `feature::FeatureAnalysis`、未来 remeshing constraints |
| 方向场重网格化是否属于当前近期目标？ | M041。 | 远期独立 field-aligned pipeline，不进入基础 remesh MVP |

## 下载状态

下载状态、来源 URL、失败记录和 OpenAlex 结果快照见：

- [`feature_recognition_download_status.md`](feature_recognition_download_status.md)
- [`paper_index_openalex_2026-07-09.json`](paper_index_openalex_2026-07-09.json)
- [`remeshing_feature_download_status_2026-07-11.md`](remeshing_feature_download_status_2026-07-11.md)
- [`paper_index_supplement_2026-07-11.json`](paper_index_supplement_2026-07-11.json)
- [`open_source_mesh_libraries.md`](open_source_mesh_libraries.md)
