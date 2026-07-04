# Large Public Mesh Validation Set

STL meshes with more than 10k triangulated faces for manual and batch validation
of simplification topology, quality, and distance metrics.

Most models come from Alec Jacobson's `common-3d-test-models` repository:

https://github.com/alecjacobson/common-3d-test-models

That repository collects common geometry-processing test models and records the
original source when known. The files here were converted to binary STL from
direct downloads in its `data/` folder, renamed only to keep local filenames
simple.

| Local file | Source file | Triangulated faces | Notes |
| --- | --- | ---: | --- |
| `armadillo.stl` | `armadillo.obj` | 99976 | Stanford-style scanned organic model. |
| `beast.stl` | `beast.obj` | 64618 | Multi-polygon source mesh; triangulated by the loader before STL conversion. |
| `beetle_alt.stl` | `beetle-alt.obj` | 38656 | Open mesh with existing boundary loops. |
| `cheburashka.stl` | `cheburashka.obj` | 13334 | Smallest model in this large set. |
| `happy.stl` | `happy.obj` | 98601 | Stanford Happy Buddha style model; has a small pre-existing boundary count. |
| `horse.stl` | `horse.obj` | 96966 | Cyberware-style scanned organic model. |
| `max_planck.stl` | `max-planck.obj` | 99991 | Bust scan with pre-existing boundary edges. |
| `nefertiti.stl` | `nefertiti.obj` | 99938 | Bust scan. |
| `rocker_arm.stl` | `rocker-arm.obj` | 20088 | Mechanical part. |
| `stanford_bunny.stl` | `stanford-bunny.obj` | 69451 | Stanford Bunny source mesh; pre-existing boundary edges. |

Related public CAD-style fixtures one directory above:

- `../fandisk_2014.stl`
- `../casting_aimshape_2014.stl`

Those are used for the Tsuchie and Higashi 2014 normal-tensor feature-line
experiments.
