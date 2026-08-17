/**
 * @file apps/CliCsv.cpp
 * @brief 实现 CLI 使用的 CSV 解析、转义和统计数据序列化。
 * @ingroup manumesh_cli
 *
 * @details 解析器支持双引号字段和双引号转义，统计列顺序保持稳定。
 */

#include "CliCsv.h"

#include <fstream>
#include <iomanip>
#include <locale>
#include <sstream>
#include <stdexcept>

namespace manumesh {
namespace cli {

std::vector<std::string> splitCsvLine(const std::string& line) {
    std::vector<std::string> out;
    std::string current;
    bool quoted = false;
    bool fieldWasQuoted = false;
    for (std::size_t i = 0; i < line.size(); ++i) {
        const char ch = line[i];
        if (!quoted && (ch == '\r' || ch == '\n')) {
            throw std::invalid_argument("Malformed CSV: embedded line break in an unquoted field.");
        }
        if (ch == '"') {
            if (quoted && i + 1 < line.size() && line[i + 1] == '"') {
                current.push_back('"');
                ++i;
            } else if (!quoted && !current.empty()) {
                throw std::invalid_argument("Malformed CSV: quote must begin at the start of a field.");
            } else {
                quoted = !quoted;
                if (quoted) {
                    fieldWasQuoted = true;
                }
            }
        } else if (ch == ',' && !quoted) {
            out.push_back(current);
            current.clear();
            fieldWasQuoted = false;
        } else {
            if (!quoted && fieldWasQuoted) {
                throw std::invalid_argument("Malformed CSV: characters after a closing quote.");
            }
            current.push_back(ch);
        }
    }
    if (quoted) {
        throw std::invalid_argument("Malformed CSV: unterminated quoted field.");
    }
    out.push_back(current);
    return out;
}

bool readCsvRecord(std::istream& input, std::string& record) {
    record.clear();
    std::string physicalLine;
    bool quoted = false;
    bool readAnyLine = false;

    while (std::getline(input, physicalLine)) {
        // std::getline removes '\n' but preserves '\r' when a CRLF file is
        // consumed without platform text-mode translation.  It is part of the
        // record delimiter, not field data, so normalize it before parsing.
        if (!physicalLine.empty() && physicalLine.back() == '\r') {
            physicalLine.pop_back();
        }
        if (readAnyLine) {
            record.push_back('\n');
        }
        record += physicalLine;
        readAnyLine = true;

        for (std::size_t i = 0; i < physicalLine.size(); ++i) {
            if (physicalLine[i] != '"') {
                continue;
            }
            if (quoted && i + 1 < physicalLine.size() && physicalLine[i + 1] == '"') {
                ++i;
            } else {
                quoted = !quoted;
            }
        }
        if (!quoted) {
            return true;
        }
    }

    if (readAnyLine && quoted) {
        throw std::invalid_argument("Malformed CSV: unterminated quoted field at end of file.");
    }
    return readAnyLine;
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

std::map<std::string, std::string> readFirstCsvRow(const manumesh::filesystem::path& path) {
    std::ifstream in(path);
    if (!in) {
        return {};
    }
    std::string headerLine;
    std::string valueLine;
    if (!readCsvRecord(in, headerLine) || !readCsvRecord(in, valueLine)) {
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
    out.imbue(std::locale::classic());
    out << std::setprecision(12);
    out << quoteCsv(label) << "," << stats.vertices << "," << stats.faces << "," << stats.edges << ","
        << stats.boundaryEdges << "," << stats.nonManifoldEdges << "," << stats.area << "," << stats.meanTriangleQuality
        << "," << stats.minTriangleQuality << "," << stats.meanEdgeLength << "," << stats.edgeLengthCv;
    if (distance) {
        out << "," << distance->meanOriginalToSimplified << "," << distance->maxOriginalToSimplified << ","
            << distance->meanSimplifiedToOriginal << "," << distance->maxSimplifiedToOriginal;
    } else {
        out << ",,,,";
    }
    return out.str();
}

} // namespace cli
} // namespace manumesh
