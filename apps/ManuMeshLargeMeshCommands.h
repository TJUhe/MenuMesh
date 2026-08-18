/**
 * @file apps/ManuMeshLargeMeshCommands.h
 * @brief Declares bounded-memory partitioned-mesh CLI commands.
 * @ingroup manumesh_cli
 */

#pragma once

#include "CliArguments.h"

namespace manumesh {
namespace cli {
namespace large_mesh_commands {

/// Streams a binary STL into a partitioned ManuMesh dataset.
int importDataset(const Args& args);
/// Streams and validates every partition in a ManuMesh dataset.
int validateDataset(const Args& args);

} // namespace large_mesh_commands
} // namespace cli
} // namespace manumesh
