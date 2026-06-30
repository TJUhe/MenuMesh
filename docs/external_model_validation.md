# External Model Validation

This document records an extra validation pass using public benchmark models.
The model files are not committed to the repository. Place them under
`examples/external/common_3d_test_models/` when needed.

## Source

The current external set is from Alec Jacobson's
`common-3d-test-models` repository:

<https://github.com/alecjacobson/common-3d-test-models>

The repository README describes it as a collection of common 3D test models and
lists the original source for each model when known. Because original licenses
vary by model, this project commits only validation commands and generated
measurements, not the downloaded OBJ files themselves.

## Added OBJ support

The command line tools now load both `.stl` and `.obj` through `loadMesh(...)`.
The OBJ loader intentionally stays minimal:

- supports `v` and `f` records;
- supports face tokens like `f v`, `f v/t`, `f v//n`, and `f v/t/n`;
- triangulates polygonal faces by fan triangulation;
- ignores materials, normals, texture coordinates, groups, and smoothing tags.

## How to run

```powershell
.\build\mingw-ninja-release\linequadrics.exe validate-external --ratio 0.25 --samples 800
```

The validation command looks for these models:

| Model | Role in validation |
| --- | --- |
| `fandisk.obj` | CAD-ish hard non-circular features. |
| `rocker-arm.obj` | Mechanical scan with noisy/fragmented hard-edge evidence. |
| `beetle.obj` | Mixed smooth/sharp model; exposes small false circular loops. |
| `cow.obj` | Organic model; tests false feature over-protection. |
| `suzanne.obj` | Low-poly hard-edged model without true CAD circular loops. |

Outputs are written to `examples/output/external_model_validation/`. Start with
`external_summary.csv`; it records input/output face counts, circular-loop
matches/misses, and rejected collapses for each model.

## Observed results

| Model | Input circular loops | Main issue found | Current behavior after fix |
| --- | ---: | --- | --- |
| Fandisk | 0 | Hard feature graph is one large non-circular component. | No circular hard lock; simplification reaches target. |
| Rocker arm | 0 | Many fragmented dihedral components, no clean circular loop at current threshold. | No circular hard lock; simplification reaches target. |
| Beetle | 1 | An 8-vertex small loop is fitted as circular, likely a false positive for CAD use. | Curve mode preserves that loop; suggests raising min circular vertices for industrial mode. |
| Cow | 0 | Earlier version over-locked many non-circular dihedral components. | Fixed by hard-protecting circular loops only by default. |
| Suzanne | 0 | Low-poly hard edges are not circular CAD features. | No circular hard lock; simplification reaches target. |

The key change prompted by these tests is:

```text
--preserve-feature-curves
  hard-protects circular feature loops by default

--protect-all-feature-edges
  opt-in mode for also hard-locking non-circular feature edges
```

This matters because CAD/STL circular-hole preservation and generic hard-edge
preservation are not the same problem. The first wants radius/plane constraints;
the second needs better feature-graph tracing and usually should not be enabled
on organic or noisy scanned meshes by default.

## Remaining problems

1. **False circular loops on tiny cycles.** `beetle.obj` shows that an 8-vertex
   loop can fit a circle numerically but still not be a semantic CAD hole. For
   industrial validation, use a higher `--min-feature-loop-vertices`, for
   example 12 or 16.
2. **Fragmented feature graphs.** `rocker-arm.obj` has many short feature
   components. A production detector should merge/split graph paths using
   continuity, scale, and analytic primitive evidence.
3. **No non-circular curve tracing yet.** Fandisk-style sharp curves are
   detected as feature evidence, but this branch mainly protects circular loops.
   Non-circular creases need polyline/spline cycle tracing instead of circle
   projection.
