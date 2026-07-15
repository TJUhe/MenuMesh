#include "ManuMeshFeatureCommands.h"

#include "CliCsv.h"
#include "CliOptionBinding.h"
#include "algorithms/feature_detection/FeatureComparison.h"
#include "algorithms/feature_detection/FeatureDetector.h"
#include "core/Mesh.h"
#include "io/MeshIo.h"

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

namespace manumesh::cli::feature_commands {

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
              << " graph_cleanup_skipped_by_cap=" << analysis.graphCleanupSkippedByCap
              << " graph_consolidation_bridges=" << analysis.graphConsolidationBridges
              << " graph_consolidation_skipped_by_cap=" << analysis.graphConsolidationSkippedByCap
              << " circular_recovery_truncated=" << analysis.circularRecoveryTruncated
              << " inconsistent_winding_edges=" << analysis.inconsistentWindingEdges
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
              << " mean_smooth_curvature_scale_stability=" << analysis.meanSmoothCurvatureScaleStability
              << " normal_filter_iterations=" << analysis.normalFilter.iterationsCompleted
              << " normal_filter_changed_faces=" << analysis.normalFilter.changedFaces
              << " normal_filter_preserved_edges=" << analysis.normalFilter.preservedEdges
              << " mean_normal_filter_angular_change_deg=" << analysis.normalFilter.meanAngularChangeDeg
              << " junction_branch_pairs=" << analysis.junctionBranchPairs
              << " ambiguous_junctions=" << analysis.ambiguousJunctions
              << " surface_patches=" << analysis.patches.size()
              << " closed_surface_patches=" << analysis.closedSurfacePatches
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
               "graph_cleanup_skipped_by_cap,circular_recovery_truncated,inconsistent_winding_edges,"
               "graph_consolidation_bridges,graph_consolidation_skipped_by_cap,"
               "normal_tensor_scored_vertices,smooth_curvature_scored_vertices,convex_edges,"
               "concave_edges,unknown_signed_edges,"
               "max_normal_tensor_score,max_normal_tensor_persistent_score,"
               "mean_normal_tensor_local_scale,mean_normal_tensor_persistence,"
               "max_smooth_curvature_score,max_smooth_curvature_persistent_score,"
               "mean_smooth_curvature_local_scale,mean_smooth_curvature_persistence,"
               "mean_smooth_curvature_scale_stability,normal_filter_iterations,"
               "normal_filter_changed_faces,normal_filter_preserved_edges,"
               "mean_normal_filter_angular_change_deg,junction_branch_pairs,ambiguous_junctions,"
               "surface_patches,closed_surface_patches,"
               "mean_feature_component_confidence,min_feature_component_confidence,"
               "loops,circular_loops,circle_loops,"
               "near_circle_loops,ellipse_loops,polygonal_loops\n";
        csv << analysis.featureEdges << "," << analysis.tracedFeatureEdges << "," << analysis.untracedFeatureEdges
            << "," << analysis.boundaryFeatureEdges << "," << analysis.dihedralFeatureEdges << ","
            << analysis.normalTensorFeatureEdges << "," << analysis.smoothCurvatureFeatureEdges << ","
            << analysis.nonManifoldFeatureEdges << "," << analysis.components.size() << ","
            << analysis.weakFeatureComponents << "," << analysis.highConfidenceFeatureComponents << ","
            << analysis.graphCleanupBridgedGaps << "," << analysis.graphCleanupRemovedSpurs << ","
            << analysis.graphCleanupMergedJunctions << "," << analysis.graphCleanupSkippedByCap << ","
            << analysis.circularRecoveryTruncated << "," << analysis.inconsistentWindingEdges << ","
            << analysis.graphConsolidationBridges << "," << analysis.graphConsolidationSkippedByCap << ","
            << analysis.normalTensorScoredVertices << "," << analysis.smoothCurvatureScoredVertices << ","
            << analysis.convexFeatureEdges << "," << analysis.concaveFeatureEdges << ","
            << analysis.unknownSignedFeatureEdges << "," << analysis.maxNormalTensorFeatureScore << ","
            << analysis.maxNormalTensorPersistentScore << "," << analysis.meanNormalTensorLocalScale << ","
            << analysis.meanNormalTensorPersistence << "," << analysis.maxSmoothCurvatureFeatureScore << ","
            << analysis.maxSmoothCurvaturePersistentScore << "," << analysis.meanSmoothCurvatureLocalScale << ","
            << analysis.meanSmoothCurvaturePersistence << "," << analysis.meanSmoothCurvatureScaleStability << ","
            << analysis.normalFilter.iterationsCompleted << "," << analysis.normalFilter.changedFaces << ","
            << analysis.normalFilter.preservedEdges << "," << analysis.normalFilter.meanAngularChangeDeg << ","
            << analysis.junctionBranchPairs << "," << analysis.ambiguousJunctions << "," << analysis.patches.size()
            << "," << analysis.closedSurfacePatches << "," << analysis.meanFeatureComponentConfidence << ","
            << analysis.minFeatureComponentConfidence << "," << analysis.loops.size() << "," << circularLoops << ","
            << circleLoops << "," << nearCircleLoops << "," << ellipseLoops << "," << polygonalLoops << "\n\n";
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
readFeatureBenchmarkLabels(const fs::path& path, manumesh::feature::FeatureBenchmarkLabels& labels, int faceCount) {
    std::ifstream in(path);
    if (!in) {
        throw std::runtime_error("Cannot open feature label CSV: " + path.string());
    }

    int skippedRows = 0;
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
                labels.junctionVertices.push_back(id);
            } else {
                ++skippedRows;
            }
            continue;
        }
        if ((fields[0] == "branch" || fields[0] == "branch_pair") && fields.size() >= 4) {
            manumesh::feature::FeatureBranchPairLabel label;
            if (tryParseIntField(fields[1], label.junctionVertex) && tryParseIntField(fields[2], label.firstNeighbor) &&
                tryParseIntField(fields[3], label.secondNeighbor)) {
                labels.branchPairs.push_back(label);
            } else {
                ++skippedRows;
            }
            continue;
        }
        if ((fields[0] == "face_patch" || fields[0] == "patch") && fields.size() >= 3) {
            int faceId = -1;
            int patchId = -1;
            if (tryParseIntField(fields[1], faceId) && tryParseIntField(fields[2], patchId) && faceId >= 0 &&
                faceId < faceCount) {
                if (labels.facePatchIds.empty()) {
                    labels.facePatchIds.assign(static_cast<std::size_t>(faceCount), -1);
                }
                labels.facePatchIds[faceId] = patchId;
            } else {
                ++skippedRows;
            }
            continue;
        }
        int a = -1;
        int b = -1;
        if (fields[0] == "edge" && fields.size() >= 3 && tryParseIntField(fields[1], a) &&
            tryParseIntField(fields[2], b)) {
            labels.edges.emplace_back(a, b);
        } else if (fields.size() >= 2 && tryParseIntField(fields[0], a) && tryParseIntField(fields[1], b)) {
            labels.edges.emplace_back(a, b);
        } else {
            ++skippedRows;
        }
    }
    if (skippedRows > 0) {
        std::cout << "feature-benchmark: skipped " << skippedRows << " unparsable label row"
                  << (skippedRows == 1 ? "" : "s") << " in " << path.string() << "\n";
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

    manumesh::feature::FeatureBenchmarkLabels labels;
    readFeatureBenchmarkLabels(positional[1], labels, static_cast<int>(input.faces.size()));

    manumesh::feature::FeatureOptions options = parseFeatureOptions(args);
    if (!labels.facePatchIds.empty()) {
        options.surfacePatches.enabled = true;
    }
    const manumesh::feature::FeatureAnalysis analysis = manumesh::feature::detectFeatureCurves(input, options);
    const manumesh::feature::FeatureEdgeBenchmark benchmark =
        manumesh::feature::benchmarkFeatureAnalysis(input, analysis, labels);

    const std::string header = "ground_truth_edges,detected_edges,true_positive_edges,false_positive_edges,"
                               "false_negative_edges,edge_precision,edge_recall,edge_f1,"
                               "ground_truth_junctions,detected_junctions,true_positive_junctions,"
                               "false_positive_junctions,false_negative_junctions,junction_precision,"
                               "junction_recall,junction_f1,loop_closure_rate,mean_component_confidence,"
                               "ground_truth_branch_pairs,detected_branch_pairs,true_positive_branch_pairs,"
                               "false_positive_branch_pairs,false_negative_branch_pairs,branch_pair_precision,"
                               "branch_pair_recall,branch_pair_f1,labeled_face_adjacencies,"
                               "correct_face_adjacencies,patch_adjacency_accuracy";
    std::ostringstream row;
    row << std::setprecision(12) << benchmark.groundTruthEdges << "," << benchmark.detectedEdges << ","
        << benchmark.truePositiveEdges << "," << benchmark.falsePositiveEdges << "," << benchmark.falseNegativeEdges
        << "," << benchmark.edgePrecision << "," << benchmark.edgeRecall << "," << benchmark.edgeF1 << ","
        << benchmark.groundTruthJunctions << "," << benchmark.detectedJunctions << ","
        << benchmark.truePositiveJunctions << "," << benchmark.falsePositiveJunctions << ","
        << benchmark.falseNegativeJunctions << "," << benchmark.junctionPrecision << "," << benchmark.junctionRecall
        << "," << benchmark.junctionF1 << "," << benchmark.loopClosureRate << "," << benchmark.meanComponentConfidence
        << "," << benchmark.groundTruthBranchPairs << "," << benchmark.detectedBranchPairs << ","
        << benchmark.truePositiveBranchPairs << "," << benchmark.falsePositiveBranchPairs << ","
        << benchmark.falseNegativeBranchPairs << "," << benchmark.branchPairPrecision << ","
        << benchmark.branchPairRecall << "," << benchmark.branchPairF1 << "," << benchmark.labeledFaceAdjacencies << ","
        << benchmark.correctFaceAdjacencies << "," << benchmark.patchAdjacencyAccuracy;

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

    manumesh::feature::LoopMatchOptions matchOptions;
    matchOptions.referenceDiagonal = original.bboxDiag();
    const manumesh::feature::LoopMatchReport matchReport =
        manumesh::feature::matchCircularLoops(originalFeatures, simplifiedFeatures, simplified, matchOptions);

    std::ostringstream rows;
    rows << std::setprecision(12);
    for (const manumesh::feature::LoopMatch& match : matchReport.matches) {
        rows << match.originalLoopId << "," << match.simplifiedLoopIndex << "," << match.originalVertices << ","
             << match.simplifiedVertices << "," << match.originalRadius << "," << match.simplifiedRadius << ","
             << match.centerError << "," << match.radiusError << "," << match.normalAngleDeg << ","
             << match.directional.radialRms << "," << match.directional.radialMax << "," << match.directional.planeRms
             << "," << match.directional.planeMax << "," << manumesh::feature::toString(match.status) << "\n";
    }

    const std::string header = "orig_loop,matched_loop,orig_vertices,simplified_vertices,orig_radius,"
                               "simplified_radius,center_error,radius_error,normal_angle_deg,"
                               "radial_rms,radial_max,plane_rms,plane_max,status";
    std::cout << "original_circular_loops=" << matchReport.originalCircularLoops
              << " simplified_circular_loops=" << matchReport.simplifiedCircularLoops
              << " matched=" << matchReport.matchedLoops << " missing=" << matchReport.missingLoops << "\n";
    std::cout << header << "\n" << rows.str();

    const std::string csvPath = getArg(args, "--csv");
    if (!csvPath.empty()) {
        const fs::path output(csvPath);
        if (output.has_parent_path()) {
            fs::create_directories(output.parent_path());
        }
        std::ofstream csv(csvPath);
        csv << "original_circular_loops,simplified_circular_loops,matched,missing\n";
        csv << matchReport.originalCircularLoops << "," << matchReport.simplifiedCircularLoops << ","
            << matchReport.matchedLoops << "," << matchReport.missingLoops << "\n\n";
        csv << header << "\n" << rows.str();
    }
    return 0;
}

} // namespace manumesh::cli::feature_commands
