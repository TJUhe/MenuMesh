# Large Model Validation

This note records the first large public mesh validation run after adding the
link-condition collapse legality filter. The purpose is to catch unexpected
holes, non-manifold edges, and severe quality regressions on meshes above 10k
faces.

## Sources

- `tests/data/external/large/*.obj`: 10 public models from
  Alec Jacobson's `common-3d-test-models` repository.
- `tests/data/external/casting_aimshape_2014.obj`: public Casting model from
  the LIRIS Mesh Benchmark, related to the AIM@SHAPE Casting model used in
  Tsuchie and Higashi 2014.
- `tests/data/external/fandisk_2014.obj`: public Fandisk model from
  `common-3d-test-models`, related to Tsuchie and Higashi 2014.

## Commands

Build the Release CLI:

```powershell
cmake --build build\codex-industrial --config Release --parallel
```

Run a 90% ratio smoke pass over all 10 large models:

```powershell
.\build\codex-industrial\bin\Release\linequadrics.exe simplify `
  tests\data\external\large\<model>.obj `
  examples\output\large_validation\<model>_line_090.stl `
  --method line --ratio 0.9 `
  --line-weight 1e-3 --weight-mode dihedral --feature-boost 0.08 `
  --feature-angle-deg 25 --samples 120 `
  --metrics-csv examples\output\large_validation\<model>_line_090_metrics.csv
```

Run a deeper 50% ratio pass over representative medium/large models:

```powershell
.\build\codex-industrial\bin\Release\linequadrics.exe simplify `
  tests\data\external\large\<model>.obj `
  examples\output\large_validation\<model>_line_050.stl `
  --method line --ratio 0.5 `
  --line-weight 1e-3 --weight-mode dihedral --feature-boost 0.08 `
  --feature-angle-deg 25 --samples 160 `
  --metrics-csv examples\output\large_validation\<model>_line_050_metrics.csv
```

## 90% Ratio Results

No model introduced new boundary edges or non-manifold edges. Existing open
boundaries stayed stable or decreased.

| Model | Input faces | Input boundary | Output faces | Output boundary | Output non-manifold | Boundary delta | Status |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| armadillo | 99976 | 0 | 89978 | 0 | 0 | 0 | ok |
| beast | 64618 | 0 | 58156 | 0 | 0 | 0 | ok |
| beetle_alt | 38656 | 1136 | 34790 | 1136 | 0 | 0 | ok |
| cheburashka | 13334 | 0 | 12000 | 0 | 0 | 0 | ok |
| happy | 98601 | 7 | 88741 | 7 | 0 | 0 | ok |
| horse | 96966 | 0 | 87268 | 0 | 0 | 0 | ok |
| max_planck | 99991 | 161 | 89992 | 156 | 0 | -5 | ok |
| nefertiti | 99938 | 0 | 89944 | 0 | 0 | 0 | ok |
| rocker_arm | 20088 | 0 | 18078 | 0 | 0 | 0 | ok |
| stanford_bunny | 69451 | 223 | 62505 | 221 | 0 | -2 | ok |

## 50% Ratio Results

The deeper pass covered five representative models from 13k to 69k faces. No
new non-manifold edges were produced.

| Model | Input faces | Input boundary | Output faces | Output boundary | Output non-manifold | Boundary delta | Status |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| cheburashka | 13334 | 0 | 6666 | 0 | 0 | 0 | ok |
| rocker_arm | 20088 | 0 | 10044 | 0 | 0 | 0 | ok |
| beetle_alt | 38656 | 1136 | 19328 | 1096 | 0 | -40 | ok |
| beast | 64618 | 0 | 32308 | 0 | 0 | 0 | ok |
| stanford_bunny | 69451 | 223 | 34725 | 221 | 0 | -2 | ok |

## Observations

- The link-condition filter is doing its job: the batch did not create new
  holes or non-manifold edges.
- Runtime improved substantially after adding incremental incident-face
  adjacency inside the simplifier. On this machine, Armadillo at 50% ratio went
  from about 264 seconds to about 2.26 seconds in Release with the same output
  topology and metrics.
- Some open public models begin with boundary edges. Validation should compare
  boundary deltas instead of requiring all outputs to have zero boundaries.
- The next quality guard should reject normal flips and very poor triangles in
  addition to the current area and link-condition checks.
