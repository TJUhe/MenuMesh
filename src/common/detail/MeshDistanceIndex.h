#pragma once

#include "core/Mesh.h"

#include <vector>

namespace manumesh::detail {

class MeshDistanceIndex {
public:
    explicit MeshDistanceIndex(const Mesh& mesh);

    bool empty() const;
    double distanceSquared(const Vec3& point) const;

private:
    struct TriangleRef {
        int face = -1;
        Vec3 lo = Vec3::Zero();
        Vec3 hi = Vec3::Zero();
        Vec3 centroid = Vec3::Zero();
    };

    struct BvhNode {
        Vec3 lo = Vec3::Zero();
        Vec3 hi = Vec3::Zero();
        int left = -1;
        int right = -1;
        int begin = 0;
        int end = 0;
    };

    int buildRecursive(int begin, int end);
    void queryRecursive(int nodeId, const Vec3& point, double& best) const;

    const Mesh& mesh_;
    std::vector<TriangleRef> triangles_;
    std::vector<int> order_;
    std::vector<BvhNode> nodes_;
};

} // namespace manumesh::detail
