#include "algorithms/feature_detection/FeatureDetector.h"
#include "algorithms/simplification/Metrics.h"
#include "algorithms/simplification/QEMSimplifier.h"
#include "core/MeshGenerators.h"

#include <algorithm>

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
    const manumesh::simplification::MeshStats stats = manumesh::simplification::computeMeshStats(output);
    if (stats.faces != static_cast<int>(output.faces.size())) {
        return 3;
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
        return 4;
    }

    const manumesh::Mesh bump = manumesh::generateBumpGrid(24, 2.0);
    const auto curvature = manumesh::feature::computeSmoothCurvatureFeatures(
        bump, manumesh::feature::SmoothCurvatureOptions{2, 3, 2, 0.65}, 0.01
    );
    const bool hasPersistentFeature =
        std::any_of(curvature.begin(), curvature.end(), [](const manumesh::feature::SmoothCurvatureVertex& vertex) {
            return vertex.persistentScales >= 2 && vertex.persistentFeatureScore > 0.01;
        });
    return hasPersistentFeature ? 0 : 5;
}
