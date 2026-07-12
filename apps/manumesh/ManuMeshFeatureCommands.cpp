#include "ManuMeshFeatureCommands.h"

#include "CliCsv.h"
#include "CliOptionBinding.h"
#include "algorithms/feature_detection/FeatureDetector.h"
#include "core/Mesh.h"
#include "io/MeshIo.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

namespace manumesh::cli::feature_commands {

constexpr double kPi = 3.141592653589793238462643383279502884;

static int countCircularLoops(const manumesh::feature::FeatureAnalysis& analysis) {
    int count = 0;
    for (const manumesh::feature::FeatureLoop& loop : analysis.loops) {
        if (loop.circular) {
            ++count;
        }
    }
    return count;
}

static int countPrimitiveLoops(
    const manumesh::feature::FeatureAnalysis& analysis, manumesh::feature::FeaturePrimitiveType primitive
) {
    int count = 0;
    for (const manumesh::feature::FeatureLoop& loop : analysis.loops) {
        if (loop.primitive == primitive) {
            ++count;
        }
    }
    return count;
}

int report(const Args& args) {
    const auto positional = positionalArgs(args);
    if (positional.empty()) {
        throw std::invalid_argument("feature-report requires input.stl.");
    }

    manumesh::Mesh input;
    std::string error;
    if (!manumesh::loadMesh(positional[0], input, &error)) {
        throw std::runtime_error(error);
    }

    const manumesh::feature::FeatureOptions options = parseFeatureOptions(args);
    const manumesh::feature::FeatureAnalysis analysis = manumesh::feature::detectFeatureCurves(input, options);
    const int circularLoops = countCircularLoops(analysis);
    const int circleLoops = countPrimitiveLoops(analysis, manumesh::feature::FeaturePrimitiveType::Circle);
    const int nearCircleLoops = countPrimitiveLoops(analysis, manumesh::feature::FeaturePrimitiveType::NearCircle);
    const int ellipseLoops = countPrimitiveLoops(analysis, manumesh::feature::FeaturePrimitiveType::Ellipse);
    const int polygonalLoops = countPrimitiveLoops(analysis, manumesh::feature::FeaturePrimitiveType::PolygonalLoop);
    std::cout << "feature_edges=" << analysis.featureEdges << " traced_edges=" << analysis.tracedFeatureEdges
              << " untraced_edges=" << analysis.untracedFeatureEdges
              << " boundary_edges=" << analysis.boundaryFeatureEdges
              << " dihedral_edges=" << analysis.dihedralFeatureEdges
              << " normal_tensor_edges=" << analysis.normalTensorFeatureEdges
              << " smooth_curvature_edges=" << analysis.smoothCurvatureFeatureEdges
              << " non_manifold_edges=" << analysis.nonManifoldFeatureEdges
              << " feature_components=" << analysis.components.size()
              << " weak_feature_components=" << analysis.weakFeatureComponents
              << " high_confidence_feature_components=" << analysis.highConfidenceFeatureComponents
              << " graph_cleanup_bridged_gaps=" << analysis.graphCleanupBridgedGaps
              << " graph_cleanup_removed_spurs=" << analysis.graphCleanupRemovedSpurs
              << " graph_cleanup_merged_junctions=" << analysis.graphCleanupMergedJunctions
              << " normal_tensor_scored_vertices=" << analysis.normalTensorScoredVertices
              << " smooth_curvature_scored_vertices=" << analysis.smoothCurvatureScoredVertices
              << " convex_edges=" << analysis.convexFeatureEdges << " concave_edges=" << analysis.concaveFeatureEdges
              << " unknown_signed_edges=" << analysis.unknownSignedFeatureEdges
              << " max_normal_tensor_score=" << analysis.maxNormalTensorFeatureScore
              << " max_normal_tensor_persistent_score=" << analysis.maxNormalTensorPersistentScore
              << " mean_normal_tensor_local_scale=" << analysis.meanNormalTensorLocalScale
              << " mean_normal_tensor_persistence=" << analysis.meanNormalTensorPersistence
              << " max_smooth_curvature_score=" << analysis.maxSmoothCurvatureFeatureScore
              << " max_smooth_curvature_persistent_score=" << analysis.maxSmoothCurvaturePersistentScore
              << " mean_smooth_curvature_local_scale=" << analysis.meanSmoothCurvatureLocalScale
              << " mean_smooth_curvature_persistence=" << analysis.meanSmoothCurvaturePersistence
              << " mean_feature_component_confidence=" << analysis.meanFeatureComponentConfidence
              << " min_feature_component_confidence=" << analysis.minFeatureComponentConfidence
              << " loops=" << analysis.loops.size() << " circular_loops=" << circularLoops
              << " circle_loops=" << circleLoops << " near_circle_loops=" << nearCircleLoops
              << " ellipse_loops=" << ellipseLoops << " polygonal_loops=" << polygonalLoops << "\n";
    std::cout << manumesh::feature::featureReportHeaderCsv() << "\n";
    for (const manumesh::feature::FeatureLoop& loop : analysis.loops) {
        std::cout << manumesh::feature::featureLoopRowCsv(loop) << "\n";
    }

    const std::string csvPath = getArg(args, "--csv");
    if (!csvPath.empty()) {
        const fs::path output(csvPath);
        if (output.has_parent_path()) {
            fs::create_directories(output.parent_path());
        }
        std::ofstream csv(csvPath);
        csv << "feature_edges,traced_edges,untraced_edges,boundary_edges,"
               "dihedral_edges,normal_tensor_edges,smooth_curvature_edges,non_manifold_edges,"
               "feature_components,weak_feature_components,"
               "high_confidence_feature_components,graph_cleanup_bridged_gaps,"
               "graph_cleanup_removed_spurs,graph_cleanup_merged_junctions,"
               "normal_tensor_scored_vertices,smooth_curvature_scored_vertices,convex_edges,"
               "concave_edges,unknown_signed_edges,"
               "max_normal_tensor_score,max_normal_tensor_persistent_score,"
               "mean_normal_tensor_local_scale,mean_normal_tensor_persistence,"
               "max_smooth_curvature_score,max_smooth_curvature_persistent_score,"
               "mean_smooth_curvature_local_scale,mean_smooth_curvature_persistence,"
               "mean_feature_component_confidence,min_feature_component_confidence,"
               "loops,circular_loops,circle_loops,"
               "near_circle_loops,ellipse_loops,polygonal_loops\n";
        csv << analysis.featureEdges << "," << analysis.tracedFeatureEdges << "," << analysis.untracedFeatureEdges
            << "," << analysis.boundaryFeatureEdges << "," << analysis.dihedralFeatureEdges << ","
            << analysis.normalTensorFeatureEdges << "," << analysis.smoothCurvatureFeatureEdges << ","
            << analysis.nonManifoldFeatureEdges << "," << analysis.components.size() << ","
            << analysis.weakFeatureComponents << "," << analysis.highConfidenceFeatureComponents << ","
            << analysis.graphCleanupBridgedGaps << "," << analysis.graphCleanupRemovedSpurs << ","
            << analysis.graphCleanupMergedJunctions << "," << analysis.normalTensorScoredVertices << ","
            << analysis.smoothCurvatureScoredVertices << "," << analysis.convexFeatureEdges << ","
            << analysis.concaveFeatureEdges << "," << analysis.unknownSignedFeatureEdges << ","
            << analysis.maxNormalTensorFeatureScore << "," << analysis.maxNormalTensorPersistentScore << ","
            << analysis.meanNormalTensorLocalScale << "," << analysis.meanNormalTensorPersistence << ","
            << analysis.maxSmoothCurvatureFeatureScore << "," << analysis.maxSmoothCurvaturePersistentScore << ","
            << analysis.meanSmoothCurvatureLocalScale << "," << analysis.meanSmoothCurvaturePersistence << ","
            << analysis.meanFeatureComponentConfidence << "," << analysis.minFeatureComponentConfidence << ","
            << analysis.loops.size() << "," << circularLoops << "," << circleLoops << "," << nearCircleLoops << ","
            << ellipseLoops << "," << polygonalLoops << "\n\n";
        csv << manumesh::feature::featureReportHeaderCsv() << "\n";
        for (const manumesh::feature::FeatureLoop& loop : analysis.loops) {
            csv << manumesh::feature::featureLoopRowCsv(loop) << "\n";
        }
    }
    return 0;
}

static bool tryParseIntField(const std::string& text, int& value) {
    try {
        std::size_t parsed = 0;
        const int candidate = std::stoi(text, &parsed);
        if (parsed != text.size()) {
            return false;
        }
        value = candidate;
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

static void
readFeatureBenchmarkLabels(const fs::path& path, std::vector<std::pair<int, int>>& edges, std::vector<int>& junctions) {
    std::ifstream in(path);
    if (!in) {
        throw std::runtime_error("Cannot open feature label CSV: " + path.string());
    }

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) {
            continue;
        }
        const std::vector<std::string> fields = splitCsvLine(line);
        if (fields.empty()) {
            continue;
        }
        if ((fields[0] == "junction" || fields[0] == "junction_vertex") && fields.size() >= 2) {
            int id = -1;
            if (tryParseIntField(fields[1], id)) {
                junctions.push_back(id);
            }
            continue;
        }
        int a = -1;
        int b = -1;
        if (fields.size() >= 2 && tryParseIntField(fields[0], a) && tryParseIntField(fields[1], b)) {
            edges.emplace_back(a, b);
        }
    }
}

int benchmark(const Args& args) {
    const auto positional = positionalArgs(args);
    if (positional.size() < 2) {
        throw std::invalid_argument("feature-benchmark requires input.stl labels.csv.");
    }

    manumesh::Mesh input;
    std::string error;
    if (!manumesh::loadMesh(positional[0], input, &error)) {
        throw std::runtime_error(error);
    }

    std::vector<std::pair<int, int>> groundTruthEdges;
    std::vector<int> groundTruthJunctions;
    readFeatureBenchmarkLabels(positional[1], groundTruthEdges, groundTruthJunctions);

    const manumesh::feature::FeatureOptions options = parseFeatureOptions(args);
    const manumesh::feature::FeatureAnalysis analysis = manumesh::feature::detectFeatureCurves(input, options);
    const manumesh::feature::FeatureEdgeBenchmark benchmark =
        manumesh::feature::benchmarkFeatureEdges(analysis, groundTruthEdges, groundTruthJunctions);

    const std::string header = "ground_truth_edges,detected_edges,true_positive_edges,false_positive_edges,"
                               "false_negative_edges,edge_precision,edge_recall,edge_f1,"
                               "ground_truth_junctions,detected_junctions,true_positive_junctions,"
                               "false_positive_junctions,false_negative_junctions,junction_precision,"
                               "junction_recall,junction_f1,loop_closure_rate,mean_component_confidence";
    std::ostringstream row;
    row << std::setprecision(12) << benchmark.groundTruthEdges << "," << benchmark.detectedEdges << ","
        << benchmark.truePositiveEdges << "," << benchmark.falsePositiveEdges << "," << benchmark.falseNegativeEdges
        << "," << benchmark.edgePrecision << "," << benchmark.edgeRecall << "," << benchmark.edgeF1 << ","
        << benchmark.groundTruthJunctions << "," << benchmark.detectedJunctions << ","
        << benchmark.truePositiveJunctions << "," << benchmark.falsePositiveJunctions << ","
        << benchmark.falseNegativeJunctions << "," << benchmark.junctionPrecision << "," << benchmark.junctionRecall
        << "," << benchmark.junctionF1 << "," << benchmark.loopClosureRate << "," << benchmark.meanComponentConfidence;

    std::cout << header << "\n" << row.str() << "\n";
    const std::string csvPath = getArg(args, "--csv");
    if (!csvPath.empty()) {
        const fs::path output(csvPath);
        if (output.has_parent_path()) {
            fs::create_directories(output.parent_path());
        }
        std::ofstream csv(csvPath);
        csv << header << "\n" << row.str() << "\n";
    }
    return 0;
}

int compare(const Args& args) {
    const auto positional = positionalArgs(args);
    if (positional.size() < 2) {
        throw std::invalid_argument("feature-compare requires original.stl simplified.stl.");
    }

    manumesh::Mesh original;
    manumesh::Mesh simplified;
    std::string error;
    if (!manumesh::loadMesh(positional[0], original, &error)) {
        throw std::runtime_error(error);
    }
    if (!manumesh::loadMesh(positional[1], simplified, &error)) {
        throw std::runtime_error(error);
    }

    const manumesh::feature::FeatureOptions options = parseFeatureOptions(args);
    const manumesh::feature::FeatureAnalysis originalFeatures =
        manumesh::feature::detectFeatureCurves(original, options);
    const manumesh::feature::FeatureAnalysis simplifiedFeatures =
        manumesh::feature::detectFeatureCurves(simplified, options);

    std::vector<int> originalCircular;
    std::vector<int> simplifiedCircular;
    for (int i = 0; i < static_cast<int>(originalFeatures.loops.size()); ++i) {
        if (originalFeatures.loops[i].circular) {
            originalCircular.push_back(i);
        }
    }
    for (int i = 0; i < static_cast<int>(simplifiedFeatures.loops.size()); ++i) {
        if (simplifiedFeatures.loops[i].circular) {
            simplifiedCircular.push_back(i);
        }
    }

    const double diag = std::max(1e-12, original.bboxDiag());
    std::vector<char> usedSimplified(simplifiedFeatures.loops.size(), 0);
    std::ostringstream rows;
    rows << std::setprecision(12);
    int matched = 0;
    int missing = 0;
    for (int originalId : originalCircular) {
        const manumesh::feature::FeatureLoop& origLoop = originalFeatures.loops[originalId];
        int bestLoopId = -1;
        double bestScore = std::numeric_limits<double>::infinity();
        double bestCenterError = 0.0;
        double bestRadiusError = 0.0;
        double bestNormalAngleDeg = 0.0;

        for (int simplifiedId : simplifiedCircular) {
            if (usedSimplified[simplifiedId]) {
                continue;
            }
            const manumesh::feature::FeatureLoop& simpLoop = simplifiedFeatures.loops[simplifiedId];
            const double centerError = (origLoop.center - simpLoop.center).norm();
            const double radiusError = std::abs(origLoop.radius - simpLoop.radius);
            const double normalDot =
                std::clamp(std::abs(origLoop.normal.normalized().dot(simpLoop.normal.normalized())), 0.0, 1.0);
            const double normalAngle = std::acos(normalDot);
            const double score =
                centerError / diag + radiusError / std::max(1e-12, origLoop.radius) + normalAngle / kPi;
            if (score < bestScore) {
                bestScore = score;
                bestLoopId = simplifiedId;
                bestCenterError = centerError;
                bestRadiusError = radiusError;
                bestNormalAngleDeg = normalAngle * 180.0 / kPi;
            }
        }

        std::string status = "missing";
        manumesh::feature::DirectionalCurveError directional;
        int simplifiedVertexCount = 0;
        double simplifiedRadius = 0.0;
        bool plausibleMatch = false;
        if (bestLoopId >= 0) {
            const manumesh::feature::FeatureLoop& simpLoop = simplifiedFeatures.loops[bestLoopId];
            const double radiusRel = bestRadiusError / std::max(1e-12, origLoop.radius);
            plausibleMatch = bestCenterError <= 0.08 * diag && radiusRel <= 0.20 && bestNormalAngleDeg <= 30.0;
            if (plausibleMatch) {
                usedSimplified[bestLoopId] = 1;
                directional = manumesh::feature::measureLoopAgainstCircle(
                    simplified, simpLoop, origLoop.center, origLoop.normal, origLoop.radius
                );
                simplifiedVertexCount = static_cast<int>(simpLoop.vertices.size());
                simplifiedRadius = simpLoop.radius;
            }
        }

        if (plausibleMatch) {
            const double radiusRel = bestRadiusError / std::max(1e-12, origLoop.radius);
            status = (bestCenterError <= 0.04 * diag && radiusRel <= 0.08 && bestNormalAngleDeg <= 15.0) ? "matched"
                                                                                                         : "weak_match";
            ++matched;
        } else {
            bestLoopId = -1;
            bestCenterError = 0.0;
            bestRadiusError = 0.0;
            bestNormalAngleDeg = 0.0;
            ++missing;
        }

        rows << origLoop.id << "," << bestLoopId << "," << origLoop.vertices.size() << "," << simplifiedVertexCount
             << "," << origLoop.radius << "," << simplifiedRadius << "," << bestCenterError << "," << bestRadiusError
             << "," << bestNormalAngleDeg << "," << directional.radialRms << "," << directional.radialMax << ","
             << directional.planeRms << "," << directional.planeMax << "," << status << "\n";
    }

    const std::string header = "orig_loop,matched_loop,orig_vertices,simplified_vertices,orig_radius,"
                               "simplified_radius,center_error,radius_error,normal_angle_deg,"
                               "radial_rms,radial_max,plane_rms,plane_max,status";
    std::cout << "original_circular_loops=" << originalCircular.size()
              << " simplified_circular_loops=" << simplifiedCircular.size() << " matched=" << matched
              << " missing=" << missing << "\n";
    std::cout << header << "\n" << rows.str();

    const std::string csvPath = getArg(args, "--csv");
    if (!csvPath.empty()) {
        const fs::path output(csvPath);
        if (output.has_parent_path()) {
            fs::create_directories(output.parent_path());
        }
        std::ofstream csv(csvPath);
        csv << "original_circular_loops,simplified_circular_loops,matched,missing\n";
        csv << originalCircular.size() << "," << simplifiedCircular.size() << "," << matched << "," << missing
            << "\n\n";
        csv << header << "\n" << rows.str();
    }
    return 0;
}

} // namespace manumesh::cli::feature_commands
