#pragma once

#include <cstdint>

namespace manumesh {

/// Typed compact handle used by topology and future attribute storage.
template <typename Tag> struct Handle {
    int id = -1;

    constexpr Handle() = default;
    explicit constexpr Handle(int value)
        : id(value) {}

    constexpr bool valid() const { return id >= 0; }
    constexpr explicit operator int() const { return id; }

    friend constexpr bool operator==(Handle a, Handle b) { return a.id == b.id; }
    friend constexpr bool operator!=(Handle a, Handle b) { return a.id != b.id; }
    friend constexpr bool operator<(Handle a, Handle b) { return a.id < b.id; }
};

struct VertexTag {};
struct FaceTag {};
struct EdgeTag {};
struct HalfedgeTag {};

using VertexId = Handle<VertexTag>;
using FaceId = Handle<FaceTag>;
using EdgeId = Handle<EdgeTag>;
using HalfedgeId = Handle<HalfedgeTag>;

} // namespace manumesh
