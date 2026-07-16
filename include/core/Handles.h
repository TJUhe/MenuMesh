/**
 * @file include/core/Handles.h
 * @brief Declares handles facilities for ManuMesh's core-mesh module.
 * @ingroup manumesh_core
 *
 * @details Core types establish the storage, validation, tolerance, topology, and status contracts consumed by every algorithm module.
 */

#pragma once

#include <cstdint>

namespace manumesh {

/**
 * @brief Strongly typed integer handle for dense topology storage.
 *
 * Different tag types prevent accidental mixing of vertex, face, edge, and
 * halfedge indices while retaining the storage and comparison cost of an int.
 * A default-constructed handle is invalid.
 *
 * @tparam Tag Empty type that defines the handle domain.
 */
template <typename Tag> struct Handle {
    ///< Zero-based dense index, or -1 when invalid.
    int id = -1;

    /// Creates an invalid handle.
    constexpr Handle() = default;
    /// Creates a handle for a zero-based dense index.
    /// @param[in] value Index value; negative values remain invalid.
    explicit constexpr Handle(int value)
        : id(value) {}

    /// @return true when the handle names a non-negative index.
    constexpr bool valid() const { return id >= 0; }
    /// @return the stored dense index without validation.
    constexpr explicit operator int() const { return id; }

    friend constexpr bool operator==(Handle a, Handle b) { return a.id == b.id; }
    friend constexpr bool operator!=(Handle a, Handle b) { return a.id != b.id; }
    friend constexpr bool operator<(Handle a, Handle b) { return a.id < b.id; }
};

struct VertexTag {};
struct FaceTag {};
struct EdgeTag {};
struct HalfedgeTag {};

using VertexId = Handle<VertexTag>;     ///< Vertex handle.
using FaceId = Handle<FaceTag>;         ///< Face handle.
using EdgeId = Handle<EdgeTag>;         ///< Undirected-edge handle.
using HalfedgeId = Handle<HalfedgeTag>; ///< Directed-halfedge handle.

} // namespace manumesh
