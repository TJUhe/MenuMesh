#include "CliArguments.h"

#include <cmath>
#include <sstream>
#include <stdexcept>
#include <unordered_set>

namespace manumesh::cli {
namespace {

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
      "--spindle-input",
      "--ring-input",
      "--pulley-input",
      "--flange-input",
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
      "--industrial-safe",
      "--no-normal-tensor-features",
      "--quick",
  };
  return flags;
}

} // namespace

bool hasFlag(const Args& args, const std::string& name) {
  for (const std::string& value : args.values) {
    if (value == name) return true;
  }
  return false;
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
                   const std::string& defaultValue) {
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

} // namespace manumesh::cli
