/**
 * @file apps/ManuMeshCommands.cpp
 * @brief Implements manu mesh commands facilities for the ManuMesh command-line application.
 * @ingroup manumesh_cli
 *
 * @details CLI parsing, validation, dispatch, and reporting are kept outside the geometry library so SDK behavior is independent of process-global command-line state.
 */

#include "CliArguments.h"
#include "CliCommands.h"
#include "CliCsv.h"
#include "CliOptionBinding.h"
#include "CliPath.h"
#include "ManuMeshFeatureCommands.h"
#include "ManuMeshWorkflowCommands.h"
#include "algorithms/analysis/MeshAnalysis.h"
#include "algorithms/simplification/QEMSimplifier.h"
#include "core/Mesh.h"
#include "core/MeshGenerators.h"
#include "io/MeshIo.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

namespace fs = std::filesystem;

namespace {

using manumesh::cli::Args;
using manumesh::cli::csvValue;
using manumesh::cli::getArg;
using manumesh::cli::getIntArg;
using manumesh::cli::hasFlag;
using manumesh::cli::parseFaceCounts;
using manumesh::cli::parseSimplifyOptions;
using manumesh::cli::parseWeights;
using manumesh::cli::pathFromUtf8;
using manumesh::cli::pathToUtf8;
using manumesh::cli::positionalArgs;
using manumesh::cli::quoteCsv;
using manumesh::cli::readFirstCsvRow;
using manumesh::cli::splitCsvLine;

int commandGenerate(const Args& args);
int commandSimplify(const Args& args);
int commandSweep(const Args& args);
int commandRatioSweep(const Args& args);
int commandFaceSweep(const Args& args);
int commandCompare(const Args& args);
int commandSummarizeMetrics(const Args& args);

std::string sanitizeWeight(double value) {
    std::ostringstream out;
    out << std::scientific << std::setprecision(0) << value;
    std::string text = out.str();
    for (char& ch : text) {
        if (ch == '+' || ch == '-' || ch == '.')
            ch = '_';
    }
    return text;
}

std::string sanitizeRatio(double value) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(2) << value;
    std::string text = out.str();
    while (text.size() > 1 && text.back() == '0') {
        text.pop_back();
    }
    if (!text.empty() && text.back() == '.') {
        text.pop_back();
    }
    for (char& ch : text) {
        if (ch == '.')
            ch = '_';
    }
    return text;
}

void printStats(const std::string& label, const manumesh::analysis::MeshStats& stats) {
    std::cout << label << ": vertices=" << stats.vertices << " faces=" << stats.faces
              << " mean_quality=" << stats.meanTriangleQuality << " min_quality=" << stats.minTriangleQuality
              << " edge_cv=" << stats.edgeLengthCv << "\n";
}

void summarizeMetrics(const fs::path& outputRoot, const fs::path& summaryPath) {
    std::vector<std::string> columns = {"case"};
    std::vector<std::map<std::string, std::string>> rows;

    if (!fs::exists(outputRoot)) {
        throw std::runtime_error("Output directory not found: " + pathToUtf8(outputRoot));
    }

    for (const fs::directory_entry& entry : fs::recursive_directory_iterator(outputRoot)) {
        if (!entry.is_regular_file() || entry.path().filename() != "metrics.csv") {
            continue;
        }

        std::ifstream in(entry.path());
        std::string headerLine;
        if (!std::getline(in, headerLine)) {
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
        while (std::getline(in, line)) {
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

    if (summaryPath.has_parent_path()) {
        fs::create_directories(summaryPath.parent_path());
    }
    std::ofstream out(summaryPath);
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
    std::cout << "Wrote " << pathToUtf8(summaryPath) << " with " << rows.size() << " rows\n";
}

int commandGenerate(const Args& args) {
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
    if (positional.size() < 2) {
        throw std::invalid_argument("compare requires original.stl simplified.stl.");
    }
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
    if (positional.size() < 2) {
        throw std::invalid_argument("simplify requires input.stl output.stl.");
    }

    const int samples = getIntArg(args, "--samples", 3000);
    const std::string metricsCsv = getArg(args, "--metrics-csv");
    manumesh::simplification::SimplifyOptions options = parseSimplifyOptions(args);

    manumesh::Mesh input;
    std::string error;
    if (!manumesh::loadMesh(positional[0], input, &error)) {
        throw std::runtime_error(error);
    }

    manumesh::simplification::SimplifyReport report;
    manumesh::simplification::QEMSimplifier simplifier(options);
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
                  << " smooth_curvature_feature_edges=" << report.smoothCurvatureFeatureEdges
                  << " smooth_curvature_scored_vertices=" << report.smoothCurvatureScoredVertices
                  << " max_smooth_curvature_persistent_score=" << report.maxSmoothCurvaturePersistentScore
                  << " mean_smooth_curvature_local_scale=" << report.meanSmoothCurvatureLocalScale
                  << " mean_smooth_curvature_persistence=" << report.meanSmoothCurvaturePersistence
                  << " mean_smooth_curvature_scale_stability=" << report.meanSmoothCurvatureScaleStability
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
        const fs::path metricsPath = pathFromUtf8(metricsCsv);
        if (metricsPath.has_parent_path()) {
            fs::create_directories(metricsPath.parent_path());
        }
        std::ofstream csv(metricsPath);
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
               "mean_normal_tensor_persistence,smooth_curvature_feature_edges,"
               "smooth_curvature_scored_vertices,max_smooth_curvature_persistent_score,"
               "mean_smooth_curvature_local_scale,mean_smooth_curvature_persistence,"
               "mean_smooth_curvature_scale_stability,feature_normal_filter_iterations_completed,"
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
            << report.smoothCurvatureFeatureEdges << "," << report.smoothCurvatureScoredVertices << ","
            << report.maxSmoothCurvaturePersistentScore << "," << report.meanSmoothCurvatureLocalScale << ","
            << report.meanSmoothCurvaturePersistence << "," << report.meanSmoothCurvatureScaleStability << ","
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
            << manumesh::simplification::toString(report.terminationReason) << "," << report.minAppliedLineWeight << ","
            << report.maxAppliedLineWeight << "\n";
    }

    std::cout << "Wrote " << positional[1] << "\n";
    return 0;
}

int commandSweep(const Args& args) {
    const auto positional = positionalArgs(args);
    if (positional.size() < 2) {
        throw std::invalid_argument("sweep requires input.stl out_dir.");
    }

    manumesh::Mesh input;
    std::string error;
    if (!manumesh::loadMesh(positional[0], input, &error)) {
        throw std::runtime_error(error);
    }

    const fs::path outDir = pathFromUtf8(positional[1]);
    fs::create_directories(outDir);
    const int samples = getIntArg(args, "--samples", 3000);
    const std::vector<double> weights = parseWeights(getArg(args, "--weights", "0,1e-5,1e-4,1e-3,1e-2,1e-1"));
    manumesh::simplification::SimplifyOptions base = parseSimplifyOptions(args);

    std::ofstream csv(outDir / "metrics.csv");
    csv << "method,line_weight,weight_mode," << manumesh::cli::statsHeaderCsv()
        << ",collapsed_edges,rejected_collapses,solver_fallbacks,"
           "min_line_weight,max_line_weight\n";

    for (double weight : weights) {
        manumesh::simplification::SimplifyOptions options = base;
        options.lineWeight = weight;
        options.useLineQuadrics = weight > 0.0 || options.weightMode != manumesh::simplification::WeightMode::Uniform;

        manumesh::simplification::SimplifyReport report;
        manumesh::simplification::QEMSimplifier simplifier(options);
        manumesh::Mesh output = simplifier.simplify(input, &report);
        const std::string method = options.useLineQuadrics ? "line" : "standard";
        const std::string label = method + "_w_" + sanitizeWeight(weight);
        const fs::path outStl = outDir / (label + ".stl");
        if (!manumesh::saveBinaryStl(pathToUtf8(outStl), output, &error)) {
            throw std::runtime_error(error);
        }

        const manumesh::analysis::MeshStats stats = manumesh::analysis::computeMeshStats(output);
        const manumesh::analysis::DistanceStats distance =
            manumesh::analysis::compareMeshesBySampledDistance(input, output, samples);
        csv << method << "," << weight << "," << manumesh::simplification::toString(options.weightMode) << ","
            << manumesh::cli::statsRowCsv(label, stats, &distance) << "," << report.collapsedEdges << ","
            << report.rejectedCollapses << "," << report.solverFallbacks << "," << report.minAppliedLineWeight << ","
            << report.maxAppliedLineWeight << "\n";
        printStats(label, stats);
    }

    std::cout << "Wrote sweep outputs to " << pathToUtf8(outDir) << "\n";
    return 0;
}

int commandRatioSweep(const Args& args) {
    const auto positional = positionalArgs(args);
    if (positional.size() < 2) {
        throw std::invalid_argument("ratio-sweep requires input.stl out_dir.");
    }

    manumesh::Mesh input;
    std::string error;
    if (!manumesh::loadMesh(positional[0], input, &error)) {
        throw std::runtime_error(error);
    }

    const fs::path outDir = pathFromUtf8(positional[1]);
    fs::create_directories(outDir);
    const int samples = getIntArg(args, "--samples", 3000);
    const std::vector<double> ratios = parseWeights(getArg(args, "--ratios", "0.8,0.5,0.25,0.1,0.05"));
    manumesh::simplification::SimplifyOptions base = parseSimplifyOptions(args);

    std::ofstream csv(outDir / "metrics.csv");
    csv << "method,line_weight,weight_mode,ratio," << manumesh::cli::statsHeaderCsv()
        << ",collapsed_edges,rejected_collapses,solver_fallbacks,"
           "min_line_weight,max_line_weight\n";

    for (double ratio : ratios) {
        if (ratio <= 0.0 || ratio >= 1.0) {
            std::cerr << "skip invalid ratio " << ratio << "\n";
            continue;
        }
        manumesh::simplification::SimplifyOptions options = base;
        options.targetFaces = -1;
        options.targetRatio = ratio;

        manumesh::simplification::SimplifyReport report;
        manumesh::simplification::QEMSimplifier simplifier(options);
        manumesh::Mesh output = simplifier.simplify(input, &report);
        const std::string method = options.useLineQuadrics ? "line" : "standard";
        const std::string label = method + "_r_" + sanitizeRatio(ratio) + "_w_" + sanitizeWeight(options.lineWeight);
        const fs::path outStl = outDir / (label + ".stl");
        if (!manumesh::saveBinaryStl(pathToUtf8(outStl), output, &error)) {
            throw std::runtime_error(error);
        }

        const manumesh::analysis::MeshStats stats = manumesh::analysis::computeMeshStats(output);
        const manumesh::analysis::DistanceStats distance =
            manumesh::analysis::compareMeshesBySampledDistance(input, output, samples);
        csv << method << "," << options.lineWeight << "," << manumesh::simplification::toString(options.weightMode)
            << "," << ratio << "," << manumesh::cli::statsRowCsv(label, stats, &distance) << ","
            << report.collapsedEdges << "," << report.rejectedCollapses << "," << report.solverFallbacks << ","
            << report.minAppliedLineWeight << "," << report.maxAppliedLineWeight << "\n";
        printStats(label, stats);
    }

    std::cout << "Wrote ratio-sweep outputs to " << pathToUtf8(outDir) << "\n";
    return 0;
}

int commandFaceSweep(const Args& args) {
    const auto positional = positionalArgs(args);
    if (positional.size() < 2) {
        throw std::invalid_argument("face-sweep requires input.stl out_dir.");
    }

    manumesh::Mesh input;
    std::string error;
    if (!manumesh::loadMesh(positional[0], input, &error)) {
        throw std::runtime_error(error);
    }

    const fs::path outDir = pathFromUtf8(positional[1]);
    fs::create_directories(outDir);
    const int samples = getIntArg(args, "--samples", 3000);
    const std::vector<int> faceCounts =
        parseFaceCounts(getArg(args, "--faces", "1000,900,800,700,600,500,400,300,200,100"));
    manumesh::simplification::SimplifyOptions base = parseSimplifyOptions(args);

    std::ofstream csv(outDir / "metrics.csv");
    csv << "method,line_weight,weight_mode,target_faces," << manumesh::cli::statsHeaderCsv()
        << ",collapsed_edges,rejected_collapses,solver_fallbacks,"
           "min_line_weight,max_line_weight\n";

    for (int targetFaces : faceCounts) {
        if (targetFaces <= 0) {
            std::cerr << "skip invalid target face count " << targetFaces << "\n";
            continue;
        }
        manumesh::simplification::SimplifyOptions options = base;
        options.targetFaces = targetFaces;

        manumesh::simplification::SimplifyReport report;
        manumesh::simplification::QEMSimplifier simplifier(options);
        manumesh::Mesh output = simplifier.simplify(input, &report);
        const std::string method = options.useLineQuadrics ? "line" : "standard";
        const std::string label =
            method + "_f_" + std::to_string(targetFaces) + "_w_" + sanitizeWeight(options.lineWeight);
        const fs::path outStl = outDir / (label + ".stl");
        if (!manumesh::saveBinaryStl(pathToUtf8(outStl), output, &error)) {
            throw std::runtime_error(error);
        }

        const manumesh::analysis::MeshStats stats = manumesh::analysis::computeMeshStats(output);
        const manumesh::analysis::DistanceStats distance =
            manumesh::analysis::compareMeshesBySampledDistance(input, output, samples);
        csv << method << "," << options.lineWeight << "," << manumesh::simplification::toString(options.weightMode)
            << "," << targetFaces << "," << manumesh::cli::statsRowCsv(label, stats, &distance) << ","
            << report.collapsedEdges << "," << report.rejectedCollapses << "," << report.solverFallbacks << ","
            << report.minAppliedLineWeight << "," << report.maxAppliedLineWeight << "\n";
        printStats(label, stats);
    }

    std::cout << "Wrote face-sweep outputs to " << pathToUtf8(outDir) << "\n";
    return 0;
}

int commandSummarizeMetrics(const Args& args) {
    const auto positional = positionalArgs(args);
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

namespace manumesh::cli {

const std::map<std::string, CommandHandler>& commandRegistry() { return registeredCommands(); }

} // namespace manumesh::cli
