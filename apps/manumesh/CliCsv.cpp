/**
 * @file apps/manumesh/CliCsv.cpp
 * @brief Implements cli csv facilities for the ManuMesh command-line application.
 * @ingroup manumesh_cli
 *
 * @details CLI parsing, validation, dispatch, and reporting are kept outside the geometry library so SDK behavior is independent of process-global command-line state.
 */

#include "CliCsv.h"

#include <fstream>
#include <iomanip>
#include <sstream>

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

std::string csvValue(const std::map<std::string, std::string>& row, const std::string& key) {
    const auto it = row.find(key);
    return it == row.end() ? "" : it->second;
}

std::string statsHeaderCsv() {
    return "label,vertices,faces,edges,boundary_edges,non_manifold_edges,area,"
           "mean_triangle_quality,min_triangle_quality,mean_edge_length,"
           "edge_length_cv,mean_orig_to_simp,max_orig_to_simp,"
           "mean_simp_to_orig,max_simp_to_orig";
}

std::string statsRowCsv(
    const std::string& label,
    const manumesh::analysis::MeshStats& stats,
    const manumesh::analysis::DistanceStats* distance
) {
    std::ostringstream out;
    out << std::setprecision(12);
    out << label << "," << stats.vertices << "," << stats.faces << "," << stats.edges << "," << stats.boundaryEdges
        << "," << stats.nonManifoldEdges << "," << stats.area << "," << stats.meanTriangleQuality << ","
        << stats.minTriangleQuality << "," << stats.meanEdgeLength << "," << stats.edgeLengthCv;
    if (distance) {
        out << "," << distance->meanOriginalToSimplified << "," << distance->maxOriginalToSimplified << ","
            << distance->meanSimplifiedToOriginal << "," << distance->maxSimplifiedToOriginal;
    } else {
        out << ",,,,";
    }
    return out.str();
}

} // namespace manumesh::cli
