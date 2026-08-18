/**
 * @file examples/sdk_consumer/sdk_cpp_c_api_feature_edges.cpp
 * @brief C++14 caller using only the ManuMesh C ABI with flat vectors.
 */

#include "api/CApi.h"

#include <algorithm>
#include <cstddef>
#include <iostream>
#include <utility>
#include <vector>

int main() {
    // The caller's application-level representation.
    const std::vector<double> vertexData = {
        0.0,
        0.0,
        0.0,
        1.0,
        0.0,
        0.0,
        1.0,
        1.0,
        0.0,
        0.0,
        1.0,
        1.0,
    };
    const std::vector<int> faceData = {
        0,
        1,
        2,
        0,
        2,
        3,
    };

    if (vertexData.size() % 3 != 0 || faceData.size() % 3 != 0) {
        std::cerr << "Flat arrays must contain triples.\n";
        return 1;
    }

    std::vector<ManuMeshVec3> vertices(vertexData.size() / 3);
    for (std::size_t i = 0; i < vertices.size(); ++i) {
        vertices[i].x = vertexData[3 * i + 0];
        vertices[i].y = vertexData[3 * i + 1];
        vertices[i].z = vertexData[3 * i + 2];
    }

    std::vector<ManuMeshFace> faces(faceData.size() / 3);
    for (std::size_t i = 0; i < faces.size(); ++i) {
        faces[i].v[0] = faceData[3 * i + 0];
        faces[i].v[1] = faceData[3 * i + 1];
        faces[i].v[2] = faceData[3 * i + 2];
    }

    ManuMeshContext* context = manumesh_context_create();
    ManuMeshMeshHandle* mesh = context ? manumesh_mesh_create(context) : nullptr;
    if (!context || !mesh) {
        manumesh_mesh_destroy(mesh);
        manumesh_context_destroy(context);
        return 2;
    }

    ManuMeshStatus status = manumesh_mesh_set_data(
        context,
        mesh,
        vertices.empty() ? nullptr : vertices.data(),
        vertices.size(),
        faces.empty() ? nullptr : faces.data(),
        faces.size()
    );
    if (status != MANUMESH_STATUS_OK) {
        std::cerr << manumesh_context_last_error(context) << "\n";
        manumesh_mesh_destroy(mesh);
        manumesh_context_destroy(context);
        return 3;
    }

    ManuMeshFeatureOptions options;
    manumesh_feature_options_init(&options);
    options.use_normal_tensor_features = 0;
    options.cleanup_feature_graph = 0;
    options.feature_angle_deg = 25.0;

    std::size_t edgeCount = 0;
    status = manumesh_detect_feature_edges_v2(context, mesh, &options, nullptr, 0, &edgeCount);
    if (status != MANUMESH_STATUS_BUFFER_TOO_SMALL && status != MANUMESH_STATUS_OK) {
        std::cerr << manumesh_context_last_error(context) << "\n";
        manumesh_mesh_destroy(mesh);
        manumesh_context_destroy(context);
        return 4;
    }

    std::vector<ManuMeshFeatureEdgeV2> edges(edgeCount);
    std::size_t written = 0;
    status = manumesh_detect_feature_edges_v2(
        context, mesh, &options, edges.empty() ? nullptr : edges.data(), edges.size(), &written
    );
    if (status != MANUMESH_STATUS_OK || written != edges.size()) {
        std::cerr << manumesh_context_last_error(context) << "\n";
        manumesh_mesh_destroy(mesh);
        manumesh_context_destroy(context);
        return 5;
    }

    for (std::size_t i = 0; i < written; ++i) {
        const ManuMeshFeatureEdgeV2& edge = edges[i];
        if (edge.a < 0 || edge.b < 0 || edge.a == edge.b || static_cast<std::size_t>(edge.a) >= vertices.size() ||
            static_cast<std::size_t>(edge.b) >= vertices.size() || edge.feature_edge_index != i) {
            std::cerr << "The C ABI returned an invalid feature-edge endpoint index.\n";
            manumesh_mesh_destroy(mesh);
            manumesh_context_destroy(context);
            return 6;
        }
        std::cout << "edge " << edge.a << " - " << edge.b << " boundary=" << edge.boundary
                  << " dihedral=" << edge.dihedral << " normal_tensor=" << edge.normal_tensor
                  << " non_manifold=" << edge.non_manifold
                  << " cleanup_bridge=" << edge.cleanup_bridge << " consolidation_bridge=" << edge.consolidation_bridge
                  << " removed_by_cleanup=" << edge.removed_by_cleanup << " signed_kind=" << edge.signed_kind
                  << " feature_edge_index=" << edge.feature_edge_index << " input_edge_index=";
        if (edge.input_edge_index == MANUMESH_INVALID_EDGE_INDEX) {
            std::cout << "invalid";
        } else {
            std::cout << edge.input_edge_index;
        }
        std::cout << " synthetic=" << edge.synthetic << " geometric_constraint=" << edge.geometric_constraint << "\n";
    }

    // Optional application-level output: [a0, b0, a1, b1, ...].
    // Feature edges are undirected, so normalize endpoints for stable comparison.
    std::vector<int> featureEdgeIndices;
    featureEdgeIndices.reserve(written * 2);
    for (std::size_t i = 0; i < written; ++i) {
        const int a = std::min(edges[i].a, edges[i].b);
        const int b = std::max(edges[i].a, edges[i].b);
        featureEdgeIndices.push_back(a);
        featureEdgeIndices.push_back(b);
    }
    std::cout << "feature_edge_vertex_indices";
    for (int vertexIndex : featureEdgeIndices) {
        std::cout << ' ' << vertexIndex;
    }
    std::cout << '\n';

    manumesh_mesh_destroy(mesh);
    manumesh_context_destroy(context);
    return 0;
}
