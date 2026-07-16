/**
 * @file include/core/MeshGenerators.h
 * @brief Declares mesh generators facilities for ManuMesh's core-mesh module.
 * @ingroup manumesh_core
 *
 * @details Core types establish the storage, validation, tolerance, topology, and status contracts consumed by every algorithm module.
 */

#pragma once

#include "Export.h"
#include "core/Mesh.h"

#include <string>

namespace manumesh {

/// Generates a rectangular plane grid; clustered mode intentionally skews cells.
/// @param[in] n Grid subdivisions per axis.
/// @param[in] size Side length in model units.
/// @param[in] clustered Whether to use deliberately non-uniform sampling.
MANUMESH_API Mesh generatePlaneGrid(int n, double size, bool clustered);
/// Generates a plane grid with a circular hole.
/// @param[in] n Approximate grid/circumferential resolution.
/// @param[in] size Outer side length.
/// @param[in] radius Hole radius.
MANUMESH_API Mesh generateHolePlaneGrid(int n, double size, double radius);
/// Generates a plane grid with a raised ridge.
/// @param[in] n Grid subdivisions per axis.
/// @param[in] size Side length.
/// @param[in] height Ridge height.
MANUMESH_API Mesh generateRidgeGrid(int n, double size, double height);
/// Generates a noisy plane useful for stress-testing normal-sensitive behavior.
/// @param[in] n Grid subdivisions per axis.
/// @param[in] size Side length.
/// @param[in] noiseAmplitude Deterministic displacement amplitude.
MANUMESH_API Mesh generateNoisyPlaneGrid(int n, double size, double noiseAmplitude);
/// Generates a smooth sine-wave terrain.
/// @param[in] n Grid subdivisions per axis.
/// @param[in] size Side length.
MANUMESH_API Mesh generateSineTerrainGrid(int n, double size);
/// Generates a stepped terrain with sharp terrace features.
/// @param[in] n Grid subdivisions per axis.
/// @param[in] size Side length.
MANUMESH_API Mesh generateTerraceGrid(int n, double size);
/// Generates a smooth bump surface.
/// @param[in] n Grid subdivisions per axis.
/// @param[in] size Side length.
MANUMESH_API Mesh generateBumpGrid(int n, double size);
/// Generates a capped cylinder grid.
/// @param[in] radialSegments Samples around the axis.
/// @param[in] heightSegments Side-wall subdivisions.
/// @param[in] radius Cylinder radius.
/// @param[in] height Full axial height.
MANUMESH_API Mesh generateCylinderGrid(int radialSegments, int heightSegments, double radius, double height);
/// Generates a torus grid.
/// @param[in] majorSegments Samples around the major circle.
/// @param[in] minorSegments Samples around the tube circle.
/// @param[in] majorRadius Distance from origin to tube centerline.
/// @param[in] minorRadius Tube radius.
MANUMESH_API Mesh generateTorusGrid(int majorSegments, int minorSegments, double majorRadius, double minorRadius);
/// Generates a subdivided cube shell.
///
/// The six faces are generated as independent patches. Coincident positions
/// on adjacent patches use distinct vertex indices, so every cube edge appears
/// as two coincident boundary edges. Use generateClosedCubeGrid for a closed
/// manifold cube.
/// @param[in] n Subdivisions on each face axis.
/// @param[in] size Cube edge length.
MANUMESH_API Mesh generateCubeGrid(int n, double size);
/// Generates a subdivided cube with the same patch layout as generateCubeGrid
/// but merges coincident vertices via a quantized position key, producing a
/// closed two-manifold shell with boundaryEdgeCount() == 0.
/// @param[in] n Subdivisions on each face axis.
/// @param[in] size Cube edge length.
MANUMESH_API Mesh generateClosedCubeGrid(int n, double size);
/// Generates a thin-fin stress case.
/// @param[in] n Surface resolution.
/// @param[in] size Fixture extent.
MANUMESH_API Mesh generateThinFinGrid(int n, double size);
/// Generates a stepped-shaft industrial test case.
/// @param[in] n Circumferential/detail resolution.
MANUMESH_API Mesh generateSteppedShaftGrid(int n);
/// Generates a pipe-coupling industrial test case.
/// @param[in] n Circumferential/detail resolution.
MANUMESH_API Mesh generatePipeCouplingGrid(int n);
/// Generates a pulley-like industrial test case.
/// @param[in] n Circumferential/detail resolution.
MANUMESH_API Mesh generatePulleyGrid(int n);

/// Dispatches one built-in generator by stable string name.
/// @param[in] type Stable generator name.
/// @param[in] n Resolution passed to the selected generator.
/// @param[out] mesh Replaced on success.
/// @param[out] error Optional unknown-name/parameter diagnostic.
/// @return true when the type is known and generation succeeds.
MANUMESH_API bool generateMeshByName(const std::string& type, int n, Mesh& mesh, std::string* error = nullptr);

} // namespace manumesh
