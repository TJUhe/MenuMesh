/**
 * @file apps/CliCsv.h
 * @brief 声明 CLI CSV 解析和网格统计行格式化辅助函数。
 * @ingroup manumesh_cli
 *
 * @details 解析遵循 RFC-4180 风格的引号与逗号规则，统计字段保持稳定顺序。
 */

#pragma once

#include "algorithms/analysis/MeshAnalysis.h"

#include "core/Filesystem.h"
#include <istream>
#include <map>
#include <string>
#include <vector>

namespace manumesh {
namespace cli {

/// 解析一行 RFC-4180 风格 CSV，支持带引号逗号和双引号转义。
std::vector<std::string> splitCsvLine(const std::string& line);
/// 从流中读取一条完整 CSV 记录；带引号字段中的物理换行会保留为 `\n`。
/// 到达干净 EOF 且尚未读取任何数据时返回 false；未闭合引号会抛出异常。
bool readCsvRecord(std::istream& input, std::string& record);
/// 必要时为一个 CSV 字段添加引号并转义。
std::string quoteCsv(const std::string& value);
/// 读取表头和首行数据，返回列名到值的映射。
std::map<std::string, std::string> readFirstCsvRow(const manumesh::filesystem::path& path);
/// 返回必需列的值；缺少列时抛出异常。
std::string csvValue(const std::map<std::string, std::string>& row, const std::string& key);

/// 返回网格统计数据行的 CSV 表头。
std::string statsHeaderCsv();
/// 将网格统计数据格式化为一行 CSV，可选附带距离统计。
std::string statsRowCsv(
    const std::string& label,
    const manumesh::analysis::MeshStats& stats,
    const manumesh::analysis::DistanceStats* distance = nullptr
);

} // namespace cli
} // namespace manumesh
