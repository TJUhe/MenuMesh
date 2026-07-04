# Line Quadrics QEM Theory Bundle

This bundle accompanies `line-quadrics-qem-theory-explained.pdf`.

## Main Note

- `line-quadrics-qem-theory-explained.pdf`  
  Rendered theory note explaining QEM, Line Quadrics, rank/conditioning, boundary and feature-curve quadrics, and the current program's energy interpretation.

## Included Papers

- `garland_heckbert_1997_surface_simplification_qem.pdf`  
  Original QEM paper. Main source for plane quadrics, vertex quadrics, and edge contraction cost.

- `liu_rahimzadeh_zordan_2025_line_quadrics.pdf`  
  Main Line Quadrics paper. Direct source for controlling QEM simplification with point-to-line quadrics.

- `garland_heckbert_1998_color_texture_qem.pdf`  
  QEM extension to attributes. Useful for understanding how additional constraints can be folded into quadric-style error terms.

- `garland_shaffer_2002_efficient_adaptive_simplification_massive_meshes.pdf`  
  Large-mesh adaptive QEM simplification context.

- `hoppe_1996_progressive_meshes.pdf`  
  Classic progressive mesh framework. Relevant for edge-collapse workflow, reconstruction, and discontinuity preservation.

- `lindstrom_turk_1998_fast_memory_efficient_simplification.pdf`  
  Edge-collapse simplification with practical memory-efficient placement and preservation constraints.

- `wang_2008_feature_sensitive_metric.pdf`  
  Feature-sensitive simplification metric. Supports extending position-only QEM with feature/normal information.

- `xu_2024_cwf_consolidating_weak_features.pdf`  
  Weak-feature consolidation for high-quality simplification. Relevant to feature graph and weak feature preservation.

- `hussain_2008_feature_preserving_mesh_simplification_vertex_cover.pdf`  
  Feature-preserving simplification via vertex-cover ideas.

- `jiao_bayyana_2008_identification_c1_c2_discontinuities_surface_meshes_cad.pdf`  
  CAD surface mesh discontinuity detection, relevant to C1/C2 and feature-loop recognition.

- `vidal_wolf_dupont_2011_robust_feature_line_extraction_cad_triangular_meshes.pdf`  
  Robust CAD triangular mesh feature-line extraction.

- `tsuchie_higashi_2014_normal_tensor_surface_feature_lines.pdf`  
  Normal tensor framework for surface feature lines, relevant to normal-tensor feature detection in the current program.

## References Mentioned But Not Bundled As PDFs

- CGAL Surface Mesh Simplification documentation.
- OpenMesh Decimation Framework documentation.
- libigl `qslim`.
- MeshLab / VCGLib simplification filters.
- Yip et al. 2024 triangular-mesh hole detection.
- CHBS-Net 2023 and Zhang et al. 2024 point-cloud circular-hole detection.

Those are referenced as online documentation or were not present as local PDFs in this repository at bundle creation time.

