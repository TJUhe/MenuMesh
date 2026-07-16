/**
 * @file src/feature_detection/detail/PrimitiveFit.h
 * @brief Declares primitive fit facilities for ManuMesh's feature-detection module.
 * @ingroup manumesh_feature_detection
 *
 * @details This file is part of the deterministic triangle-surface feature pipeline. Local evidence is kept separate from graph cleanup, tracing, primitive recovery, and patch segmentation so each stage has an explicit contract.
 */

#pragma once

#include "algorithms/feature_detection/FeatureTypes.h"

#include <vector>

namespace manumesh::feature::primitive_fit_detail {

/**
 * @brief Internal plane, circle, and ellipse fit returned by primitive fitting.
 */
struct PrimitiveFit {
    bool valid = false;
    FeaturePrimitiveType primitive = FeaturePrimitiveType::Unknown;
    Vec3 center = Vec3::Zero();
    /**
     * @brief Center of the directly fitted ellipse (Halir-Flusser); coincides with
     * `center` for symmetric loops but not for asymmetric vertex sampling.
     */
    Vec3 ellipseCenter = Vec3::Zero();
    Vec3 normal = Vec3(0.0, 0.0, 1.0);
    Vec3 majorAxis = Vec3(1.0, 0.0, 0.0);
    Vec3 minorAxis = Vec3(0.0, 1.0, 0.0);
    double radius = 0.0;
    double majorRadius = 0.0;
    double minorRadius = 0.0;
    double axisRatio = 0.0;
    double rmsRadialError = 0.0;
    double maxRadialError = 0.0;
    double rmsEllipseError = 0.0;
    double maxEllipseError = 0.0;
    double rmsPlaneError = 0.0;
    double maxPlaneError = 0.0;
};

/**
 * @brief Fits the best supported analytic primitive in the loop's best-fit plane.
 */
PrimitiveFit fitPrimitive(const Mesh& mesh, const FeatureLoop& loop, const FeatureOptions& options);
/**
 * @brief Copies a valid internal fit into the public loop representation.
 */
void applyPrimitiveFit(const PrimitiveFit& fit, FeatureLoop& loop);
/**
 * @return true when every consecutive cycle edge follows the fitted circle within policy.
 */
bool cycleEdgesFollowCircle(
    const std::vector<int>& vertices, const PrimitiveFit& fit, const Mesh& mesh, const FeatureOptions& options
);
/**
 * @brief Measures loop samples against a supplied circle without refitting it.
 */
DirectionalCurveError measureLoopAgainstCircle(
    const Mesh& mesh, const FeatureLoop& loop, const Vec3& center, const Vec3& normalIn, double radius
);

} // namespace manumesh::feature::primitive_fit_detail
