/**
 * @file apps/CliPath.h
 * @brief 在 CLI UTF-8 路径文本与本机文件系统路径之间转换。
 * @ingroup manumesh_cli
 */

#pragma once

#include "core/Filesystem.h"
#include <string>

namespace manumesh {
namespace cli {

inline manumesh::filesystem::path pathFromUtf8(const std::string& value) { return manumesh::filesystem::u8path(value); }

inline std::string pathToUtf8(const manumesh::filesystem::path& value) { return value.u8string(); }

} // namespace cli
} // namespace manumesh
