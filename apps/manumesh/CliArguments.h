#pragma once

#include <string>
#include <vector>

namespace manumesh::cli {

struct Args {
  std::vector<std::string> values;
};

bool hasFlag(const Args& args, const std::string& name);
bool isKnownFlag(const std::string& value);
bool takesValue(const std::string& value);
void validateArgs(const Args& args);
std::string getArg(const Args& args, const std::string& name,
                   const std::string& defaultValue = "");
int parseIntStrict(const std::string& value, const std::string& name);
double parseDoubleStrict(const std::string& value, const std::string& name);
int getIntArg(const Args& args, const std::string& name, int defaultValue);
double getDoubleArg(const Args& args, const std::string& name, double defaultValue);
std::vector<std::string> positionalArgs(const Args& args);
std::vector<double> parseWeights(const std::string& text);
std::vector<int> parseFaceCounts(const std::string& text);

} // namespace manumesh::cli
