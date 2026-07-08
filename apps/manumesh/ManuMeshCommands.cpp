#include "CliArguments.h"
#include "CliCommands.h"
#include "algorithms/feature_detection/FeatureDetector.h"
#include "algorithms/simplification/Metrics.h"
#include "algorithms/simplification/QEMSimplifier.h"
#include "core/Mesh.h"
#include "core/MeshGenerators.h"

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
using manumesh::cli::getArg;
using manumesh::cli::getDoubleArg;
using manumesh::cli::getIntArg;
using manumesh::cli::hasFlag;
using manumesh::cli::parseDoubleStrict;
using manumesh::cli::parseFaceCounts;
using manumesh::cli::parseWeights;
using manumesh::cli::positionalArgs;

int commandGenerate(const Args& args);
int commandSimplify(const Args& args);
int commandFeatureReport(const Args& args);
int commandFeatureCompare(const Args& args);
int commandSweep(const Args& args);
int commandRatioSweep(const Args& args);
int commandFaceSweep(const Args& args);
int commandCompare(const Args& args);
int commandDemo(const Args& args);
int commandSummarizeMetrics(const Args& args);
int commandValidateFeatures(const Args& args);
int commandValidateExternal(const Args& args);

constexpr double kPi = 3.141592653589793238462643383279502884;

std::string sanitizeWeight(double value) {
  std::ostringstream out;
  out << std::scientific << std::setprecision(0) << value;
  std::string text = out.str();
  for (char& ch : text) {
    if (ch == '+' || ch == '-' || ch == '.') ch = '_';
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
    if (ch == '.') ch = '_';
  }
  return text;
}

manumesh::simplification::SimplifyOptions parseSimplifyOptions(const Args& args) {
  manumesh::simplification::SimplifyOptions options;
  options.targetRatio = getDoubleArg(args, "--ratio", options.targetRatio);
  options.targetFaces = getIntArg(args, "--target-faces", options.targetFaces);
  options.lineWeight = getDoubleArg(args, "--line-weight", options.lineWeight);
  options.featureBoost = getDoubleArg(args, "--feature-boost", options.featureBoost);
  options.featureAngleDeg =
      getDoubleArg(args, "--feature-angle-deg", options.featureAngleDeg);
  options.loopTraceAngleDeg =
      getDoubleArg(args, "--loop-trace-angle-deg", options.loopTraceAngleDeg);
  options.boundaryWeight =
      getDoubleArg(args, "--boundary-weight", options.boundaryWeight);
  options.featureCurveWeight =
      getDoubleArg(args, "--feature-curve-weight", options.featureCurveWeight);
  options.maxFeatureCurveDeviationRatio =
      getDoubleArg(args, "--max-feature-curve-deviation-ratio",
                   options.maxFeatureCurveDeviationRatio);
  options.circleFitRelativeThreshold =
      getDoubleArg(args, "--circle-fit-threshold", options.circleFitRelativeThreshold);
  options.ellipseFitRelativeThreshold = getDoubleArg(
      args, "--ellipse-fit-threshold", options.ellipseFitRelativeThreshold);
  options.nearCircleAxisRatioTolerance = getDoubleArg(
      args, "--near-circle-axis-ratio-tolerance", options.nearCircleAxisRatioTolerance);
  options.minFeatureLoopVertices =
      getIntArg(args, "--min-feature-loop-vertices", options.minFeatureLoopVertices);
  options.minCircularFeatureLoopVertices =
      getIntArg(args, "--min-circular-feature-loop-vertices",
                options.minCircularFeatureLoopVertices);
  options.adaptiveBaseLineWeight =
      getDoubleArg(args, "--adaptive-base-line-weight", options.adaptiveBaseLineWeight);
  options.normalTensorFeatureThreshold = getDoubleArg(
      args, "--normal-tensor-threshold", options.normalTensorFeatureThreshold);
  options.normalTensorMinEdgeAlignment = getDoubleArg(
      args, "--normal-tensor-edge-alignment", options.normalTensorMinEdgeAlignment);
  options.normalTensorSmoothingIterations = getIntArg(
      args, "--normal-tensor-smoothing", options.normalTensorSmoothingIterations);
  options.normalTensorScaleCount =
      getIntArg(args, "--normal-tensor-scales", options.normalTensorScaleCount);
  options.normalTensorMinPersistentScales =
      getIntArg(args, "--normal-tensor-min-persistent-scales",
                options.normalTensorMinPersistentScales);
  options.minTriangleQuality =
      getDoubleArg(args, "--min-triangle-quality", options.minTriangleQuality);
  options.maxNormalDeviationDeg =
      getDoubleArg(args, "--max-normal-deviation-deg", options.maxNormalDeviationDeg);
  options.maxLocalError =
      getDoubleArg(args, "--max-local-error", options.maxLocalError);
  options.maxLocalErrorRatio =
      getDoubleArg(args, "--max-local-error-ratio", options.maxLocalErrorRatio);
  options.verbose = hasFlag(args, "--verbose");
  options.adaptiveScale = hasFlag(args, "--adaptive-scale");
  options.preserveBoundary = hasFlag(args, "--preserve-boundary");
  options.preventLocalIntersections = hasFlag(args, "--prevent-local-intersections");
  options.preserveFeatureCurves = hasFlag(args, "--preserve-feature-curves");
  options.featureProtectionMode = manumesh::simplification::parseFeatureProtectionMode(
      getArg(args, "--feature-protection-mode",
             manumesh::simplification::toString(options.featureProtectionMode)));
  options.useNormalTensorFeatures = !hasFlag(args, "--no-normal-tensor-features");
  if (hasFlag(args, "--industrial-safe")) {
    options.preserveBoundary = true;
    options.minTriangleQuality = std::max(options.minTriangleQuality, 1e-4);
    options.maxNormalDeviationDeg = std::min(options.maxNormalDeviationDeg, 75.0);
    options.maxLocalErrorRatio = std::max(options.maxLocalErrorRatio, 0.02);
    options.preventLocalIntersections = true;
  }

  const std::string mode = getArg(args, "--weight-mode", "uniform");
  options.weightMode = manumesh::simplification::parseWeightMode(mode);

  const std::string method = getArg(args, "--method", "line");
  if (method == "standard" || method == "qem") {
    options.useLineQuadrics = false;
    options.lineWeight = 0.0;
  } else if (method == "line") {
    options.useLineQuadrics = true;
  } else {
    throw std::invalid_argument("Unknown --method. Use standard or line.");
  }
  return options;
}

manumesh::feature::FeatureOptions parseFeatureOptions(const Args& args) {
  manumesh::feature::FeatureOptions options;
  options.featureAngleDeg =
      getDoubleArg(args, "--feature-angle-deg", options.featureAngleDeg);
  options.loopTraceAngleDeg =
      getDoubleArg(args, "--loop-trace-angle-deg", options.loopTraceAngleDeg);
  options.circleFitRelativeThreshold =
      getDoubleArg(args, "--circle-fit-threshold", options.circleFitRelativeThreshold);
  options.ellipseFitRelativeThreshold = getDoubleArg(
      args, "--ellipse-fit-threshold", options.ellipseFitRelativeThreshold);
  options.nearCircleAxisRatioTolerance = getDoubleArg(
      args, "--near-circle-axis-ratio-tolerance", options.nearCircleAxisRatioTolerance);
  options.minFeatureLoopVertices =
      getIntArg(args, "--min-feature-loop-vertices", options.minFeatureLoopVertices);
  options.normalTensorFeatureThreshold = getDoubleArg(
      args, "--normal-tensor-threshold", options.normalTensorFeatureThreshold);
  options.normalTensorMinEdgeAlignment = getDoubleArg(
      args, "--normal-tensor-edge-alignment", options.normalTensorMinEdgeAlignment);
  options.normalTensorSmoothingIterations = getIntArg(
      args, "--normal-tensor-smoothing", options.normalTensorSmoothingIterations);
  options.normalTensorScaleCount =
      getIntArg(args, "--normal-tensor-scales", options.normalTensorScaleCount);
  options.normalTensorMinPersistentScales =
      getIntArg(args, "--normal-tensor-min-persistent-scales",
                options.normalTensorMinPersistentScales);
  options.useNormalTensorFeatures = !hasFlag(args, "--no-normal-tensor-features");
  return options;
}

void printStats(const std::string& label,
                const manumesh::simplification::MeshStats& stats) {
  std::cout << label << ": vertices=" << stats.vertices << " faces=" << stats.faces
            << " mean_quality=" << stats.meanTriangleQuality
            << " min_quality=" << stats.minTriangleQuality
            << " edge_cv=" << stats.edgeLengthCv << "\n";
}

Args makeArgs(std::initializer_list<std::string> values) {
  Args args;
  args.values.assign(values.begin(), values.end());
  return args;
}

std::string pathString(const fs::path& path) {
  return path.string();
}

std::vector<std::string> splitCsvLine(const std::string& line) {
  std::vector<std::string> out;
  std::string current;
  bool quoted = false;
  for (std::size_t i = 0; i < line.size(); ++i) {
    const char ch = line[i];
    if (ch == '"') {
      if (quoted && i + 1 < line.size() && line[i + 1] == '"') {
        current.push_back('"');
        ++i;
      } else {
        quoted = !quoted;
      }
    } else if (ch == ',' && !quoted) {
      out.push_back(current);
      current.clear();
    } else {
      current.push_back(ch);
    }
  }
  out.push_back(current);
  return out;
}

std::string quoteCsv(const std::string& value) {
  bool needsQuotes = false;
  std::string escaped;
  for (char ch : value) {
    if (ch == '"') {
      escaped += "\"\"";
      needsQuotes = true;
    } else {
      if (ch == ',' || ch == '\n' || ch == '\r') {
        needsQuotes = true;
      }
      escaped.push_back(ch);
    }
  }
  return needsQuotes ? '"' + escaped + '"' : escaped;
}

std::map<std::string, std::string> readFirstCsvRow(const fs::path& path) {
  std::ifstream in(path);
  if (!in) {
    return {};
  }
  std::string headerLine;
  std::string valueLine;
  if (!std::getline(in, headerLine) || !std::getline(in, valueLine)) {
    return {};
  }
  const std::vector<std::string> headers = splitCsvLine(headerLine);
  const std::vector<std::string> values = splitCsvLine(valueLine);
  std::map<std::string, std::string> row;
  for (std::size_t i = 0; i < headers.size(); ++i) {
    row[headers[i]] = i < values.size() ? values[i] : "";
  }
  return row;
}

std::string csvValue(const std::map<std::string, std::string>& row,
                     const std::string& key) {
  const auto it = row.find(key);
  return it == row.end() ? "" : it->second;
}

void runGenerate(const fs::path& inputDir, const std::string& name,
                 const std::string& type, int n) {
  commandGenerate(makeArgs({"--type", type, "--n", std::to_string(n), "--out",
                            pathString(inputDir / (name + ".stl"))}));
}

fs::path copyExternalInput(const fs::path& inputDir, const std::string& name,
                           const fs::path& source) {
  if (!fs::exists(source)) {
    throw std::runtime_error("External input does not exist: " + source.string());
  }
  fs::create_directories(inputDir);
  const fs::path destination = inputDir / (name + ".stl");
  std::error_code ec;
  fs::remove(destination, ec);
  ec.clear();
  fs::copy_file(source, destination, fs::copy_options::none, ec);
  if (ec) {
    throw std::runtime_error("Failed to copy external input from " + source.string() +
                             " to " + destination.string() + ": " + ec.message());
  }
  return destination;
}

void runSweep(const fs::path& input, const fs::path& outDir,
              std::initializer_list<std::string> options) {
  Args args;
  args.values.push_back(pathString(input));
  args.values.push_back(pathString(outDir));
  args.values.insert(args.values.end(), options.begin(), options.end());
  commandSweep(args);
}

void runRatioSweep(const fs::path& input, const fs::path& outDir,
                   std::initializer_list<std::string> options) {
  Args args;
  args.values.push_back(pathString(input));
  args.values.push_back(pathString(outDir));
  args.values.insert(args.values.end(), options.begin(), options.end());
  commandRatioSweep(args);
}

void runFaceSweep(const fs::path& input, const fs::path& outDir,
                  std::initializer_list<std::string> options) {
  Args args;
  args.values.push_back(pathString(input));
  args.values.push_back(pathString(outDir));
  args.values.insert(args.values.end(), options.begin(), options.end());
  commandFaceSweep(args);
}

void runSimplify(const fs::path& input, const fs::path& output,
                 std::initializer_list<std::string> options) {
  Args args;
  args.values.push_back(pathString(input));
  args.values.push_back(pathString(output));
  args.values.insert(args.values.end(), options.begin(), options.end());
  commandSimplify(args);
}

void runFeatureReport(const fs::path& input,
                      std::initializer_list<std::string> options) {
  Args args;
  args.values.push_back(pathString(input));
  args.values.insert(args.values.end(), options.begin(), options.end());
  commandFeatureReport(args);
}

void runFeatureCompare(const fs::path& original, const fs::path& simplified,
                       std::initializer_list<std::string> options) {
  Args args;
  args.values.push_back(pathString(original));
  args.values.push_back(pathString(simplified));
  args.values.insert(args.values.end(), options.begin(), options.end());
  commandFeatureCompare(args);
}

void summarizeMetrics(const fs::path& outputRoot, const fs::path& summaryPath) {
  std::vector<std::string> columns = {"case"};
  std::vector<std::map<std::string, std::string>> rows;

  if (!fs::exists(outputRoot)) {
    throw std::runtime_error("Output directory not found: " + outputRoot.string());
  }

  for (const fs::directory_entry& entry :
       fs::recursive_directory_iterator(outputRoot)) {
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
    const std::string caseName = entry.path().parent_path().filename().string();
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
    if (i > 0) out << ",";
    out << quoteCsv(columns[i]);
  }
  out << "\n";
  for (const auto& row : rows) {
    for (std::size_t i = 0; i < columns.size(); ++i) {
      if (i > 0) out << ",";
      out << quoteCsv(csvValue(row, columns[i]));
    }
    out << "\n";
  }
  std::cout << "Wrote " << summaryPath << " with " << rows.size() << " rows\n";
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
  if (!manumesh::saveAsciiStl(outPath, mesh, type, &error)) {
    throw std::runtime_error(error);
  }

  printStats(type, manumesh::simplification::computeMeshStats(mesh));
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

  const manumesh::simplification::MeshStats stats =
      manumesh::simplification::computeMeshStats(simplified);
  const manumesh::simplification::DistanceStats distance =
      manumesh::simplification::compareMeshesBySampledDistance(original, simplified,
                                                               samples);
  printStats("simplified", stats);
  std::cout << "distance mean original->simplified="
            << distance.meanOriginalToSimplified
            << " max=" << distance.maxOriginalToSimplified << "\n";
  std::cout << "distance mean simplified->original="
            << distance.meanSimplifiedToOriginal
            << " max=" << distance.maxSimplifiedToOriginal << "\n";
  return 0;
}

int countCircularLoops(const manumesh::feature::FeatureAnalysis& analysis) {
  int count = 0;
  for (const manumesh::feature::FeatureLoop& loop : analysis.loops) {
    if (loop.circular) {
      ++count;
    }
  }
  return count;
}

int countPrimitiveLoops(const manumesh::feature::FeatureAnalysis& analysis,
                        manumesh::feature::FeaturePrimitiveType primitive) {
  int count = 0;
  for (const manumesh::feature::FeatureLoop& loop : analysis.loops) {
    if (loop.primitive == primitive) {
      ++count;
    }
  }
  return count;
}

int commandFeatureReport(const Args& args) {
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
  const manumesh::feature::FeatureAnalysis analysis =
      manumesh::feature::detectFeatureCurves(input, options);
  const int circularLoops = countCircularLoops(analysis);
  const int circleLoops =
      countPrimitiveLoops(analysis, manumesh::feature::FeaturePrimitiveType::Circle);
  const int nearCircleLoops = countPrimitiveLoops(
      analysis, manumesh::feature::FeaturePrimitiveType::NearCircle);
  const int ellipseLoops =
      countPrimitiveLoops(analysis, manumesh::feature::FeaturePrimitiveType::Ellipse);
  const int polygonalLoops = countPrimitiveLoops(
      analysis, manumesh::feature::FeaturePrimitiveType::PolygonalLoop);
  std::cout << "feature_edges=" << analysis.featureEdges
            << " traced_edges=" << analysis.tracedFeatureEdges
            << " untraced_edges=" << analysis.untracedFeatureEdges
            << " boundary_edges=" << analysis.boundaryFeatureEdges
            << " dihedral_edges=" << analysis.dihedralFeatureEdges
            << " normal_tensor_edges=" << analysis.normalTensorFeatureEdges
            << " non_manifold_edges=" << analysis.nonManifoldFeatureEdges
            << " normal_tensor_scored_vertices="
            << analysis.normalTensorScoredVertices
            << " convex_edges=" << analysis.convexFeatureEdges
            << " concave_edges=" << analysis.concaveFeatureEdges
            << " unknown_signed_edges=" << analysis.unknownSignedFeatureEdges
            << " max_normal_tensor_score=" << analysis.maxNormalTensorFeatureScore
            << " max_normal_tensor_persistent_score="
            << analysis.maxNormalTensorPersistentScore
            << " mean_normal_tensor_local_scale="
            << analysis.meanNormalTensorLocalScale
            << " mean_normal_tensor_persistence="
            << analysis.meanNormalTensorPersistence
            << " loops=" << analysis.loops.size() << " circular_loops=" << circularLoops
            << " circle_loops=" << circleLoops
            << " near_circle_loops=" << nearCircleLoops
            << " ellipse_loops=" << ellipseLoops
            << " polygonal_loops=" << polygonalLoops << "\n";
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
           "dihedral_edges,normal_tensor_edges,non_manifold_edges,"
           "normal_tensor_scored_vertices,convex_edges,"
           "concave_edges,unknown_signed_edges,"
           "max_normal_tensor_score,max_normal_tensor_persistent_score,"
           "mean_normal_tensor_local_scale,mean_normal_tensor_persistence,"
           "loops,circular_loops,circle_loops,"
           "near_circle_loops,ellipse_loops,polygonal_loops\n";
    csv << analysis.featureEdges << "," << analysis.tracedFeatureEdges << ","
        << analysis.untracedFeatureEdges << "," << analysis.boundaryFeatureEdges << ","
        << analysis.dihedralFeatureEdges << "," << analysis.normalTensorFeatureEdges
        << "," << analysis.nonManifoldFeatureEdges << ","
        << analysis.normalTensorScoredVertices << ","
        << analysis.convexFeatureEdges << "," << analysis.concaveFeatureEdges << ","
        << analysis.unknownSignedFeatureEdges << ","
        << analysis.maxNormalTensorFeatureScore << ","
        << analysis.maxNormalTensorPersistentScore << ","
        << analysis.meanNormalTensorLocalScale << ","
        << analysis.meanNormalTensorPersistence << "," << analysis.loops.size() << ","
        << circularLoops << "," << circleLoops << "," << nearCircleLoops << ","
        << ellipseLoops << "," << polygonalLoops << "\n\n";
    csv << manumesh::feature::featureReportHeaderCsv() << "\n";
    for (const manumesh::feature::FeatureLoop& loop : analysis.loops) {
      csv << manumesh::feature::featureLoopRowCsv(loop) << "\n";
    }
  }
  return 0;
}

int commandFeatureCompare(const Args& args) {
  const auto positional = positionalArgs(args);
  if (positional.size() < 2) {
    throw std::invalid_argument(
        "feature-compare requires original.stl simplified.stl.");
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
      const manumesh::feature::FeatureLoop& simpLoop =
          simplifiedFeatures.loops[simplifiedId];
      const double centerError = (origLoop.center - simpLoop.center).norm();
      const double radiusError = std::abs(origLoop.radius - simpLoop.radius);
      const double normalDot = std::clamp(
          std::abs(origLoop.normal.normalized().dot(simpLoop.normal.normalized())), 0.0,
          1.0);
      const double normalAngle = std::acos(normalDot);
      const double score = centerError / diag +
                           radiusError / std::max(1e-12, origLoop.radius) +
                           normalAngle / kPi;
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
      const manumesh::feature::FeatureLoop& simpLoop =
          simplifiedFeatures.loops[bestLoopId];
      const double radiusRel = bestRadiusError / std::max(1e-12, origLoop.radius);
      plausibleMatch = bestCenterError <= 0.08 * diag && radiusRel <= 0.20 &&
                       bestNormalAngleDeg <= 30.0;
      if (plausibleMatch) {
        usedSimplified[bestLoopId] = 1;
        directional = manumesh::feature::measureLoopAgainstCircle(
            simplified, simpLoop, origLoop.center, origLoop.normal, origLoop.radius);
        simplifiedVertexCount = static_cast<int>(simpLoop.vertices.size());
        simplifiedRadius = simpLoop.radius;
      }
    }

    if (plausibleMatch) {
      const double radiusRel = bestRadiusError / std::max(1e-12, origLoop.radius);
      status = (bestCenterError <= 0.04 * diag && radiusRel <= 0.08 &&
                bestNormalAngleDeg <= 15.0)
                   ? "matched"
                   : "weak_match";
      ++matched;
    } else {
      bestLoopId = -1;
      bestCenterError = 0.0;
      bestRadiusError = 0.0;
      bestNormalAngleDeg = 0.0;
      ++missing;
    }

    rows << origLoop.id << "," << bestLoopId << "," << origLoop.vertices.size() << ","
         << simplifiedVertexCount << "," << origLoop.radius << "," << simplifiedRadius
         << "," << bestCenterError << "," << bestRadiusError << ","
         << bestNormalAngleDeg << "," << directional.radialRms << ","
         << directional.radialMax << "," << directional.planeRms << ","
         << directional.planeMax << "," << status << "\n";
  }

  const std::string header =
      "orig_loop,matched_loop,orig_vertices,simplified_vertices,orig_radius,"
      "simplified_radius,center_error,radius_error,normal_angle_deg,"
      "radial_rms,radial_max,plane_rms,plane_max,status";
  std::cout << "original_circular_loops=" << originalCircular.size()
            << " simplified_circular_loops=" << simplifiedCircular.size()
            << " matched=" << matched << " missing=" << missing << "\n";
  std::cout << header << "\n" << rows.str();

  const std::string csvPath = getArg(args, "--csv");
  if (!csvPath.empty()) {
    const fs::path output(csvPath);
    if (output.has_parent_path()) {
      fs::create_directories(output.parent_path());
    }
    std::ofstream csv(csvPath);
    csv << "original_circular_loops,simplified_circular_loops,matched,missing\n";
    csv << originalCircular.size() << "," << simplifiedCircular.size() << "," << matched
        << "," << missing << "\n\n";
    csv << header << "\n" << rows.str();
  }
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
  if (!manumesh::saveAsciiStl(positional[1], output, "manumesh", &error)) {
    throw std::runtime_error(error);
  }

  const manumesh::simplification::MeshStats inStats =
      manumesh::simplification::computeMeshStats(input);
  const manumesh::simplification::MeshStats outStats =
      manumesh::simplification::computeMeshStats(output);
  const manumesh::simplification::DistanceStats distance =
      manumesh::simplification::compareMeshesBySampledDistance(input, output, samples);

  printStats("input", inStats);
  printStats("output", outStats);
  std::cout << "collapsed=" << report.collapsedEdges
            << " rejected=" << report.rejectedCollapses
            << " feature_rejected=" << report.featureRejectedCollapses
            << " boundary_rejected=" << report.boundaryRejectedCollapses
            << " topology_rejected=" << report.topologyRejectedCollapses
            << " normal_flip_rejected=" << report.normalFlipRejectedCollapses
            << " quality_rejected=" << report.qualityRejectedCollapses
            << " self_intersection_rejected="
            << report.selfIntersectionRejectedCollapses
            << " curve_budget_rejected=" << report.curveBudgetRejectedCollapses
            << " error_rejected=" << report.errorRejectedCollapses
            << " solver_fallbacks=" << report.solverFallbacks << " termination="
            << manumesh::simplification::toString(report.terminationReason)
            << " line_weight_range=[" << report.minAppliedLineWeight << ", "
            << report.maxAppliedLineWeight << "]\n";
  if (options.preserveFeatureCurves) {
    std::cout << "feature_loops=" << report.featureLoops
              << " circular_feature_loops=" << report.circularFeatureLoops
              << " feature_vertices=" << report.featureVertices
              << " traced_feature_edges=" << report.tracedFeatureEdges
              << " untraced_feature_edges=" << report.untracedFeatureEdges
              << " feature_protection_mode="
              << manumesh::simplification::toString(options.featureProtectionMode)
              << " normal_tensor_feature_edges=" << report.normalTensorFeatureEdges
              << " normal_tensor_scored_vertices="
              << report.normalTensorScoredVertices
              << " max_normal_tensor_persistent_score="
              << report.maxNormalTensorPersistentScore
              << " mean_normal_tensor_local_scale="
              << report.meanNormalTensorLocalScale
              << " mean_normal_tensor_persistence="
              << report.meanNormalTensorPersistence
              << " feature_rejected=" << report.featureRejectedCollapses
              << " primitive_feature_rejected="
              << report.primitiveFeatureRejectedCollapses
              << " generic_feature_rejected=" << report.genericFeatureRejectedCollapses
              << " curve_budget_rejected=" << report.curveBudgetRejectedCollapses
              << " projected_feature_placements=" << report.projectedFeaturePlacements
              << "\n";
  }
  std::cout << "distance mean original->simplified="
            << distance.meanOriginalToSimplified
            << " max=" << distance.maxOriginalToSimplified << "\n";

  if (!metricsCsv.empty()) {
    const fs::path metricsPath(metricsCsv);
    if (metricsPath.has_parent_path()) {
      fs::create_directories(metricsPath.parent_path());
    }
    std::ofstream csv(metricsCsv);
    csv << manumesh::simplification::statsHeaderCsv()
        << ",collapsed_edges,rejected_collapses,solver_fallbacks,"
           "feature_loops,circular_feature_loops,feature_vertices,"
           "traced_feature_edges,untraced_feature_edges,"
           "normal_tensor_feature_edges,normal_tensor_scored_vertices,"
           "max_normal_tensor_persistent_score,mean_normal_tensor_local_scale,"
           "mean_normal_tensor_persistence,feature_protection_mode,"
           "feature_rejected_collapses,boundary_rejected_collapses,"
           "primitive_feature_rejected_collapses,"
           "generic_feature_rejected_collapses,"
           "topology_rejected_collapses,normal_flip_rejected_collapses,"
           "quality_rejected_collapses,self_intersection_rejected_collapses,"
           "curve_budget_rejected_collapses,error_rejected_collapses,"
           "projected_feature_placements,termination_reason,"
           "min_line_weight,max_line_weight\n";
    csv << manumesh::simplification::statsRowCsv("output", outStats, &distance) << ","
        << report.collapsedEdges << "," << report.rejectedCollapses << ","
        << report.solverFallbacks << "," << report.featureLoops << ","
        << report.circularFeatureLoops << "," << report.featureVertices << ","
        << report.tracedFeatureEdges << "," << report.untracedFeatureEdges << ","
        << report.normalTensorFeatureEdges << ","
        << report.normalTensorScoredVertices << ","
        << report.maxNormalTensorPersistentScore << ","
        << report.meanNormalTensorLocalScale << ","
        << report.meanNormalTensorPersistence << ","
        << manumesh::simplification::toString(options.featureProtectionMode) << ","
        << report.featureRejectedCollapses << "," << report.boundaryRejectedCollapses
        << "," << report.primitiveFeatureRejectedCollapses << ","
        << report.genericFeatureRejectedCollapses << ","
        << report.topologyRejectedCollapses << "," << report.normalFlipRejectedCollapses
        << "," << report.qualityRejectedCollapses << ","
        << report.selfIntersectionRejectedCollapses << ","
        << report.curveBudgetRejectedCollapses << "," << report.errorRejectedCollapses
        << "," << report.projectedFeaturePlacements << ","
        << manumesh::simplification::toString(report.terminationReason) << ","
        << report.minAppliedLineWeight << "," << report.maxAppliedLineWeight << "\n";
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

  const fs::path outDir = positional[1];
  fs::create_directories(outDir);
  const int samples = getIntArg(args, "--samples", 3000);
  const std::vector<double> weights =
      parseWeights(getArg(args, "--weights", "0,1e-5,1e-4,1e-3,1e-2,1e-1"));
  manumesh::simplification::SimplifyOptions base = parseSimplifyOptions(args);

  std::ofstream csv(outDir / "metrics.csv");
  csv << "method,line_weight,weight_mode," << manumesh::simplification::statsHeaderCsv()
      << ",collapsed_edges,rejected_collapses,solver_fallbacks,"
         "min_line_weight,max_line_weight\n";

  for (double weight : weights) {
    manumesh::simplification::SimplifyOptions options = base;
    options.lineWeight = weight;
    options.useLineQuadrics =
        weight > 0.0 ||
        options.weightMode != manumesh::simplification::WeightMode::Uniform;
    if (weight <= 0.0 &&
        options.weightMode == manumesh::simplification::WeightMode::Uniform) {
      options.useLineQuadrics = false;
    }

    manumesh::simplification::SimplifyReport report;
    manumesh::simplification::QEMSimplifier simplifier(options);
    manumesh::Mesh output = simplifier.simplify(input, &report);
    const std::string method = options.useLineQuadrics ? "line" : "standard";
    const std::string label = method + "_w_" + sanitizeWeight(weight);
    const fs::path outStl = outDir / (label + ".stl");
    if (!manumesh::saveAsciiStl(outStl.string(), output, label, &error)) {
      throw std::runtime_error(error);
    }

    const manumesh::simplification::MeshStats stats =
        manumesh::simplification::computeMeshStats(output);
    const manumesh::simplification::DistanceStats distance =
        manumesh::simplification::compareMeshesBySampledDistance(input, output,
                                                                 samples);
    csv << method << "," << weight << ","
        << manumesh::simplification::toString(options.weightMode) << ","
        << manumesh::simplification::statsRowCsv(label, stats, &distance) << ","
        << report.collapsedEdges << "," << report.rejectedCollapses << ","
        << report.solverFallbacks << "," << report.minAppliedLineWeight << ","
        << report.maxAppliedLineWeight << "\n";
    printStats(label, stats);
  }

  std::cout << "Wrote sweep outputs to " << outDir << "\n";
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

  const fs::path outDir = positional[1];
  fs::create_directories(outDir);
  const int samples = getIntArg(args, "--samples", 3000);
  const std::vector<double> ratios =
      parseWeights(getArg(args, "--ratios", "0.8,0.5,0.25,0.1,0.05"));
  manumesh::simplification::SimplifyOptions base = parseSimplifyOptions(args);

  std::ofstream csv(outDir / "metrics.csv");
  csv << "method,line_weight,weight_mode,ratio,"
      << manumesh::simplification::statsHeaderCsv()
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
    const std::string label = method + "_r_" + sanitizeRatio(ratio) + "_w_" +
                              sanitizeWeight(options.lineWeight);
    const fs::path outStl = outDir / (label + ".stl");
    if (!manumesh::saveAsciiStl(outStl.string(), output, label, &error)) {
      throw std::runtime_error(error);
    }

    const manumesh::simplification::MeshStats stats =
        manumesh::simplification::computeMeshStats(output);
    const manumesh::simplification::DistanceStats distance =
        manumesh::simplification::compareMeshesBySampledDistance(input, output,
                                                                 samples);
    csv << method << "," << options.lineWeight << ","
        << manumesh::simplification::toString(options.weightMode) << "," << ratio << ","
        << manumesh::simplification::statsRowCsv(label, stats, &distance) << ","
        << report.collapsedEdges << "," << report.rejectedCollapses << ","
        << report.solverFallbacks << "," << report.minAppliedLineWeight << ","
        << report.maxAppliedLineWeight << "\n";
    printStats(label, stats);
  }

  std::cout << "Wrote ratio-sweep outputs to " << outDir << "\n";
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

  const fs::path outDir = positional[1];
  fs::create_directories(outDir);
  const int samples = getIntArg(args, "--samples", 3000);
  const std::vector<int> faceCounts = parseFaceCounts(
      getArg(args, "--faces", "1000,900,800,700,600,500,400,300,200,100"));
  manumesh::simplification::SimplifyOptions base = parseSimplifyOptions(args);

  std::ofstream csv(outDir / "metrics.csv");
  csv << "method,line_weight,weight_mode,target_faces,"
      << manumesh::simplification::statsHeaderCsv()
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
    const std::string label = method + "_f_" + std::to_string(targetFaces) + "_w_" +
                              sanitizeWeight(options.lineWeight);
    const fs::path outStl = outDir / (label + ".stl");
    if (!manumesh::saveAsciiStl(outStl.string(), output, label, &error)) {
      throw std::runtime_error(error);
    }

    const manumesh::simplification::MeshStats stats =
        manumesh::simplification::computeMeshStats(output);
    const manumesh::simplification::DistanceStats distance =
        manumesh::simplification::compareMeshesBySampledDistance(input, output,
                                                                 samples);
    csv << method << "," << options.lineWeight << ","
        << manumesh::simplification::toString(options.weightMode) << "," << targetFaces
        << "," << manumesh::simplification::statsRowCsv(label, stats, &distance) << ","
        << report.collapsedEdges << "," << report.rejectedCollapses << ","
        << report.solverFallbacks << "," << report.minAppliedLineWeight << ","
        << report.maxAppliedLineWeight << "\n";
    printStats(label, stats);
  }

  std::cout << "Wrote face-sweep outputs to " << outDir << "\n";
  return 0;
}

int commandSummarizeMetrics(const Args& args) {
  const auto positional = positionalArgs(args);
  const fs::path outputRoot =
      positional.empty() ? fs::path("output/demo") : fs::path(positional[0]);
  const fs::path summaryPath =
      positional.size() < 2 ? outputRoot / "demo_summary.csv" : fs::path(positional[1]);
  summarizeMetrics(outputRoot, summaryPath);
  return 0;
}

int commandDemo(const Args& args) {
  const fs::path inputDir = getArg(args, "--input-dir", "output/demo_input");
  const fs::path outputDir = getArg(args, "--output-dir", "output/demo");
  const fs::path flangeSource =
      getArg(args, "--flange-input", "tests/data/external/openfoam_flange.stl");
  const bool quick = hasFlag(args, "--quick");
  const std::string samples = getArg(args, "--samples", quick ? "500" : "1000");
  fs::create_directories(inputDir);
  fs::create_directories(outputDir);

  if (quick) {
    const fs::path flange =
        copyExternalInput(inputDir, "external_flange", flangeSource);
    runSweep(flange, outputDir / "external_flange_standard_budget",
             {"--method", "standard", "--ratio", "0.15", "--weights", "0", "--samples",
              samples});
    runSweep(flange, outputDir / "external_flange_line_budget",
             {"--method", "line", "--ratio", "0.15", "--weight-mode", "dihedral",
              "--feature-boost", "0.08", "--feature-angle-deg", "25", "--weights",
              "1e-4,1e-3,1e-2", "--samples", samples});
    runRatioSweep(flange, outputDir / "external_flange_ratio_dihedral",
                  {"--method", "line", "--line-weight", "1e-3", "--weight-mode",
                   "dihedral", "--feature-boost", "0.08", "--feature-angle-deg", "25",
                   "--ratios", "0.8,0.5,0.25,0.15,0.08,0.05", "--samples", samples});
    runFaceSweep(flange, outputDir / "external_flange_face_ladder",
                 {"--method", "line", "--line-weight", "1e-3", "--weight-mode",
                  "dihedral", "--feature-boost", "0.08", "--feature-angle-deg", "25",
                  "--faces", "1000,900,800,700,600,500,400,300,200,100", "--samples",
                  samples});
    summarizeMetrics(outputDir, outputDir / "demo_summary.csv");
    return 0;
  }

  runGenerate(inputDir, "clustered_plane", "clustered-plane", 60);
  runGenerate(inputDir, "hole_plane", "hole-plane", 60);
  runGenerate(inputDir, "ridge", "ridge", 60);
  runGenerate(inputDir, "noisy_plane", "noisy-plane", 60);
  runGenerate(inputDir, "sine_terrain", "sine-terrain", 56);
  runGenerate(inputDir, "terrace", "terrace", 56);
  runGenerate(inputDir, "bump", "bump", 56);
  runGenerate(inputDir, "cylinder", "cylinder", 48);
  runGenerate(inputDir, "torus", "torus", 48);
  runGenerate(inputDir, "cube", "cube", 45);
  runGenerate(inputDir, "thin_fin", "thin-fin", 48);
  const fs::path flange = copyExternalInput(inputDir, "external_flange", flangeSource);

  runSweep(
      inputDir / "clustered_plane.stl", outputDir / "clustered_plane",
      {"--ratio", "0.12", "--weights", "0,1e-5,1e-4,1e-3,1e-2", "--samples", samples});
  runSweep(inputDir / "clustered_plane.stl", outputDir / "clustered_plane_boundary",
           {"--ratio", "0.12", "--boundary-weight", "5", "--weights",
            "0,1e-5,1e-4,1e-3,1e-2", "--samples", samples});
  runSweep(inputDir / "hole_plane.stl", outputDir / "hole_plane_boundary",
           {"--ratio", "0.15", "--boundary-weight", "5", "--weights",
            "0,1e-4,1e-3,1e-2", "--samples", samples});
  runSweep(inputDir / "ridge.stl", outputDir / "ridge_uniform",
           {"--ratio", "0.12", "--weights", "0,1e-4,1e-3,1e-2", "--samples", samples});
  runSweep(inputDir / "ridge.stl", outputDir / "ridge_dihedral",
           {"--ratio", "0.12", "--weight-mode", "dihedral", "--feature-boost", "0.08",
            "--feature-angle-deg", "25", "--weights", "1e-3", "--samples", samples});
  runSweep(inputDir / "noisy_plane.stl", outputDir / "noisy_plane",
           {"--ratio", "0.12", "--weights", "0,1e-3,1e-2,1e-1", "--samples", samples});
  runSweep(
      inputDir / "sine_terrain.stl", outputDir / "sine_terrain",
      {"--ratio", "0.15", "--weights", "0,1e-4,1e-3,1e-2,1e-1", "--samples", samples});
  runSweep(inputDir / "terrace.stl", outputDir / "terrace_uniform",
           {"--ratio", "0.15", "--weights", "0,1e-4,1e-3,1e-2", "--samples", samples});
  runSweep(inputDir / "terrace.stl", outputDir / "terrace_dihedral",
           {"--ratio", "0.15", "--weight-mode", "dihedral", "--feature-boost", "0.08",
            "--feature-angle-deg", "20", "--weights", "1e-3", "--samples", samples});
  runSweep(inputDir / "bump.stl", outputDir / "bump_height",
           {"--ratio", "0.15", "--weight-mode", "height", "--feature-boost", "0.05",
            "--weights", "1e-4,1e-3,1e-2", "--samples", samples});
  runSweep(inputDir / "cylinder.stl", outputDir / "cylinder",
           {"--ratio", "0.18", "--boundary-weight", "2", "--weights",
            "0,1e-4,1e-3,1e-2", "--samples", samples});
  runSweep(inputDir / "torus.stl", outputDir / "torus",
           {"--ratio", "0.18", "--weights", "0,1e-4,1e-3,1e-2", "--samples", samples});
  runSweep(inputDir / "cube.stl", outputDir / "cube_dihedral",
           {"--ratio", "0.18", "--weight-mode", "dihedral", "--feature-boost", "0.08",
            "--feature-angle-deg", "25", "--weights", "1e-3", "--samples", samples});
  runSweep(inputDir / "thin_fin.stl", outputDir / "thin_fin_uniform",
           {"--ratio", "0.18", "--boundary-weight", "2", "--weights",
            "0,1e-4,1e-3,1e-2", "--samples", samples});
  runSweep(inputDir / "thin_fin.stl", outputDir / "thin_fin_dihedral",
           {"--ratio", "0.18", "--boundary-weight", "2", "--weight-mode", "dihedral",
            "--feature-boost", "0.1", "--feature-angle-deg", "20", "--weights", "1e-3",
            "--samples", samples});

  runSweep(flange, outputDir / "external_flange_standard_budget",
           {"--method", "standard", "--ratio", "0.15", "--weights", "0", "--samples",
            samples});
  runSweep(flange, outputDir / "external_flange_line_budget",
           {"--method", "line", "--ratio", "0.15", "--weight-mode", "dihedral",
            "--feature-boost", "0.08", "--feature-angle-deg", "25", "--weights",
            "1e-4,1e-3,1e-2", "--samples", samples});
  runRatioSweep(inputDir / "sine_terrain.stl", outputDir / "sine_terrain_ratio_line",
                {"--method", "line", "--line-weight", "1e-3", "--ratios",
                 "0.8,0.5,0.25,0.15,0.08,0.05", "--samples", samples});
  runRatioSweep(inputDir / "ridge.stl", outputDir / "ridge_ratio_line",
                {"--method", "line", "--line-weight", "1e-3", "--ratios",
                 "0.8,0.5,0.25,0.15,0.08,0.05", "--samples", samples});
  runRatioSweep(inputDir / "cube.stl", outputDir / "cube_ratio_dihedral",
                {"--method", "line", "--line-weight", "1e-3", "--weight-mode",
                 "dihedral", "--feature-boost", "0.08", "--feature-angle-deg", "25",
                 "--ratios", "0.8,0.5,0.25,0.15,0.08,0.05", "--samples", samples});
  runRatioSweep(flange, outputDir / "external_flange_ratio_dihedral",
                {"--method", "line", "--line-weight", "1e-3", "--weight-mode",
                 "dihedral", "--feature-boost", "0.08", "--feature-angle-deg", "25",
                 "--ratios", "0.8,0.5,0.25,0.15,0.08,0.05", "--samples", samples});
  runFaceSweep(flange, outputDir / "external_flange_face_ladder",
               {"--method", "line", "--line-weight", "1e-3", "--weight-mode",
                "dihedral", "--feature-boost", "0.08", "--feature-angle-deg", "25",
                "--faces", "1000,900,800,700,600,500,400,300,200,100", "--samples",
                samples});

  summarizeMetrics(outputDir, outputDir / "demo_summary.csv");
  return 0;
}

int commandValidateFeatures(const Args& args) {
  struct CaseSpec {
    std::string name;
    fs::path externalInput;
  };
  const fs::path inputDir =
      getArg(args, "--input-dir", "tests/output/generated_inputs");
  const fs::path outDir =
      getArg(args, "--output-dir", "tests/output/feature_curve_validation");
  const fs::path spindleSource =
      getArg(args, "--spindle-input",
             "tests/data/external/thingi10k/"
             "thingi10k_79361_zheng3_tinkeriffic_40mm_spool_spindle.stl");
  const fs::path ringSource = getArg(
      args, "--ring-input", "tests/data/external/nasa_antenna_azimuth_track.stl");
  const fs::path pulleySource =
      getArg(args, "--pulley-input",
             "tests/data/external/thingi10k/thingi10k_318045_moko_mini_pulley.stl");
  const fs::path flangeSource =
      getArg(args, "--flange-input", "tests/data/external/openfoam_flange.stl");
  const std::string ratio = getArg(args, "--ratio", "0.20");
  const std::string samples = getArg(args, "--samples", "1000");
  const std::vector<CaseSpec> cases = {
      {"external_spindle", spindleSource},
      {"external_ring_track", ringSource},
      {"external_pulley", pulleySource},
      {"external_flange", flangeSource},
  };
  fs::create_directories(inputDir);
  fs::create_directories(outDir);

  for (const CaseSpec& spec : cases) {
    const fs::path input = copyExternalInput(inputDir, spec.name, spec.externalInput);
    runFeatureReport(input, {"--feature-angle-deg", "25", "--circle-fit-threshold",
                             "0.04", "--min-feature-loop-vertices", "8", "--csv",
                             pathString(outDir / (spec.name + "_features.csv"))});

    const fs::path lineOut = outDir / (spec.name + "_line.stl");
    const fs::path curveOut = outDir / (spec.name + "_curve.stl");
    runSimplify(input, lineOut,
                {"--method", "line", "--ratio", ratio, "--line-weight", "1e-3",
                 "--weight-mode", "dihedral", "--feature-boost", "0.08",
                 "--feature-angle-deg", "25", "--samples", samples, "--metrics-csv",
                 pathString(outDir / (spec.name + "_line_metrics.csv"))});
    runSimplify(input, curveOut,
                {"--method",
                 "line",
                 "--ratio",
                 ratio,
                 "--line-weight",
                 "1e-3",
                 "--weight-mode",
                 "dihedral",
                 "--feature-boost",
                 "0.08",
                 "--feature-angle-deg",
                 "25",
                 "--preserve-feature-curves",
                 "--feature-curve-weight",
                 "0.08",
                 "--circle-fit-threshold",
                 "0.04",
                 "--min-feature-loop-vertices",
                 "16",
                 "--samples",
                 samples,
                 "--metrics-csv",
                 pathString(outDir / (spec.name + "_curve_metrics.csv"))});
    runFeatureCompare(input, lineOut,
                      {"--feature-angle-deg", "25", "--circle-fit-threshold", "0.04",
                       "--min-feature-loop-vertices", "8", "--csv",
                       pathString(outDir / (spec.name + "_line_feature_compare.csv"))});
    runFeatureCompare(
        input, curveOut,
        {"--feature-angle-deg", "25", "--circle-fit-threshold", "0.04",
         "--min-feature-loop-vertices", "8", "--csv",
         pathString(outDir / (spec.name + "_curve_feature_compare.csv"))});
  }

  std::cout << "Feature validation outputs written to " << outDir << "\n";
  return 0;
}

int commandValidateExternal(const Args& args) {
  struct ExternalSpec {
    std::string name;
    std::vector<std::string> candidates;
    std::string notes;
  };
  const fs::path inputDir =
      getArg(args, "--input-dir", "tests/data/external/common_3d_test_models");
  const fs::path outDir =
      getArg(args, "--output-dir", "tests/output/external_model_validation");
  const std::string ratio = getArg(args, "--ratio", "0.25");
  const std::string samples = getArg(args, "--samples", "800");
  const std::vector<ExternalSpec> models = {
      {"fandisk", {"fandisk.obj"}, "CAD-ish benchmark with hard non-circular features"},
      {"rocker_arm",
       {"rocker_arm.obj", "rocker-arm.obj"},
       "mechanical scan with holes and irregular tessellation"},
      {"beetle", {"beetle.obj"}, "small mixed smooth/sharp organic-style model"},
      {"cow",
       {"cow.obj"},
       "organic model; useful for checking false circular features"},
      {"suzanne",
       {"suzanne.obj"},
       "low-poly hard-edged mesh without true circular CAD loops"},
  };
  fs::create_directories(outDir);
  const fs::path summaryPath = outDir / "external_summary.csv";
  std::ofstream summary(summaryPath);
  summary << "model,notes,input_path,line_output,curve_output,input_faces,line_faces,"
             "curve_faces,line_matched,line_missing,curve_matched,curve_missing,"
             "line_rejected_collapses,curve_rejected_collapses\n";

  int processed = 0;
  for (const ExternalSpec& model : models) {
    fs::path input;
    for (const std::string& candidate : model.candidates) {
      const fs::path path = inputDir / candidate;
      if (fs::exists(path)) {
        input = path;
        break;
      }
    }
    if (input.empty()) {
      std::cout << "Skipping " << model.name << ": OBJ not found in " << inputDir
                << "\n";
      continue;
    }

    ++processed;
    runFeatureReport(input, {"--feature-angle-deg", "35", "--circle-fit-threshold",
                             "0.05", "--min-feature-loop-vertices", "8", "--csv",
                             pathString(outDir / (model.name + "_features.csv"))});
    const fs::path lineOut = outDir / (model.name + "_line.stl");
    const fs::path curveOut = outDir / (model.name + "_curve.stl");
    const fs::path lineMetrics = outDir / (model.name + "_line_metrics.csv");
    const fs::path curveMetrics = outDir / (model.name + "_curve_metrics.csv");
    const fs::path lineCompare = outDir / (model.name + "_line_feature_compare.csv");
    const fs::path curveCompare = outDir / (model.name + "_curve_feature_compare.csv");

    runSimplify(input, lineOut,
                {"--method", "line", "--ratio", ratio, "--line-weight", "1e-3",
                 "--weight-mode", "dihedral", "--feature-boost", "0.08",
                 "--feature-angle-deg", "35", "--samples", samples, "--metrics-csv",
                 pathString(lineMetrics)});
    runSimplify(input, curveOut,
                {"--method",
                 "line",
                 "--ratio",
                 ratio,
                 "--line-weight",
                 "1e-3",
                 "--weight-mode",
                 "dihedral",
                 "--feature-boost",
                 "0.08",
                 "--feature-angle-deg",
                 "35",
                 "--preserve-feature-curves",
                 "--feature-curve-weight",
                 "0.05",
                 "--circle-fit-threshold",
                 "0.05",
                 "--min-feature-loop-vertices",
                 "12",
                 "--samples",
                 samples,
                 "--metrics-csv",
                 pathString(curveMetrics)});
    runFeatureCompare(input, lineOut,
                      {"--feature-angle-deg", "35", "--circle-fit-threshold", "0.05",
                       "--min-feature-loop-vertices", "8", "--csv",
                       pathString(lineCompare)});
    runFeatureCompare(input, curveOut,
                      {"--feature-angle-deg", "35", "--circle-fit-threshold", "0.05",
                       "--min-feature-loop-vertices", "8", "--csv",
                       pathString(curveCompare)});

    manumesh::Mesh mesh;
    std::string error;
    if (!manumesh::loadMesh(input.string(), mesh, &error)) {
      throw std::runtime_error(error);
    }
    const auto lineMetricRow = readFirstCsvRow(lineMetrics);
    const auto curveMetricRow = readFirstCsvRow(curveMetrics);
    const auto lineCompareRow = readFirstCsvRow(lineCompare);
    const auto curveCompareRow = readFirstCsvRow(curveCompare);
    const std::vector<std::string> fields = {
        model.name,
        model.notes,
        input.generic_string(),
        lineOut.generic_string(),
        curveOut.generic_string(),
        std::to_string(mesh.faces.size()),
        csvValue(lineMetricRow, "faces"),
        csvValue(curveMetricRow, "faces"),
        csvValue(lineCompareRow, "matched"),
        csvValue(lineCompareRow, "missing"),
        csvValue(curveCompareRow, "matched"),
        csvValue(curveCompareRow, "missing"),
        csvValue(lineMetricRow, "rejected_collapses"),
        csvValue(curveMetricRow, "rejected_collapses"),
    };
    for (std::size_t i = 0; i < fields.size(); ++i) {
      if (i > 0) summary << ",";
      summary << quoteCsv(fields[i]);
    }
    summary << "\n";
  }

  if (processed == 0) {
    throw std::runtime_error(
        "No external OBJ files found. Place common-3d-test-models OBJ files in " +
        inputDir.string() + " first.");
  }
  std::cout << "External validation outputs written to " << outDir << "\n";
  return 0;
}

const std::map<std::string, manumesh::cli::CommandHandler>& registeredCommands() {
  static const std::map<std::string, manumesh::cli::CommandHandler> commands = {
      {"compare", commandCompare},
      {"demo", commandDemo},
      {"face-sweep", commandFaceSweep},
      {"feature-compare", commandFeatureCompare},
      {"feature-report", commandFeatureReport},
      {"generate", commandGenerate},
      {"ratio-sweep", commandRatioSweep},
      {"simplify", commandSimplify},
      {"summarize-metrics", commandSummarizeMetrics},
      {"sweep", commandSweep},
      {"validate-external", commandValidateExternal},
      {"validate-features", commandValidateFeatures},
  };
  return commands;
}

} // namespace

namespace manumesh::cli {

const std::map<std::string, CommandHandler>& commandRegistry() {
  return registeredCommands();
}

} // namespace manumesh::cli
