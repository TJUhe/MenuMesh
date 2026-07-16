/**
 * @file include/core/MathConstants.h
 * @brief Declares math constants facilities for ManuMesh's core-mesh module.
 * @ingroup manumesh_core
 *
 * @details Core types establish the storage, validation, tolerance, topology, and status contracts consumed by every algorithm module.
 */

#pragma once

namespace manumesh {

/// Circle constant pi shared by every module so generators, feature
/// detection, and simplification agree on one representation.
inline constexpr double kPi = 3.141592653589793238462643383279502884;

} // namespace manumesh
