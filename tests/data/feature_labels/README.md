# Feature Label Fixtures

These CSV files are small ground-truth labels for `manumesh feature-benchmark`.

- Edge rows use `edge,a,b`, or the backward-compatible short form `a,b`, with
  0-based vertex indices after the fixture is loaded
  by ManuMesh. OBJ fixtures may be remapped by `Mesh::removeUnusedVertices()`,
  so these are not necessarily raw OBJ line numbers minus one.
- Optional junction rows use `junction,id`.
- Optional continuation rows use `branch,junction,neighborA,neighborB`.
- Optional face labels use `face_patch,faceId,patchId`. Patch ids are arbitrary;
  the benchmark compares whether labeled adjacent faces agree on same/different
  patch ownership, so a detector may renumber patches without being penalized.
- `coaxial_hole_plate_inner_top_edges.csv` labels the top inner circular hole
  loop of `tests/data/feature_fixtures/coaxial_hole_plate.obj`.
- `FeatureDetection.FixtureBenchmarkUsesCoaxialHoleGroundTruthLabels` consumes
  this file as a labeled accuracy test. Dataset smoke tests intentionally do
  not replace this precision/recall assertion.
- `elliptical_hole_plate_inner_top_edges.csv` labels the 40-edge top inner
  ellipse, providing a non-circular primitive benchmark.
- `boss_pocket_primary_edges.csv` labels all 60 non-coplanar manifold edges in
  the axis-aligned boss/pocket fixture, covering convex and concave CAD edges.
- `multi_junction_polygon_edges.csv` labels an eight-edge polygonal loop and
  three known branch junctions in the deterministic synthetic fixture.
