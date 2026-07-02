# Feature Fixtures

Deterministic OBJ fixtures for feature-detection and feature-preserving
simplification tests. Regenerate derived OBJ files with:

```powershell
python tests\data\feature_fixtures\generate_feature_fixtures.py
```

- `coaxial_hole_plate.obj`: vertical annular plate, hole radius 0.6, two coaxial
  circular boundary loops.
- `tilted_coaxial_hole_plate.obj`: annular plate whose bore axis is
  `(0.35, 0.2, 1.0)`, useful for checking non-Z-axis circular-loop normals and
  center-line coaxiality.
- `eccentric_hole_plate.obj`: same circular radius on both faces, but the two
  centers are deliberately offset to make a non-coaxial negative control.
- `elliptical_hole_plate.obj`: vertical plate with two true elliptical loops,
  major radius 0.8 and minor radius 0.45.
- `near_circular_hole_plate.obj`: vertical plate with a mild ellipse, major
  radius 0.62 and minor radius 0.59, useful for near-circle classification.
- `boss_pocket_plate.obj`: one raised rectangular boss and one recessed pocket
  on a planar block, useful for planar patch and convex/concave hard-edge tests.
