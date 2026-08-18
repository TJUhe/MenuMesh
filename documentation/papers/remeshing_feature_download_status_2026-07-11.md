# 重网格与网格特征论文下载状态

日期：2026-07-11

本补充仅限三角形/多边形表面网格，有意排除 B-Rep 重建、实体建模内核、CAD 特征树和体网格论文。

这些 PDF 作为 ManuMesh 测试和实验的算法与实现参考保存。

## 已下载 PDF

| ID | 领域 | 论文 | 本地文件 | 公共来源 | SHA-256 |
| --- | --- | --- | --- | --- | --- |
| M037 | 各向同性重网格 | Isotropic Remeshing of Surfaces: a Local Parameterization Approach | `remeshing/surazhsky_2003_isotropic_remeshing_local_parameterization.pdf` | https://inria.hal.science/inria-00071612/document | `A12EDA89864565D914E17468A7F4EC5BA862103CEF1DF65C9AF71CEF0F817470` |
| M038 | 局部算子重网格 | A Remeshing Approach to Multiresolution Modeling | `remeshing/botsch_kobbelt_2004_remeshing_multiresolution_modeling.pdf` | https://www.graphics.rwth-aachen.de/media/papers/remeshing1.pdf | `5073052704EC25768BE9BB5AA6FEBD4E248DFF6307058D6C8805563BC77D8118` |
| M039 | 度量/Voronoi 重网格 | Generic Remeshing of 3D Triangular Meshes with Metric-Dependent Discrete Voronoi Diagrams | `remeshing/valette_2008_generic_metric_voronoi_remeshing.pdf` | https://hal.science/hal-00537025/document | `B816F3D5EBC69D4BF21EDED84BFA91F88B48962E25D36154DE944F0948C2509F` |
| M040 | 自适应重网格 | Adaptive Remeshing for Real-Time Mesh Deformation | `remeshing/dunyach_2013_adaptive_remeshing_realtime_deformation.pdf` | https://hal.science/hal-01295339/document | `8A8C9AB9AED79425BA2AA7717EEEFCB985A84EE1B63C4CDA119828312087B8C0` |
| M041 | 方向场对齐网格 | Instant Field-Aligned Meshes | `remeshing/jakob_2015_instant_field_aligned_meshes.pdf` | https://igl.ethz.ch/projects/instant-meshes/instant-meshes-SA-2015-jakob-et-al-compressed.pdf | `55542D125770B4FF76DBE25347FA5549050029629F4350FA30AEC87EE57FF266` |
| M042 | 平滑网格特征线 | Smooth Feature Lines on Surface Meshes | `feature_detection/hildebrandt_2005_smooth_feature_lines_surface_meshes.pdf` | https://diglib.eg.org/server/api/core/bitstreams/7aba3571-6d99-4adf-8410-b5252897b0d8/content | `35AFA013EFFA173F294CCBB18FBDCA7F1159BA851DF7D83D3EF2218F7D38D2AB` |
| M043 | ridge/ravine 检测 | An Image Processing Approach to Detection of Ridges and Ravines on Polygonal Surfaces | `feature_detection/belyaev_ohtake_2000_ridges_ravines_polygonal_surfaces.pdf` | https://diglib.eg.org/server/api/core/bitstreams/e0baae2c-c3ca-4510-83be-1e364f802948/content | `C052C44345A0C145D78510F5D1C52B80E482A7377BE850A5C86410AF31D71386` |
| M044 | 特征曲线网络 | Feature Curve Network Extraction via Quadric Surface Fitting | `feature_detection/lu_2019_feature_curve_network_quadric_surface_fitting.pdf` | https://diglib.eg.org/server/api/core/bitstreams/6bd087bc-3a67-4837-852f-fd9384dffac0/content | `4778572F8377200DF35119AEB174637328AD929062A21ACE0705774144C9417D` |

## 直接的工程启示

1. 从重复的边 split、collapse、flip、切向平滑和重新投影开始构建 remesh MVP。将目标长度和合法性决策放在
   `remeshing` 中，同时复用 `mesh_edit` 管理编辑状态和压缩。
2. 将特征边和边界环视为约束，而不只是代价权重。split 可以沿特征曲线进行；collapse、flip 和平滑需要更严格的归属检查。
3. 使用边长分布、最小角/长宽比、法向误差、参考曲面距离、特征漂移、边界保持和拓扑不变量验证重网格。
4. 将方向场对齐或四边形主导的生成与初始各向同性三角重网格器分开。它需要超出 MVP 范围的方向场奇点和整数网格推理。
5. 对后续曲线识别研究，在追踪图之前先稳定局部几何证据；没有连续性和 junction 处理的局部分数不是可用的特征网络。该路线不属于当前运行时管线。
