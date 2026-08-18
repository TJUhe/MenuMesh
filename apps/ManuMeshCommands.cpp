/**
 * @file apps/ManuMeshCommands.cpp
 * @brief 实现生成、简化、比较和批量扫描等 CLI 命令。
 * @ingroup manumesh_cli
 *
 * @details 命令层负责文件路径、参数组合、统计输出和 CSV 结果编排。
 */

#include "CliArguments.h"
#include "CliCommands.h"
#include "CliCsv.h"
#include "CliOptionBinding.h"
#include "CliPath.h"
#include "ManuMeshFeatureCommands.h"
#include "ManuMeshLargeMeshCommands.h"
#include "ManuMeshWorkflowCommands.h"
#include "algorithms/analysis/MeshAnalysis.h"
#include "algorithms/simplification/QEMSimplifier.h"
#include "core/Mesh.h"
#include "core/MeshGenerators.h"
#include "io/MeshIo.h"

#include "core/Filesystem.h"
#include <algorithm>
#include <fstream>
#include <initializer_list>
#include <iomanip>
#include <iostream>
#include <limits>
#include <locale>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <unordered_set>
#include <vector>

namespace fs = manumesh::filesystem;
namespace {

using manumesh::cli::Args;
using manumesh::cli::AtomicCsvOutput;
using manumesh::cli::csvValue;
using manumesh::cli::emitOptionWarnings;
using manumesh::cli::formatResolvedSimplifyOptions;
using manumesh::cli::getArg;
using manumesh::cli::getIntArg;
using manumesh::cli::hasFlag;
using manumesh::cli::parseFaceCounts;
using manumesh::cli::parseSimplifyOptions;
using manumesh::cli::parseWeights;
using manumesh::cli::pathIdentityKey;
using manumesh::cli::pathFromUtf8;
using manumesh::cli::pathToUtf8;
using manumesh::cli::pathsReferToSameLocation;
using manumesh::cli::positionalArgs;
using manumesh::cli::quoteCsv;
using manumesh::cli::readCsvRecord;
using manumesh::cli::readFirstCsvRow;
using manumesh::cli::splitCsvLine;

int commandGenerate(const Args& args);
int commandSimplify(const Args& args);
int commandSweep(const Args& args);
int commandRatioSweep(const Args& args);
int commandFaceSweep(const Args& args);
int commandCompare(const Args& args);
int commandSummarizeMetrics(const Args& args);

std::string sanitizeNumber(double value) {
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << std::setprecision(std::numeric_limits<double>::max_digits10) << std::defaultfloat << value;
    std::string text = out.str();
    for (char& ch : text) {
        if (ch == '+') {
            ch = 'p';
        } else if (ch == '-') {
            ch = 'm';
        } else if (ch == '.') {
            ch = 'd';
        }
    }
    return text;
}

std::string sanitizeWeight(double value) { return sanitizeNumber(value); }

std::string sanitizeRatio(double value) { return sanitizeNumber(value); }

double configuredLineWeight(const manumesh::simplification::SimplifyOptions& options) {
    return options.adaptiveScale ? options.adaptiveBaseLineWeight : options.lineWeight;
}

double reportedLineWeight(const manumesh::simplification::SimplifyOptions& options) {
    return options.useLineQuadrics ? configuredLineWeight(options) : 0.0;
}

void requireUniqueSweepValues(const std::vector<double>& values, const char* flag) {
    std::set<double> unique;
    for (double value : values) {
        if (!unique.insert(value).second) {
            throw std::invalid_argument(std::string(flag) + " contains duplicate value " + std::to_string(value) + ".");
        }
    }
}

void requireUniqueSweepValues(const std::vector<int>& values, const char* flag) {
    std::set<int> unique;
    for (int value : values) {
        if (!unique.insert(value).second) {
            throw std::invalid_argument(std::string(flag) + " contains duplicate value " + std::to_string(value) + ".");
        }
    }
}

void requireNonNegativeWeights(const std::vector<double>& values, const char* flag) {
    for (double value : values) {
        if (value < 0.0) {
            throw std::invalid_argument(
                std::string(flag) + " values must be non-negative; got " + std::to_string(value) + "."
            );
        }
    }
}

void requireDistinctOutputPath(
    const fs::path& output, const fs::path& protectedPath, const std::string& outputFlag,
    const std::string& protectedLabel
) {
    if (pathsReferToSameLocation(output, protectedPath)) {
        throw std::invalid_argument(outputFlag + " must not overwrite " + protectedLabel + ".");
    }
}

void requireUniquePlannedOutputs(const std::vector<fs::path>& outputs) {
    std::unordered_set<std::string> identities;
    std::vector<fs::path> checked;
    checked.reserve(outputs.size());
    for (const fs::path& output : outputs) {
        for (const fs::path& previous : checked) {
            if (pathsReferToSameLocation(output, previous)) {
                throw std::invalid_argument("Sweep values produce colliding output files: " + pathToUtf8(output));
            }
        }
        if (!identities.insert(pathIdentityKey(output)).second) {
            throw std::invalid_argument("Sweep values produce colliding output filename: " + pathToUtf8(output));
        }
        checked.push_back(output);
    }
}

void printStats(const std::string& label, const manumesh::analysis::MeshStats& stats) {
    std::cout << label << ": vertices=" << stats.vertices << " faces=" << stats.faces
              << " mean_quality=" << stats.meanTriangleQuality << " min_quality=" << stats.minTriangleQuality
              << " edge_cv=" << stats.edgeLengthCv << "\n";
}

bool pathsReferToSameExistingFile(const fs::path& first, const fs::path& second) {
    return pathsReferToSameLocation(first, second);
}

void requireExactPositionalArguments(
    const std::vector<std::string>& positional, std::size_t expectedCount, const char* command, const char* usage
) {
    if (positional.size() != expectedCount) {
        throw std::invalid_argument(std::string(command) + " requires exactly " + usage + ".");
    }
}

void rejectSweepTargetOverride(const Args& args, const char* command, const char* sweepFlag) {
    if (hasFlag(args, "--ratio") || hasFlag(args, "--target-faces")) {
        throw std::invalid_argument(
            std::string(command) + " derives each target from " + sweepFlag +
            "; do not pass --ratio or --target-faces."
        );
    }
}

void summarizeMetrics(const fs::path& outputRoot, const fs::path& summaryPath) {
    std::vector<std::string> columns = {"case"};
    std::vector<std::map<std::string, std::string>> rows;
    std::vector<fs::path> sourceMetrics;

    if (!fs::exists(outputRoot)) {
        throw std::runtime_error("Output directory not found: " + pathToUtf8(outputRoot));
    }

    for (const fs::directory_entry& entry : fs::recursive_directory_iterator(outputRoot)) {
        if (!fs::is_regular_file(entry.path()) || entry.path().filename() != "metrics.csv") {
            continue;
        }
        sourceMetrics.push_back(entry.path());

        std::ifstream in(entry.path());
        std::string headerLine;
        if (!readCsvRecord(in, headerLine)) {
            continue;
        }
        const std::vector<std::string> headers = splitCsvLine(headerLine);
        for (const std::string& header : headers) {
            if (std::find(columns.begin(), columns.end(), header) == columns.end()) {
                columns.push_back(header);
            }
        }

        std::string line;
        const std::string caseName = pathToUtf8(entry.path().parent_path().filename());
        while (readCsvRecord(in, line)) {
            if (line.empty()) {
                continue;
            }
            const std::vector<std::string> values = splitCsvLine(line);
            std::map<std::string, std::string> row;
            row["case"] = caseName;
            for (std::size_t i = 0; i < headers.size(); ++i) {
                row[headers[i]] = i < values.size() ? values[i] : "";
            }
            rows.push_back(std::move(row));
        }
    }

    for (const fs::path& source : sourceMetrics) {
        if (pathsReferToSameLocation(summaryPath, source)) {
            throw std::invalid_argument("summary CSV must not overwrite an input metrics.csv file.");
        }
    }

    if (summaryPath.has_parent_path()) {
        fs::create_directories(summaryPath.parent_path());
    }
    AtomicCsvOutput csvFile(summaryPath);
    std::ofstream& out = csvFile.stream();
    for (std::size_t i = 0; i < columns.size(); ++i) {
        if (i > 0)
            out << ",";
        out << quoteCsv(columns[i]);
    }
    out << "\n";
    for (const auto& row : rows) {
        for (std::size_t i = 0; i < columns.size(); ++i) {
            if (i > 0)
                out << ",";
            out << quoteCsv(csvValue(row, columns[i]));
        }
        out << "\n";
    }
    csvFile.commit();
    std::cout << "Wrote " << pathToUtf8(summaryPath) << " with " << rows.size() << " rows\n";
}

int commandGenerate(const Args& args) {
    if (!positionalArgs(args).empty()) {
        throw std::invalid_argument("generate does not accept positional paths; use --out for the output file.");
    }
    const std::string type = getArg(args, "--type", "clustered-plane");
    const std::string outPath = getArg(args, "--out");
    const int n = getIntArg(args, "--n", 50);
    if (outPath.empty()) {
        throw std::invalid_argument("generate requires --out path.");
    }

    manumesh::Mesh mesh;
    std::string error;
    if (!manumesh::generateMeshByName(type, n, mesh, &error)) {
        throw std::runtime_error(error);
    }
    if (!manumesh::saveBinaryStl(outPath, mesh, &error)) {
        throw std::runtime_error(error);
    }

    printStats(type, manumesh::analysis::computeMeshStats(mesh));
    std::cout << "Wrote " << outPath << "\n";
    return 0;
}

int commandCompare(const Args& args) {
    const auto positional = positionalArgs(args);
    requireExactPositionalArguments(positional, 2, "compare", "original.stl simplified.stl");
    const int samples = getIntArg(args, "--samples", 3000);

    manumesh::Mesh original;
    manumesh::Mesh simplified;
    std::string error;
    if (!manumesh::loadMesh(positional[0], original, &error))
        throw std::runtime_error(error);
    if (!manumesh::loadMesh(positional[1], simplified, &error)) {
        throw std::runtime_error(error);
    }

    const manumesh::analysis::MeshStats stats = manumesh::analysis::computeMeshStats(simplified);
    const manumesh::analysis::DistanceStats distance =
        manumesh::analysis::compareMeshesBySampledDistance(original, simplified, samples);
    printStats("simplified", stats);
    std::cout << "distance mean original->simplified=" << distance.meanOriginalToSimplified
              << " max=" << distance.maxOriginalToSimplified << "\n";
    std::cout << "distance mean simplified->original=" << distance.meanSimplifiedToOriginal
              << " max=" << distance.maxSimplifiedToOriginal << "\n";
    return 0;
}

int commandSimplify(const Args& args) {
    const auto positional = positionalArgs(args);
    requireExactPositionalArguments(positional, 2, "simplify", "input.stl output.stl");

    const int samples = getIntArg(args, "--samples", 3000);
    const std::string metricsCsv = getArg(args, "--metrics-csv");
    const fs::path inputPath = pathFromUtf8(positional[0]);
    const fs::path outputPath = pathFromUtf8(positional[1]);
    const fs::path metricsPath = metricsCsv.empty() ? fs::path() : pathFromUtf8(metricsCsv);
    manumesh::simplification::SimplifyOptions options = parseSimplifyOptions(args);
    const manumesh::feature::FeatureOptions effectiveFeatures = options.featureOptionsOverride.has_value()
                                                                    ? *options.featureOptionsOverride
                                                                    : manumesh::feature::FeatureOptions{};
    emitOptionWarnings(args, effectiveFeatures, true, std::cerr);
    if (hasFlag(args, "--print-resolved-config")) {
        std::cout << formatResolvedSimplifyOptions(args, options);
    }
    if (pathsReferToSameExistingFile(inputPath, outputPath)) {
        throw std::invalid_argument("simplify input and output must not refer to the same file.");
    }
    if (!metricsCsv.empty()) {
        requireDistinctOutputPath(metricsPath, inputPath, "--metrics-csv", "the input mesh");
        requireDistinctOutputPath(metricsPath, outputPath, "--metrics-csv", "the simplified output");
    }

    manumesh::Mesh input;
    std::string error;
    if (!manumesh::loadMesh(positional[0], input, &error)) {
        throw std::runtime_error(error);
    }

    manumesh::simplification::SimplifyReport report;
    manumesh::simplification::QEMSimplifier simplifier(options);
    simplifier.setExecutionOptions(parseExecutionOptions(args));
    manumesh::Mesh output = simplifier.simplify(input, &report);
    if (!manumesh::saveBinaryStl(positional[1], output, &error)) {
        throw std::runtime_error(error);
    }

    const manumesh::analysis::MeshStats inStats = manumesh::analysis::computeMeshStats(input);
    const manumesh::analysis::MeshStats outStats = manumesh::analysis::computeMeshStats(output);
    const manumesh::analysis::DistanceStats distance =
        manumesh::analysis::compareMeshesBySampledDistance(input, output, samples);

    printStats("input", inStats);
    printStats("output", outStats);
    std::cout << "collapsed=" << report.collapsedEdges << " rejected=" << report.rejectedCollapses
              << " feature_rejected=" << report.featureRejectedCollapses
              << " boundary_rejected=" << report.boundaryRejectedCollapses
              << " topology_rejected=" << report.topologyRejectedCollapses
              << " normal_flip_rejected=" << report.normalFlipRejectedCollapses
              << " quality_rejected=" << report.qualityRejectedCollapses
              << " self_intersection_rejected=" << report.selfIntersectionRejectedCollapses
              << " curve_budget_rejected=" << report.curveBudgetRejectedCollapses
              << " error_rejected=" << report.errorRejectedCollapses << " solver_fallbacks=" << report.solverFallbacks
              << " refinement_iterations=" << report.qualityRefinementIterationsCompleted
              << " refinement_attempted=" << report.qualityRefinementAttemptedMoves
              << " refinement_accepted=" << report.qualityRefinementAcceptedMoves
              << " refinement_skipped_for_texture=" << (report.qualityRefinementSkippedForTexture ? 1 : 0)
              << " termination=" << manumesh::simplification::toString(report.terminationReason)
              << " line_weight_range=[" << report.minAppliedLineWeight << ", " << report.maxAppliedLineWeight << "]\n";
    if (options.preserveFeatureCurves) {
        std::cout << "feature_loops=" << report.featureLoops
                  << " circular_feature_loops=" << report.circularFeatureLoops
                  << " feature_vertices=" << report.featureVertices
                  << " traced_feature_edges=" << report.tracedFeatureEdges
                  << " untraced_feature_edges=" << report.untracedFeatureEdges
                  << " feature_components=" << report.featureComponents
                  << " weak_feature_components=" << report.weakFeatureComponents
                  << " high_confidence_feature_components=" << report.highConfidenceFeatureComponents
                  << " graph_cleanup_bridged_gaps=" << report.graphCleanupBridgedGaps
                  << " graph_cleanup_removed_spurs=" << report.graphCleanupRemovedSpurs
                  << " graph_cleanup_merged_junctions=" << report.graphCleanupMergedJunctions
                  << " graph_cleanup_skipped_by_cap=" << report.graphCleanupSkippedByCap
                  << " graph_consolidation_bridges=" << report.graphConsolidationBridges
                  << " graph_consolidation_skipped_by_cap=" << report.graphConsolidationSkippedByCap
                  << " circular_recovery_truncated=" << report.circularRecoveryTruncated
                  << " inconsistent_winding_edges=" << report.inconsistentWindingEdges
                  << " mean_feature_component_confidence=" << report.meanFeatureComponentConfidence
                  << " min_feature_component_confidence=" << report.minFeatureComponentConfidence
                  << " feature_protection_mode=" << manumesh::simplification::toString(options.featureProtectionMode)
                  << " normal_tensor_feature_edges=" << report.normalTensorFeatureEdges
                  << " normal_tensor_scored_vertices=" << report.normalTensorScoredVertices
                  << " max_normal_tensor_persistent_score=" << report.maxNormalTensorPersistentScore
                  << " mean_normal_tensor_local_scale=" << report.meanNormalTensorLocalScale
                  << " mean_normal_tensor_persistence=" << report.meanNormalTensorPersistence
                  << " normal_filter_iterations=" << report.featureNormalFilterIterationsCompleted
                  << " normal_filter_changed_faces=" << report.featureNormalFilterChangedFaces
                  << " normal_filter_preserved_edges=" << report.featureNormalFilterPreservedEdges
                  << " mean_normal_filter_angular_change_deg=" << report.meanFeatureNormalFilterAngularChangeDeg
                  << " junction_branch_pairs=" << report.junctionBranchPairs
                  << " ambiguous_feature_junctions=" << report.ambiguousFeatureJunctions
                  << " feature_rejected=" << report.featureRejectedCollapses
                  << " primitive_feature_rejected=" << report.primitiveFeatureRejectedCollapses
                  << " generic_feature_rejected=" << report.genericFeatureRejectedCollapses
                  << " curve_budget_rejected=" << report.curveBudgetRejectedCollapses
                  << " projected_feature_placements=" << report.projectedFeaturePlacements << "\n";
    }
    std::cout << "distance mean original->simplified=" << distance.meanOriginalToSimplified
              << " max=" << distance.maxOriginalToSimplified << "\n";

    if (!metricsCsv.empty()) {
        if (metricsPath.has_parent_path()) {
            fs::create_directories(metricsPath.parent_path());
        }
        AtomicCsvOutput csvFile(metricsPath);
        std::ofstream& csv = csvFile.stream();
        csv << manumesh::cli::statsHeaderCsv()
            << ",collapsed_edges,rejected_collapses,solver_fallbacks,"
               "feature_loops,circular_feature_loops,feature_vertices,"
               "traced_feature_edges,untraced_feature_edges,"
               "feature_components,weak_feature_components,"
               "high_confidence_feature_components,graph_cleanup_bridged_gaps,"
               "graph_cleanup_removed_spurs,graph_cleanup_merged_junctions,"
               "graph_cleanup_skipped_by_cap,circular_recovery_truncated,inconsistent_winding_edges,"
               "graph_consolidation_bridges,graph_consolidation_skipped_by_cap,"
               "mean_feature_component_confidence,min_feature_component_confidence,"
               "normal_tensor_feature_edges,normal_tensor_scored_vertices,"
               "max_normal_tensor_persistent_score,mean_normal_tensor_local_scale,"
               "mean_normal_tensor_persistence,feature_normal_filter_iterations_completed,"
               "feature_normal_filter_changed_faces,feature_normal_filter_preserved_edges,"
               "mean_feature_normal_filter_angular_change_deg,max_feature_normal_filter_angular_change_deg,"
               "mean_feature_normal_filter_edge_indicator,junction_branch_pairs,ambiguous_feature_junctions,"
               "feature_protection_mode,"
               "feature_rejected_collapses,boundary_rejected_collapses,"
               "primitive_feature_rejected_collapses,"
               "generic_feature_rejected_collapses,"
               "topology_rejected_collapses,normal_flip_rejected_collapses,"
               "quality_rejected_collapses,self_intersection_rejected_collapses,"
               "curve_budget_rejected_collapses,error_rejected_collapses,"
               "projected_feature_placements,quality_refinement_iterations_completed,"
               "quality_refinement_attempted_moves,quality_refinement_accepted_moves,"
               "quality_refinement_skipped_for_texture,"
               "termination_reason,"
               "min_line_weight,max_line_weight\n";
        csv << manumesh::cli::statsRowCsv("output", outStats, &distance) << "," << report.collapsedEdges << ","
            << report.rejectedCollapses << "," << report.solverFallbacks << "," << report.featureLoops << ","
            << report.circularFeatureLoops << "," << report.featureVertices << "," << report.tracedFeatureEdges << ","
            << report.untracedFeatureEdges << "," << report.featureComponents << "," << report.weakFeatureComponents
            << "," << report.highConfidenceFeatureComponents << "," << report.graphCleanupBridgedGaps << ","
            << report.graphCleanupRemovedSpurs << "," << report.graphCleanupMergedJunctions << ","
            << report.graphCleanupSkippedByCap << "," << report.circularRecoveryTruncated << ","
            << report.inconsistentWindingEdges << "," << report.graphConsolidationBridges << ","
            << report.graphConsolidationSkippedByCap << "," << report.meanFeatureComponentConfidence << ","
            << report.minFeatureComponentConfidence << "," << report.normalTensorFeatureEdges << ","
            << report.normalTensorScoredVertices << "," << report.maxNormalTensorPersistentScore << ","
            << report.meanNormalTensorLocalScale << "," << report.meanNormalTensorPersistence << ","
            << report.featureNormalFilterIterationsCompleted << "," << report.featureNormalFilterChangedFaces << ","
            << report.featureNormalFilterPreservedEdges << "," << report.meanFeatureNormalFilterAngularChangeDeg << ","
            << report.maxFeatureNormalFilterAngularChangeDeg << "," << report.meanFeatureNormalFilterEdgeIndicator
            << "," << report.junctionBranchPairs << "," << report.ambiguousFeatureJunctions << ","
            << manumesh::simplification::toString(options.featureProtectionMode) << ","
            << report.featureRejectedCollapses << "," << report.boundaryRejectedCollapses << ","
            << report.primitiveFeatureRejectedCollapses << "," << report.genericFeatureRejectedCollapses << ","
            << report.topologyRejectedCollapses << "," << report.normalFlipRejectedCollapses << ","
            << report.qualityRejectedCollapses << "," << report.selfIntersectionRejectedCollapses << ","
            << report.curveBudgetRejectedCollapses << "," << report.errorRejectedCollapses << ","
            << report.projectedFeaturePlacements << "," << report.qualityRefinementIterationsCompleted << ","
            << report.qualityRefinementAttemptedMoves << "," << report.qualityRefinementAcceptedMoves << ","
            << (report.qualityRefinementSkippedForTexture ? 1 : 0) << ","
            << manumesh::simplification::toString(report.terminationReason) << "," << report.minAppliedLineWeight << ","
            << report.maxAppliedLineWeight << "\n";
        csvFile.commit();
    }

    std::cout << "Wrote " << positional[1] << "\n";
    return 0;
}

int commandSweep(const Args& args) {
    const auto positional = positionalArgs(args);
    requireExactPositionalArguments(positional, 2, "sweep", "input.stl out_dir");
    if (hasFlag(args, "--line-weight") || hasFlag(args, "--adaptive-base-line-weight")) {
        throw std::invalid_argument(
            "sweep derives line-quadric weights from --weights; do not pass --line-weight or "
            "--adaptive-base-line-weight."
        );
    }

    const int samples = getIntArg(args, "--samples", 3000);
    const std::string requestedMethod = getArg(args, "--method");
    const bool standardMethod = requestedMethod == "standard" || requestedMethod == "qem";
    const std::string defaultWeights = standardMethod ? "0" : "0,1e-5,1e-4,1e-3,1e-2,1e-1";
    const std::vector<double> weights = parseWeights(getArg(args, "--weights", defaultWeights), "--weights");
    requireUniqueSweepValues(weights, "--weights");
    requireNonNegativeWeights(weights, "--weights");
    if (standardMethod && (weights.size() != 1 || weights.front() != 0.0)) {
        throw std::invalid_argument(
            "standard QEM does not use line-quadric weights; omit --weights or use exactly --weights 0."
        );
    }
    manumesh::simplification::SimplifyOptions base = parseSimplifyOptions(args);
    const manumesh::feature::FeatureOptions effectiveFeatures = base.featureOptionsOverride.has_value()
                                                                    ? *base.featureOptionsOverride
                                                                    : manumesh::feature::FeatureOptions{};
    emitOptionWarnings(args, effectiveFeatures, true, std::cerr);

    const fs::path inputPath = pathFromUtf8(positional[0]);
    const fs::path outDir = pathFromUtf8(positional[1]);
    const fs::path metricsPath = outDir / "metrics.csv";
    requireDistinctOutputPath(metricsPath, inputPath, "sweep metrics.csv", "the input mesh");

    std::vector<fs::path> plannedOutputs;
    plannedOutputs.reserve(weights.size());
    for (std::size_t weightIndex = 0; weightIndex < weights.size(); ++weightIndex) {
        const double weight = weights[weightIndex];
        const bool useLineQuadrics = base.useLineQuadrics && weight > 0.0;
        const std::string method = useLineQuadrics ? "line" : "standard";
        const std::string label = method + "_w_" + sanitizeWeight(weight);
        const fs::path outStl = outDir / (label + ".stl");
        requireDistinctOutputPath(outStl, inputPath, "sweep output", "the input mesh");
        requireDistinctOutputPath(metricsPath, outStl, "sweep metrics.csv", "a sweep output");
        plannedOutputs.push_back(outStl);
    }
    requireUniquePlannedOutputs(plannedOutputs);

    manumesh::Mesh input;
    std::string error;
    if (!manumesh::loadMesh(positional[0], input, &error)) {
        throw std::runtime_error(error);
    }

    fs::create_directories(outDir);
    AtomicCsvOutput csvFile(metricsPath);
    std::ofstream& csv = csvFile.stream();
    csv << "method,line_weight,weight_mode," << manumesh::cli::statsHeaderCsv()
        << ",collapsed_edges,rejected_collapses,solver_fallbacks,"
           "min_line_weight,max_line_weight\n";

    for (std::size_t weightIndex = 0; weightIndex < weights.size(); ++weightIndex) {
        const double weight = weights[weightIndex];
        manumesh::simplification::SimplifyOptions options = base;
        if (options.adaptiveScale) {
            options.adaptiveBaseLineWeight = weight;
        } else {
            options.lineWeight = weight;
        }
        options.useLineQuadrics = base.useLineQuadrics && weight > 0.0;
        if (hasFlag(args, "--print-resolved-config")) {
            std::cout << formatResolvedSimplifyOptions(args, options);
        }

        manumesh::simplification::SimplifyReport report;
        manumesh::simplification::QEMSimplifier simplifier(options);
        simplifier.setExecutionOptions(parseExecutionOptions(args));
        manumesh::Mesh output = simplifier.simplify(input, &report);
        const std::string method = options.useLineQuadrics ? "line" : "standard";
        const std::string label = method + "_w_" + sanitizeWeight(weight);
        const fs::path& outStl = plannedOutputs[weightIndex];
        if (!manumesh::saveBinaryStl(pathToUtf8(outStl), output, &error)) {
            throw std::runtime_error(error);
        }

        const manumesh::analysis::MeshStats stats = manumesh::analysis::computeMeshStats(output);
        const manumesh::analysis::DistanceStats distance =
            manumesh::analysis::compareMeshesBySampledDistance(input, output, samples);
        csv << method << "," << reportedLineWeight(options) << ","
            << manumesh::simplification::toString(options.weightMode) << ","
            << manumesh::cli::statsRowCsv(label, stats, &distance) << "," << report.collapsedEdges << ","
            << report.rejectedCollapses << "," << report.solverFallbacks << "," << report.minAppliedLineWeight << ","
            << report.maxAppliedLineWeight << "\n";
        printStats(label, stats);
    }

    csvFile.commit();
    std::cout << "Wrote sweep outputs to " << pathToUtf8(outDir) << "\n";
    return 0;
}

int commandRatioSweep(const Args& args) {
    const auto positional = positionalArgs(args);
    requireExactPositionalArguments(positional, 2, "ratio-sweep", "input.stl out_dir");
    rejectSweepTargetOverride(args, "ratio-sweep", "--ratios");

    const int samples = getIntArg(args, "--samples", 3000);
    const std::vector<double> ratios =
        parseWeights(getArg(args, "--ratios", "0.8,0.5,0.25,0.1,0.05"), "--ratios");
    requireUniqueSweepValues(ratios, "--ratios");
    for (double ratio : ratios) {
        if (ratio <= 0.0 || ratio >= 1.0) {
            throw std::invalid_argument("--ratios values must be strictly between 0 and 1; got " +
                                        std::to_string(ratio) + ".");
        }
    }
    manumesh::simplification::SimplifyOptions base = parseSimplifyOptions(args);
    const manumesh::feature::FeatureOptions effectiveFeatures = base.featureOptionsOverride.has_value()
                                                                    ? *base.featureOptionsOverride
                                                                    : manumesh::feature::FeatureOptions{};
    emitOptionWarnings(args, effectiveFeatures, true, std::cerr);

    const fs::path inputPath = pathFromUtf8(positional[0]);
    const fs::path outDir = pathFromUtf8(positional[1]);
    const fs::path metricsPath = outDir / "metrics.csv";
    requireDistinctOutputPath(metricsPath, inputPath, "ratio-sweep metrics.csv", "the input mesh");
    std::vector<fs::path> plannedOutputs;
    plannedOutputs.reserve(ratios.size());
    const std::string plannedMethod = base.useLineQuadrics ? "line" : "standard";
    const std::string weightToken = sanitizeWeight(configuredLineWeight(base));
    for (double ratio : ratios) {
        const std::string label = plannedMethod + "_r_" + sanitizeRatio(ratio) + "_w_" + weightToken;
        const fs::path outStl = outDir / (label + ".stl");
        requireDistinctOutputPath(outStl, inputPath, "ratio-sweep output", "the input mesh");
        requireDistinctOutputPath(metricsPath, outStl, "ratio-sweep metrics.csv", "a sweep output");
        plannedOutputs.push_back(outStl);
    }
    requireUniquePlannedOutputs(plannedOutputs);

    manumesh::Mesh input;
    std::string error;
    if (!manumesh::loadMesh(positional[0], input, &error)) {
        throw std::runtime_error(error);
    }

    fs::create_directories(outDir);
    AtomicCsvOutput csvFile(metricsPath);
    std::ofstream& csv = csvFile.stream();
    csv << "method,line_weight,weight_mode,ratio," << manumesh::cli::statsHeaderCsv()
        << ",collapsed_edges,rejected_collapses,solver_fallbacks,"
           "min_line_weight,max_line_weight\n";

    for (std::size_t ratioIndex = 0; ratioIndex < ratios.size(); ++ratioIndex) {
        const double ratio = ratios[ratioIndex];
        manumesh::simplification::SimplifyOptions options = base;
        options.targetFaces = -1;
        options.targetRatio = ratio;
        if (hasFlag(args, "--print-resolved-config")) {
            std::cout << formatResolvedSimplifyOptions(args, options);
        }

        manumesh::simplification::SimplifyReport report;
        manumesh::simplification::QEMSimplifier simplifier(options);
        simplifier.setExecutionOptions(parseExecutionOptions(args));
        manumesh::Mesh output = simplifier.simplify(input, &report);
        const std::string method = options.useLineQuadrics ? "line" : "standard";
        const std::string label = method + "_r_" + sanitizeRatio(ratio) + "_w_" +
                                  sanitizeWeight(configuredLineWeight(options));
        const fs::path& outStl = plannedOutputs[ratioIndex];
        if (!manumesh::saveBinaryStl(pathToUtf8(outStl), output, &error)) {
            throw std::runtime_error(error);
        }

        const manumesh::analysis::MeshStats stats = manumesh::analysis::computeMeshStats(output);
        const manumesh::analysis::DistanceStats distance =
            manumesh::analysis::compareMeshesBySampledDistance(input, output, samples);
        csv << method << "," << reportedLineWeight(options) << ","
            << manumesh::simplification::toString(options.weightMode)
            << "," << ratio << "," << manumesh::cli::statsRowCsv(label, stats, &distance) << ","
            << report.collapsedEdges << "," << report.rejectedCollapses << "," << report.solverFallbacks << ","
            << report.minAppliedLineWeight << "," << report.maxAppliedLineWeight << "\n";
        printStats(label, stats);
    }

    csvFile.commit();
    std::cout << "Wrote ratio-sweep outputs to " << pathToUtf8(outDir) << "\n";
    return 0;
}

int commandFaceSweep(const Args& args) {
    const auto positional = positionalArgs(args);
    requireExactPositionalArguments(positional, 2, "face-sweep", "input.stl out_dir");
    rejectSweepTargetOverride(args, "face-sweep", "--faces");

    const int samples = getIntArg(args, "--samples", 3000);
    const std::vector<int> faceCounts =
        parseFaceCounts(getArg(args, "--faces", "1000,900,800,700,600,500,400,300,200,100"), "--faces");
    requireUniqueSweepValues(faceCounts, "--faces");
    manumesh::simplification::SimplifyOptions base = parseSimplifyOptions(args);
    const manumesh::feature::FeatureOptions effectiveFeatures = base.featureOptionsOverride.has_value()
                                                                    ? *base.featureOptionsOverride
                                                                    : manumesh::feature::FeatureOptions{};
    emitOptionWarnings(args, effectiveFeatures, true, std::cerr);

    const fs::path inputPath = pathFromUtf8(positional[0]);
    const fs::path outDir = pathFromUtf8(positional[1]);
    const fs::path metricsPath = outDir / "metrics.csv";
    requireDistinctOutputPath(metricsPath, inputPath, "face-sweep metrics.csv", "the input mesh");
    std::vector<fs::path> plannedOutputs;
    plannedOutputs.reserve(faceCounts.size());
    const std::string plannedMethod = base.useLineQuadrics ? "line" : "standard";
    const std::string weightToken = sanitizeWeight(configuredLineWeight(base));
    for (int targetFaces : faceCounts) {
        const std::string label = plannedMethod + "_f_" + std::to_string(targetFaces) + "_w_" + weightToken;
        const fs::path outStl = outDir / (label + ".stl");
        requireDistinctOutputPath(outStl, inputPath, "face-sweep output", "the input mesh");
        requireDistinctOutputPath(metricsPath, outStl, "face-sweep metrics.csv", "a sweep output");
        plannedOutputs.push_back(outStl);
    }
    requireUniquePlannedOutputs(plannedOutputs);

    manumesh::Mesh input;
    std::string error;
    if (!manumesh::loadMesh(positional[0], input, &error)) {
        throw std::runtime_error(error);
    }

    fs::create_directories(outDir);
    AtomicCsvOutput csvFile(metricsPath);
    std::ofstream& csv = csvFile.stream();
    csv << "method,line_weight,weight_mode,target_faces," << manumesh::cli::statsHeaderCsv()
        << ",collapsed_edges,rejected_collapses,solver_fallbacks,"
           "min_line_weight,max_line_weight\n";

    for (std::size_t faceIndex = 0; faceIndex < faceCounts.size(); ++faceIndex) {
        const int targetFaces = faceCounts[faceIndex];
        manumesh::simplification::SimplifyOptions options = base;
        options.targetFaces = targetFaces;
        if (hasFlag(args, "--print-resolved-config")) {
            std::cout << formatResolvedSimplifyOptions(args, options);
        }

        manumesh::simplification::SimplifyReport report;
        manumesh::simplification::QEMSimplifier simplifier(options);
        simplifier.setExecutionOptions(parseExecutionOptions(args));
        manumesh::Mesh output = simplifier.simplify(input, &report);
        const std::string method = options.useLineQuadrics ? "line" : "standard";
        const std::string label = method + "_f_" + std::to_string(targetFaces) + "_w_" +
                                  sanitizeWeight(configuredLineWeight(options));
        const fs::path& outStl = plannedOutputs[faceIndex];
        if (!manumesh::saveBinaryStl(pathToUtf8(outStl), output, &error)) {
            throw std::runtime_error(error);
        }

        const manumesh::analysis::MeshStats stats = manumesh::analysis::computeMeshStats(output);
        const manumesh::analysis::DistanceStats distance =
            manumesh::analysis::compareMeshesBySampledDistance(input, output, samples);
        csv << method << "," << reportedLineWeight(options) << ","
            << manumesh::simplification::toString(options.weightMode)
            << "," << targetFaces << "," << manumesh::cli::statsRowCsv(label, stats, &distance) << ","
            << report.collapsedEdges << "," << report.rejectedCollapses << "," << report.solverFallbacks << ","
            << report.minAppliedLineWeight << "," << report.maxAppliedLineWeight << "\n";
        printStats(label, stats);
    }

    csvFile.commit();
    std::cout << "Wrote face-sweep outputs to " << pathToUtf8(outDir) << "\n";
    return 0;
}

int commandSummarizeMetrics(const Args& args) {
    const auto positional = positionalArgs(args);
    if (positional.size() > 2) {
        throw std::invalid_argument("summarize-metrics accepts at most output_root and summary.csv.");
    }
    const fs::path outputRoot = positional.empty() ? fs::path("output/demo") : pathFromUtf8(positional[0]);
    const fs::path summaryPath = positional.size() < 2 ? outputRoot / "demo_summary.csv" : pathFromUtf8(positional[1]);
    summarizeMetrics(outputRoot, summaryPath);
    return 0;
}

const std::map<std::string, manumesh::cli::CommandHandler>& registeredCommands() {
    static const std::map<std::string, manumesh::cli::CommandHandler> commands = {
        {"compare", commandCompare},
        {"demo", manumesh::cli::workflow_commands::demo},
        {"face-sweep", commandFaceSweep},
        {"feature-benchmark", manumesh::cli::feature_commands::benchmark},
        {"feature-compare", manumesh::cli::feature_commands::compare},
        {"feature-report", manumesh::cli::feature_commands::report},
        {"generate", commandGenerate},
        {"large-import", manumesh::cli::large_mesh_commands::importDataset},
        {"large-validate", manumesh::cli::large_mesh_commands::validateDataset},
        {"ratio-sweep", commandRatioSweep},
        {"simplify", commandSimplify},
        {"summarize-metrics", commandSummarizeMetrics},
        {"sweep", commandSweep},
        {"validate-external", manumesh::cli::workflow_commands::validateExternal},
        {"validate-features", manumesh::cli::workflow_commands::validateFeatures},
    };
    return commands;
}

} // namespace

namespace manumesh {
namespace cli {

const std::map<std::string, CommandHandler>& commandRegistry() { return registeredCommands(); }

} // namespace cli
} // namespace manumesh
