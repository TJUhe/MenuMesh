# Paper Index

Canonical paper archive for the `line-quadrics-qem` project.

## Core Papers

| File | Project role | Public source |
| --- | --- | --- |
| `garland_heckbert_1997_surface_simplification_qem.pdf` | Original quadric error metric paper; explains plane quadrics and edge contraction. | https://www.cs.cmu.edu/~garland/Papers/quadrics.pdf |
| `liu_rahimzadeh_zordan_2025_line_quadrics.pdf` | Main paper reproduced by this project; explains line quadrics as soft control for QEM simplification. | https://www.dgp.toronto.edu/~hsuehtil/pdf/lineQuadric.pdf |
| `wang_2008_feature_sensitive_metric.pdf` | Feature-sensitive metric paper; supports the idea that position-only QEM should be extended with normal/feature information. | https://cg.cs.tsinghua.edu.cn/papers/weijin.pdf |
| `xu_2024_cwf_consolidating_weak_features.pdf` | Weak-feature simplification paper; useful for understanding feature alignment, quality, and accuracy as a combined objective. | https://arxiv.org/pdf/2404.15661 |
| `vidal_wolf_dupont_2011_robust_feature_line_extraction_cad_triangular_meshes.pdf` | Robust CAD triangular mesh feature-line extraction; relevant to identifying feature edges before simplification. | https://www.scitepress.org/Papers/2011/33617/33617.pdf |
| `hussain_2008_feature_preserving_mesh_simplification_vertex_cover.pdf` | Feature-preserving simplification using vertex-cover ideas; relevant to protecting small/salient features during contraction. | https://www.grahn.cse.bth.se/Papers/cgv2008.pdf |
| `jiao_bayyana_2008_identification_c1_c2_discontinuities_surface_meshes_cad.pdf` | Identifies C1/C2 discontinuities in CAD surface meshes; relevant to hard/transition feature loop detection. | https://www.ams.sunysb.edu/~jiao/papers/feature_detect.pdf |
| `tsuchie_higashi_2014_normal_tensor_surface_feature_lines.pdf` | Normal tensor framework for surface feature lines; relevant to more robust feature-line extraction beyond simple dihedral thresholds. | https://www.cad-journal.net/files/vol_11/CAD_11%282%29_2014_172-181.pdf |
| `lindstrom_turk_1998_fast_memory_efficient_simplification.pdf` | Edge-collapse simplification with memory-efficient local decisions; useful context for boundary/volume/shape preservation constraints. | https://faculty.cc.gatech.edu/~turk/my_papers/memless_vis98.pdf |
| `hoppe_1996_progressive_meshes.pdf` | Progressive mesh edge-collapse framework; useful context for legality, reconstruction, and constrained simplification workflows. | https://hhoppe.com/pm.pdf |
| `garland_heckbert_1998_color_texture_qem.pdf` | Extends QEM to preserve attributes; useful for treating geometry and auxiliary constraints as separate error terms. | https://www.cs.cmu.edu/~garland/Papers/quadric2.pdf |
| `garland_shaffer_2002_efficient_adaptive_simplification_massive_meshes.pdf` | Large-mesh adaptive simplification; useful for thinking about scalable local simplification policies. | https://mgarland.org/papers/massive.pdf |

## Recent QEM And Mesh Simplification

| File | Year | Paper | Project role |
| --- | ---: | --- | --- |
| `chang_2025_two_round_optimization_qem.pdf` | 2025 | Hui-Huang Chang et al. *Two-Round Optimization Algorithm Based on Quadric Error Metrics*. IEEE Access. DOI: https://doi.org/10.1109/ACCESS.2025.3541436 | Recent QEM variant. Useful for comparing whether a second optimization/refinement pass improves quality after ordinary collapse ordering. |
| `ha_2025_deep_learning_salient_feature_preserving_mesh_simplification.pdf` | 2025 | Lan/Zeng et al. *A Deep Learning-Based Salient Feature-Preserving Algorithm for Mesh Simplification*. Computers, Materials & Continua. DOI: https://doi.org/10.32604/cmc.2025.060260 | Learning-guided feature scoring. Useful if current dihedral/normal-tensor features fail on visually salient but geometrically weak regions. |
| `yokota_2024_tracked_qem_temporal_consistency.pdf` | 2024 | Yokota et al. *Tracked QEM Algorithm: Adding Temporal Consistency to Dynamic Mesh Simplification Based on Mesh Registration*. MTA. DOI: https://doi.org/10.3169/mta.12.175 | Relevant if the target expands from static meshes to sequences. Introduces consistency constraints across frames instead of simplifying each mesh independently. |
| `maruani_2024_ponq_neural_qem_representation.pdf` | 2024 | Maruani et al. *PoNQ: a Neural QEM-based Mesh Representation*. CVPR. Source: https://openaccess.thecvf.com/content/CVPR2024/html/Maruani_PoNQ_a_Neural_QEM-based_Mesh_Representation_CVPR_2024_paper.html | Not a decimator, but treats QEM-like local quadrics as a neural shape representation. Useful for thinking about learned/local anisotropic quadrics. |
| `li_2025_qemesh_qem_based_mesh_generation.pdf` | 2025 | Li et al. *QEMesh: Employing A Quadric Error Metrics-Based Representation for 3D Mesh Generation*. arXiv. Source: https://arxiv.org/abs/2504.05720 | Also not a classical simplifier. Useful as evidence that QEM-style representations remain valuable in neural mesh generation. |
| `rose_2025_mesh_simplification_edge_collapse_guide.pdf` | 2025 | Rose et al. *A Comprehensive Guide to Mesh Simplification using Edge Collapse*. arXiv. Source: https://arxiv.org/abs/2512.19959 | Practical edge-collapse survey/guide. Good for implementation checklists: priority queues, placement, legality, boundaries, and error filters. |

## External References

| Reference | Reason | Link |
| --- | --- | --- |
| CGAL Surface Mesh Simplification documentation | Online documentation, not a paper PDF. It is still important for constrained edges and constrained placement. | https://doc.cgal.org/latest/Surface_mesh_simplification/index.html |
| OpenMesh Decimation Framework documentation | Online documentation, not a paper PDF. It shows the engineering pattern of cost modules plus legality modules. | https://www.graphics.rwth-aachen.de/media/openmesh_static/Documentations/OpenMesh-6.2-Documentation/a00004.html |
| libigl `qslim` implementation | Open-source reference for QEM-style edge-collapse simplification in a compact geometry-processing library. | https://github.com/libigl/libigl |
| MeshLab / VCGLib simplification filters | Open-source reference for production-oriented mesh decimation filters and mesh-quality safeguards. | https://github.com/cnr-isti-vclab/meshlab |
| CGAL constrained edge-map examples | Open-source/reference implementation pattern for keeping selected edges from being collapsed. | https://doc.cgal.org/latest/Surface_mesh_simplification/index.html |
| Yamakawa and Shimada, Polygon Crawling Feature-Edge Extraction | Feature-edge extraction reference. | https://doi.org/10.1007/s00366-009-0165-y |
| A Novel Boundary Extraction Algorithm on Triangular Meshes of STL Model | STL boundary extraction reference. | https://doi.org/10.4028/www.scientific.net/AMR.472-475.2549 |

## Watchlist

| Year | Paper | Source | Status |
| ---: | --- | --- | --- |
| 2026 | *A Structure-Aware Triangular Mesh Simplification Based on Graph Neural Network (GNN)-Guided Quadric Error Metrics (QEM)* | DOI: https://doi.org/10.3390/math14101610 | Candidate reference for GNN-guided QEM. |
| 2025 | *SDF-CWF: Consolidating Weak Features in High-Quality Mesh Extraction from Signed Distance Functions* | DOI: https://doi.org/10.1016/j.cad.2025.103912 | Candidate reference for weak-feature consolidation in SDF mesh extraction. |
