#include "line_quadrics_qem/algorithms/simplification/QEMSimplifier.h"

#include "detail/SimplificationRun.h"
#include "detail/SimplificationValidation.h"

#include <memory>
#include <stdexcept>
#include <utility>

namespace lq {

struct QEMSimplifier::Impl {
  SimplifyOptions options;
  SimplifyReport report;
};

WeightMode parseWeightMode(const std::string& value) {
  if (value == "uniform") return WeightMode::Uniform;
  if (value == "dihedral") return WeightMode::Dihedral;
  if (value == "normal-tensor" || value == "normal_tensor") {
    return WeightMode::NormalTensor;
  }
  if (value == "height") return WeightMode::Height;
  if (value == "xband") return WeightMode::XBand;
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

QEMSimplifier::QEMSimplifier() : impl_(std::make_unique<Impl>()) {
}

QEMSimplifier::QEMSimplifier(SimplifyOptions options)
    : impl_(std::make_unique<Impl>()) {
  impl_->options = std::move(options);
}

QEMSimplifier::~QEMSimplifier() = default;

QEMSimplifier::QEMSimplifier(const QEMSimplifier& other)
    : impl_(other.impl_ ? std::make_unique<Impl>(*other.impl_)
                        : std::make_unique<Impl>()) {
}

QEMSimplifier& QEMSimplifier::operator=(const QEMSimplifier& other) {
  if (this != &other) {
    impl_ =
        other.impl_ ? std::make_unique<Impl>(*other.impl_) : std::make_unique<Impl>();
  }
  return *this;
}

QEMSimplifier::QEMSimplifier(QEMSimplifier&& other) noexcept
    : impl_(std::move(other.impl_)) {
  if (!impl_) {
    impl_ = std::make_unique<Impl>();
  }
  other.impl_ = std::make_unique<Impl>();
}

QEMSimplifier& QEMSimplifier::operator=(QEMSimplifier&& other) noexcept {
  if (this != &other) {
    impl_ = std::move(other.impl_);
    if (!impl_) {
      impl_ = std::make_unique<Impl>();
    }
    other.impl_ = std::make_unique<Impl>();
  }
  return *this;
}

const SimplifyOptions& QEMSimplifier::options() const {
  return impl_->options;
}

void QEMSimplifier::setOptions(SimplifyOptions options) {
  impl_->options = std::move(options);
}

const SimplifyReport& QEMSimplifier::report() const {
  return impl_->report;
}

Mesh QEMSimplifier::simplify(const Mesh& input) {
  return simplify(input, nullptr);
}

Mesh QEMSimplifier::simplify(const Mesh& input, SimplifyReport* outReport) {
  validateSimplifyOptions(impl_->options);
  validateSimplifierInput(input);
  SimplificationRun run(input, impl_->options);
  Mesh output = run.execute(&impl_->report);
  if (outReport) {
    *outReport = impl_->report;
  }
  return output;
}

Mesh simplifyMesh(const Mesh& input, const SimplifyOptions& options,
                  SimplifyReport* outReport) {
  QEMSimplifier simplifier(options);
  return simplifier.simplify(input, outReport);
}

} // namespace lq
