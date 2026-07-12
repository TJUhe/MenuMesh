#pragma once

#include "Export.h"
#include "core/Mesh.h"

namespace manumesh::analysis {

// Cross-algorithm mesh analysis: generic statistics and mesh-to-mesh
// comparison. These utilities are consumed by simplification acceptance
// checks today and by future repair/remeshing modules; they carry no
// simplification-specific semantics.
//
// Error handling (see docs/design/error_handling_policy.md): malformed mesh
// data does not cause either entry point to throw. Faces with invalid indices,
// non-finite coordinates, repeated indices, zero area, or numerically unsafe
// geometry are skipped. A quantity that cannot be computed from the remaining
// surface is reported as zero.
// Thread contract: both functions are pure (no shared mutable state); calls
// on different meshes may run concurrently.

/// Basic geometric and topological mesh quality metrics.
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
MANUMESH_API MeshStats computeMeshStats(const Mesh& mesh);
/// Estimates bidirectional mesh distance using deterministic surface samples.
/// Invalid faces are skipped independently in each mesh. All fields are zero
/// when either mesh has no usable surface, maxSamples is non-positive, or no
/// finite distance sample can be produced.
MANUMESH_API DistanceStats compareMeshesBySampledDistance(const Mesh& original, const Mesh& simplified, int maxSamples);

} // namespace manumesh::analysis
