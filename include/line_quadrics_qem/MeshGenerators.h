#pragma once

#include "line_quadrics_qem/Export.h"
#include "line_quadrics_qem/Mesh.h"

#include <string>

namespace lq {

/// Generates a rectangular plane grid; clustered mode intentionally skews cells.
LQ_API Mesh generatePlaneGrid(int n, double size, bool clustered);
/// Generates a plane grid with a circular hole.
LQ_API Mesh generateHolePlaneGrid(int n, double size, double radius);
/// Generates a plane grid with a raised ridge.
LQ_API Mesh generateRidgeGrid(int n, double size, double height);
/// Generates a noisy plane useful for stress-testing normal-sensitive behavior.
LQ_API Mesh generateNoisyPlaneGrid(int n, double size, double noiseAmplitude);
/// Generates a smooth sine-wave terrain.
LQ_API Mesh generateSineTerrainGrid(int n, double size);
/// Generates a stepped terrain with sharp terrace features.
LQ_API Mesh generateTerraceGrid(int n, double size);
/// Generates a smooth bump surface.
LQ_API Mesh generateBumpGrid(int n, double size);
/// Generates a capped cylinder grid.
LQ_API Mesh generateCylinderGrid(int radialSegments, int heightSegments, double radius,
                                 double height);
/// Generates a torus grid.
LQ_API Mesh generateTorusGrid(int majorSegments, int minorSegments, double majorRadius,
                              double minorRadius);
/// Generates a subdivided cube shell.
LQ_API Mesh generateCubeGrid(int n, double size);
/// Generates a thin-fin stress case.
LQ_API Mesh generateThinFinGrid(int n, double size);
/// Generates a flanged-boss industrial test case.
LQ_API Mesh generateFlangedBossGrid(int n);
/// Generates a stepped-shaft industrial test case.
LQ_API Mesh generateSteppedShaftGrid(int n);
/// Generates a pipe-coupling industrial test case.
LQ_API Mesh generatePipeCouplingGrid(int n);
/// Generates a pulley-like industrial test case.
LQ_API Mesh generatePulleyGrid(int n);

/// Dispatches one built-in generator by stable string name.
LQ_API bool generateMeshByName(const std::string& type, int n, Mesh& mesh,
                               std::string* error = nullptr);

} // namespace lq
