#pragma once

#include "line_quadrics_qem/Export.h"
#include "line_quadrics_qem/algorithms/feature_detection/FeatureTypes.h"

#include <memory>
#include <string>
#include <vector>

namespace lq::feature {

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
class LQ_API FeatureDetector {
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
LQ_API std::vector<NormalTensorVertex>
computeNormalTensorFeatures(const Mesh& mesh, const NormalTensorOptions& options = {});

/// Detects boundary, non-manifold, dihedral, tensor, and fitted primitive curves.
///
/// The implementation first traces graph-supported loops, then applies bounded
/// CAD repair fallbacks for sparse circular loops. It is not a general
/// curvature-ridge extractor for noisy scans; enable tensor features and tune
/// scale/threshold parameters for that regime.
LQ_API FeatureAnalysis detectFeatureCurves(const Mesh& mesh,
                                           const FeatureOptions& options);

/// Measures one detected loop against a supplied circle.
LQ_API DirectionalCurveError measureLoopAgainstCircle(const Mesh& mesh,
                                                      const FeatureLoop& loop,
                                                      const Vec3& center,
                                                      const Vec3& normal,
                                                      double radius);

/// CSV header for feature-loop reports.
LQ_API std::string featureReportHeaderCsv();
/// CSV row for one feature loop.
LQ_API std::string featureLoopRowCsv(const FeatureLoop& loop);
/// Stable string name for a fitted feature primitive.
LQ_API std::string toString(FeaturePrimitiveType primitive);

} // namespace lq::feature

namespace lq {

using feature::computeNormalTensorFeatures;
using feature::detectFeatureCurves;
using feature::FeatureDetector;
using feature::featureLoopRowCsv;
using feature::featureReportHeaderCsv;
using feature::measureLoopAgainstCircle;
using feature::toString;

} // namespace lq
