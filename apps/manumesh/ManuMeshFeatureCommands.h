/**
 * @file apps/manumesh/ManuMeshFeatureCommands.h
 * @brief Declares manu mesh feature commands facilities for the ManuMesh command-line application.
 * @ingroup manumesh_cli
 *
 * @details CLI parsing, validation, dispatch, and reporting are kept outside the geometry library so SDK behavior is independent of process-global command-line state.
 */

#pragma once

#include "CliArguments.h"

namespace manumesh::cli::feature_commands {

/// Runs feature detection and writes loop/component diagnostics.
int report(const Args& args);
/// Scores feature output against explicit ground-truth labels.
int benchmark(const Args& args);
/// Compares feature curves before and after simplification.
int compare(const Args& args);

} // namespace manumesh::cli::feature_commands
