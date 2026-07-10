#include "detail/ResultBuilder.h"

namespace manumesh::simplification {

Mesh compactResult(const std::vector<VertexState>& vertices, const std::vector<FaceState>& faces) {
    Mesh result;
    std::vector<int> remap(vertices.size(), -1);
    result.faces.reserve(faces.size());
    for (const FaceState& face : faces) {
        if (!face.active) {
            continue;
        }
        Face out;
        bool ok = true;
        for (int i = 0; i < 3; ++i) {
            const int old = face.v[i];
            if (!vertices[old].active) {
                ok = false;
                break;
            }
            if (remap[old] < 0) {
                remap[old] = static_cast<int>(result.vertices.size());
                result.vertices.push_back(vertices[old].p);
            }
            out.v[i] = remap[old];
        }
        if (ok && out.v[0] != out.v[1] && out.v[1] != out.v[2] && out.v[0] != out.v[2]) {
            result.faces.push_back(out);
        }
    }
    return result;
}

} // namespace manumesh::simplification
