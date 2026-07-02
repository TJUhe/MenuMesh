# External Industrial Mesh Fixtures

These STL files are binary STL regression fixtures downloaded from public model
repositories. They let GoogleTest exercise real, non-synthetic
industrial-style geometry without requiring network access.

## NASA 3D Resources

The NASA files are downloaded from NASA 3D Resources:

https://github.com/nasa/NASA-3D-Resources

The NASA repository states that these resources are free to download and use,
and are free and without copyright.

Downloaded fixtures:

| File | NASA source path | Purpose |
| --- | --- | --- |
| `nasa_antenna_azimuth_track.stl` | `3D Printing/Beam Waveguide Deep Space Station Antenna/Azimuth track.stl` | Compact mechanical ring/track model used directly in method-comparison tests. |
| `nasa_cubesat_middle.stl` | `3D Printing/CubeSat/CubeSat middle.stl` | Larger boxy spacecraft component used for load/statistics/feature-topology coverage. |
| `nasa_mars2020_wheel.stl` | `3D Printing/Mars 2020 5 inch wheel/Mars 2020 5 inch wheel.stl` | Larger wheel model with circular and thin-wall features used for load/statistics/feature-topology coverage. |

## Public 2014 CAD Model Fixtures

These STL files are public CAD-style models used to reproduce the model regime
from Tsuchie and Higashi, "Extraction of Surface-feature Lines on Meshes Using
Normal Tensor Framework", CAD&A 2014. They were converted to binary STL for
compact repository storage while keeping triangulated geometry intact:

| File | Source | Purpose |
| --- | --- | --- |
| `fandisk_2014.stl` | Common 3D Test Models mirror of Fandisk | Corresponds to the Fandisk CAD model used in the paper's Fig. 9. |
| `casting_aimshape_2014.stl` | LIRIS Mesh Benchmark mirror of the AIM@SHAPE Casting model | Corresponds to the Casting CAD model used in the paper's Fig. 10 and stresses circular/fillet-like feature loops. |
