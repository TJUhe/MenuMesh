#pragma once

#include <array>

namespace manumesh::mesh_edit {

/// Triangle record shared by topology-editing algorithms.
///
/// Algorithms may mark faces inactive while preserving stable face indices.
/// compactActiveMesh() converts this edit-time representation back to Mesh.
struct EditableFace {
    std::array<int, 3> v{};
    bool active = true;
};

} // namespace manumesh::mesh_edit
