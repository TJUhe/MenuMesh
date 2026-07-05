# Feature Protection Roadmap

This note records the current conclusion after validating the simplifier on
new external meshes and checking related literature/open-source designs.

## Problem Found

The current `--preserve-feature-curves` mode is too hard for fragmented
industrial STL feature graphs.

New validation outputs under `tests/output/new_model_validation/` show:

| Model | Line result | Curve result | Main issue |
| --- | --- | --- | --- |
| `nasa_cubesat_middle` | reached target at 5772 faces | stopped at 17674 faces | Too many feature vertices are hard-protected. |
| `nasa_mars2020_wheel` | reached target at 9066 faces | stopped at 11038 faces | Loop vertex budget and hard feature ownership over-constrain collapse. |
| `thingi10k_differential_gear` | reached target at 1236 faces | stopped at 2670 faces | Dense gear features cause high error and rejection-limit termination. |
| `fandisk_2014` | reached target | reached target but rejected generic feature collapses | Non-circular feature loops are still hard-protected. |
| `rocker_arm_large` | reached target | reached target but rejected generic feature collapses | `preserve-feature-curves` is broader than its name implies. |

The topology counters stayed clean in this probe: no unexpected boundary or
non-manifold edges were introduced. The main failure is therefore policy
over-constraint, not basic mesh validity.

## External Designs To Borrow From

The distilled literature and open-source survey point to a common pattern:
QEM should rank candidates, while independent policies decide placement,
envelope/tolerance, topology, quality, and feature behavior.

- **CGAL Surface Mesh Simplification**: policy-based cost, placement, filters,
  and constrained placement. The key lesson is not to encode every rule inside
  one collapse predicate.
- **OpenMesh Decimater**: one continuous priority module plus binary legality
  modules such as normal deviation, Hausdorff, aspect ratio, and topology. The
  key lesson is to separate scoring from vetoes.
- **MeshLab/VCG decimation**: quadric collapse with topology, boundary, quality,
  planar, and weighting knobs. The key lesson is to prefer soft penalties for
  many feature classes instead of hard-locking every detected edge.
- **CWF weak-feature simplification**: consolidate weak/fragmented features
  before decimation. The key lesson is that feature graphs need cleanup and
  ownership before they are used as constraints.
- **Line quadrics and recent QEM work**: line/feature quadrics are useful
  ranking terms, but they do not replace explicit topology, quality, and
  tolerance filters.

## Planned Algorithm Direction

1. Add a `FeatureProtectionMode` concept:
   - `none`
   - `primitive-curves`
   - `circular-only`
   - `all-feature-edges`
2. Make the default curve mode hard-protect only primitive curves:
   `circle`, `near-circle`, and `ellipse`.
3. Treat generic polygonal/dihedral feature loops as soft costs by default:
   feature weights, normal-deviation guards, and optional placement filters,
   but not automatic feature-to-non-feature collapse vetoes.
4. Split feature handling into independent modules:
   - feature cost weighting
   - feature placement projection
   - feature distance/envelope filter
   - topology and triangle-quality filters
5. Add multi-loop ownership for dense CAD/STL graphs:
   split high-degree feature components into simple cycles, fit primitives per
   cycle, and preserve ownership through simplification.
6. Keep validation honest:
   report `rejection-limit`, feature rejected collapse counts, projected
   placements, face target miss, and feature-compare recall instead of hiding
   bad cases behind procedural models.

## Expected Impact

The goal is to keep the current SDK-style deterministic simplifier while
reducing over-protection on imported industrial models:

- Mars wheel-like models should reach target faces without losing all circular
  features.
- Gear-like models should stop producing large curve-mode distance errors.
- Fandisk/rocker-arm hard edges should influence cost and quality filters
  without being treated as protected circular curves.
