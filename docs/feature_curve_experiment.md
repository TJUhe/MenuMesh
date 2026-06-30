# Curve Feature Constraints Experiment

This branch adds a feature-preserving experiment on top of the original QEM +
line-quadrics reproduction. The goal is not triangle quality. The goal is to
make circular CAD features measurable and harder to destroy during edge
collapse.

## Why this branch exists

A useful mental model is:

- Standard QEM penalizes distance to original face planes. It mainly prevents
  leaving the local tangent plane, but it can still allow drift inside that
  plane.
- Line quadrics add distance to the vertex normal line. That helps regularize
  tangential drift on broad surface regions.
- A circular hole, boss shoulder, flange rim, or pulley groove is different:
  the valid free direction is the feature curve tangent. The bad directions are
  radial drift and out-of-plane drift.

So this branch adds a curve-feature version:

```text
Q_total =
    Q_plane
  + w_line  * Q_normal_line
  + w_curve * Q_feature_tangent_line
```

For circular loops it also constrains placement:

```text
solve QEM placement
if the collapsed edge belongs to the same circular feature loop:
    project the new vertex back to the fitted circle
```

And it adds hard legality rules:

- Do not collapse a feature vertex into a non-feature vertex.
- Do not collapse across two different feature loops.
- Do not collapse feature junctions.
- Stop collapsing a feature loop below `--min-feature-loop-vertices`.

## Literature anchors

The local `mesh-feature-literature` corpus suggests that CAD/STL feature
preservation usually needs more than one scalar QEM cost:

- Garland and Heckbert QEM: plane quadrics are the base error model.
- Liu, Rahimzadeh, and Zordan line quadrics: normal-line quadrics regularize
  tangential drift, but they do not detect CAD feature loops by themselves.
- Source IDs 010, 063, and 067 in the local corpus: CAD/STL feature extraction
  often starts from boundary and dihedral edges, then traces coherent loops.
- Source IDs 037, 021, 024, and 048: feature-preserving simplification commonly
  combines modified QEM costs with constraints, protected vertices/edges, or
  feature-aware placement.

This implementation follows that engineering pattern: feature edge detection +
loop/circle fitting + QEM cost terms + collapse legality + projected placement.

## New commands

Detect feature loops:

```powershell
.\build\Release\linequadrics.exe feature-report examples\input\pipe_coupling.stl `
  --feature-angle-deg 25 `
  --circle-fit-threshold 0.04 `
  --min-feature-loop-vertices 8 `
  --csv examples\output\feature_curve_validation\pipe_coupling_features.csv
```

Simplify with circular feature protection:

```powershell
.\build\Release\linequadrics.exe simplify examples\input\pipe_coupling.stl `
  examples\output\feature_curve_validation\pipe_coupling_curve.stl `
  --method line `
  --ratio 0.20 `
  --line-weight 1e-3 `
  --weight-mode dihedral `
  --feature-boost 0.08 `
  --feature-angle-deg 25 `
  --preserve-feature-curves `
  --feature-curve-weight 0.08 `
  --circle-fit-threshold 0.04 `
  --min-feature-loop-vertices 16
```

Compare circular features after simplification:

```powershell
.\build\Release\linequadrics.exe feature-compare examples\input\pipe_coupling.stl `
  examples\output\feature_curve_validation\pipe_coupling_curve.stl `
  --feature-angle-deg 25 `
  --circle-fit-threshold 0.04 `
  --csv examples\output\feature_curve_validation\pipe_coupling_curve_feature_compare.csv
```

Run all validation cases:

```powershell
.\run_feature_validation.ps1 -Config Release -Ratio 0.20 -N 96
```

## Validation cases

The branch adds/generated these industrial-style STL cases:

- `stepped_shaft.stl`: stepped turned shaft with multiple shoulder rings.
- `pipe_coupling.stl`: hollow coupling with inner/outer circular edges.
- `pulley.stl`: V-groove pulley profile with several circular feature loops.
- `flange_curve.stl`: raised-boss flange with central bore and bolt-hole loops.

The clean rotational cases are meant to test whether circle constraints work.
The flange case is intentionally more troublesome: its generated top/bottom
surfaces include some jagged cut-hole boundary loops, so it tests whether the
detector can distinguish exact circular loops from non-circular boundary loops.

## Current observed results

Using `--ratio 0.20`, `--feature-angle-deg 25`, and
`--circle-fit-threshold 0.04`:

| Case | Input circular loops | Line QEM matched/missing | Curve constrained matched/missing | Main observation |
| --- | ---: | ---: | ---: | --- |
| stepped shaft | 10 | 8 / 2 | 8 / 2 | Matched rings have near-zero curve error with projection; the two end rings become a non-circular boundary component after simplification, exposing a loop-tracing limitation. |
| pipe coupling | 8 | 8 / 0 | 8 / 0 | Curve constraints reduce radial and plane errors to numerical noise on all detected rings. |
| pulley | 7 | 5 / 2 | 7 / 0 | The curve-constrained version keeps the small bore rings that plain line QEM loses. |
| flange | 18 | 6 / 12 | 8 / 10 | Exact main rings and several bolt-hole rings are preserved, but jagged generated hole boundaries still confuse the loop graph. |

The most important CSV columns are:

- `radial_rms`, `radial_max`: radius drift from the original fitted circle.
- `plane_rms`, `plane_max`: out-of-feature-plane drift.
- `center_error`, `radius_error`, `normal_angle_deg`: analytic-circle drift.
- `status`: `matched`, `weak_match`, or `missing`.

## Known shortcomings

The detector currently treats each connected feature-edge component as one
loop. That is good for clean rings, but not enough for dense CAD graphs where
several circular loops are connected by extra boundary or dihedral edges. The
flange and stepped-shaft end rings show this clearly.

The next upgrade should be multi-loop tracing inside one connected component:

1. Build the feature graph from boundary and dihedral edges.
2. Split high-degree components into simple cycles instead of accepting the
   whole connected component as one loop.
3. Fit circles to each simple cycle.
4. Keep a mapping from protected cycles to original feature labels during
   simplification, so validation does not depend only on redetection after
   collapse.

This is the main remaining gap if the target is production-grade circular-hole
preservation on arbitrary industrial STL files.
