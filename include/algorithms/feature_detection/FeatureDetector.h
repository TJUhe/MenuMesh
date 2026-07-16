/**
 * @file include/algorithms/feature_detection/FeatureDetector.h
 * @brief Declares feature detector facilities for ManuMesh's feature-detection module.
 * @ingroup manumesh_feature_detection
 *
 * @details This file is part of the deterministic triangle-surface feature pipeline. Local evidence is kept separate from graph cleanup, tracing, primitive recovery, and patch segmentation so each stage has an explicit contract.
 */

#pragma once

#include "Export.h"
#include "algorithms/feature_detection/FeatureTypes.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace manumesh::feature {

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4251)
#endif

/// Stateful feature-detection facade.
///
/// This module is a first-class algorithm next to QEM simplification. It only
/// depends on core mesh types, so simplification, validation, repair, remeshing,
/// or future CAD-oriented algorithms can all consume the same FeatureAnalysis
/// without creating a dependency back into QEM.
class MANUMESH_API FeatureDetector {
public:
    /// @param[in] options Validated options copied into the detector.
    /// @throws std::invalid_argument when option ranges are inconsistent.
    explicit FeatureDetector(FeatureOptions options = {});
    ~FeatureDetector();

    FeatureDetector(const FeatureDetector& other);
    FeatureDetector& operator=(const FeatureDetector& other);
    FeatureDetector(FeatureDetector&& other) noexcept;
    FeatureDetector& operator=(FeatureDetector&& other) noexcept;

    /// Returns the options used by subsequent analyses.
    const FeatureOptions& options() const;
    /// Replaces the options used by subsequent analyses.
    /// @param[in] options New validated option set.
    /// @throws std::invalid_argument when option ranges are inconsistent.
    void setOptions(FeatureOptions options);

    /// Detects hard evidence, optional tensor/curvature evidence, and fitted curves.
    /// @param[in] mesh Triangle surface mesh; zero-area faces are tolerated and reported.
    /// @return Complete feature graph, curves, components, diagnostics, and optional patches.
    /// @throws std::invalid_argument for invalid indices, non-finite coordinates, or repeated face vertices.
    FeatureAnalysis analyze(const Mesh& mesh) const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

#if defined(_MSC_VER)
#pragma warning(pop)
#endif

/// Stabilizes face normals for noisy-input feature detection while preserving
/// mesh topology and vertex positions.
/// @param[in] mesh Input triangle mesh.
/// @param[in] options Filter iterations, angular bandwidth, preservation angle, and relaxation.
/// @return One filtered normal per face plus quantitative diagnostics.
/// @algorithm Iteratively computes an angular edge indicator, freezes strong
/// discontinuities, and relaxes remaining face normals with area weighting.
MANUMESH_API FeatureNormalFilterResult
filterFeatureNormals(const Mesh& mesh, const FeatureNormalFilterOptions& options = {});

/// Computes local normal-tensor scores from multiscale face-normal voting.
/// @param[in] mesh Input triangle surface.
/// @param[in] options Smoothing and scale schedule.
/// @return One tensor decomposition and persistence record per vertex.
/// @algorithm Accumulates area/spatially weighted normal outer products,
/// eigendecomposes the symmetric tensor, and derives surface, crease, and
/// corner saliency from ordered eigenvalue differences.
/// @failuremodes Isolated vertices and neighborhoods containing only unusable
/// faces return zero evidence rather than fabricating a direction.
MANUMESH_API std::vector<NormalTensorVertex>
computeNormalTensorFeatures(const Mesh& mesh, const NormalTensorOptions& options = {});

/// Counts a scale as persistent only when its saliency reaches the supplied threshold.
/// @param[in] mesh Input triangle surface.
/// @param[in] options Smoothing and scale schedule.
/// @param[in] persistenceThreshold Minimum scale-normalized support per scale.
/// @return One tensor record per input vertex.
MANUMESH_API std::vector<NormalTensorVertex>
computeNormalTensorFeatures(const Mesh& mesh, const NormalTensorOptions& options, double persistenceThreshold);

/// Computes deterministic smooth ridge/valley evidence from robust local
/// quadric fits, principal curvatures, directional extrema, and scale
/// persistence. No learned model or training data is used.
/// @param[in] mesh Input triangle surface.
/// @param[in] options Neighborhood radii, robust iterations, and stability policy.
/// @return One signed curvature-evidence record per vertex.
/// @algorithm Gathers deterministic k-ring neighborhoods, normalizes by local
/// sampling scale, fits a robust Monge quadric, recovers principal curvatures
/// and directions, tests two-sided directional extrema, then requires sign and
/// tangent persistence across scales.
/// @complexity O(V * S * N), where S is scale count and N is the bounded
/// neighborhood size used by each local least-squares fit.
/// @failuremodes Rank-deficient or one-sided neighborhoods, unstable principal
/// frames, and inconsistent extrema are reported as zero evidence.
MANUMESH_API std::vector<SmoothCurvatureVertex>
computeSmoothCurvatureFeatures(const Mesh& mesh, const SmoothCurvatureOptions& options = {});

/// Counts a scale as persistent only when its normalized score reaches the
/// supplied threshold.
/// @param[in] mesh Input triangle surface.
/// @param[in] options Neighborhood and robust-fit schedule.
/// @param[in] persistenceThreshold Minimum normalized score at a supporting scale.
/// @return One signed curvature-evidence record per vertex.
MANUMESH_API std::vector<SmoothCurvatureVertex>
computeSmoothCurvatureFeatures(const Mesh& mesh, const SmoothCurvatureOptions& options, double persistenceThreshold);

/// Validates every feature-detection option and cross-field range.
/// @param[in] options Options to validate.
/// @throws std::invalid_argument on a non-finite value, invalid range, or a
/// persistence count larger than its scale count.
MANUMESH_API void validateFeatureOptions(const FeatureOptions& options);

/// Detects boundary, non-manifold, dihedral, tensor, optional smooth-curvature,
/// and fitted primitive curves.
///
/// The implementation first traces graph-supported loops, then applies bounded
/// CAD repair fallbacks for sparse circular loops. It is not a general
/// curvature-ridge extractor for noisy scans; enable tensor features and tune
/// scale/threshold parameters for that regime.
/// @param[in] mesh Triangle surface mesh.
/// @param[in] options Detection, recovery, cleanup, and segmentation policy.
/// @return Complete deterministic feature analysis.
/// @algorithm Collects hard and weak evidence, builds the explicit trace graph,
/// cleans and consolidates compatible components, traces chains and cycles,
/// recovers bounded fallback cycles, fits analytic primitives, computes
/// component confidence, and optionally partitions faces into patches.
/// @invariants Evidence counts exclude synthetic bridge edges; graph edge
/// endpoints remain valid mesh vertices; each simplified loop owns a stable id.
MANUMESH_API FeatureAnalysis detectFeatureCurves(const Mesh& mesh, const FeatureOptions& options);

/// Builds a face partition separated by active feature-graph edges and writes
/// it into analysis.facePatchIds / patches / patchAdjacencies.
/// @param[in] mesh Mesh that produced `analysis`.
/// @param[in,out] analysis Existing graph plus destination patch arrays.
/// @param[in] options Patch enablement and weak-edge boundary policy.
MANUMESH_API void
segmentFeaturePatches(const Mesh& mesh, FeatureAnalysis& analysis, const SurfacePatchOptions& options = {});

/// Measures one detected loop against a supplied circle.
/// @param[in] mesh Mesh containing the loop vertices.
/// @param[in] loop Loop to measure.
/// @param[in] center Circle center in model coordinates.
/// @param[in] normal Circle-plane normal; normalized internally.
/// @param[in] radius Positive circle radius.
/// @return Directional radial/plane deviations and sample counts.
MANUMESH_API DirectionalCurveError measureLoopAgainstCircle(
    const Mesh& mesh, const FeatureLoop& loop, const Vec3& center, const Vec3& normal, double radius
);

/// CSV header for feature-loop reports.
MANUMESH_API std::string featureReportHeaderCsv();
/// CSV row for one feature loop.
MANUMESH_API std::string featureLoopRowCsv(const FeatureLoop& loop);
/// Stable string name for a fitted feature primitive.
MANUMESH_API std::string toString(FeaturePrimitiveType primitive);
/// Compares detected graph edges against vertex-index ground-truth labels.
/// @param[in] analysis Detection result to score.
/// @param[in] groundTruthEdges Undirected labeled edges.
/// @param[in] groundTruthJunctionVertices Optional labeled junction vertices.
/// @return Precision/recall/F1 and junction diagnostics.
MANUMESH_API FeatureEdgeBenchmark benchmarkFeatureEdges(
    const FeatureAnalysis& analysis,
    const std::vector<std::pair<int, int>>& groundTruthEdges,
    const std::vector<int>& groundTruthJunctionVertices = {}
);

/// Extended benchmark for branch continuation and face-patch partition labels.
/// @param[in] mesh Mesh that produced `analysis`.
/// @param[in] analysis Detection result to score.
/// @param[in] labels Edge, junction, continuation, and patch ground truth.
/// @return Aggregate benchmark metrics for every supplied label family.
MANUMESH_API FeatureEdgeBenchmark
benchmarkFeatureAnalysis(const Mesh& mesh, const FeatureAnalysis& analysis, const FeatureBenchmarkLabels& labels);

} // namespace manumesh::feature
