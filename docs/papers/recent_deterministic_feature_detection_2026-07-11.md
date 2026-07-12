# Recent Deterministic Surface-Mesh Feature References

Snapshot date: 2026-07-11.

This list is restricted to triangle/polygon surface meshes and deterministic
geometry processing. Neural, learned, point-cloud-only, B-Rep, solid-modeling,
and CAD feature-tree methods are excluded from the implementation route.

| ID | Year | Title | DOI / source | Local status | ManuMesh relevance |
| --- | ---: | --- | --- | --- | --- |
| RFD001 | 2017/2018 | Feature Edge Extraction Via Angle-Based Edge Collapsing and Recovery | `10.1115/1.4037227` | Metadata and extracted corpus notes; publisher PDF endpoint is currently protected by an interactive challenge | Scale-independent normal-error simplification and recovery for small fillet-center edges. |
| M044 | 2019 | Feature Curve Network Extraction via Quadric Surface Fitting | `10.2312/pg.20191338` | Local PDF | Primary local-quadric, continuity, junction, and curve-network anchor. |
| RFD002 | 2020 | HT-Based Identification of 3D Feature Curves and Their Insertion into 3D Meshes | `10.1016/j.cag.2020.05.012` | Public metadata; no public PDF recorded | Global curve identification and explicit insertion into a mesh. |
| M026 | 2024 | CWF: Consolidating Weak Features in High-quality Mesh Simplification | `10.1145/3658159` | Local PDF | Weak-feature consolidation and downstream protection policy. |
| RFD003 | 2025 | Feature Line Extraction Based on Winding Number | `10.1016/j.gmod.2025.101296` | Public metadata; no public PDF recorded | Future global evidence for fragmented local feature lines. |

## Implementation selection

The current implementation uses M014 and M044 for local differential geometry,
M042-M043 for smooth ridge/valley semantics, RFD001 for scale exposure, and M026
for weak-evidence policy. RFD002 and RFD003 remain roadmap references because
they require a separate global curve-recovery stage.

The bibliographic metadata was checked through Crossref and OpenAlex on the
snapshot date. Citation counts are intentionally omitted here because they
change over time and do not determine implementation quality.
