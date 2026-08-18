/**
 * @file apps/CliOptionBinding.cpp
 * @brief Implements CLI binding to feature-detection and simplification configuration.
 * @ingroup manumesh_cli
 *
 * @details The CLI starts from a small source-mesh profile, then applies only
 * explicitly supplied flags. This keeps profiles predictable and retains the
 * legacy flat SimplifyOptions adapter at one boundary.
 */

#include "CliOptionBinding.h"

#include <algorithm>
#include <iomanip>
#include <initializer_list>
#include <locale>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace manumesh {
namespace cli {
namespace {

bool hasAnyFlag(const Args& args, std::initializer_list<const char*> names) {
    for (const char* name : names) {
        if (hasFlag(args, name)) {
            return true;
        }
    }
    return false;
}

void overrideDouble(const Args& args, const char* name, double& value) {
    if (hasFlag(args, name)) {
        value = getDoubleArg(args, name, value);
    }
}

void overrideInt(const Args& args, const char* name, int& value) {
    if (hasFlag(args, name)) {
        value = getIntArg(args, name, value);
    }
}

const char* onOff(bool value) { return value ? "on" : "off"; }

void applyIndustrialSafeCompatibility(simplification::SimplifyOptions& options) {
    options.preserveBoundary = true;
    options.minTriangleQuality = std::max(options.minTriangleQuality, 1e-4);
    options.maxNormalDeviationDeg = std::min(options.maxNormalDeviationDeg, 75.0);
    if (options.maxLocalErrorRatio == 0.0) {
        options.maxLocalErrorRatio = 0.02;
    } else if (options.maxLocalErrorRatio > 0.0) {
        options.maxLocalErrorRatio = std::min(options.maxLocalErrorRatio, 0.02);
    }
    options.preventLocalIntersections = true;
    options.qualityRefinementIterations = std::max(options.qualityRefinementIterations, 2);
}

void writeFeatureSummary(std::ostringstream& out, const feature::FeatureOptions& options) {
    out << "  feature_detection: angle_deg=" << options.featureAngleDeg
        << " loop_trace_angle_deg=" << options.loopTraceAngleDeg
        << " min_loop_vertices=" << options.minFeatureLoopVertices
        << " circle_fit_threshold=" << options.circleFitRelativeThreshold
        << " ellipse_fit_threshold=" << options.ellipseFitRelativeThreshold
        << " near_circle_axis_ratio_tolerance=" << options.nearCircleAxisRatioTolerance << "\n";
    out << "  normal_tensor: enabled=" << onOff(options.useNormalTensorFeatures)
        << " threshold=" << options.normalTensorFeatureThreshold
        << " edge_alignment=" << options.normalTensorMinEdgeAlignment
        << " smoothing=" << options.normalTensorSmoothingIterations << " scales=" << options.normalTensorScaleCount
        << " persistent_scales=" << options.normalTensorMinPersistentScales << "\n";
    out << "  feature_graph: cleanup=" << onOff(options.cleanupFeatureGraph)
        << " gap_ratio=" << options.featureGraphGapLengthRatio
        << " max_weak_spur_edges=" << options.featureGraphMaxWeakSpurEdges
        << " min_weak_spur_strength=" << options.featureGraphMinWeakSpurStrength
        << " component_confidence_report_threshold=" << options.featureComponentMinConfidence << "\n";
    out << "  normal_filter: enabled=" << onOff(options.normalFilter.enabled)
        << " iterations=" << options.normalFilter.iterations << " angle_sigma_deg=" << options.normalFilter.angleSigmaDeg
        << " preserve_angle_deg=" << options.normalFilter.preserveAngleDeg
        << " relaxation=" << options.normalFilter.relaxation << "\n";
    out << "  graph_consolidation: enabled=" << onOff(options.graphConsolidation.enabled)
        << " gap_ratio=" << options.graphConsolidation.maxGapLengthRatio
        << " alignment=" << options.graphConsolidation.minAlignment << "\n";
    out << "  surface_patches: enabled=" << onOff(options.surfacePatches.enabled)
        << " include_weak_evidence=" << onOff(options.surfacePatches.includeWeakEvidence) << "\n";
}

void writeWarning(std::ostream& output, const std::string& message) { output << "warning: " << message << "\n"; }

} // namespace

feature::FeatureProfile parseFeatureProfile(const Args& args) {
    const std::string value = getArg(args, "--profile", "default");
    if (value == "default") {
        return feature::FeatureProfile::Default;
    }
    if (value == "cad") {
        return feature::FeatureProfile::Cad;
    }
    if (value == "scan" || value == "noisy-scan") {
        return feature::FeatureProfile::NoisyScan;
    }
    throw std::invalid_argument("Unknown --profile. Use default, cad, scan (or noisy-scan).");
}

const char* featureProfileName(feature::FeatureProfile profile) noexcept {
    switch (profile) {
    case feature::FeatureProfile::Default:
        return "default";
    case feature::FeatureProfile::Cad:
        return "cad";
    case feature::FeatureProfile::NoisyScan:
        return "scan";
    }
    return "default";
}

feature::FeatureOptions parseFeatureOptions(const Args& args) {
    feature::FeatureOptions options = feature::makeFeatureOptions(parseFeatureProfile(args));
    overrideDouble(args, "--feature-angle-deg", options.featureAngleDeg);
    overrideDouble(args, "--loop-trace-angle-deg", options.loopTraceAngleDeg);
    overrideDouble(args, "--circle-fit-threshold", options.circleFitRelativeThreshold);
    overrideDouble(args, "--ellipse-fit-threshold", options.ellipseFitRelativeThreshold);
    overrideDouble(args, "--near-circle-axis-ratio-tolerance", options.nearCircleAxisRatioTolerance);
    overrideInt(args, "--min-feature-loop-vertices", options.minFeatureLoopVertices);
    overrideDouble(args, "--normal-tensor-threshold", options.normalTensorFeatureThreshold);
    overrideDouble(args, "--normal-tensor-edge-alignment", options.normalTensorMinEdgeAlignment);
    overrideInt(args, "--normal-tensor-smoothing", options.normalTensorSmoothingIterations);
    overrideInt(args, "--normal-tensor-scales", options.normalTensorScaleCount);
    overrideInt(args, "--normal-tensor-min-persistent-scales", options.normalTensorMinPersistentScales);
    overrideDouble(args, "--feature-graph-gap-ratio", options.featureGraphGapLengthRatio);
    overrideInt(args, "--feature-graph-max-weak-spur-edges", options.featureGraphMaxWeakSpurEdges);
    overrideDouble(args, "--feature-graph-min-weak-spur-strength", options.featureGraphMinWeakSpurStrength);
    overrideDouble(args, "--feature-component-min-confidence", options.featureComponentMinConfidence);
    overrideInt(args, "--feature-normal-filter-iterations", options.normalFilter.iterations);
    overrideDouble(args, "--feature-normal-filter-angle-sigma-deg", options.normalFilter.angleSigmaDeg);
    overrideDouble(args, "--feature-normal-filter-preserve-angle-deg", options.normalFilter.preserveAngleDeg);
    overrideDouble(args, "--feature-normal-filter-relaxation", options.normalFilter.relaxation);
    overrideDouble(args, "--feature-graph-consolidation-gap-ratio", options.graphConsolidation.maxGapLengthRatio);
    overrideDouble(args, "--feature-graph-consolidation-alignment", options.graphConsolidation.minAlignment);

    if (hasFlag(args, "--normal-tensor-features")) {
        options.useNormalTensorFeatures = true;
    }
    if (hasFlag(args, "--no-normal-tensor-features")) {
        options.useNormalTensorFeatures = false;
    }
    options.cleanupFeatureGraph = !hasFlag(args, "--no-feature-graph-cleanup");
    if (hasFlag(args, "--feature-normal-filter")) {
        options.normalFilter.enabled = true;
    }
    if (hasFlag(args, "--no-feature-normal-filter")) {
        options.normalFilter.enabled = false;
    }
    options.graphConsolidation.enabled = hasFlag(args, "--feature-graph-consolidation");
    options.surfacePatches.enabled = hasFlag(args, "--surface-patches");
    options.surfacePatches.includeWeakEvidence = !hasFlag(args, "--surface-patches-strong-only");
    return options;
}

ExecutionOptions parseExecutionOptions(const Args& args) {
    ExecutionOptions options;
    if (!hasFlag(args, "--threads")) {
        return options;
    }

    const int threads = getIntArg(args, "--threads", 0);
    if (threads < 0) {
        throw std::invalid_argument("--threads must be non-negative (0 selects the backend default).");
    }
    if (threads != 1 && !isParallelExecutionAvailable()) {
        throw std::invalid_argument(
            "--threads requires a build with MANUMESH_ENABLE_ONETBB=ON; use --threads 1 for a serial run."
        );
    }
    options.mode = ExecutionMode::Parallel;
    options.maxConcurrency = threads;
    validateExecutionOptions(options);
    return options;
}

simplification::SimplifyConfig parseSimplifyConfig(const Args& args) {
    simplification::SimplifyConfig config = simplification::makeSimplifyConfig(parseFeatureProfile(args));
    config.features.detection = parseFeatureOptions(args);

    if (hasFlag(args, "--target-faces")) {
        config.target = simplification::SimplifyTarget::faceCount(getIntArg(args, "--target-faces", -1));
    } else if (hasFlag(args, "--ratio")) {
        config.target = simplification::SimplifyTarget::ratio(getDoubleArg(args, "--ratio", config.target.ratio()));
    }

    const std::string method = getArg(args, "--method");
    if (!method.empty() && method != "standard" && method != "qem" && method != "line") {
        throw std::invalid_argument("Unknown --method. Use standard (or qem) or line.");
    }
    const bool standardMethod = method == "standard" || method == "qem";
    const bool adaptiveLineQuadrics = hasFlag(args, "--adaptive-scale");
    if (standardMethod) {
        config.cost.lineQuadrics = simplification::LineQuadricConfig::disabled();
    } else if (adaptiveLineQuadrics) {
        const double baseWeight = getDoubleArg(args, "--adaptive-base-line-weight", 1e-2);
        if (baseWeight < 0.0) {
            throw std::invalid_argument("--adaptive-base-line-weight must be non-negative.");
        }
        config.cost.lineQuadrics = baseWeight > 0.0 ? simplification::LineQuadricConfig::adaptive(baseWeight)
                                                     : simplification::LineQuadricConfig::disabled();
    } else if (hasFlag(args, "--line-weight")) {
        const double lineWeight = getDoubleArg(args, "--line-weight", config.cost.lineQuadrics.weight());
        if (lineWeight < 0.0) {
            throw std::invalid_argument("--line-weight must be non-negative.");
        }
        config.cost.lineQuadrics = lineWeight > 0.0 ? simplification::LineQuadricConfig::uniform(lineWeight)
                                                     : simplification::LineQuadricConfig::disabled();
    } else if (method == "line" && config.cost.lineQuadrics.kind() == simplification::LineQuadricConfig::Kind::Disabled) {
        config.cost.lineQuadrics = simplification::LineQuadricConfig::uniform(1e-3);
    }

    if (hasFlag(args, "--weight-mode")) {
        config.cost.weightMode = simplification::parseWeightMode(getArg(args, "--weight-mode"));
    }
    overrideDouble(args, "--feature-boost", config.cost.featureBoost);
    overrideDouble(args, "--boundary-weight", config.cost.boundaryWeight);

    if (hasFlag(args, "--preserve-feature-curves")) {
        config.features.enabled = true;
    }
    if (hasFlag(args, "--no-preserve-feature-curves")) {
        config.features.enabled = false;
    } else if (hasAnyFlag(args, {"--feature-normal-filter", "--feature-graph-consolidation"})) {
        // Retain the 0.x convenience behavior, but emit a note in emitOptionWarnings.
        config.features.enabled = true;
    }
    if (hasFlag(args, "--feature-protection-mode")) {
        config.features.protectionMode =
            simplification::parseFeatureProtectionMode(getArg(args, "--feature-protection-mode"));
    }
    overrideDouble(args, "--feature-curve-weight", config.features.curveWeight);
    overrideDouble(args, "--max-feature-curve-deviation-ratio", config.features.maxCurveDeviationRatio);
    overrideInt(args, "--min-circular-feature-loop-vertices", config.features.minCircularLoopVertices);

    if (hasFlag(args, "--preserve-boundary")) {
        config.quality.preserveBoundary = true;
    }
    overrideDouble(args, "--min-triangle-quality", config.quality.minTriangleQuality);
    overrideDouble(args, "--max-normal-deviation-deg", config.quality.maxNormalDeviationDeg);
    if (hasFlag(args, "--max-local-error")) {
        config.quality.localError =
            simplification::SimplifyErrorLimit::absolute(getDoubleArg(args, "--max-local-error", 0.0));
    } else if (hasFlag(args, "--max-local-error-ratio")) {
        config.quality.localError = simplification::SimplifyErrorLimit::boundingBoxRatio(
            getDoubleArg(args, "--max-local-error-ratio", 0.0)
        );
    }
    if (hasFlag(args, "--prevent-local-intersections")) {
        config.quality.preventLocalIntersections = true;
    }
    overrideInt(args, "--quality-refinement-iterations", config.quality.refinementIterations);

    if (hasFlag(args, "--industrial-safe")) {
        config.quality.preserveBoundary = true;
        config.quality.minTriangleQuality = std::max(config.quality.minTriangleQuality, 1e-4);
        config.quality.maxNormalDeviationDeg = std::min(config.quality.maxNormalDeviationDeg, 75.0);
        // Grouped configs carry one unambiguous local-error unit. Keep an
        // explicit absolute budget, otherwise make the safe ratio visible here.
        if (!hasFlag(args, "--max-local-error")) {
            const double requestedRatio = getDoubleArg(args, "--max-local-error-ratio", 0.0);
            const double ratio = requestedRatio == 0.0
                                     ? 0.02
                                     : (requestedRatio > 0.0 ? std::min(requestedRatio, 0.02) : requestedRatio);
            config.quality.localError = simplification::SimplifyErrorLimit::boundingBoxRatio(ratio);
        }
        config.quality.preventLocalIntersections = true;
        config.quality.refinementIterations = std::max(config.quality.refinementIterations, 2);
    }
    config.verbose = hasFlag(args, "--verbose");
    return config;
}

simplification::SimplifyOptions parseSimplifyOptions(const Args& args) {
    simplification::SimplifyOptions options = simplification::makeSimplifyOptions(parseSimplifyConfig(args));

    // The old CLI accepted both units and chose the looser bound after the
    // input diagonal was known. Preserve that behavior only at this adapter.
    if (hasFlag(args, "--max-local-error") && hasFlag(args, "--max-local-error-ratio")) {
        options.maxLocalError = getDoubleArg(args, "--max-local-error", options.maxLocalError);
        options.maxLocalErrorRatio = getDoubleArg(args, "--max-local-error-ratio", options.maxLocalErrorRatio);
    }
    if (hasFlag(args, "--industrial-safe")) {
        applyIndustrialSafeCompatibility(options);
    }
    return options;
}

std::string formatResolvedFeatureOptions(const Args& args, const feature::FeatureOptions& options) {
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << std::setprecision(12) << "resolved_feature_config profile=" << featureProfileName(parseFeatureProfile(args))
        << "\n";
    const ExecutionOptions execution = parseExecutionOptions(args);
    out << "  execution: mode=" << (execution.mode == ExecutionMode::Parallel ? "parallel" : "serial")
        << " max_threads=" << execution.maxConcurrency << " backend=" << parallelExecutionBackendName() << "\n";
    writeFeatureSummary(out, options);
    return out.str();
}

std::string formatResolvedSimplifyOptions(const Args& args, const simplification::SimplifyOptions& options) {
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << std::setprecision(12) << "resolved_simplify_config profile=" << featureProfileName(parseFeatureProfile(args))
        << "\n";
    const ExecutionOptions execution = parseExecutionOptions(args);
    out << "  execution: mode=" << (execution.mode == ExecutionMode::Parallel ? "parallel" : "serial")
        << " max_threads=" << execution.maxConcurrency << " backend=" << parallelExecutionBackendName() << "\n";
    if (options.targetFaces > 0) {
        out << "  target: faces=" << options.targetFaces << "\n";
    } else {
        out << "  target: ratio=" << options.targetRatio << "\n";
    }
    out << "  line_quadrics: enabled=" << onOff(options.useLineQuadrics)
        << " adaptive=" << onOff(options.adaptiveScale)
        << " line_weight=" << options.lineWeight << " adaptive_base_weight=" << options.adaptiveBaseLineWeight
        << " weight_mode=" << simplification::toString(options.weightMode)
        << " feature_boost=" << options.featureBoost << " boundary_weight=" << options.boundaryWeight << "\n";
    out << "  feature_protection: enabled=" << onOff(options.preserveFeatureCurves)
        << " mode=" << simplification::toString(options.featureProtectionMode)
        << " curve_weight=" << options.featureCurveWeight
        << " max_curve_deviation_ratio=" << options.maxFeatureCurveDeviationRatio
        << " min_circular_loop_vertices=" << options.minCircularFeatureLoopVertices
        << " feature_loop_safety_floor=5\n";
    const feature::FeatureOptions detection = options.featureOptionsOverride.has_value()
                                                  ? *options.featureOptionsOverride
                                                  : feature::FeatureOptions{};
    writeFeatureSummary(out, detection);
    out << "  quality: preserve_boundary=" << onOff(options.preserveBoundary)
        << " min_triangle_quality=" << options.minTriangleQuality
        << " max_normal_deviation_deg=" << options.maxNormalDeviationDeg
        << " prevent_local_intersections=" << onOff(options.preventLocalIntersections)
        << " refinement_iterations=" << options.qualityRefinementIterations << "\n";
    out << "  local_error: absolute=" << options.maxLocalError << " bbox_ratio=" << options.maxLocalErrorRatio;
    if (options.maxLocalError > 0.0 && options.maxLocalErrorRatio > 0.0) {
        out << " (legacy effective budget is the larger converted value)";
    }
    out << "\n";
    return out.str();
}

void emitOptionWarnings(const Args& args, const feature::FeatureOptions& options, bool simplifying, std::ostream& output) {
    if (hasFlag(args, "--target-faces") && hasFlag(args, "--ratio")) {
        writeWarning(output, "--target-faces takes precedence over --ratio. Use one target unit for reproducible runs.");
    }
    if (hasFlag(args, "--max-local-error") && hasFlag(args, "--max-local-error-ratio")) {
        writeWarning(
            output,
            "both local-error units were supplied; 0.x compatibility keeps the larger converted budget. Use one unit."
        );
    }
    if (hasFlag(args, "--normal-tensor-features") && hasFlag(args, "--no-normal-tensor-features")) {
        writeWarning(output, "both normal-tensor enable and disable flags were supplied; --no-normal-tensor-features wins.");
    }
    if (hasFlag(args, "--feature-normal-filter") && hasFlag(args, "--no-feature-normal-filter")) {
        writeWarning(output, "both feature-normal-filter enable and disable flags were supplied; --no-feature-normal-filter wins.");
    }
    if (hasFlag(args, "--preserve-feature-curves") && hasFlag(args, "--no-preserve-feature-curves")) {
        writeWarning(output, "both feature-curve protection enable and disable flags were supplied; --no-preserve-feature-curves wins.");
    }

    const std::string method = getArg(args, "--method");
    const bool standardMethod = method == "standard" || method == "qem";
    const feature::FeatureProfile profile = parseFeatureProfile(args);
    // Warnings must follow profile defaults as well as explicit flags. In
    // particular, scan implicitly selects NormalTensor weighting.
    const simplification::SimplifyConfig effectiveSimplifyConfig = simplifying
                                                                       ? parseSimplifyConfig(args)
                                                                       : simplification::SimplifyConfig{};
    const bool profileSelectsFeatureWeighting =
        simplifying && !hasFlag(args, "--weight-mode") &&
        (profile == feature::FeatureProfile::Cad || profile == feature::FeatureProfile::NoisyScan);
    if (standardMethod &&
        (profileSelectsFeatureWeighting ||
         hasAnyFlag(
             args,
             {"--line-weight", "--adaptive-scale", "--adaptive-base-line-weight", "--weight-mode", "--feature-boost"}
         ))) {
        writeWarning(
            output,
            "line-quadric weight settings, including --weight-mode and --feature-boost, are ignored by standard QEM. "
             "Feature detection is unaffected; feature protection follows its separate setting."
        );
    } else if (hasFlag(args, "--adaptive-scale") && hasFlag(args, "--line-weight")) {
        writeWarning(output, "--line-weight is ignored by adaptive line quadrics; use --adaptive-base-line-weight.");
    } else if (!hasFlag(args, "--adaptive-scale") && hasFlag(args, "--adaptive-base-line-weight")) {
        writeWarning(output, "--adaptive-base-line-weight requires --adaptive-scale and is otherwise ignored.");
    }
    if (simplifying && !standardMethod &&
        effectiveSimplifyConfig.cost.lineQuadrics.kind() == simplification::LineQuadricConfig::Kind::Disabled) {
        writeWarning(output, "a zero line-quadric base weight disables line quadrics; simplification runs standard QEM.");
    }
    if (simplifying && !standardMethod && hasFlag(args, "--feature-boost") &&
        effectiveSimplifyConfig.cost.weightMode == simplification::WeightMode::Uniform) {
        writeWarning(
            output,
            "--feature-boost is ignored when the effective --weight-mode is uniform; choose a non-uniform weight mode."
        );
    }

    const bool usesNormalTensorWeighting =
        simplifying && !standardMethod &&
        effectiveSimplifyConfig.cost.weightMode == simplification::WeightMode::NormalTensor;
    if (!options.useNormalTensorFeatures) {
        if (hasAnyFlag(
                args,
                {"--normal-tensor-threshold", "--normal-tensor-smoothing", "--normal-tensor-scales",
                 "--normal-tensor-min-persistent-scales"}
            ) &&
            !usesNormalTensorWeighting) {
            writeWarning(output, "normal-tensor values are ignored until --normal-tensor-features is active.");
        }
        if (hasFlag(args, "--normal-tensor-edge-alignment")) {
            writeWarning(
                output,
                "--normal-tensor-edge-alignment only affects normal-tensor feature evidence and is ignored until "
                "--normal-tensor-features is active."
            );
        }
    }
    if (hasAnyFlag(
            args,
            {"--feature-normal-filter-iterations", "--feature-normal-filter-angle-sigma-deg",
             "--feature-normal-filter-preserve-angle-deg", "--feature-normal-filter-relaxation"}
        ) &&
        !options.normalFilter.enabled) {
        writeWarning(output, "feature-normal-filter values are ignored until --feature-normal-filter or --profile scan is active.");
    }
    if (hasAnyFlag(args, {"--feature-graph-consolidation-gap-ratio", "--feature-graph-consolidation-alignment"}) &&
        !options.graphConsolidation.enabled) {
        writeWarning(output, "graph-consolidation values are ignored until --feature-graph-consolidation is active.");
    }
    if (hasFlag(args, "--surface-patches-strong-only") && !options.surfacePatches.enabled) {
        writeWarning(output, "--surface-patches-strong-only requires --surface-patches and is otherwise ignored.");
    }
    if (hasFlag(args, "--no-feature-graph-cleanup") &&
        hasAnyFlag(
            args,
            {"--feature-graph-gap-ratio", "--feature-graph-max-weak-spur-edges", "--feature-graph-min-weak-spur-strength"}
        )) {
        writeWarning(output, "feature-graph cleanup values are ignored while --no-feature-graph-cleanup is active.");
    }
    if (hasFlag(args, "--feature-component-min-confidence")) {
        output << "note: --feature-component-min-confidence only classifies report counters; it does not filter detected edges or protection.\n";
    }
    if (simplifying && hasFlag(args, "--min-feature-loop-vertices") &&
        getIntArg(args, "--min-feature-loop-vertices", 5) < 5) {
        writeWarning(
            output,
            "simplification keeps its feature-loop protection safety floor at 5; the lower value still applies to detection/reporting."
        );
    }
    if (simplifying &&
        hasAnyFlag(
            args,
            {"--feature-protection-mode", "--feature-curve-weight", "--max-feature-curve-deviation-ratio",
             "--min-circular-feature-loop-vertices"}
        ) &&
        !parseSimplifyConfig(args).features.enabled) {
        writeWarning(output, "feature-protection settings require --preserve-feature-curves or a feature profile.");
    }
    if (simplifying && !hasFlag(args, "--preserve-feature-curves") && !hasFlag(args, "--no-preserve-feature-curves") &&
        hasAnyFlag(args, {"--feature-normal-filter", "--feature-graph-consolidation"})) {
        output << "note: simplify enables feature-curve protection for these feature options to preserve 0.x behavior; use --no-preserve-feature-curves to opt out.\n";
    }
}

} // namespace cli
} // namespace manumesh
