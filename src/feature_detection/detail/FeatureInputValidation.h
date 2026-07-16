/**
 * @file src/feature_detection/detail/FeatureInputValidation.h
 * @brief Declares feature input validation facilities for ManuMesh's feature-detection module.
 * @ingroup manumesh_feature_detection
 *
 * @details This file is part of the deterministic triangle-surface feature pipeline. Local evidence is kept separate from graph cleanup, tracing, primitive recovery, and patch segmentation so each stage has an explicit contract.
 */

#pragma once

#include "core/Mesh.h"

#include <stdexcept>
#include <string>

namespace manumesh::feature::detector_detail {

/// Shared entry validation for the public feature-detection APIs.
///
/// Meshes without faces are accepted so vertex-only inputs produce empty
/// results; every other mesh must pass lenient geometry validation (indices
/// in range, finite coordinates, no face repeating a vertex index) before
/// any per-face data is read. Zero-area faces are tolerated: dirty CAD/scan
/// input is the norm, so the evidence stages skip the contribution of
/// degenerate faces instead of failing the whole analysis. Callers surface
/// the tolerated count through FeatureAnalysis::degenerateFaces.
inline void validateFeatureMeshInput(const Mesh& mesh) {
    if (mesh.faces.empty()) {
        return;
    }
    std::string error;
    if (!validateMeshGeometryLenient(mesh, &error)) {
        throw std::invalid_argument(error);
    }
}

} // namespace manumesh::feature::detector_detail
