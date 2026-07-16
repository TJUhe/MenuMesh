/**
 * @file include/algorithms/analysis/MeshAnalysis.h
 * @brief Declares mesh analysis facilities for ManuMesh's analysis module.
 * @ingroup manumesh_analysis
 *
 * @details Analysis routines tolerate unusable faces where documented and report measurements without changing their input meshes.
 */

#pragma once

#include "Export.h"
#include "core/Mesh.h"

namespace manumesh::analysis {

/**
 * @brief Basic geometric and topological mesh quality metrics.
 *
 * Container counts describe the original input. Geometric fields use only
 * finite, index-valid, non-degenerate faces. Lengths use model units, area
 * uses squared model units, quality lies in [0,1], and edgeLengthCv is the
 * dimensionless coefficient of variation.
 */
struct MeshStats {
    int vertices = 0;
    int faces = 0;
    int edges = 0;
    int boundaryEdges = 0;
    int nonManifoldEdges = 0;
    double area = 0.0;
    double meanTriangleQuality = 0.0;
    double minTriangleQuality = 0.0;
    double meanEdgeLength = 0.0;
    double edgeLengthCv = 0.0;
};

/// Symmetric sampled distance summary between two meshes. All values are in
/// the meshes' native length unit.
struct DistanceStats {
    double meanOriginalToSimplified = 0.0;
    double maxOriginalToSimplified = 0.0;
    double meanSimplifiedToOriginal = 0.0;
    double maxSimplifiedToOriginal = 0.0;
};

/// Computes mesh quality and topology statistics. `vertices` and `faces`
/// report the input container sizes (clamped to INT_MAX); all other fields are
/// computed only from usable faces and are zero when no usable face remains.
/// @param[in] mesh Mesh to inspect; malformed faces are skipped.
/// @return Deterministic statistics. Unavailable quantities are zero.
/// @complexity Expected O(V + F).
/// @note Pure and thread-safe for concurrent calls on distinct immutable meshes.
MANUMESH_API MeshStats computeMeshStats(const Mesh& mesh);
/// Estimates bidirectional mesh distance using deterministic surface samples.
/// Invalid faces are skipped independently in each mesh. All fields are zero
/// when either mesh has no usable surface, maxSamples is non-positive, or no
/// finite distance sample can be produced.
/// @param[in] original First surface, typically the unsimplified reference.
/// @param[in] simplified Second surface to compare against the reference.
/// @param[in] maxSamples Maximum deterministic samples drawn from each direction.
/// @return Bidirectional mean and maximum point-to-surface distances.
/// @complexity O((F_o + F_s) log(F_o + F_s) + maxSamples log(F_o + F_s)).
MANUMESH_API DistanceStats compareMeshesBySampledDistance(const Mesh& original, const Mesh& simplified, int maxSamples);

} // namespace manumesh::analysis
