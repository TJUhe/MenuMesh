#pragma once

namespace manumesh {

/// Shared degeneracy tolerances in model units.
///
/// All triangle-degeneracy tests derive from one canonical minimum area so
/// validation, normal computation, distance indexing, and quality metrics
/// classify the same triangles as degenerate. The unified value keeps the
/// most conservative of the previously scattered thresholds: any triangle
/// rejected by one of the old checks is still rejected here.

/// Minimum triangle area (units: length^2). Triangles at or below this area
/// are treated as degenerate.
inline constexpr double kMinTriangleArea = 1e-24;

/// Minimum length of an unnormalized triangle normal, i.e. the cross-product
/// magnitude, which equals twice the triangle area (units: length^2).
inline constexpr double kMinNormalLength = 2.0 * kMinTriangleArea;

/// Minimum squared length of an unnormalized triangle normal
/// (units: length^4). Equals kMinNormalLength squared.
inline constexpr double kMinSquaredNormalLength = kMinNormalLength * kMinNormalLength;

/// Minimum sum of squared triangle edge lengths (units: length^2) below which
/// shape-quality ratios are reported as zero. Kept equal to kMinNormalLength
/// so all degeneracy tests share one scale.
inline constexpr double kMinSquaredEdgeLengthSum = kMinNormalLength;

} // namespace manumesh
