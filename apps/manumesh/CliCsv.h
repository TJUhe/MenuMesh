#pragma once

#include "algorithms/analysis/MeshAnalysis.h"

#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace manumesh::cli {

std::vector<std::string> splitCsvLine(const std::string& line);
std::string quoteCsv(const std::string& value);
std::map<std::string, std::string> readFirstCsvRow(const std::filesystem::path& path);
std::string csvValue(const std::map<std::string, std::string>& row,
                     const std::string& key);

/// CSV header for mesh statistics rows.
std::string statsHeaderCsv();
/// CSV row for a labeled mesh-statistics record.
std::string statsRowCsv(
    const std::string& label,
    const manumesh::analysis::MeshStats& stats,
    const manumesh::analysis::DistanceStats* distance = nullptr
);

} // namespace manumesh::cli
