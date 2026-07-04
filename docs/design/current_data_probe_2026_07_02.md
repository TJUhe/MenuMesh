# Current Data Probe 2026-07-02

This note records a local batch probe of the current simplifier on geometry files
already present under `tests/data`. It is not a full benchmark; it is a failure
discovery pass.

## Commands And Outputs

Generated files:

- `tests/output/current_data_probe/line025_probe_summary.csv`
- `tests/output/current_data_probe/curve025_probe_summary.csv`
- `tests/output/current_data_probe/thingi10k_full_line025/summary.csv`

Main simplify settings:

- Baseline: `simplify --method line --ratio 0.25 --weight-mode dihedral
  --feature-boost 0.08 --feature-angle-deg 25`.
- Feature curve pass: baseline plus `--preserve-feature-curves
  --feature-curve-weight 0.08 --circle-fit-threshold 0.04
  --min-feature-loop-vertices 12`.

The probe used small sample counts for distance metrics so it could scan many
files quickly. Treat distance values as coarse warning signals, not final
Hausdorff-quality measurements.

## What Passed

1. Representative line-mode run:
   - 28 CAD/OBJ, NASA, large public, and selected Thingi10K meshes.
   - 0 CLI failures.
   - All reached approximately the 25% face target.
   - No new non-manifold edges were observed in this representative set.

2. Full Thingi10K light scan:
   - 97 binary STL fixtures.
   - 0 CLI failures.
   - All reached approximately the 25% face target.
   - The models reported as output non-manifold were already non-manifold in the
     input when checked with `feature-report`.

## Problems Found

### 1. CLI could not enable the new legality controls

The C++ API now has:

- `preserveBoundary`
- `minTriangleQuality`
- `maxNormalDeviationDeg`
- detailed reject counters for boundary, topology, normal flip, and quality

Status after follow-up implementation: addressed.

The CLI and C API now expose boundary preservation, minimum triangle quality,
maximum normal deviation, and local self-intersection prevention. `--metrics-csv`
also writes the detailed reject counters.

Observed consequence:

- `beetle_alt.stl`: boundary edges changed from 1136 to 908 in default line mode.
- `stanford_bunny.stl`: boundary edges changed from 223 to 165 in default line
  mode.

This is expected because `preserveBoundary` was not enabled, but the CLI gives
no way to enable it.

### 2. Circular feature preservation over-locks small CAD fixtures

Feature-curve mode missed the 25% face target on these fixtures:

| File | Input faces | Output faces | Ratio | Issue |
| --- | ---: | ---: | ---: | --- |
| `coaxial_hole_plate.obj` | 192 | 96 | 0.500 | Feature over-lock |
| `eccentric_hole_plate.obj` | 256 | 96 | 0.375 | Feature over-lock |
| `near_circular_hole_plate.obj` | 320 | 96 | 0.300 | Feature over-lock |
| `tilted_coaxial_hole_plate.obj` | 256 | 96 | 0.375 | Feature over-lock |

The reject counts were dominated by feature rejects:

- `coaxial_hole_plate.obj`: 1749 feature rejects for 48 collapses.
- `near_circular_hole_plate.obj`: 2901 feature rejects for 112 collapses.

Likely cause:

- The current hard loop minimum is vertex-count based. On small fixtures, four
  circular loops with `minFeatureLoopVertices=12` can consume most of the mesh
  budget.

Recommended next step:

- Replace hard vertex-count locking with loop-budget allocation and
  error-bounded circular re-sampling. A ring should be allowed to simplify below
  a fixed vertex count if radial, plane, and angular errors remain bounded.

### 3. Protected circular loops are not reliably rediscovered after simplification

Feature-curve mode internally reports circular loops and projected placements,
but `feature-compare` often cannot find circular loops in the simplified output.

Examples:

| File | Original circular loops | Simplified circular loops | Missing |
| --- | ---: | ---: | ---: |
| `coaxial_hole_plate.obj` | 4 | 0 | 4 |
| `near_circular_hole_plate.obj` | 4 | 0 | 4 |
| `tilted_coaxial_hole_plate.obj` | 4 | 0 | 4 |
| `casting_aimshape_2014.stl` | 18 | 2 | 16 |
| `nasa_antenna_azimuth_track.stl` | 10 | 4 | 6 |

For `coaxial_hole_plate_curve025.stl`, `feature-report` on the output found 69
feature edges but 63 mostly tiny open chains and 0 circular loops.

Likely cause:

- The simplifier constrains feature vertices and projects positions, but it does
  not preserve a first-class feature-edge graph. After collapses, the geometric
  points may still lie near circles, while the dihedral/feature graph no longer
  forms closed recoverable cycles.

Recommended next step:

- Store constrained feature edges/loops as explicit topology constraints during
  collapse.
- Preserve loop adjacency during edge rewrites.
- Add a gtest that simplifies `coaxial_hole_plate.obj` in curve mode and asserts
  that `feature-compare` matches all four circular loops.

### 4. Some outputs contain very low-quality triangles

In the full Thingi10K light scan, 14 of 97 outputs had minimum triangle quality
below `1e-4`. Worst cases:

| File | Min quality | Mean quality | Edge CV |
| --- | ---: | ---: | ---: |
| `thingi10k_104188_iphone_tank_case_gen_4_and_4s.stl` | 9.09e-09 | 0.389 | 0.946 |
| `thingi10k_101647_recursive_snowflakes.stl` | 4.94e-06 | 0.712 | 0.547 |
| `thingi10k_96545_ferrari_458_model_kit.stl` | 5.88e-06 | 0.491 | 0.876 |
| `thingi10k_108312_balloon_powered_jet_car_with_snap_in_wheel.stl` | 8.18e-06 | 0.323 | 1.108 |

The C++ algorithm now supports `minTriangleQuality`, but the default is 0 and
the CLI cannot enable it.

Follow-up implemented:

- `--min-triangle-quality`, `--max-normal-deviation-deg`,
  `--prevent-local-intersections`, and `--industrial-safe`.
- A dataset regression compares default and strict quality mode on
  `thingi10k_104188_iphone_tank_case_gen_4_and_4s.stl`.
- Strict mode is exposed as an opt-in preset rather than changing the default.

### 5. Edge-length distribution can become highly uneven

High edge-length CV cases:

| File | Edge CV | Min quality |
| --- | ---: | ---: |
| `thingi10k_1085686_snowman_maker_add_two_ball_snowman.stl` | 3.096 | 7.26e-05 |
| `thingi10k_1013014_universal_stand_alone_filament_spool_holde.stl` | 2.716 | 2.85e-04 |

This suggests the current edge-collapse cost can preserve geometry while still
leaving poor local sampling distribution.

Recommended next step:

- Add optional local edge-length or valence regularization to the collapse cost.
- Add a quality/regularity validation gate for high-CV outputs.

## Priority Fixes From This Probe

1. Expose legality controls and detailed reject counters in CLI and C API.
2. Add a regression for circular-loop rediscovery after feature-preserving
   simplification.
3. Replace fixed `minFeatureLoopVertices` hard locking with error-bounded
   circular loop simplification.
4. Add strict quality-mode dataset tests using the worst Thingi10K cases.
5. Add a feature-graph preservation policy instead of relying on post-hoc
   dihedral rediscovery.

Follow-up status:

- Items 1, 2, and 4 are now covered by implementation and gtests.
- Item 3 is partially addressed by separating circular simplification floor
  (`minCircularFeatureLoopVertices`) from feature-loop detection threshold.
- Item 5 is partially addressed for circular features by stronger loop tracing
  and circular vertex-cluster recovery. The fallback is gated to small CAD-style
  candidate sets without normal-tensor feature edges, so it does not become a
  cubic-time general feature clustering pass. A bounded small-component
  cycle-basis pass now recovers polygonal cycles through three or more junctions.
  Large non-circular feature graphs still need first-class graph constraints and
  deterministic ownership resolution.
- Local self-intersection protection now uses an incrementally updated spatial
  hash for candidate face lookup. This removes the previous all-active-face scan
  from the normal path while keeping an overflow fallback for very large AABBs.
  The triangle-intersection guard also handles coplanar overlaps by projecting
  to 2D; gtests cover coplanar overlap rejection, coplanar separation, and legal
  shared coplanar edges.
- Ellipse/polyline features are no longer report-only during simplification.
  Ellipse parameters now flow into simplification, ellipse collapses are
  hard-protected by default, and placements project back to the fitted ellipse.
  Same-loop non-ellipse feature collapses project placement to the local feature
  segment or tangent. Full spline/polyline graph preservation remains future
  work.

## Follow-Up Probe 2026-07-03

Generated files:

- `build/industrial_probe_2026_07_03/boss_pocket.*`
- `build/industrial_probe_2026_07_03/elliptical_hole.*`
- `build/industrial_probe_2026_07_03/nasa_antenna.*`
- `build/industrial_probe_2026_07_03/thingi_pulley.*`

Settings:

- `simplify --method line --weight-mode dihedral --preserve-feature-curves
  --feature-angle-deg 30 --feature-curve-weight 0.08
  --max-feature-curve-deviation-ratio 0.01 --industrial-safe --samples 512`
- Ratios: 0.35 for the local feature fixtures, 0.45 for the NASA/Thingi10K
  external files.

Summary:

| Case | Output faces | Boundary edges | Non-manifold edges | Min quality | Rejected | Feature rejects | Normal rejects | Curve-budget rejects | Projected placements | Max sampled distance |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| `boss_pocket_plate.obj` | 38 | 0 | 0 | 0.278662 | 0 | 0 | 0 | 0 | 15 | 0.292934 |
| `elliptical_hole_plate.obj` | 184 | 0 | 0 | 0.041682 | 2965 | 2965 | 0 | 0 | 68 | 0.378977 |
| `nasa_antenna_azimuth_track.stl` | 1476 | 0 | 0 | 0.002807 | 520 | 453 | 33 | 5 | 514 | 0.892369 |
| `thingi10k_318045_moko_mini_pulley.stl` | 2666 | 0 | 0 | 0.014149 | 291 | 202 | 89 | 0 | 358 | 0.042453 |

Interpretation:

- The new polygonal curve-budget path is exercised on a real industrial mesh:
  `nasa_antenna_azimuth_track.stl` produced 5 curve-budget rejects.
- All four outputs remained closed manifold according to the current mesh stats
  (`boundary_edges=0`, `non_manifold_edges=0`).
- The elliptical fixture still shows heavy feature-locking pressure. This is
  expected with primitive hard protection and confirms that loop-level vertex
  budgets or error-bounded ring resampling are still needed.
- `nasa_antenna_azimuth_track.stl` still has low minimum triangle quality even
  with `--industrial-safe`; the preset currently uses a very conservative
  `1e-4` floor to avoid over-blocking simplification. A stronger quality preset
  or adaptive local remeshing pass remains open.
