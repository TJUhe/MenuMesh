/**
 * @file apps/ManuMeshWorkflowCommands.h
 * @brief Declares manu mesh workflow commands facilities for the ManuMesh command-line application.
 * @ingroup manumesh_cli
 *
 * @details CLI parsing, validation, dispatch, and reporting are kept outside the geometry library so SDK behavior is independent of process-global command-line state.
 */

#pragma once

#include "CliArguments.h"

namespace manumesh::cli::workflow_commands {

/// Runs the built-in end-to-end demonstration workflow.
int demo(const Args& args);
/// Executes analytic feature-preservation acceptance cases.
int validateFeatures(const Args& args);
/// Executes dataset-driven validation on caller-supplied external meshes.
int validateExternal(const Args& args);

} // namespace manumesh::cli::workflow_commands
