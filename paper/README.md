# Recent QEM / Mesh Simplification Papers

Collected on 2026-07-03 for the `line-quadrics-qem` project. Focus is recent QEM, edge-collapse simplification, feature preservation, and adjacent QEM-based learning methods.

## Downloaded PDFs

| File | Year | Paper | Why it matters here |
| --- | ---: | --- | --- |
| `liu_2025_line_quadrics_qem.pdf` | 2025 | Hsueh-Ti Derek Liu, Mehdi Rahimzadeh, Victor Zordan. *Controlling Quadric Error Simplification with Line Quadrics*. CGF/SGP. DOI: https://doi.org/10.1111/cgf.70184 | Direct baseline for this repo. It addresses flat-region tangential drift by adding line quadrics as a controllable regularizer. |
| `xu_2024_cwf_weak_features_mesh_simplification.pdf` | 2024 | Xu et al. *CWF: Consolidating Weak Features in High-quality Mesh Simplification*. ACM TOG. DOI: https://doi.org/10.1145/3658159 | Strongest match for weak/soft feature retention. Suggests that preserving feature geometry may require consolidation and alignment, not only increasing QEM weights. |
| `chang_2025_two_round_optimization_qem.pdf` | 2025 | Hui-Huang Chang et al. *Two-Round Optimization Algorithm Based on Quadric Error Metrics*. IEEE Access. DOI: https://doi.org/10.1109/ACCESS.2025.3541436 | Recent QEM variant. Useful for comparing whether a second optimization/refinement pass improves quality after ordinary collapse ordering. |
| `ha_2025_deep_learning_salient_feature_preserving_mesh_simplification.pdf` | 2025 | Lan/Zeng et al. *A Deep Learning-Based Salient Feature-Preserving Algorithm for Mesh Simplification*. Computers, Materials & Continua. DOI: https://doi.org/10.32604/cmc.2025.060260 | Learning-guided feature scoring. Useful if current dihedral/normal-tensor features fail on visually salient but geometrically weak regions. |
| `yokota_2024_tracked_qem_temporal_consistency.pdf` | 2024 | Yokota et al. *Tracked QEM Algorithm: Adding Temporal Consistency to Dynamic Mesh Simplification Based on Mesh Registration*. MTA. DOI: https://doi.org/10.3169/mta.12.175 | Relevant if the target expands from static meshes to sequences. Introduces consistency constraints across frames instead of simplifying each mesh independently. |
| `maruani_2024_ponq_neural_qem_representation.pdf` | 2024 | Maruani et al. *PoNQ: a Neural QEM-based Mesh Representation*. CVPR. Source: https://openaccess.thecvf.com/content/CVPR2024/html/Maruani_PoNQ_a_Neural_QEM-based_Mesh_Representation_CVPR_2024_paper.html | Not a decimator, but treats QEM-like local quadrics as a neural shape representation. Useful for thinking about learned/local anisotropic quadrics. |
| `li_2025_qemesh_qem_based_mesh_generation.pdf` | 2025 | Li et al. *QEMesh: Employing A Quadric Error Metrics-Based Representation for 3D Mesh Generation*. arXiv. Source: https://arxiv.org/abs/2504.05720 | Also not a classical simplifier. Useful as evidence that QEM-style representations remain valuable in neural mesh generation. |
| `rose_2025_mesh_simplification_edge_collapse_guide.pdf` | 2025 | Rose et al. *A Comprehensive Guide to Mesh Simplification using Edge Collapse*. arXiv. Source: https://arxiv.org/abs/2512.19959 | Practical edge-collapse survey/guide. Good for implementation checklists: priority queues, placement, legality, boundaries, and error filters. |

## Found But Not Downloaded

| Year | Paper | Source | Status |
| ---: | --- | --- | --- |
| 2026 | *A Structure-Aware Triangular Mesh Simplification Based on Graph Neural Network (GNN)-Guided Quadric Error Metrics (QEM)* | DOI: https://doi.org/10.3390/math14101610 | MDPI official PDF returned HTTP 403 from this environment. Worth manually downloading; likely the most direct "GNN-guided QEM" paper. |
| 2025 | *SDF-CWF: Consolidating Weak Features in High-Quality Mesh Extraction from Signed Distance Functions* | DOI: https://doi.org/10.1016/j.cad.2025.103912 | Elsevier metadata available, PDF not publicly downloadable here. Relevant to weak-feature consolidation, but more about SDF mesh extraction than edge-collapse simplification. |

## Reading Order For This Repo

1. `liu_2025_line_quadrics_qem.pdf`: confirm the repo's current line-quadric energy and parameter assumptions.
2. `xu_2024_cwf_weak_features_mesh_simplification.pdf`: study weak-feature consolidation and compare it with current `preserveFeatureCurves` and normal-tensor detection.
3. `chang_2025_two_round_optimization_qem.pdf`: check whether a post-collapse optimization/refinement pass can improve the current one-pass queue result.
4. `rose_2025_mesh_simplification_edge_collapse_guide.pdf`: use as an engineering checklist for legality filters and quality guards.
5. `ha_2025_deep_learning_salient_feature_preserving_mesh_simplification.pdf` and the missing 2026 GNN-guided QEM paper: read if hand-crafted feature scores are not robust enough.

## Algorithm Improvement Hypotheses

The current code already has standard QEM, line quadrics, boundary terms, feature-loop protection, circular loop fitting/projection, normal-tensor features, quality guards, normal flip guards, and local intersection checks. The newer papers suggest these next experiments:

1. Add a hard tolerance layer after QEM ranking: use QEM/line quadrics to rank candidates, then reject collapses by sampled Hausdorff distance, normal deviation, feature distance, and triangle quality.
2. Separate feature graph simplification from patch-interior simplification: simplify detected feature curves first, preserve junctions/loops, then simplify smooth regions and stitch to the protected graph.
3. Add weak-feature consolidation: before collapse, snap or regularize noisy/nearby weak features into coherent curves; after collapse, reproject vertices to consolidated feature supports.
4. Try a two-round pipeline: first perform conservative simplification, then run a local refinement/relocation pass to reduce geometric and normal error without changing topology too much.
5. Replace single threshold feature scoring with multi-scale or learned scoring: dihedral plus normal tensor is a good start, but weak fillets, shallow ridges, and visually salient details need persistence or saliency scores.
6. Make collapse policies pluggable: cost, placement, and legality filters should be separate modules so Line Quadrics, feature constraints, tolerance checks, and future GNN scores can be compared cleanly.

