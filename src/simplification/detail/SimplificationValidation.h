/**
 * @file src/simplification/detail/SimplificationValidation.h
 * @brief Declares simplification validation facilities for ManuMesh's simplification module.
 * @ingroup manumesh_simplification
 *
 * @details This file is part of the feature-aware edge-collapse pipeline. Quadric costs rank candidates; topology, geometry, feature, boundary, error, and optional texture policies decide whether a placement may mutate the mesh.
 */

#pragma once

#include "algorithms/simplification/SimplificationTypes.h"
#include "core/Mesh.h"

namespace manumesh::simplification {

/**
 * @throws std::invalid_argument when any option or cross-field range is invalid.
 */
void validateSimplifyOptions(const SimplifyOptions& options);
/**
 * @throws std::invalid_argument when input cannot be processed safely.
 */
void validateSimplifierInput(const Mesh& input);

} // namespace manumesh::simplification
