#pragma once

#include "Export.h"
#include "core/Mesh.h"

#include <string>

namespace manumesh {

/// Generates a rectangular plane grid; clustered mode intentionally skews cells.
MANUMESH_API Mesh generatePlaneGrid(int n, double size, bool clustered);
/// Generates a plane grid with a circular hole.
MANUMESH_API Mesh generateHolePlaneGrid(int n, double size, double radius);
/// Generates a plane grid with a raised ridge.
MANUMESH_API Mesh generateRidgeGrid(int n, double size, double height);
/// Generates a noisy plane useful for stress-testing normal-sensitive behavior.
MANUMESH_API Mesh generateNoisyPlaneGrid(int n, double size, double noiseAmplitude);
/// Generates a smooth sine-wave terrain.
MANUMESH_API Mesh generateSineTerrainGrid(int n, double size);
/// Generates a stepped terrain with sharp terrace features.
MANUMESH_API Mesh generateTerraceGrid(int n, double size);
/// Generates a smooth bump surface.
MANUMESH_API Mesh generateBumpGrid(int n, double size);
/// Generates a capped cylinder grid.
MANUMESH_API Mesh generateCylinderGrid(int radialSegments, int heightSegments, double radius, double height);
/// Generates a torus grid.
MANUMESH_API Mesh generateTorusGrid(int majorSegments, int minorSegments, double majorRadius, double minorRadius);
/// Generates a subdivided cube shell.
///
/// The six faces are generated as independent patches whose edges are not
/// welded: every cube edge appears as two coincident boundary edges. Use
/// generateWeldedCubeGrid for a closed-manifold cube.
MANUMESH_API Mesh generateCubeGrid(int n, double size);
/// Generates a subdivided cube with the same patch layout as generateCubeGrid
/// but with coincident vertices welded (merged via a quantized position key),
/// producing a closed two-manifold shell with boundaryEdgeCount() == 0.
MANUMESH_API Mesh generateWeldedCubeGrid(int n, double size);
/// Generates a thin-fin stress case.
MANUMESH_API Mesh generateThinFinGrid(int n, double size);
/// Generates a stepped-shaft industrial test case.
MANUMESH_API Mesh generateSteppedShaftGrid(int n);
/// Generates a pipe-coupling industrial test case.
MANUMESH_API Mesh generatePipeCouplingGrid(int n);
/// Generates a pulley-like industrial test case.
MANUMESH_API Mesh generatePulleyGrid(int n);

/// Dispatches one built-in generator by stable string name.
MANUMESH_API bool generateMeshByName(const std::string& type, int n, Mesh& mesh, std::string* error = nullptr);

} // namespace manumesh
