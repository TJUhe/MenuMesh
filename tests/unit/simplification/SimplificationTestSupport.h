/**
 * @file tests/unit/simplification/SimplificationTestSupport.h
 * @brief Verifies simplification test support behavior in the ManuMesh tests.
 * @ingroup manumesh_tests
 *
 * @details The fixture and assertions document observable contracts, numeric tolerances, determinism requirements, and previously fixed regressions.
 */

#pragma once

#include "TestSupport.h"
#include "core/MeshTopology.h"

#include <algorithm>
#include <vector>

namespace manumesh::test::simplification {

inline int countBoundaryVertices(const ::manumesh::Mesh& mesh) {
    const ::manumesh::Result<::manumesh::MeshTopology> topologyResult = ::manumesh::MeshTopology::build(mesh);
    if (!topologyResult.ok()) {
        return -1;
    }
    std::vector<char> boundary(mesh.vertices.size(), 0);
    for (const ::manumesh::TopologyEdge& edge : topologyResult.value().edges()) {
        if (!edge.boundary()) {
            continue;
        }
        boundary[edge.vertices[0]] = 1;
        boundary[edge.vertices[1]] = 1;
    }
    return static_cast<int>(std::count(boundary.begin(), boundary.end(), 1));
}

inline int countBoundaryComponents(const ::manumesh::Mesh& mesh) {
    const ::manumesh::Result<::manumesh::MeshTopology> topologyResult = ::manumesh::MeshTopology::build(mesh);
    if (!topologyResult.ok()) {
        return -1;
    }

    std::vector<std::vector<int>> adjacency(mesh.vertices.size());
    for (const ::manumesh::TopologyEdge& edge : topologyResult.value().edges()) {
        if (!edge.boundary()) {
            continue;
        }
        adjacency[edge.vertices[0]].push_back(edge.vertices[1]);
        adjacency[edge.vertices[1]].push_back(edge.vertices[0]);
    }

    int components = 0;
    std::vector<char> visited(mesh.vertices.size(), 0);
    for (int seed = 0; seed < static_cast<int>(adjacency.size()); ++seed) {
        if (adjacency[seed].empty() || visited[seed]) {
            continue;
        }
        ++components;
        std::vector<int> stack{seed};
        visited[seed] = 1;
        while (!stack.empty()) {
            const int v = stack.back();
            stack.pop_back();
            for (int neighbor : adjacency[v]) {
                if (!visited[neighbor]) {
                    visited[neighbor] = 1;
                    stack.push_back(neighbor);
                }
            }
        }
    }
    return components;
}

inline ::manumesh::feature::FeatureOptions circularFeatureOptions() {
    return ::manumesh::test::circularFeatureOptions(0.04);
}

inline ::manumesh::simplification::SimplifyOptions standardQemOptions(double ratio) {
    return ::manumesh::test::standardOptions(ratio);
}

inline ::manumesh::simplification::SimplifyOptions paperLineQuadricsOptions(double ratio) {
    return ::manumesh::test::lineOptions(ratio);
}

inline ::manumesh::simplification::SimplifyOptions protectedIndustrialFeatureOptions(double ratio) {
    return ::manumesh::test::protectedOptions(ratio);
}

inline void expectBudgetedSimplification(
    const ::manumesh::test::SimplifiedMesh& result, const ::manumesh::Mesh& input, double ratio
) {
    ::manumesh::test::expectBudget(result, input, ratio);
}

inline ::manumesh::Mesh makeLocalIntersectionGuardMesh() {
    ::manumesh::Mesh mesh;
    mesh.vertices = {
        ::manumesh::Vec3(0.0, 0.0, 0.0),
        ::manumesh::Vec3(0.1, 0.0, 0.0),
        ::manumesh::Vec3(0.1, 1.0, 0.0),
        ::manumesh::Vec3(0.2, -1.0, 0.0),
        ::manumesh::Vec3(0.12, -0.3, -1.0),
        ::manumesh::Vec3(0.12, 0.3, 1.0),
        ::manumesh::Vec3(0.12, 0.9, -1.0),
    };
    mesh.faces = {
        ::manumesh::Face{{0, 1, 2}},
        ::manumesh::Face{{0, 3, 1}},
        ::manumesh::Face{{1, 2, 3}},
        ::manumesh::Face{{4, 5, 6}},
    };
    return mesh;
}

inline ::manumesh::Mesh makeSpatialIntersectionGuardMeshWithFarFaces() {
    ::manumesh::Mesh mesh = makeLocalIntersectionGuardMesh();
    for (int i = 0; i < 96; ++i) {
        const int base = static_cast<int>(mesh.vertices.size());
        const double x = 100.0 + 3.0 * static_cast<double>(i);
        mesh.vertices.push_back(::manumesh::Vec3(x, 100.0, 0.0));
        mesh.vertices.push_back(::manumesh::Vec3(x + 1.4, 100.0, 0.0));
        mesh.vertices.push_back(::manumesh::Vec3(x, 101.2, 0.0));
        mesh.faces.push_back(::manumesh::Face{{base, base + 1, base + 2}});
    }
    return mesh;
}

inline ::manumesh::Mesh makeCoplanarOverlapGuardMesh() {
    ::manumesh::Mesh mesh;
    mesh.vertices = {
        ::manumesh::Vec3(0.0, 0.0, 0.0),
        ::manumesh::Vec3(2.0, 0.0, 0.0),
        ::manumesh::Vec3(0.0, -0.2, 0.0),
        ::manumesh::Vec3(1.0, 1.0, 0.0),
        ::manumesh::Vec3(0.0, 1.0, 0.0),
        ::manumesh::Vec3(0.2, 0.2, 0.0),
        ::manumesh::Vec3(0.7, 0.2, 0.0),
        ::manumesh::Vec3(0.2, 0.7, 0.0),
        ::manumesh::Vec3(2.0, -1.0, 0.0),
        ::manumesh::Vec3(3.0, -0.5, 0.0),
    };
    mesh.faces = {
        ::manumesh::Face{{0, 1, 2}},
        ::manumesh::Face{{1, 8, 2}},
        ::manumesh::Face{{1, 9, 8}},
        ::manumesh::Face{{1, 3, 9}},
        ::manumesh::Face{{1, 4, 3}},
        ::manumesh::Face{{5, 6, 7}},
    };
    return mesh;
}

inline ::manumesh::Mesh makeCoplanarSeparatedGuardMesh() {
    ::manumesh::Mesh mesh = makeCoplanarOverlapGuardMesh();
    mesh.vertices[5] = ::manumesh::Vec3(2.2, 2.2, 0.0);
    mesh.vertices[6] = ::manumesh::Vec3(2.7, 2.2, 0.0);
    mesh.vertices[7] = ::manumesh::Vec3(2.2, 2.7, 0.0);
    return mesh;
}

inline ::manumesh::Mesh makePolygonalFeatureChordMesh() {
    ::manumesh::Mesh mesh;
    mesh.vertices = {
        ::manumesh::Vec3(0.0, 0.0, 0.0),
        ::manumesh::Vec3(3.0, 0.0, 0.0),
        ::manumesh::Vec3(2.5, 1.0, 0.0),
        ::manumesh::Vec3(1.2, 2.2, 0.0),
        ::manumesh::Vec3(-0.4, 1.3, 0.0),
    };
    mesh.faces = {
        ::manumesh::Face{{0, 1, 3}},
        ::manumesh::Face{{1, 2, 3}},
        ::manumesh::Face{{0, 3, 4}},
    };
    return mesh;
}

inline ::manumesh::Mesh makePlacementFallbackMesh() {
    ::manumesh::Mesh mesh;
    mesh.vertices = {
        ::manumesh::Vec3(0.0, 0.0, 0.0),
        ::manumesh::Vec3(1.0, 0.0, 0.0),
        ::manumesh::Vec3(0.0, 0.9, 0.0),
        ::manumesh::Vec3(1.0, 0.1, 0.0),
    };
    mesh.faces = {
        ::manumesh::Face{{0, 1, 2}},
        ::manumesh::Face{{1, 3, 2}},
    };
    return mesh;
}

} // namespace manumesh::test::simplification
