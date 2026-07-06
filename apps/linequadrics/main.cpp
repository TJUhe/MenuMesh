#include "line_quadrics_qem/algorithms/simplification/Metrics.h"
#include "line_quadrics_qem/algorithms/simplification/QEMSimplifier.h"
#include "line_quadrics_qem/core/Mesh.h"
#include "line_quadrics_qem/core/MeshGenerators.h"
#include "line_quadrics_qem/features/FeatureDetection.h"

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

constexpr double kPi = 3.141592653589793238462643383279502884;

struct Args {
  std::vector<std::string> values;
};

bool hasFlag(const Args& args, const std::string& name) {
  for (const std::string& value : args.values) {
    if (value == name) return true;
  }
  return false;
}

const std::unordered_set<std::string>& valueFlags() {
  static const std::unordered_set<std::string> flags = {
      "--type",
      "--n",
      "--out",
      "--ratio",
      "--target-faces",
      "--line-weight",
      "--weight-mode",
      "--feature-boost",
      "--feature-angle-deg",
      "--adaptive-base-line-weight",
      "--boundary-weight",
      "--feature-protection-mode",
      "--feature-curve-weight",
      "--max-feature-curve-deviation-ratio",
      "--circle-fit-threshold",
      "--ellipse-fit-threshold",
      "--near-circle-axis-ratio-tolerance",
      "--min-feature-loop-vertices",
      "--min-circular-feature-loop-vertices",
      "--normal-tensor-threshold",
      "--normal-tensor-edge-alignment",
      "--normal-tensor-smoothing",
      "--normal-tensor-scales",
      "--min-triangle-quality",
      "--max-normal-deviation-deg",
      "--max-local-error",
      "--max-local-error-ratio",
      "--metrics-csv",
      "--samples",
      "--ratios",
      "--faces",
      "--weights",
      "--csv",
      "--input-dir",
      "--output-dir",
      "--method",
  };
  return flags;
}

const std::unordered_set<std::string>& switchFlags() {
  static const std::unordered_set<std::string> flags = {
      "--verbose",
      "--adaptive-scale",
      "--preserve-boundary",
      "--prevent-local-intersections",
      "--preserve-feature-curves",
      "--protect-all-feature-edges",
      "--industrial-safe",
      "--no-normal-tensor-features",
      "--quick",
  };
  return flags;
}

bool isKnownFlag(const std::string& value) {
  return valueFlags().find(value) != valueFlags().end() ||
         switchFlags().find(value) != switchFlags().end();
}

bool takesValue(const std::string& value) {
  return valueFlags().find(value) != valueFlags().end();
}

void validateArgs(const Args& args) {
  for (std::size_t i = 0; i < args.values.size(); ++i) {
    const std::string& value = args.values[i];
    if (value.empty() || value[0] != '-') {
      continue;
    }
    if (!isKnownFlag(value)) {
      throw std::invalid_argument("Unknown option: " + value);
    }
    if (!takesValue(value)) {
      continue;
    }
    if (i + 1 >= args.values.size() || isKnownFlag(args.values[i + 1])) {
      throw std::invalid_argument(value + " requires a value.");
    }
    ++i;
  }
}

std::string getArg(const Args& args, const std::string& name,
                   const std::string& defaultValue = "") {
  for (std::size_t i = 0; i + 1 < args.values.size(); ++i) {
    if (args.values[i] == name) {
      if (isKnownFlag(args.values[i + 1])) {
        throw std::invalid_argument(name + " requires a value.");
      }
      return args.values[i + 1];
    }
  }
  if (!args.values.empty() && args.values.back() == name) {
    throw std::invalid_argument(name + " requires a value.");
  }
  return defaultValue;
}

int parseIntStrict(const std::string& value, const std::string& name) {
  try {
    std::size_t parsed = 0;
    const int result = std::stoi(value, &parsed);
    if (parsed != value.size()) {
      throw std::invalid_argument("");
    }
    return result;
  } catch (const std::exception&) {
    throw std::invalid_argument(name + " must be an integer.");
  }
}

double parseDoubleStrict(const std::string& value, const std::string& name) {
  try {
    std::size_t parsed = 0;
    const double result = std::stod(value, &parsed);
    if (parsed != value.size() || !std::isfinite(result)) {
      throw std::invalid_argument("");
    }
    return result;
  } catch (const std::exception&) {
    throw std::invalid_argument(name + " must be a finite number.");
  }
}

int getIntArg(const Args& args, const std::string& name, int defaultValue) {
  const std::string value = getArg(args, name);
  return value.empty() ? defaultValue : parseIntStrict(value, name);
}

double getDoubleArg(const Args& args, const std::string& name, double defaultValue) {
  const std::string value = getArg(args, name);
  return value.empty() ? defaultValue : parseDoubleStrict(value, name);
}

std::vector<std::string> positionalArgs(const Args& args) {
  std::vector<std::string> result;
  for (std::size_t i = 0; i < args.values.size(); ++i) {
    const std::string& value = args.values[i];
    if (!value.empty() && value[0] == '-') {
      if (takesValue(value) && i + 1 < args.values.size()) {
        ++i;
      }
      continue;
    }
    result.push_back(value);
  }
  return result;
}

std::vector<double> parseWeights(const std::string& text) {
  std::vector<double> weights;
  std::stringstream ss(text);
  std::string item;
  while (std::getline(ss, item, ',')) {
    if (!item.empty()) {
      weights.push_back(parseDoubleStrict(item, "--weights"));
    }
  }
  return weights;
}

std::vector<int> parseFaceCounts(const std::string& text) {
  std::vector<int> counts;
  std::stringstream ss(text);
  std::string item;
  while (std::getline(ss, item, ',')) {
    if (!item.empty()) {
      counts.push_back(parseIntStrict(item, "--faces"));
    }
  }
  return counts;
}

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

lq::SimplifyOptions parseSimplifyOptions(const Args& args) {
  lq::SimplifyOptions options;
  options.targetRatio = getDoubleArg(args, "--ratio", options.targetRatio);
  options.targetFaces = getIntArg(args, "--target-faces", options.targetFaces);
  options.lineWeight = getDoubleArg(args, "--line-weight", options.lineWeight);
  options.featureBoost = getDoubleArg(args, "--feature-boost", options.featureBoost);
  options.featureAngleDeg =
      getDoubleArg(args, "--feature-angle-deg", options.featureAngleDeg);
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
  options.featureProtectionMode = lq::parseFeatureProtectionMode(
      getArg(args, "--feature-protection-mode",
             lq::toString(options.featureProtectionMode)));
  options.protectAllFeatureEdges = hasFlag(args, "--protect-all-feature-edges");
  options.useNormalTensorFeatures = !hasFlag(args, "--no-normal-tensor-features");
  if (hasFlag(args, "--industrial-safe")) {
    options.preserveBoundary = true;
    options.minTriangleQuality = std::max(options.minTriangleQuality, 1e-4);
    options.maxNormalDeviationDeg = std::min(options.maxNormalDeviationDeg, 75.0);
    options.maxLocalErrorRatio = std::max(options.maxLocalErrorRatio, 0.02);
    options.preventLocalIntersections = true;
  }

  const std::string mode = getArg(args, "--weight-mode", "uniform");
  options.weightMode = lq::parseWeightMode(mode);

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

lq::FeatureOptions parseFeatureOptions(const Args& args) {
  lq::FeatureOptions options;
  options.featureAngleDeg =
      getDoubleArg(args, "--feature-angle-deg", options.featureAngleDeg);
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
  options.useNormalTensorFeatures = !hasFlag(args, "--no-normal-tensor-features");
  return options;
}

void printUsage() {
  std::cout
      << "Line Quadrics QEM reproduction\n\n"
      << "Commands:\n"
      << "  linequadrics generate --type clustered-plane --n 50 --out input.stl\n"
      << "  linequadrics simplify input.stl output.stl [options]\n"
      << "  linequadrics compare original.stl simplified.stl [--samples 3000]\n"
      << "  linequadrics feature-report input.stl [--csv report.csv]\n"
      << "  linequadrics feature-compare original.stl simplified.stl [--csv "
         "report.csv]\n"
      << "  linequadrics sweep input.stl out_dir [options]\n\n"
      << "  linequadrics ratio-sweep input.stl out_dir [options]\n\n"
      << "  linequadrics face-sweep input.stl out_dir [options]\n\n"
      << "  linequadrics demo [--quick] [--samples N]\n\n"
      << "  linequadrics summarize-metrics [output_root] [summary.csv]\n\n"
      << "  linequadrics validate-features [--ratio R] [--samples N] "
         "[--input-dir dir] [--output-dir dir]\n\n"
      << "  linequadrics validate-external [--input-dir dir] [--ratio R] "
         "[--output-dir dir]\n\n"
      << "Simplify options:\n"
      << "  --method standard|line          Standard QEM or line-quadric QEM\n"
      << "  --ratio 0.25                    Target face ratio\n"
      << "  --target-faces N                Overrides --ratio\n"
      << "  --line-weight W                 Paper default is around 1e-3\n"
      << "  --weight-mode uniform|dihedral|normal-tensor|height|xband\n"
      << "  --feature-boost W               Added line weight for feature modes\n"
      << "  --feature-angle-deg A           Dihedral threshold for feature mode\n"
      << "  --adaptive-scale                Add small line quadrics then scale Q\n"
      << "  --boundary-weight W             Optional boundary plane quadrics\n"
      << "  --preserve-boundary             Preserve open boundary topology\n"
      << "  --preserve-feature-curves       Protect detected crease/boundary loops\n"
      << "  --feature-protection-mode none|circular-only|primitive-curves|"
         "all-feature-edges\n"
      << "                                  Hard policy for detected features; "
         "default primitive-curves\n"
      << "  --protect-all-feature-edges     Compatibility alias for "
         "all-feature-edges\n"
      << "  --feature-curve-weight W        Tangent-line quadric weight for loops\n"
      << "  --max-feature-curve-deviation-ratio R  Reject polygonal feature "
         "collapses whose raw placement drifts beyond R*bbox_diag\n"
      << "  --circle-fit-threshold R        Relative fit threshold for circular loops\n"
      << "  --ellipse-fit-threshold R       Relative fit threshold for ellipse "
         "reports\n"
      << "  --near-circle-axis-ratio-tolerance R  Axis-ratio tolerance for "
         "near-circles\n"
      << "  --min-feature-loop-vertices N   Stop collapsing a loop below N vertices\n"
      << "  --min-circular-feature-loop-vertices N  Stop circular loops below N\n"
      << "  --normal-tensor-threshold S     Feature score threshold for tensor edges\n"
      << "  --normal-tensor-edge-alignment A Minimum edge/tangent alignment\n"
      << "  --normal-tensor-smoothing N     Optional tensor smoothing iterations\n"
      << "  --normal-tensor-scales N        Number of tensor smoothing scales\n"
      << "  --no-normal-tensor-features     Disable tensor candidates in feature "
         "detection\n"
      << "  --min-triangle-quality Q        Reject collapses below quality Q in [0,1]\n"
      << "  --max-normal-deviation-deg A    Reject local face normal changes above A\n"
      << "  --max-local-error D             Reject local collapse drift above D\n"
      << "  --max-local-error-ratio R       Reject local drift above R*bbox diagonal\n"
      << "  --prevent-local-intersections   Reject local triangle intersections\n"
      << "  --industrial-safe               Enable conservative boundary/quality "
         "guards\n"
      << "  --metrics-csv path              Write one-row CSV metrics\n"
      << "  --samples N                     Distance sample count\n"
      << "  --ratios list                   For ratio-sweep, e.g. 0.8,0.5,0.25,0.1\n"
      << "  --faces list                    For face-sweep, e.g. 1000,900,800\n"
      << "  --spindle-input path            External spindle/shaft STL for "
         "validate-features\n"
      << "  --ring-input path               External ring/track STL for "
         "validate-features\n"
      << "  --pulley-input path             External pulley STL for "
         "validate-features\n"
      << "  --flange-input path             External finished flange STL for demo/"
         "validate-features\n";
  std::cout << "\nGenerator types:\n"
            << "  plane, clustered-plane, hole-plane, ridge, noisy-plane,\n"
            << "  sine-terrain, terrace, bump, cylinder, torus, cube, thin-fin,\n"
            << "  stepped-shaft, pipe-coupling, pulley\n";
}

void printStats(const std::string& label, const lq::MeshStats& stats) {
  std::cout << label << ": vertices=" << stats.vertices << " faces=" << stats.faces
            << " mean_quality=" << stats.meanTriangleQuality
            << " min_quality=" << stats.minTriangleQuality
            << " edge_cv=" << stats.edgeLengthCv << "\n";
}

int commandGenerate(const Args& args);
int commandSimplify(const Args& args);
int commandFeatureReport(const Args& args);
int commandFeatureCompare(const Args& args);
int commandSweep(const Args& args);
int commandRatioSweep(const Args& args);
int commandFaceSweep(const Args& args);

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
    throw std::runtime_error("Failed to copy external input from " +
                             source.string() + " to " +
                             destination.string() + ": " + ec.message());
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

  lq::Mesh mesh;
  std::string error;
  if (!lq::generateMeshByName(type, n, mesh, &error)) {
    throw std::runtime_error(error);
  }
  if (!lq::saveAsciiStl(outPath, mesh, type, &error)) {
    throw std::runtime_error(error);
  }

  printStats(type, lq::computeMeshStats(mesh));
  std::cout << "Wrote " << outPath << "\n";
  return 0;
}

int commandCompare(const Args& args) {
  const auto positional = positionalArgs(args);
  if (positional.size() < 2) {
    throw std::invalid_argument("compare requires original.stl simplified.stl.");
  }
  const int samples = getIntArg(args, "--samples", 3000);

  lq::Mesh original;
  lq::Mesh simplified;
  std::string error;
  if (!lq::loadMesh(positional[0], original, &error)) throw std::runtime_error(error);
  if (!lq::loadMesh(positional[1], simplified, &error)) {
    throw std::runtime_error(error);
  }

  const lq::MeshStats stats = lq::computeMeshStats(simplified);
  const lq::DistanceStats distance =
      lq::compareMeshesBySampledDistance(original, simplified, samples);
  printStats("simplified", stats);
  std::cout << "distance mean original->simplified="
            << distance.meanOriginalToSimplified
            << " max=" << distance.maxOriginalToSimplified << "\n";
  std::cout << "distance mean simplified->original="
            << distance.meanSimplifiedToOriginal
            << " max=" << distance.maxSimplifiedToOriginal << "\n";
  return 0;
}

int countCircularLoops(const lq::FeatureAnalysis& analysis) {
  int count = 0;
  for (const lq::FeatureLoop& loop : analysis.loops) {
    if (loop.circular) {
      ++count;
    }
  }
  return count;
}

int countPrimitiveLoops(const lq::FeatureAnalysis& analysis,
                        lq::FeaturePrimitiveType primitive) {
  int count = 0;
  for (const lq::FeatureLoop& loop : analysis.loops) {
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

  lq::Mesh input;
  std::string error;
  if (!lq::loadMesh(positional[0], input, &error)) {
    throw std::runtime_error(error);
  }

  const lq::FeatureOptions options = parseFeatureOptions(args);
  const lq::FeatureAnalysis analysis = lq::detectFeatureCurves(input, options);
  const int circularLoops = countCircularLoops(analysis);
  const int circleLoops =
      countPrimitiveLoops(analysis, lq::FeaturePrimitiveType::Circle);
  const int nearCircleLoops =
      countPrimitiveLoops(analysis, lq::FeaturePrimitiveType::NearCircle);
  const int ellipseLoops =
      countPrimitiveLoops(analysis, lq::FeaturePrimitiveType::Ellipse);
  const int polygonalLoops =
      countPrimitiveLoops(analysis, lq::FeaturePrimitiveType::PolygonalLoop);
  std::cout << "feature_edges=" << analysis.featureEdges
            << " boundary_edges=" << analysis.boundaryFeatureEdges
            << " dihedral_edges=" << analysis.dihedralFeatureEdges
            << " normal_tensor_edges=" << analysis.normalTensorFeatureEdges
            << " non_manifold_edges=" << analysis.nonManifoldFeatureEdges
            << " convex_edges=" << analysis.convexFeatureEdges
            << " concave_edges=" << analysis.concaveFeatureEdges
            << " unknown_signed_edges=" << analysis.unknownSignedFeatureEdges
            << " max_normal_tensor_score=" << analysis.maxNormalTensorFeatureScore
            << " loops=" << analysis.loops.size() << " circular_loops=" << circularLoops
            << " circle_loops=" << circleLoops
            << " near_circle_loops=" << nearCircleLoops
            << " ellipse_loops=" << ellipseLoops
            << " polygonal_loops=" << polygonalLoops << "\n";
  std::cout << lq::featureReportHeaderCsv() << "\n";
  for (const lq::FeatureLoop& loop : analysis.loops) {
    std::cout << lq::featureLoopRowCsv(loop) << "\n";
  }

  const std::string csvPath = getArg(args, "--csv");
  if (!csvPath.empty()) {
    const fs::path output(csvPath);
    if (output.has_parent_path()) {
      fs::create_directories(output.parent_path());
    }
    std::ofstream csv(csvPath);
    csv << "feature_edges,boundary_edges,dihedral_edges,normal_tensor_edges,"
           "non_manifold_edges,convex_edges,concave_edges,unknown_signed_edges,"
           "max_normal_tensor_score,loops,circular_loops,circle_loops,"
           "near_circle_loops,ellipse_loops,polygonal_loops\n";
    csv << analysis.featureEdges << "," << analysis.boundaryFeatureEdges << ","
        << analysis.dihedralFeatureEdges << "," << analysis.normalTensorFeatureEdges
        << "," << analysis.nonManifoldFeatureEdges << "," << analysis.convexFeatureEdges
        << "," << analysis.concaveFeatureEdges << ","
        << analysis.unknownSignedFeatureEdges << ","
        << analysis.maxNormalTensorFeatureScore << "," << analysis.loops.size() << ","
        << circularLoops << "," << circleLoops << "," << nearCircleLoops << ","
        << ellipseLoops << "," << polygonalLoops << "\n\n";
    csv << lq::featureReportHeaderCsv() << "\n";
    for (const lq::FeatureLoop& loop : analysis.loops) {
      csv << lq::featureLoopRowCsv(loop) << "\n";
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

  lq::Mesh original;
  lq::Mesh simplified;
  std::string error;
  if (!lq::loadMesh(positional[0], original, &error)) {
    throw std::runtime_error(error);
  }
  if (!lq::loadMesh(positional[1], simplified, &error)) {
    throw std::runtime_error(error);
  }

  const lq::FeatureOptions options = parseFeatureOptions(args);
  const lq::FeatureAnalysis originalFeatures =
      lq::detectFeatureCurves(original, options);
  const lq::FeatureAnalysis simplifiedFeatures =
      lq::detectFeatureCurves(simplified, options);

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
    const lq::FeatureLoop& origLoop = originalFeatures.loops[originalId];
    int bestLoopId = -1;
    double bestScore = std::numeric_limits<double>::infinity();
    double bestCenterError = 0.0;
    double bestRadiusError = 0.0;
    double bestNormalAngleDeg = 0.0;

    for (int simplifiedId : simplifiedCircular) {
      if (usedSimplified[simplifiedId]) {
        continue;
      }
      const lq::FeatureLoop& simpLoop = simplifiedFeatures.loops[simplifiedId];
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
    lq::DirectionalCurveError directional;
    int simplifiedVertexCount = 0;
    double simplifiedRadius = 0.0;
    bool plausibleMatch = false;
    if (bestLoopId >= 0) {
      const lq::FeatureLoop& simpLoop = simplifiedFeatures.loops[bestLoopId];
      const double radiusRel = bestRadiusError / std::max(1e-12, origLoop.radius);
      plausibleMatch = bestCenterError <= 0.08 * diag && radiusRel <= 0.20 &&
                       bestNormalAngleDeg <= 30.0;
      if (plausibleMatch) {
        usedSimplified[bestLoopId] = 1;
        directional = lq::measureLoopAgainstCircle(
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
  lq::SimplifyOptions options = parseSimplifyOptions(args);

  lq::Mesh input;
  std::string error;
  if (!lq::loadMesh(positional[0], input, &error)) {
    throw std::runtime_error(error);
  }

  lq::SimplifyReport report;
  lq::QEMSimplifier simplifier(options);
  lq::Mesh output = simplifier.simplify(input, &report);
  if (!lq::saveAsciiStl(positional[1], output, "linequadrics", &error)) {
    throw std::runtime_error(error);
  }

  const lq::MeshStats inStats = lq::computeMeshStats(input);
  const lq::MeshStats outStats = lq::computeMeshStats(output);
  const lq::DistanceStats distance =
      lq::compareMeshesBySampledDistance(input, output, samples);

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
            << " solver_fallbacks=" << report.solverFallbacks
            << " termination=" << lq::toString(report.terminationReason)
            << " line_weight_range=[" << report.minAppliedLineWeight << ", "
            << report.maxAppliedLineWeight << "]\n";
  if (options.preserveFeatureCurves) {
    std::cout << "feature_loops=" << report.featureLoops
              << " circular_feature_loops=" << report.circularFeatureLoops
              << " feature_vertices=" << report.featureVertices
              << " feature_protection_mode="
              << lq::toString(options.protectAllFeatureEdges
                                  ? lq::FeatureProtectionMode::AllFeatureEdges
                                  : options.featureProtectionMode)
              << " normal_tensor_feature_edges=" << report.normalTensorFeatureEdges
              << " feature_rejected=" << report.featureRejectedCollapses
              << " primitive_feature_rejected="
              << report.primitiveFeatureRejectedCollapses
              << " generic_feature_rejected="
              << report.genericFeatureRejectedCollapses
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
    csv << lq::statsHeaderCsv()
        << ",collapsed_edges,rejected_collapses,solver_fallbacks,"
           "feature_loops,circular_feature_loops,feature_vertices,"
           "normal_tensor_feature_edges,feature_protection_mode,"
           "feature_rejected_collapses,boundary_rejected_collapses,"
           "primitive_feature_rejected_collapses,"
           "generic_feature_rejected_collapses,"
           "topology_rejected_collapses,normal_flip_rejected_collapses,"
           "quality_rejected_collapses,self_intersection_rejected_collapses,"
           "curve_budget_rejected_collapses,error_rejected_collapses,"
           "projected_feature_placements,termination_reason,"
           "min_line_weight,max_line_weight\n";
    csv << lq::statsRowCsv("output", outStats, &distance) << ","
        << report.collapsedEdges << "," << report.rejectedCollapses << ","
        << report.solverFallbacks << "," << report.featureLoops << ","
        << report.circularFeatureLoops << "," << report.featureVertices << ","
        << report.normalTensorFeatureEdges << ","
        << lq::toString(options.protectAllFeatureEdges
                            ? lq::FeatureProtectionMode::AllFeatureEdges
                            : options.featureProtectionMode)
        << "," << report.featureRejectedCollapses
        << "," << report.boundaryRejectedCollapses << ","
        << report.primitiveFeatureRejectedCollapses << ","
        << report.genericFeatureRejectedCollapses << ","
        << report.topologyRejectedCollapses << "," << report.normalFlipRejectedCollapses
        << "," << report.qualityRejectedCollapses << ","
        << report.selfIntersectionRejectedCollapses << ","
        << report.curveBudgetRejectedCollapses << "," << report.errorRejectedCollapses
        << "," << report.projectedFeaturePlacements << ","
        << lq::toString(report.terminationReason) << "," << report.minAppliedLineWeight
        << "," << report.maxAppliedLineWeight << "\n";
  }

  std::cout << "Wrote " << positional[1] << "\n";
  return 0;
}

int commandSweep(const Args& args) {
  const auto positional = positionalArgs(args);
  if (positional.size() < 2) {
    throw std::invalid_argument("sweep requires input.stl out_dir.");
  }

  lq::Mesh input;
  std::string error;
  if (!lq::loadMesh(positional[0], input, &error)) {
    throw std::runtime_error(error);
  }

  const fs::path outDir = positional[1];
  fs::create_directories(outDir);
  const int samples = getIntArg(args, "--samples", 3000);
  const std::vector<double> weights =
      parseWeights(getArg(args, "--weights", "0,1e-5,1e-4,1e-3,1e-2,1e-1"));
  lq::SimplifyOptions base = parseSimplifyOptions(args);

  std::ofstream csv(outDir / "metrics.csv");
  csv << "method,line_weight,weight_mode," << lq::statsHeaderCsv()
      << ",collapsed_edges,rejected_collapses,solver_fallbacks,"
         "min_line_weight,max_line_weight\n";

  for (double weight : weights) {
    lq::SimplifyOptions options = base;
    options.lineWeight = weight;
    options.useLineQuadrics =
        weight > 0.0 || options.weightMode != lq::WeightMode::Uniform;
    if (weight <= 0.0 && options.weightMode == lq::WeightMode::Uniform) {
      options.useLineQuadrics = false;
    }

    lq::SimplifyReport report;
    lq::QEMSimplifier simplifier(options);
    lq::Mesh output = simplifier.simplify(input, &report);
    const std::string method = options.useLineQuadrics ? "line" : "standard";
    const std::string label = method + "_w_" + sanitizeWeight(weight);
    const fs::path outStl = outDir / (label + ".stl");
    if (!lq::saveAsciiStl(outStl.string(), output, label, &error)) {
      throw std::runtime_error(error);
    }

    const lq::MeshStats stats = lq::computeMeshStats(output);
    const lq::DistanceStats distance =
        lq::compareMeshesBySampledDistance(input, output, samples);
    csv << method << "," << weight << "," << lq::toString(options.weightMode) << ","
        << lq::statsRowCsv(label, stats, &distance) << "," << report.collapsedEdges
        << "," << report.rejectedCollapses << "," << report.solverFallbacks << ","
        << report.minAppliedLineWeight << "," << report.maxAppliedLineWeight << "\n";
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

  lq::Mesh input;
  std::string error;
  if (!lq::loadMesh(positional[0], input, &error)) {
    throw std::runtime_error(error);
  }

  const fs::path outDir = positional[1];
  fs::create_directories(outDir);
  const int samples = getIntArg(args, "--samples", 3000);
  const std::vector<double> ratios =
      parseWeights(getArg(args, "--ratios", "0.8,0.5,0.25,0.1,0.05"));
  lq::SimplifyOptions base = parseSimplifyOptions(args);

  std::ofstream csv(outDir / "metrics.csv");
  csv << "method,line_weight,weight_mode,ratio," << lq::statsHeaderCsv()
      << ",collapsed_edges,rejected_collapses,solver_fallbacks,"
         "min_line_weight,max_line_weight\n";

  for (double ratio : ratios) {
    if (ratio <= 0.0 || ratio >= 1.0) {
      std::cerr << "skip invalid ratio " << ratio << "\n";
      continue;
    }
    lq::SimplifyOptions options = base;
    options.targetFaces = -1;
    options.targetRatio = ratio;

    lq::SimplifyReport report;
    lq::QEMSimplifier simplifier(options);
    lq::Mesh output = simplifier.simplify(input, &report);
    const std::string method = options.useLineQuadrics ? "line" : "standard";
    const std::string label = method + "_r_" + sanitizeRatio(ratio) + "_w_" +
                              sanitizeWeight(options.lineWeight);
    const fs::path outStl = outDir / (label + ".stl");
    if (!lq::saveAsciiStl(outStl.string(), output, label, &error)) {
      throw std::runtime_error(error);
    }

    const lq::MeshStats stats = lq::computeMeshStats(output);
    const lq::DistanceStats distance =
        lq::compareMeshesBySampledDistance(input, output, samples);
    csv << method << "," << options.lineWeight << ","
        << lq::toString(options.weightMode) << "," << ratio << ","
        << lq::statsRowCsv(label, stats, &distance) << "," << report.collapsedEdges
        << "," << report.rejectedCollapses << "," << report.solverFallbacks << ","
        << report.minAppliedLineWeight << "," << report.maxAppliedLineWeight << "\n";
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

  lq::Mesh input;
  std::string error;
  if (!lq::loadMesh(positional[0], input, &error)) {
    throw std::runtime_error(error);
  }

  const fs::path outDir = positional[1];
  fs::create_directories(outDir);
  const int samples = getIntArg(args, "--samples", 3000);
  const std::vector<int> faceCounts = parseFaceCounts(
      getArg(args, "--faces", "1000,900,800,700,600,500,400,300,200,100"));
  lq::SimplifyOptions base = parseSimplifyOptions(args);

  std::ofstream csv(outDir / "metrics.csv");
  csv << "method,line_weight,weight_mode,target_faces," << lq::statsHeaderCsv()
      << ",collapsed_edges,rejected_collapses,solver_fallbacks,"
         "min_line_weight,max_line_weight\n";

  for (int targetFaces : faceCounts) {
    if (targetFaces <= 0) {
      std::cerr << "skip invalid target face count " << targetFaces << "\n";
      continue;
    }
    lq::SimplifyOptions options = base;
    options.targetFaces = targetFaces;

    lq::SimplifyReport report;
    lq::QEMSimplifier simplifier(options);
    lq::Mesh output = simplifier.simplify(input, &report);
    const std::string method = options.useLineQuadrics ? "line" : "standard";
    const std::string label = method + "_f_" + std::to_string(targetFaces) + "_w_" +
                              sanitizeWeight(options.lineWeight);
    const fs::path outStl = outDir / (label + ".stl");
    if (!lq::saveAsciiStl(outStl.string(), output, label, &error)) {
      throw std::runtime_error(error);
    }

    const lq::MeshStats stats = lq::computeMeshStats(output);
    const lq::DistanceStats distance =
        lq::compareMeshesBySampledDistance(input, output, samples);
    csv << method << "," << options.lineWeight << ","
        << lq::toString(options.weightMode) << "," << targetFaces << ","
        << lq::statsRowCsv(label, stats, &distance) << "," << report.collapsedEdges
        << "," << report.rejectedCollapses << "," << report.solverFallbacks << ","
        << report.minAppliedLineWeight << "," << report.maxAppliedLineWeight << "\n";
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

    lq::Mesh mesh;
    std::string error;
    if (!lq::loadMesh(input.string(), mesh, &error)) {
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

} // namespace

int main(int argc, char** argv) {
  try {
    if (argc < 2) {
      printUsage();
      return 0;
    }

    const std::string command = argv[1];
    Args args;
    for (int i = 2; i < argc; ++i) {
      args.values.emplace_back(argv[i]);
    }
    validateArgs(args);

    if (command == "generate") return commandGenerate(args);
    if (command == "simplify") return commandSimplify(args);
    if (command == "compare") return commandCompare(args);
    if (command == "feature-report") return commandFeatureReport(args);
    if (command == "feature-compare") return commandFeatureCompare(args);
    if (command == "sweep") return commandSweep(args);
    if (command == "ratio-sweep") return commandRatioSweep(args);
    if (command == "face-sweep") return commandFaceSweep(args);
    if (command == "demo") return commandDemo(args);
    if (command == "summarize-metrics") return commandSummarizeMetrics(args);
    if (command == "validate-features") return commandValidateFeatures(args);
    if (command == "validate-external") return commandValidateExternal(args);
    if (command == "--help" || command == "-h" || command == "help") {
      printUsage();
      return 0;
    }

    throw std::invalid_argument("Unknown command: " + command);
  } catch (const std::exception& e) {
    std::cerr << "error: " << e.what() << "\n\n";
    printUsage();
    return 1;
  }
}
