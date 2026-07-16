/**
 * @file apps/manumesh/CliCsv.h
 * @brief Declares cli csv facilities for the ManuMesh command-line application.
 * @ingroup manumesh_cli
 *
 * @details CLI parsing, validation, dispatch, and reporting are kept outside the geometry library so SDK behavior is independent of process-global command-line state.
 */

#pragma once

#include "algorithms/analysis/MeshAnalysis.h"

#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace manumesh::cli {

/// Parses one RFC-4180-style row including quoted commas and doubled quotes.
std::vector<std::string> splitCsvLine(const std::string& line);
/// Quotes and escapes one CSV field when required.
std::string quoteCsv(const std::string& value);
/// Reads the header and first data row into a name/value map.
std::map<std::string, std::string> readFirstCsvRow(const std::filesystem::path& path);
/// Returns a required value or throws when the column is absent.
std::string csvValue(const std::map<std::string, std::string>& row, const std::string& key);

/// CSV header for mesh statistics rows.
std::string statsHeaderCsv();
/// CSV row for a labeled mesh-statistics record.
std::string statsRowCsv(
    const std::string& label,
    const manumesh::analysis::MeshStats& stats,
    const manumesh::analysis::DistanceStats* distance = nullptr
);

} // namespace manumesh::cli
