/**
 * @file apps/CliPath.h
 * @brief 在 CLI UTF-8 路径文本与本机文件系统路径之间转换。
 * @ingroup manumesh_cli
 */

#pragma once

#include "core/Filesystem.h"
#include <algorithm>
#include <cctype>
#include <system_error>
#include <string>
#include <vector>

namespace manumesh {
namespace cli {

inline manumesh::filesystem::path pathFromUtf8(const std::string& value) { return manumesh::filesystem::u8path(value); }

inline std::string pathToUtf8(const manumesh::filesystem::path& value) { return value.u8string(); }

/// Return a path suitable for comparing an existing or not-yet-created output.
/// ManuMesh targets C++14's experimental filesystem, which has no
/// `weakly_canonical`; resolve the nearest existing parent and append the
/// missing suffix instead.
inline manumesh::filesystem::path normalizedPathForComparison(const manumesh::filesystem::path& value) {
    std::error_code ec;
    manumesh::filesystem::path current = manumesh::filesystem::system_complete(value, ec);
    if (ec) {
        current = value;
    }

    std::vector<manumesh::filesystem::path> missingSuffix;
    while (!current.empty()) {
        ec.clear();
        if (manumesh::filesystem::exists(current, ec) && !ec) {
            ec.clear();
            manumesh::filesystem::path resolved = manumesh::filesystem::canonical(current, ec);
            if (ec) {
                resolved = current;
            }
            for (auto it = missingSuffix.rbegin(); it != missingSuffix.rend(); ++it) {
                resolved /= *it;
            }
            return resolved;
        }

        missingSuffix.push_back(current.filename());
        const manumesh::filesystem::path parent = current.parent_path();
        if (parent.empty() || parent == current) {
            break;
        }
        current = parent;
    }
    return manumesh::filesystem::system_complete(value, ec);
}

/// Build a stable comparison key. Windows paths are case-insensitive; the
/// filesystem's `equivalent` check below still handles existing non-ASCII
/// case-folding and hard links.
inline std::string pathIdentityKey(const manumesh::filesystem::path& value) {
    std::string key = pathToUtf8(normalizedPathForComparison(value));
    // VS2019's experimental filesystem path has no lexical_normal operation.
    // Collapse the only non-existent-output alias we accept (`.`) in the text
    // key after resolving the nearest existing parent.
    for (;;) {
        const std::size_t backslashDot = key.find("\\.\\");
        const std::size_t slashDot = key.find("/./");
        if (backslashDot != std::string::npos) {
            key.replace(backslashDot, 3, "\\");
            continue;
        }
        if (slashDot != std::string::npos) {
            key.replace(slashDot, 3, "/");
            continue;
        }
        break;
    }
#if defined(_WIN32)
    std::transform(key.begin(), key.end(), key.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
#endif
    return key;
}

/**
 * @brief Return true when two paths identify the same existing file or normalize
 *        to the same eventual location (including aliases such as ./out.csv).
 */
inline bool pathsReferToSameLocation(
    const manumesh::filesystem::path& first, const manumesh::filesystem::path& second
) {
    std::error_code ec;
    if (manumesh::filesystem::exists(first, ec)) {
        ec.clear();
        if (manumesh::filesystem::exists(second, ec)) {
            ec.clear();
            if (manumesh::filesystem::equivalent(first, second, ec) && !ec) {
                return true;
            }
        }
    }
    return pathIdentityKey(first) == pathIdentityKey(second);
}

} // namespace cli
} // namespace manumesh
