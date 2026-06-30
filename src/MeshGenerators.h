#pragma once

#include "Mesh.h"

#include <string>

namespace lq {

Mesh generatePlaneGrid(int n, double size, bool clustered);
Mesh generateHolePlaneGrid(int n, double size, double radius);
Mesh generateRidgeGrid(int n, double size, double height);
Mesh generateNoisyPlaneGrid(int n, double size, double noiseAmplitude);
Mesh generateSineTerrainGrid(int n, double size);
Mesh generateTerraceGrid(int n, double size);
Mesh generateBumpGrid(int n, double size);
Mesh generateCylinderGrid(int radialSegments, int heightSegments, double radius,
                          double height);
Mesh generateTorusGrid(int majorSegments, int minorSegments, double majorRadius,
                       double minorRadius);
Mesh generateCubeGrid(int n, double size);
Mesh generateThinFinGrid(int n, double size);
Mesh generateFlangedBossGrid(int n);
Mesh generateSteppedShaftGrid(int n);
Mesh generatePipeCouplingGrid(int n);
Mesh generatePulleyGrid(int n);

bool generateMeshByName(const std::string& type, int n, Mesh& mesh,
                        std::string* error = nullptr);

}  // namespace lq
