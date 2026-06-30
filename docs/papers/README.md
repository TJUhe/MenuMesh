# Paper PDF Index

This directory stores open-access or publicly available PDFs that support the notes in `docs/feature_curve_constraints.md`.

I did not use Sci-Hub or other copyright-bypass sources. When a paper was not available from an author page, institution page, arXiv, conference open page, or other public source, it is listed under "not included" instead of being mirrored here.

## Included PDFs

| File | Why it is included | Public source |
| --- | --- | --- |
| `garland_heckbert_1997_surface_simplification_qem.pdf` | Original quadric error metric paper; explains plane quadrics and edge contraction. | https://www.cs.cmu.edu/~garland/Papers/quadrics.pdf |
| `liu_rahimzadeh_zordan_2025_line_quadrics.pdf` | Main paper reproduced by this project; explains line quadrics as soft control for QEM simplification. | https://www.dgp.toronto.edu/~hsuehtil/pdf/lineQuadric.pdf |
| `wang_2008_feature_sensitive_metric.pdf` | Feature-sensitive metric paper; supports the idea that position-only QEM should be extended with normal/feature information. | https://cg.cs.tsinghua.edu.cn/papers/weijin.pdf |
| `xu_2024_cwf_consolidating_weak_features.pdf` | Weak-feature simplification paper; useful for understanding feature alignment, quality, and accuracy as a combined objective. | https://arxiv.org/pdf/2404.15661 |
| `vidal_wolf_dupont_2011_robust_feature_line_extraction_cad_triangular_meshes.pdf` | Robust CAD triangular mesh feature-line extraction; relevant to identifying feature edges before simplification. | https://www.scitepress.org/Papers/2011/33617/33617.pdf |
| `hussain_2008_feature_preserving_mesh_simplification_vertex_cover.pdf` | Feature-preserving simplification using vertex-cover ideas; relevant to protecting small/salient features during contraction. | https://www.grahn.cse.bth.se/Papers/cgv2008.pdf |
| `jiao_bayyana_2008_identification_c1_c2_discontinuities_surface_meshes_cad.pdf` | Identifies C1/C2 discontinuities in CAD surface meshes; relevant to hard/transition feature loop detection. | https://www.ams.sunysb.edu/~jiao/papers/feature_detect.pdf |
| `tsuchie_higashi_2014_normal_tensor_surface_feature_lines.pdf` | Normal tensor framework for surface feature lines; relevant to more robust feature-line extraction beyond simple dihedral thresholds. | https://www.cad-journal.net/files/vol_11/CAD_11%282%29_2014_172-181.pdf |

## Useful References Not Included As PDFs

| Reference | Reason | Link |
| --- | --- | --- |
| CGAL Surface Mesh Simplification documentation | Online documentation, not a paper PDF. It is still important for constrained edges and constrained placement. | https://doc.cgal.org/latest/Surface_mesh_simplification/index.html |
| OpenMesh Decimation Framework documentation | Online documentation, not a paper PDF. It shows the engineering pattern of cost modules plus legality modules. | https://www.graphics.rwth-aachen.de/media/openmesh_static/Documentations/OpenMesh-6.2-Documentation/a00004.html |
| Yamakawa and Shimada, Polygon Crawling Feature-Edge Extraction | I found citation and landing pages, but not a direct open PDF suitable for mirroring. | https://doi.org/10.1007/s00366-009-0165-y |
| A Novel Boundary Extraction Algorithm on Triangular Meshes of STL Model | I found bibliographic/abstract pages, but not a direct open PDF suitable for mirroring. | https://doi.org/10.4028/www.scientific.net/AMR.472-475.2549 |

## Integrity Check

All files in this folder were checked to begin with the `%PDF` magic bytes before being committed.
