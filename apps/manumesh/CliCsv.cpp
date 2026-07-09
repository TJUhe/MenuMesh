#include "CliCsv.h"

#include <fstream>

namespace manumesh::cli {

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

std::map<std::string, std::string> readFirstCsvRow(const std::filesystem::path& path) {
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

} // namespace manumesh::cli
