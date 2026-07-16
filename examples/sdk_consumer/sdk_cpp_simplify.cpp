/**
 * @file examples/sdk_consumer/sdk_cpp_simplify.cpp
 * @brief Demonstrates sdk cpp simplify through the ManuMesh SDK examples.
 * @ingroup manumesh_examples
 *
 * @details The example intentionally uses only supported public entry points and doubles as executable integration documentation.
 */

#include "algorithms/analysis/MeshAnalysis.h"
#include "algorithms/feature_detection/FeatureDetector.h"
#include "algorithms/simplification/Metrics.h"
#include "algorithms/simplification/QEMSimplifier.h"
#include "core/MeshGenerators.h"

#include <algorithm>
#include <cmath>
#include <string>

int main() {
    manumesh::Mesh input = manumesh::generateCylinderGrid(32, 8, 1.0, 2.0);

    manumesh::simplification::SimplifyOptions options;
    options.targetRatio = 0.35;
    options.useLineQuadrics = true;
    options.weightMode = manumesh::simplification::WeightMode::Dihedral;

    manumesh::simplification::QEMSimplifier simplifier(options);
    manumesh::simplification::SimplifyReport report;
    manumesh::Mesh output = simplifier.simplify(input, &report);

    if (output.empty()) {
        return 1;
    }
    if (report.finalFaces >= report.initialFaces) {
        return 2;
    }
    const manumesh::analysis::MeshStats stats = manumesh::analysis::computeMeshStats(output);
    if (stats.faces != static_cast<int>(output.faces.size())) {
        return 3;
    }
    const manumesh::simplification::MeshStats legacyStats = manumesh::simplification::computeMeshStats(output);
    const manumesh::simplification::DistanceStats legacyDistance =
        manumesh::simplification::compareMeshesBySampledDistance(output, output, 16);
    const double distanceTolerance = 1e-9 * std::max(1.0, output.bboxDiag());
    if (legacyStats.faces != stats.faces || !std::isfinite(legacyDistance.maxOriginalToSimplified) ||
        !std::isfinite(legacyDistance.maxSimplifiedToOriginal) ||
        legacyDistance.maxOriginalToSimplified > distanceTolerance ||
        legacyDistance.maxSimplifiedToOriginal > distanceTolerance) {
        return 4;
    }
    const std::string legacyHeader = manumesh::simplification::statsHeaderCsv();
    const std::string legacyRow = manumesh::simplification::statsRowCsv("sdk", legacyStats, &legacyDistance);
    if (legacyHeader.rfind("label,", 0) != 0 || legacyRow.rfind("sdk,", 0) != 0) {
        return 5;
    }

    manumesh::Mesh textured = manumesh::generatePlaneGrid(4, 1.0, false);
    textured.faceTexCoords.resize(textured.faces.size());
    for (int face = 0; face < static_cast<int>(textured.faces.size()); ++face) {
        textured.faceTexCoords[face].valid = true;
        for (int corner = 0; corner < 3; ++corner) {
            const manumesh::Vec3& point = textured.vertices[textured.faces[face].v[corner]];
            textured.faceTexCoords[face].uv[corner] = manumesh::Vec2(point.x(), point.y());
        }
    }
    options.targetRatio = 0.5;
    options.preserveTexture = true;
    const manumesh::Mesh texturedOutput = manumesh::simplification::simplifyMesh(textured, options);
    if (!texturedOutput.hasTextureCoordinates() || texturedOutput.faceTexCoords.size() != texturedOutput.faces.size()) {
        return 6;
    }

    const manumesh::Mesh bump = manumesh::generateBumpGrid(24, 2.0);
    const auto curvature = manumesh::feature::computeSmoothCurvatureFeatures(
        bump, manumesh::feature::SmoothCurvatureOptions{2, 3, 2, 0.65}, 0.01
    );
    const bool hasPersistentFeature =
        std::any_of(curvature.begin(), curvature.end(), [](const manumesh::feature::SmoothCurvatureVertex& vertex) {
            return vertex.persistentScales >= 2 && vertex.persistentFeatureScore > 0.01;
        });
    return hasPersistentFeature ? 0 : 7;
}
