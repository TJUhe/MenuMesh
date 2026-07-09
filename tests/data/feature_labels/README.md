# Feature Label Fixtures

These CSV files are small ground-truth labels for `manumesh feature-benchmark`.

- Edge rows use `a,b` with 0-based vertex indices after the fixture is loaded
  by ManuMesh. OBJ fixtures may be remapped by `Mesh::removeUnusedVertices()`,
  so these are not necessarily raw OBJ line numbers minus one.
- Optional junction rows use `junction,id`.
- `coaxial_hole_plate_inner_top_edges.csv` labels the top inner circular hole
  loop of `tests/data/feature_fixtures/coaxial_hole_plate.obj`.
