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
  explicit FeatureDetector(FeatureOptions options = {});
  ~FeatureDetector();

  FeatureDetector(const FeatureDetector& other);
  FeatureDetector& operator=(const FeatureDetector& other);
  FeatureDetector(FeatureDetector&& other) noexcept;
  FeatureDetector& operator=(FeatureDetector&& other) noexcept;

  /// Returns the options used by subsequent analyses.
  const FeatureOptions& options() const;
  /// Replaces the options used by subsequent analyses.
  void setOptions(FeatureOptions options);

  /// Detects boundary, non-manifold, dihedral, tensor, and fitted primitive curves.
  FeatureAnalysis analyze(const Mesh& mesh) const;

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

#if defined(_MSC_VER)
#pragma warning(pop)
#endif

/// Computes local normal-tensor scores from one-ring face normals.
MANUMESH_API std::vector<NormalTensorVertex>
computeNormalTensorFeatures(const Mesh& mesh, const NormalTensorOptions& options = {});

/// Throws std::invalid_argument when feature-detection options are inconsistent.
MANUMESH_API void validateFeatureOptions(const FeatureOptions& options);

/// Detects boundary, non-manifold, dihedral, tensor, and fitted primitive curves.
///
/// The implementation first traces graph-supported loops, then applies bounded
/// CAD repair fallbacks for sparse circular loops. It is not a general
/// curvature-ridge extractor for noisy scans; enable tensor features and tune
/// scale/threshold parameters for that regime.
MANUMESH_API FeatureAnalysis detectFeatureCurves(const Mesh& mesh,
                                                 const FeatureOptions& options);

/// Measures one detected loop against a supplied circle.
MANUMESH_API DirectionalCurveError measureLoopAgainstCircle(const Mesh& mesh,
                                                            const FeatureLoop& loop,
                                                            const Vec3& center,
                                                            const Vec3& normal,
                                                            double radius);

/// CSV header for feature-loop reports.
MANUMESH_API std::string featureReportHeaderCsv();
/// CSV row for one feature loop.
MANUMESH_API std::string featureLoopRowCsv(const FeatureLoop& loop);
/// Stable string name for a fitted feature primitive.
MANUMESH_API std::string toString(FeaturePrimitiveType primitive);
/// Compares detected graph edges against vertex-index ground-truth labels.
MANUMESH_API FeatureEdgeBenchmark
benchmarkFeatureEdges(const FeatureAnalysis& analysis,
                      const std::vector<std::pair<int, int>>& groundTruthEdges,
                      const std::vector<int>& groundTruthJunctionVertices = {});

} // namespace manumesh::feature
