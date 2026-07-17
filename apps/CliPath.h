/**
 * @file apps/CliPath.h
 * @brief Converts CLI UTF-8 path text to and from native filesystem paths.
 * @ingroup manumesh_cli
 */

#pragma once

#include <filesystem>
#include <string>

namespace manumesh::cli {

inline std::filesystem::path pathFromUtf8(const std::string& value) { return std::filesystem::u8path(value); }

inline std::string pathToUtf8(const std::filesystem::path& value) { return value.u8string(); }

} // namespace manumesh::cli
