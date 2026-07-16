/**
 * @file apps/main.cpp
 * @brief Implements main facilities for the ManuMesh command-line application.
 * @ingroup manumesh_cli
 *
 * @details CLI parsing, validation, dispatch, and reporting are kept outside the geometry library so SDK behavior is independent of process-global command-line state.
 */

#include "ManuMeshCli.h"

int main(int argc, char** argv) { return manumesh::cli::run(argc, argv); }
