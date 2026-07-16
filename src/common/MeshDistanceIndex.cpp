/**
 * @file src/common/MeshDistanceIndex.cpp
 * @brief Implements mesh distance index facilities for ManuMesh's common-geometry module.
 * @ingroup manumesh_common
 *
 * @details Builds a bounding-volume hierarchy for repeated point-to-surface distance queries.
 * @algorithm Valid triangles are partitioned recursively by centroid along the
 * longest AABB axis. Queries visit the nearer child first and prune nodes whose
 * box distance exceeds the current best exact point-triangle distance.
 * @complexity O(F log F) construction and expected O(log F) per query.
 */

#include "common/detail/MeshDistanceIndex.h"

#include "common/detail/GeometryPredicates.h"
#include "core/Tolerances.h"

#include <algorithm>
#include <limits>

namespace manumesh::common {

MeshDistanceIndex::MeshDistanceIndex(const Mesh& mesh)
    : mesh_(mesh) {
    const int vertexCount = static_cast<int>(mesh.vertices.size());
    triangles_.reserve(mesh.faces.size());
    for (int fi = 0; fi < static_cast<int>(mesh.faces.size()); ++fi) {
        const Face& face = mesh.faces[fi];
        const bool validIndices = face.v[0] >= 0 && face.v[0] < vertexCount && face.v[1] >= 0 &&
                                  face.v[1] < vertexCount && face.v[2] >= 0 && face.v[2] < vertexCount;
        if (!validIndices) {
            ++skippedFaceCount_;
            continue;
        }
        const Vec3& a = mesh.vertices[face.v[0]];
        const Vec3& b = mesh.vertices[face.v[1]];
        const Vec3& c = mesh.vertices[face.v[2]];
        if (triangleArea(a, b, c) <= kMinTriangleArea) {
            ++skippedFaceCount_;
            continue;
        }

        TriangleRef ref;
        ref.face = fi;
        ref.lo = a.cwiseMin(b).cwiseMin(c);
        ref.hi = a.cwiseMax(b).cwiseMax(c);
        ref.centroid = (a + b + c) / 3.0;
        order_.push_back(static_cast<int>(triangles_.size()));
        triangles_.push_back(ref);
    }

    if (!order_.empty()) {
        buildRecursive(0, static_cast<int>(order_.size()));
    }
}

bool MeshDistanceIndex::empty() const { return nodes_.empty(); }

double MeshDistanceIndex::distanceSquared(const Vec3& point) const {
    if (nodes_.empty()) {
        return std::numeric_limits<double>::infinity();
    }

    double best = std::numeric_limits<double>::infinity();
    queryRecursive(0, point, best);
    return best;
}

int MeshDistanceIndex::buildRecursive(int begin, int end) {
    BvhNode node;
    node.begin = begin;
    node.end = end;
    node.lo = Vec3(
        std::numeric_limits<double>::infinity(),
        std::numeric_limits<double>::infinity(),
        std::numeric_limits<double>::infinity()
    );
    node.hi = Vec3(
        -std::numeric_limits<double>::infinity(),
        -std::numeric_limits<double>::infinity(),
        -std::numeric_limits<double>::infinity()
    );
    for (int i = begin; i < end; ++i) {
        const TriangleRef& tri = triangles_[order_[i]];
        node.lo = node.lo.cwiseMin(tri.lo);
        node.hi = node.hi.cwiseMax(tri.hi);
    }

    const int nodeId = static_cast<int>(nodes_.size());
    nodes_.push_back(node);
    if (end - begin <= 8) {
        return nodeId;
    }

    const Vec3 extent = node.hi - node.lo;
    int axis = 0;
    if (extent.y() > extent.x() && extent.y() >= extent.z()) {
        axis = 1;
    } else if (extent.z() > extent.x() && extent.z() > extent.y()) {
        axis = 2;
    }

    const int mid = begin + (end - begin) / 2;
    std::nth_element(order_.begin() + begin, order_.begin() + mid, order_.begin() + end, [&](int lhs, int rhs) {
        return triangles_[lhs].centroid[axis] < triangles_[rhs].centroid[axis];
    });
    nodes_[nodeId].left = buildRecursive(begin, mid);
    nodes_[nodeId].right = buildRecursive(mid, end);
    return nodeId;
}

void MeshDistanceIndex::queryRecursive(int nodeId, const Vec3& point, double& best) const {
    const BvhNode& node = nodes_[nodeId];
    if (pointAabbDistanceSquared(point, node.lo, node.hi) >= best) {
        return;
    }

    if (node.left < 0 && node.right < 0) {
        for (int i = node.begin; i < node.end; ++i) {
            const Face& face = mesh_.faces[triangles_[order_[i]].face];
            best = std::min(
                best,
                pointTriangleDistanceSquared(
                    point, mesh_.vertices[face.v[0]], mesh_.vertices[face.v[1]], mesh_.vertices[face.v[2]]
                )
            );
        }
        return;
    }

    const int first = node.left;
    const int second = node.right;
    const double leftDistance = pointAabbDistanceSquared(point, nodes_[first].lo, nodes_[first].hi);
    const double rightDistance = pointAabbDistanceSquared(point, nodes_[second].lo, nodes_[second].hi);
    if (leftDistance < rightDistance) {
        queryRecursive(first, point, best);
        queryRecursive(second, point, best);
    } else {
        queryRecursive(second, point, best);
        queryRecursive(first, point, best);
    }
}

} // namespace manumesh::common
