/**
 * @file src/simplification/QEMSimplifier.cpp
 * @brief 实现公共 QEM 简化器外观和便捷入口。
 * @ingroup manumesh_simplification
 *
 * @details 实现公开的简化器外观、选项校验以及预计算特征的重载接口。
 * @algorithm 一次运行要么只计算一次特征分析，要么校验调用方提供的分析结果；随后构建 SimplificationRun，执行折叠和可选的细化阶段，并保存或复制最终报告。
 */

#include "algorithms/simplification/QEMSimplifier.h"

#include "algorithms/feature_detection/FeatureDetector.h"
#include "algorithms/simplification/PlainSimplifier.h"
#include "detail/SimplificationRun.h"
#include "detail/SimplificationValidation.h"

#include <memory>
#include <stdexcept>
#include <utility>

namespace manumesh {
namespace simplification {

/** @brief QEMSimplifier 所拥有的私有选项和最新报告。*/
struct QEMSimplifier::Impl {
    SimplifyOptions options;
    SimplifyReport report;
};

WeightMode parseWeightMode(const std::string& value) {
    if (value == "uniform")
        return WeightMode::Uniform;
    if (value == "dihedral")
        return WeightMode::Dihedral;
    if (value == "normal-tensor" || value == "normal_tensor") {
        return WeightMode::NormalTensor;
    }
    if (value == "height")
        return WeightMode::Height;
    if (value == "xband")
        return WeightMode::XBand;
    throw std::invalid_argument("Unknown weight mode: " + value);
}

std::string toString(WeightMode mode) {
    switch (mode) {
    case WeightMode::Uniform:
        return "uniform";
    case WeightMode::Dihedral:
        return "dihedral";
    case WeightMode::NormalTensor:
        return "normal-tensor";
    case WeightMode::Height:
        return "height";
    case WeightMode::XBand:
        return "xband";
    }
    return "unknown";
}

std::string toString(SimplifyTerminationReason reason) {
    switch (reason) {
    case SimplifyTerminationReason::NotStarted:
        return "not-started";
    case SimplifyTerminationReason::ReachedTarget:
        return "reached-target";
    case SimplifyTerminationReason::AlreadyAtOrBelowTarget:
        return "already-at-or-below-target";
    case SimplifyTerminationReason::NoCandidates:
        return "no-candidates";
    case SimplifyTerminationReason::RejectionLimit:
        return "rejection-limit";
    }
    return "unknown";
}

FeatureProtectionMode parseFeatureProtectionMode(const std::string& value) {
    if (value == "none")
        return FeatureProtectionMode::None;
    if (value == "circular-only" || value == "circular_only") {
        return FeatureProtectionMode::CircularOnly;
    }
    if (value == "primitive-curves" || value == "primitive_curves" || value == "primitive") {
        return FeatureProtectionMode::PrimitiveCurves;
    }
    if (value == "all-feature-edges" || value == "all_feature_edges" || value == "all") {
        return FeatureProtectionMode::AllFeatureEdges;
    }
    throw std::invalid_argument("Unknown feature protection mode: " + value);
}

std::string toString(FeatureProtectionMode mode) {
    switch (mode) {
    case FeatureProtectionMode::None:
        return "none";
    case FeatureProtectionMode::CircularOnly:
        return "circular-only";
    case FeatureProtectionMode::PrimitiveCurves:
        return "primitive-curves";
    case FeatureProtectionMode::AllFeatureEdges:
        return "all-feature-edges";
    }
    return "unknown";
}

QEMSimplifier::QEMSimplifier()
    : impl_(std::make_unique<Impl>()) {}

QEMSimplifier::QEMSimplifier(SimplifyOptions options)
    : impl_(std::make_unique<Impl>()) {
    validateSimplifyOptions(options);
    impl_->options = std::move(options);
}

QEMSimplifier::~QEMSimplifier() = default;

QEMSimplifier::QEMSimplifier(const QEMSimplifier& other)
    : impl_(other.impl_ ? std::make_unique<Impl>(*other.impl_) : std::make_unique<Impl>()) {}

QEMSimplifier& QEMSimplifier::operator=(const QEMSimplifier& other) {
    if (this != &other) {
        impl_ = other.impl_ ? std::make_unique<Impl>(*other.impl_) : std::make_unique<Impl>();
    }
    return *this;
}

QEMSimplifier::QEMSimplifier(QEMSimplifier&& other) noexcept
    : impl_(std::move(other.impl_)) {}

QEMSimplifier& QEMSimplifier::operator=(QEMSimplifier&& other) noexcept {
    if (this != &other) {
        impl_ = std::move(other.impl_);
    }
    return *this;
}

const SimplifyOptions& QEMSimplifier::options() const {
    static const SimplifyOptions defaultOptions{};
    return impl_ ? impl_->options : defaultOptions;
}

void QEMSimplifier::setOptions(SimplifyOptions options) {
    validateSimplifyOptions(options);
    if (!impl_) {
        impl_ = std::make_unique<Impl>();
    }
    impl_->options = std::move(options);
}

void QEMSimplifier::setConfig(const SimplifyConfig& config) {
    setOptions(makeSimplifyOptions(config));
}

const SimplifyReport& QEMSimplifier::report() const {
    static const SimplifyReport emptyReport;
    return impl_ ? impl_->report : emptyReport;
}

Mesh QEMSimplifier::simplify(const Mesh& input) { return simplify(input, nullptr); }

Mesh QEMSimplifier::simplify(const Mesh& input, SimplifyReport* outReport) {
    if (!impl_) {
        impl_ = std::make_unique<Impl>();
    }
    validateSimplifyOptions(impl_->options);
    validateSimplifierInput(input);
    SimplificationRun run(input, impl_->options);
    Mesh output = run.execute(&impl_->report);
    if (outReport) {
        *outReport = impl_->report;
    }
    return output;
}

Mesh QEMSimplifier::simplify(const Mesh& input, const feature::FeatureAnalysis& features) {
    return simplify(input, features, nullptr);
}

Mesh QEMSimplifier::simplify(const Mesh& input, const feature::FeatureAnalysis& features, SimplifyReport* outReport) {
    if (!impl_) {
        impl_ = std::make_unique<Impl>();
    }
    validateSimplifyOptions(impl_->options);
    validateSimplifierInput(input);
    feature::validateFeatureAnalysis(input, features);
    SimplificationRun run(input, impl_->options, &features);
    Mesh output = run.execute(&impl_->report);
    if (outReport) {
        *outReport = impl_->report;
    }
    return output;
}

Mesh simplifyMesh(const Mesh& input, const SimplifyOptions& options, SimplifyReport* outReport) {
    QEMSimplifier simplifier(options);
    return simplifier.simplify(input, outReport);
}

Mesh simplifyMesh(
    const Mesh& input,
    const SimplifyOptions& options,
    const feature::FeatureAnalysis& features,
    SimplifyReport* outReport
) {
    QEMSimplifier simplifier(options);
    return simplifier.simplify(input, features, outReport);
}

PlainMesh simplifyPlainMesh(const PlainMesh& input, const SimplifyOptions& options, SimplifyReport* outReport) {
    return toPlainMesh(simplifyMesh(toMesh(input), options, outReport));
}

} // namespace simplification
} // namespace manumesh
