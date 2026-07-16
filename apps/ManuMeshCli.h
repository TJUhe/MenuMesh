/**
 * @file apps/ManuMeshCli.h
 * @brief Declares manu mesh cli facilities for the ManuMesh command-line application.
 * @ingroup manumesh_cli
 *
 * @details CLI parsing, validation, dispatch, and reporting are kept outside the geometry library so SDK behavior is independent of process-global command-line state.
 */

#pragma once

namespace manumesh::cli {

/// Validates global arguments, dispatches one command, and translates exceptions.
/// @return Process exit code: zero on success, nonzero on usage/operation failure.
int run(int argc, char** argv);

} // namespace manumesh::cli
