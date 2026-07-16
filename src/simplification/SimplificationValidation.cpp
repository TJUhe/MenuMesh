/**
 * @file src/simplification/SimplificationValidation.cpp
 * @brief Implements simplification validation facilities for ManuMesh's simplification module.
 * @ingroup manumesh_simplification
 *
 * @details Validates mesh and option contracts before mutable run state is allocated.
 * @failuremodes Invalid indices, non-finite geometry/options, repeated face
 * vertices, impossible persistence counts, and inconsistent target ranges
 * raise invalid_argument at the C++ boundary.
 */

#include "detail/SimplificationValidation.h"

#include "algorithms/feature_detection/FeatureDetector.h"
#include "core/MeshTopology.h"
#include "detail/SimplificationPolicies.h"

#include <cmath>
#include <stdexcept>
#include <string>

namespace manumesh::simplification {
namespace {

void requireFiniteNonNegative(double value, const char* name) {
    if (!std::isfinite(value) || value < 0.0) {
        throw std::invalid_argument(std::string(name) + " must be finite and non-negative.");
    }
}

} // namespace

void validateSimplifyOptions(const SimplifyOptions& options) {
    if (options.targetFaces == 0 || options.targetFaces < -1) {
        throw std::invalid_argument("targetFaces must be -1 or a positive face count.");
    }
    if (options.targetFaces < 0 &&
        (!std::isfinite(options.targetRatio) || options.targetRatio <= 0.0 || options.targetRatio > 1.0)) {
        throw std::invalid_argument("targetRatio must be finite and in the range (0, 1].");
    }
    requireFiniteNonNegative(options.lineWeight, "lineWeight");
    requireFiniteNonNegative(options.featureBoost, "featureBoost");
    requireFiniteNonNegative(options.adaptiveBaseLineWeight, "adaptiveBaseLineWeight");
    requireFiniteNonNegative(options.boundaryWeight, "boundaryWeight");
    requireFiniteNonNegative(options.textureWeight, "textureWeight");
    requireFiniteNonNegative(options.textureSeamTolerance, "textureSeamTolerance");
    requireFiniteNonNegative(options.minTextureAreaRatio, "minTextureAreaRatio");
    if (options.minTextureAreaRatio > 1.0) {
        throw std::invalid_argument("minTextureAreaRatio must be in [0, 1].");
    }
    requireFiniteNonNegative(options.featureCurveWeight, "featureCurveWeight");
    requireFiniteNonNegative(options.maxFeatureCurveDeviationRatio, "maxFeatureCurveDeviationRatio");
    feature::validateFeatureOptions(featureOptionsFromSimplifyOptions(options));
    if (options.minCircularFeatureLoopVertices < 3) {
        throw std::invalid_argument("minCircularFeatureLoopVertices must be at least 3.");
    }
    requireFiniteNonNegative(options.minTriangleQuality, "minTriangleQuality");
    if (options.minTriangleQuality > 1.0) {
        throw std::invalid_argument("minTriangleQuality must be in [0, 1].");
    }
    requireFiniteNonNegative(options.maxLocalError, "maxLocalError");
    requireFiniteNonNegative(options.maxLocalErrorRatio, "maxLocalErrorRatio");
    if (options.qualityRefinementIterations < 0) {
        throw std::invalid_argument("qualityRefinementIterations must be non-negative.");
    }
    if (!std::isfinite(options.maxNormalDeviationDeg) || options.maxNormalDeviationDeg < 0.0 ||
        options.maxNormalDeviationDeg > 180.0) {
        throw std::invalid_argument("maxNormalDeviationDeg must be finite and in [0, 180].");
    }
}

void validateSimplifierInput(const Mesh& input) {
    if (input.empty()) {
        return;
    }
    // Lenient geometry validation: zero-area faces are tolerated (the QEM
    // pipeline routes them through the degenerate-face quadric fallback and
    // reports them via SimplifyReport::degenerateInputFaces); only inputs no
    // algorithm can process are rejected here.
    std::string error;
    if (!validateMeshGeometryLenient(input, &error)) {
        throw std::invalid_argument(error);
    }
    const Result<MeshTopology> topology = MeshTopology::build(input);
    if (!topology.ok()) {
        throw std::invalid_argument(topology.status().message());
    }
}

} // namespace manumesh::simplification
