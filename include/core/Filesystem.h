/**
 * @file include/core/Filesystem.h
 * @brief Selects the filesystem implementation available in C++14.
 * @ingroup manumesh_core
 */

#pragma once

#if defined(_MSC_VER) && !defined(_SILENCE_EXPERIMENTAL_FILESYSTEM_DEPRECATION_WARNING)
#define _SILENCE_EXPERIMENTAL_FILESYSTEM_DEPRECATION_WARNING
#endif

#include <experimental/filesystem>

namespace manumesh {

namespace filesystem = std::experimental::filesystem;

} // namespace manumesh
