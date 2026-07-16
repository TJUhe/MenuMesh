# Remeshing and Mesh Feature Paper Download Status

Date: 2026-07-11

This supplement is limited to triangle/polygon surface meshes. It intentionally excludes B-Rep
reconstruction, solid modeling kernels, CAD feature trees, and volumetric meshing papers.

The PDFs are stored as algorithm and implementation references for ManuMesh tests and experiments.

## Downloaded PDFs

| ID | Area | Paper | Local file | Public source | SHA-256 |
| --- | --- | --- | --- | --- | --- |
| M037 | isotropic remeshing | Isotropic Remeshing of Surfaces: a Local Parameterization Approach | `remeshing/surazhsky_2003_isotropic_remeshing_local_parameterization.pdf` | https://inria.hal.science/inria-00071612/document | `A12EDA89864565D914E17468A7F4EC5BA862103CEF1DF65C9AF71CEF0F817470` |
| M038 | local-operator remeshing | A Remeshing Approach to Multiresolution Modeling | `remeshing/botsch_kobbelt_2004_remeshing_multiresolution_modeling.pdf` | https://www.graphics.rwth-aachen.de/media/papers/remeshing1.pdf | `5073052704EC25768BE9BB5AA6FEBD4E248DFF6307058D6C8805563BC77D8118` |
| M039 | metric/Voronoi remeshing | Generic Remeshing of 3D Triangular Meshes with Metric-Dependent Discrete Voronoi Diagrams | `remeshing/valette_2008_generic_metric_voronoi_remeshing.pdf` | https://hal.science/hal-00537025/document | `B816F3D5EBC69D4BF21EDED84BFA91F88B48962E25D36154DE944F0948C2509F` |
| M040 | adaptive remeshing | Adaptive Remeshing for Real-Time Mesh Deformation | `remeshing/dunyach_2013_adaptive_remeshing_realtime_deformation.pdf` | https://hal.science/hal-01295339/document | `8A8C9AB9AED79425BA2AA7717EEEFCB985A84EE1B63C4CDA119828312087B8C0` |
| M041 | field-aligned meshing | Instant Field-Aligned Meshes | `remeshing/jakob_2015_instant_field_aligned_meshes.pdf` | https://igl.ethz.ch/projects/instant-meshes/instant-meshes-SA-2015-jakob-et-al-compressed.pdf | `55542D125770B4FF76DBE25347FA5549050029629F4350FA30AEC87EE57FF266` |
| M042 | smooth mesh feature lines | Smooth Feature Lines on Surface Meshes | `feature_detection/hildebrandt_2005_smooth_feature_lines_surface_meshes.pdf` | https://diglib.eg.org/server/api/core/bitstreams/7aba3571-6d99-4adf-8410-b5252897b0d8/content | `35AFA013EFFA173F294CCBB18FBDCA7F1159BA851DF7D83D3EF2218F7D38D2AB` |
| M043 | ridge/ravine detection | An Image Processing Approach to Detection of Ridges and Ravines on Polygonal Surfaces | `feature_detection/belyaev_ohtake_2000_ridges_ravines_polygonal_surfaces.pdf` | https://diglib.eg.org/server/api/core/bitstreams/e0baae2c-c3ca-4510-83be-1e364f802948/content | `C052C44345A0C145D78510F5D1C52B80E482A7377BE850A5C86410AF31D71386` |
| M044 | feature curve networks | Feature Curve Network Extraction via Quadric Surface Fitting | `feature_detection/lu_2019_feature_curve_network_quadric_surface_fitting.pdf` | https://diglib.eg.org/server/api/core/bitstreams/6bd087bc-3a67-4837-852f-fd9384dffac0/content | `4778572F8377200DF35119AEB174637328AD929062A21ACE0705774144C9417D` |

## Immediate Engineering Takeaways

1. Start the remesh MVP with repeated edge split, collapse, flip, tangential smoothing, and
   reprojection. Keep target-length and legality decisions in `remeshing`, while reusing
   `mesh_edit` for edit state and compaction.
2. Treat feature edges and boundary loops as constraints, not merely cost weights. Splits may
   follow a feature curve; collapses, flips, and smoothing need stricter ownership checks.
3. Validate remeshing with edge-length distribution, minimum angle/aspect ratio, normal error,
   reference-surface distance, feature drift, boundary preservation, and topology invariants.
4. Keep field-aligned or quad-dominant generation separate from the initial isotropic triangle
   remesher. It needs direction-field singularity and integer-grid reasoning beyond the MVP.
5. For smooth feature detection, stabilize curvature/quadric fitting across scale before tracing a
   graph. A local score without continuity and junction handling is not a usable feature network.
