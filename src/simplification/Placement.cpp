/**
 * @file src/simplification/Placement.cpp
 * @brief Implements placement facilities for ManuMesh's simplification module.
 * @ingroup manumesh_simplification
 *
 * @details Solves and ranks candidate positions for one edge contraction.
 * @algorithm Attempts the full Garland-Heckbert 3x3 optimum under scale-aware
 * spectral conditioning. Rank-deficient systems use a one-dimensional optimum
 * on the collapsing edge, followed by endpoints and midpoint. Boundary and
 * primitive constraints project or clamp candidates before final cost sorting.
 * @failuremodes Non-finite or ill-conditioned solves are rejected, not returned as placements.
 */

#include "detail/Placement.h"

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

namespace manumesh::simplification {
namespace {

/// Directed boundary chain summary around a collapsing boundary edge:
/// e1 = sum of directed boundary half-edges (v1 - v0), e2 = sum of v0 x v1,
/// following Lindstrom-Turk's boundary-preservation constraint. Half-edge
/// direction comes from the (consistent) face orientation.
struct BoundaryChainSums {
    Vec3 e1 = Vec3::Zero();
    Vec3 e2 = Vec3::Zero();
    double lengthSum = 0.0;
    int edgeCount = 0;
};

BoundaryChainSums collectBoundaryChain(const BoundaryProjectionInput& input) {
    BoundaryChainSums sums;
    // Gather the directed boundary half-edges incident to either endpoint in
    // a deterministic order (sorted, deduplicated) so the floating-point
    // accumulation never depends on unordered_set iteration order.
    std::vector<std::pair<int, int>> halfEdges;
    for (const int vertex : {input.edge.keep, input.edge.remove}) {
        if (vertex < 0 || vertex >= static_cast<int>(input.topology.vertexFaces.size())) {
            continue;
        }
        for (const int faceId : input.topology.vertexFaces[vertex]) {
            if (faceId < 0 || faceId >= static_cast<int>(input.faces.size()) || !input.faces[faceId].active) {
                continue;
            }
            const FaceState& face = input.faces[faceId];
            for (int corner = 0; corner < 3; ++corner) {
                const int u = face.v[corner];
                const int w = face.v[(corner + 1) % 3];
                if (u != vertex && w != vertex) {
                    continue;
                }
                if (mesh_edit::activeIncidentFaceCountForEdge(u, w, input.faces, input.topology) == 1) {
                    halfEdges.emplace_back(u, w);
                }
            }
        }
    }
    std::sort(halfEdges.begin(), halfEdges.end());
    halfEdges.erase(std::unique(halfEdges.begin(), halfEdges.end()), halfEdges.end());
    for (const auto& [u, w] : halfEdges) {
        const Vec3& p0 = input.vertices[u].p;
        const Vec3& p1 = input.vertices[w].p;
        sums.e1 += p1 - p0;
        sums.e2 += p0.cross(p1);
        sums.lengthSum += (p1 - p0).norm();
        ++sums.edgeCount;
    }
    return sums;
}

Vec3 clampToSegment(const Vec3& position, const Vec3& a, const Vec3& b) {
    const Vec3 edge = b - a;
    const double len2 = edge.squaredNorm();
    if (len2 <= 1e-30) {
        return 0.5 * (a + b);
    }
    const double t = std::clamp((position - a).dot(edge) / len2, 0.0, 1.0);
    return a + t * edge;
}

} // namespace

bool projectBoundaryPlacement(const BoundaryProjectionInput& input, Vec3& position) {
    if (!input.decision.boundaryEdge) {
        return false;
    }

    const int keep = input.edge.keep;
    const int remove = input.edge.remove;
    const std::vector<VertexState>& vertices = input.vertices;
    const Vec3& a = vertices[keep].p;
    const Vec3& b = vertices[remove].p;
    if ((b - a).squaredNorm() <= 1e-30) {
        position = 0.5 * (a + b);
        return true;
    }

    // Lindstrom-Turk boundary preservation: the minimizers of the directed
    // boundary-area change fB(v) = ||e2 - v x e1||^2 form the line
    // p0 + t * e1 with p0 = (e1 x e2) / (e1 . e1). Projecting the placement
    // onto that line (instead of clamping it back to the old segment) keeps
    // the boundary polyline first-order intact: for a straight chain the line
    // IS the boundary; for corners it balances the swept area on both sides.
    const BoundaryChainSums chain = collectBoundaryChain(input);
    const double e1Norm = chain.e1.norm();
    if (chain.edgeCount >= 2 && e1Norm > 1e-6 * std::max(chain.lengthSum, 1e-30)) {
        const Vec3 direction = chain.e1 / e1Norm;
        const Vec3 p0 = chain.e1.cross(chain.e2) / chain.e1.squaredNorm();
        // Clamp to the collapsing edge's shadow on the constraint line so the
        // placement cannot slide along the whole boundary.
        const double ta = (a - p0).dot(direction);
        const double tb = (b - p0).dot(direction);
        const double t = std::clamp((position - p0).dot(direction), std::min(ta, tb), std::max(ta, tb));
        const Vec3 constrained = p0 + t * direction;
        // Safety net: reject constraint solutions that drift further than one
        // edge length away from the collapsing segment (defective chains,
        // near-cancelling e1) and fall back to the plain segment clamp.
        if (constrained.allFinite() &&
            (constrained - clampToSegment(constrained, a, b)).squaredNorm() <= (b - a).squaredNorm()) {
            position = constrained;
            return true;
        }
    }

    position = clampToSegment(position, a, b);
    return true;
}

} // namespace manumesh::simplification
