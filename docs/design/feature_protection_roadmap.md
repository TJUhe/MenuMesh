# Feature Protection Roadmap

This note records the current conclusion after validating the simplifier on
external meshes and checking related literature/open-source designs. Stage 1
of the roadmap is now implemented in the SDK API and CLI.

## Problem Found Before Stage 1

The old `--preserve-feature-curves` mode was too hard for fragmented
industrial STL feature graphs because every detected feature loop could become
a hard collapse constraint.

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

## Implemented Stage 1

1. Added `FeatureProtectionMode`:
   - `none`
   - `circular-only`
   - `primitive-curves`
   - `all-feature-edges`
2. Made `primitive-curves` the default hard policy when
   `preserveFeatureCurves` is enabled. It hard-protects only `circle`,
   `near-circle`, and `ellipse` primitives.
3. Moved generic polygonal/dihedral feature loops out of the default hard
   collapse veto. They still affect ranking through feature curve quadrics and
   line-quadric feature weighting, then pass through the existing topology,
   normal-deviation, triangle-quality, local-error, and optional intersection
   filters.
4. Preserved the old strict behavior as `all-feature-edges` and through the
   legacy `protectAllFeatureEdges` / `--protect-all-feature-edges` alias.
5. Added primitive/generic rejection counters so validation can show where
   hard constraints are still active.

## Stage 1 Validation

Outputs are under `tests/output/feature_policy_validation/`.

| Model | `all-feature-edges` | `primitive-curves` | Effect |
| --- | --- | --- | --- |
| `nasa_mars2020_wheel` | 10974 faces, `rejection-limit`, 468702 feature rejections, 466681 generic rejections | 9066 faces, `reached-target`, 31 feature rejections, 0 generic rejections | Fragmented wheel creases no longer hard-lock the queue. |
| `thingi10k_37880_functional_differential_gear_system` | 2662 faces, `rejection-limit`, 68993 feature rejections, 68184 generic rejections | 1236 faces, `reached-target`, 0 feature rejections | Dense gear features become soft ranking cues instead of a stop condition. |
| `fandisk_2014` | 3236 faces, `reached-target`, 513 generic rejections | 3236 faces, `reached-target`, 0 generic rejections | Non-circular hard edges influence cost/guards without feature hard locks. |

The normal, topology, triangle-quality, local-error, and optional intersection
guards remain independent legality filters. `validate-features` also passes on
the four default external fixtures with 0 generic feature rejections in curve
mode; remaining feature rejections are primitive-circle/ellipse protection.

## Remaining Direction

1. Add a stricter envelope/Hausdorff placement filter for production tolerance
   control.
2. Add multi-loop ownership for dense CAD/STL graphs: split high-degree feature
   components into simple cycles, fit primitives per cycle, and preserve
   ownership through simplification.
3. Consider weak-feature consolidation before decimation for CWF-style dense
   graphs.
4. Keep validation honest: continue reporting `rejection-limit`, primitive vs
   generic rejected collapse counts, projected placements, target-face miss, and
   feature-compare recall.

## Expected Impact

The goal is to keep the current SDK-style deterministic simplifier while
reducing over-protection on imported industrial models:

- Mars wheel-like models should reach target faces without losing all circular
  features.
- Gear-like models should stop producing large curve-mode distance errors.
- Fandisk/rocker-arm hard edges should influence cost and quality filters
  without being treated as protected circular curves.
