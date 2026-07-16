/**
 * @file src/common/detail/MeshDistanceIndex.h
 * @brief Declares mesh distance index facilities for ManuMesh's common-geometry module.
 * @ingroup manumesh_common
 *
 * @details The routines here are policy-free geometry foundations shared by feature detection, simplification, analysis, and mesh editing.
 */

#pragma once

#include "core/Mesh.h"

#include <vector>

namespace manumesh::common {

/**
 * @brief BVH-backed point-to-surface distance queries against a reference mesh.
 *
 * Contract: the index holds a const Mesh& reference and does not own or copy
 * the mesh data. The referenced mesh must outlive the index and must not be
 * modified or moved after construction; otherwise queries read stale or
 * dangling data. Construction validates face vertex indices: faces that
 * reference an out-of-range vertex (and degenerate, near-zero-area faces)
 * are skipped and counted instead of causing undefined behavior.
 */
class MeshDistanceIndex {
public:
    /**
     * @brief Builds a point-to-triangle BVH over valid faces of `mesh`.
     * @param[in] mesh Reference mesh that must outlive this index.
     */
    explicit MeshDistanceIndex(const Mesh& mesh);

    /** @brief Reports whether the reference mesh contributed any valid face. */
    bool empty() const;
    /**
     * @brief Returns the squared distance from a point to the reference surface.
     * @return Positive infinity when the index is empty.
     */
    double distanceSquared(const Vec3& point) const;
    /**
     * @brief Number of faces skipped during construction because they referenced an
     * out-of-range vertex index or were degenerate.
     */
    int skippedFaceCount() const { return skippedFaceCount_; }

private:
    /** @brief Cached bounds and centroid for one valid reference triangle. */
    struct TriangleRef {
        int face = -1;
        Vec3 lo = Vec3::Zero();
        Vec3 hi = Vec3::Zero();
        Vec3 centroid = Vec3::Zero();
    };

    /** @brief One binary BVH node over a half-open range of triangle indices. */
    struct BvhNode {
        Vec3 lo = Vec3::Zero();
        Vec3 hi = Vec3::Zero();
        int left = -1;
        int right = -1;
        int begin = 0;
        int end = 0;
    };

    /** @brief Builds one BVH subtree over `[begin,end)` and returns its node id. */
    int buildRecursive(int begin, int end);
    /** @brief Updates `best` while traversing one BVH subtree. */
    void queryRecursive(int nodeId, const Vec3& point, double& best) const;

    const Mesh& mesh_;
    std::vector<TriangleRef> triangles_;
    std::vector<int> order_;
    std::vector<BvhNode> nodes_;
    int skippedFaceCount_ = 0;
};

} // namespace manumesh::common

namespace manumesh {
// Transitional alias: manumesh::detail was renamed to manumesh::common
// (architecture v2, R6). New code must use manumesh::common; this alias is
// removed after one minor version.
namespace detail = common;
} // namespace manumesh
