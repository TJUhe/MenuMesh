/**
 * @file apps/CliPath.h
 * @brief 在 CLI UTF-8 路径文本与本机文件系统路径之间转换。
 * @ingroup manumesh_cli
 */

#pragma once

#include <filesystem>
#include <string>

namespace manumesh::cli {

inline std::filesystem::path pathFromUtf8(const std::string& value) { return std::filesystem::u8path(value); }

inline std::string pathToUtf8(const std::filesystem::path& value) { return value.u8string(); }

} // 命令行命名空间
