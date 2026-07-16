/**
 * @file apps/CliCommands.h
 * @brief Declares cli commands facilities for the ManuMesh command-line application.
 * @ingroup manumesh_cli
 *
 * @details CLI parsing, validation, dispatch, and reporting are kept outside the geometry library so SDK behavior is independent of process-global command-line state.
 */

#pragma once

#include "CliArguments.h"

#include <map>
#include <string>

namespace manumesh::cli {

using CommandHandler = int (*)(const Args&); ///< Process-style command callback.

/// @return Stable command-name to handler registry used by help and dispatch.
const std::map<std::string, CommandHandler>& commandRegistry();

} // namespace manumesh::cli
