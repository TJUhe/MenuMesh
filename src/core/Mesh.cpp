/**
 * @file src/core/Mesh.cpp
 * @brief 实现 Mesh 校验、边界查询、压缩和基础几何运算。
 * @ingroup manumesh_core
 *
 * @details 实现网格边界、校验、压缩和基本三角形几何运算。
 * @algorithm 严格和宽松校验共用索引、有限性、重复索引和 UV 对齐检查；
 * 只有宽松路径允许零面积三角形。压缩会丢弃无效面，并保留顶点首次使用顺序。
 */

#include "core/Mesh.h"

#include "core/MeshTopology.h"
#include "core/Tolerances.h"
#include "core/detail/MeshValidation.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <unordered_set>

namespace manumesh {
namespace {

bool finitePoint(const Vec3& p) { return std::isfinite(p.x()) && std::isfinite(p.y()) && std::isfinite(p.z()); }

bool finiteTexCoord(const Vec2& uv) { return std::isfinite(uv.x()) && std::isfinite(uv.y()); }

double maximumSafeCoordinateMagnitude() { return std::sqrt(std::numeric_limits<double>::max()) / 16.0; }

double robustTriangleArea(const Vec3& a, const Vec3& b, const Vec3& c) {
    const Vec3 ab = b - a;
    const Vec3 ac = c - a;
    const double scale = std::max(ab.cwiseAbs().maxCoeff(), ac.cwiseAbs().maxCoeff());
    if (!std::isfinite(scale)) {
        return std::numeric_limits<double>::infinity();
    }
    if (scale <= 0.0) {
        return 0.0;
    }
    const double factor = 0.5 * (ab / scale).cross(ac / scale).stableNorm();
    if (!std::isfinite(factor)) {
        return std::numeric_limits<double>::infinity();
    }
    if (factor <= 0.0) {
        return 0.0;
    }
    if (scale > std::sqrt(std::numeric_limits<double>::max() / factor)) {
        return std::numeric_limits<double>::infinity();
    }
    return (factor * scale) * scale;
}

/**
 * @brief 一次计算三角形面积和单位法向，避免重复构造缩放后的叉积。
 *
 * 面积和法向都沿用 robustTriangleArea/triangleNormal 的数值路径；当面
 * 不可用时，法向保持零向量，调用方可只读取面积。
 */
struct TriangleGeometry {
    double area = 0.0;
    Vec3 normal = Vec3::Zero();
};

TriangleGeometry computeTriangleGeometry(const Vec3& a, const Vec3& b, const Vec3& c) {
    TriangleGeometry result;
    const Vec3 ab = b - a;
    const Vec3 ac = c - a;
    const double scale = std::max(ab.cwiseAbs().maxCoeff(), ac.cwiseAbs().maxCoeff());
    if (!std::isfinite(scale)) {
        result.area = std::numeric_limits<double>::infinity();
        return result;
    }
    if (scale <= 0.0) {
        return result;
    }

    const Vec3 scaledCross = (ab / scale).cross(ac / scale);
    const double crossLength = scaledCross.stableNorm();
    const double factor = 0.5 * crossLength;
    if (!std::isfinite(factor)) {
        result.area = std::numeric_limits<double>::infinity();
        return result;
    }
    if (factor <= 0.0) {
        return result;
    }
    if (scale > std::sqrt(std::numeric_limits<double>::max() / factor)) {
        result.area = std::numeric_limits<double>::infinity();
        return result;
    }

    result.area = (factor * scale) * scale;
    if (!std::isfinite(result.area) || result.area <= kMinTriangleArea) {
        return result;
    }
    if (std::isfinite(crossLength) && crossLength > 0.0) {
        result.normal = scaledCross / crossLength;
    }
    return result;
}

bool validFaceIndices(const Mesh& mesh, const Face& face) {
    for (int id : face.v) {
        if (id < 0 || static_cast<std::size_t>(id) >= mesh.vertices.size()) {
            return false;
        }
    }
    return true;
}

bool usableFace(const Mesh& mesh, const Face& face) {
    if (!validFaceIndices(mesh, face) || face.v[0] == face.v[1] || face.v[1] == face.v[2] || face.v[0] == face.v[2]) {
        return false;
    }
    const Vec3& a = mesh.vertices[face.v[0]];
    const Vec3& b = mesh.vertices[face.v[1]];
    const Vec3& c = mesh.vertices[face.v[2]];
    if (!finitePoint(a) || !finitePoint(b) || !finitePoint(c)) {
        return false;
    }
    const double area = robustTriangleArea(a, b, c);
    return std::isfinite(area) && area > kMinTriangleArea;
}

} // 命名空间

bool Mesh::empty() const { return vertices.empty() || faces.empty(); }

Vec3 Mesh::bboxMin() const {
    if (vertices.empty()) {
        return Vec3::Zero();
    }
    Vec3 lo(
        std::numeric_limits<double>::infinity(),
        std::numeric_limits<double>::infinity(),
        std::numeric_limits<double>::infinity()
    );
    for (const Vec3& p : vertices) {
        lo = lo.cwiseMin(p);
    }
    return lo;
}

Vec3 Mesh::bboxMax() const {
    if (vertices.empty()) {
        return Vec3::Zero();
    }
    Vec3 hi(
        -std::numeric_limits<double>::infinity(),
        -std::numeric_limits<double>::infinity(),
        -std::numeric_limits<double>::infinity()
    );
    for (const Vec3& p : vertices) {
        hi = hi.cwiseMax(p);
    }
    return hi;
}

double Mesh::bboxDiag() const {
    if (vertices.empty()) {
        return 0.0;
    }
    return (bboxMax() - bboxMin()).stableNorm();
}

bool Mesh::hasTextureCoordinates() const {
    return std::any_of(faceTexCoords.begin(), faceTexCoords.end(), [](const FaceTexCoords& texcoords) {
        return texcoords.valid;
    });
}

void Mesh::removeUnusedVertices() {
    std::vector<int> remap(vertices.size(), -1);
    std::vector<Vec3> newVertices;
    newVertices.reserve(vertices.size());
    std::vector<char> validFace(faces.size(), 1);

    // 第一遍：按整个面进行分类，确保仅被丢弃面引用的顶点不会进入重映射表。
    for (std::size_t faceIndex = 0; faceIndex < faces.size(); ++faceIndex) {
        const Face& face = faces[faceIndex];
        for (int id : face.v) {
            if (id < 0 || static_cast<std::size_t>(id) >= vertices.size()) {
                validFace[faceIndex] = 0;
                break;
            }
        }
    }

    // 第二遍：仅根据有效面构建顶点重映射表。
    for (std::size_t faceIndex = 0; faceIndex < faces.size(); ++faceIndex) {
        if (!validFace[faceIndex]) {
            continue;
        }
        for (int id : faces[faceIndex].v) {
            if (remap[id] < 0) {
                if (newVertices.size() >= static_cast<std::size_t>(std::numeric_limits<int>::max())) {
                    throw std::length_error("Compacted mesh exceeds the supported int-index range.");
                }
                remap[id] = static_cast<int>(newVertices.size());
                newVertices.push_back(vertices[id]);
            }
        }
    }

    const bool alignedTexcoords = faceTexCoords.size() == faces.size();
    std::vector<Face> newFaces;
    std::vector<FaceTexCoords> newTexcoords;
    newFaces.reserve(faces.size());
    if (alignedTexcoords) {
        newTexcoords.reserve(faceTexCoords.size());
    }
    for (std::size_t faceIndex = 0; faceIndex < faces.size(); ++faceIndex) {
        if (!validFace[faceIndex]) {
            continue;
        }
        Face face = faces[faceIndex];
        for (int& id : face.v) {
            id = remap[id];
        }
        newFaces.push_back(face);
        if (alignedTexcoords) {
            newTexcoords.push_back(faceTexCoords[faceIndex]);
        }
    }
    faces.swap(newFaces);
    if (alignedTexcoords) {
        faceTexCoords.swap(newTexcoords);
    } else {
        faceTexCoords.clear();
    }
    vertices.swap(newVertices);
}

bool validateMeshIndices(const Mesh& mesh, std::string* error) {
    if (error) {
        error->clear();
    }
    if (mesh.vertices.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        if (error)
            *error = "Vertex count exceeds the supported int-index range.";
        return false;
    }
    if (mesh.faces.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        if (error)
            *error = "Face count exceeds the supported int-index range.";
        return false;
    }
    for (std::size_t faceIndex = 0; faceIndex < mesh.faces.size(); ++faceIndex) {
        const Face& face = mesh.faces[faceIndex];
        for (int id : face.v) {
            if (id < 0 || static_cast<std::size_t>(id) >= mesh.vertices.size()) {
                if (error) {
                    *error = "Mesh face " + std::to_string(faceIndex) + " references an invalid vertex index.";
                }
                return false;
            }
        }
    }
    return true;
}

namespace {
bool validateMeshGeometryImpl(const Mesh& mesh, std::string* error, bool rejectZeroAreaFaces, bool validateIndices) {
    if (validateIndices && !validateMeshIndices(mesh, error)) {
        return false;
    }
    for (std::size_t vertexIndex = 0; vertexIndex < mesh.vertices.size(); ++vertexIndex) {
        if (!finitePoint(mesh.vertices[vertexIndex])) {
            if (error) {
                *error = "Mesh vertex " + std::to_string(vertexIndex) + " contains a non-finite coordinate.";
            }
            return false;
        }
        if (mesh.vertices[vertexIndex].cwiseAbs().maxCoeff() > maximumSafeCoordinateMagnitude()) {
            if (error) {
                *error =
                    "Mesh vertex " + std::to_string(vertexIndex) + " exceeds the supported numeric coordinate range.";
            }
            return false;
        }
    }
    if (!mesh.faceTexCoords.empty() && mesh.faceTexCoords.size() != mesh.faces.size()) {
        if (error) {
            *error = "Per-corner texture coordinates must be empty or aligned with mesh faces.";
        }
        return false;
    }
    for (std::size_t faceIndex = 0; faceIndex < mesh.faceTexCoords.size(); ++faceIndex) {
        const FaceTexCoords& texcoords = mesh.faceTexCoords[faceIndex];
        if (!texcoords.valid) {
            continue;
        }
        for (const Vec2& uv : texcoords.uv) {
            if (!finiteTexCoord(uv)) {
                if (error) {
                    *error = "Mesh face " + std::to_string(faceIndex) + " contains a non-finite texture coordinate.";
                }
                return false;
            }
        }
    }
    for (std::size_t faceIndex = 0; faceIndex < mesh.faces.size(); ++faceIndex) {
        const Face& face = mesh.faces[faceIndex];
        if (face.v[0] == face.v[1] || face.v[1] == face.v[2] || face.v[0] == face.v[2]) {
            if (error) {
                *error = "Mesh face " + std::to_string(faceIndex) + " is degenerate.";
            }
            return false;
        }
        const Vec3& a = mesh.vertices[face.v[0]];
        const Vec3& b = mesh.vertices[face.v[1]];
        const Vec3& c = mesh.vertices[face.v[2]];
        const double area = robustTriangleArea(a, b, c);
        if (!std::isfinite(area)) {
            if (error)
                *error = "Mesh face " + std::to_string(faceIndex) + " exceeds the supported numeric range.";
            return false;
        }
        if (rejectZeroAreaFaces && area <= kMinTriangleArea) {
            if (error) {
                *error = "Mesh face " + std::to_string(faceIndex) + " has zero area.";
            }
            return false;
        }
    }
    return true;
}

} // 命名空间

namespace detail {

bool validateMeshGeometryLenientAfterIndices(const Mesh& mesh, std::string* error) {
    return validateMeshGeometryImpl(mesh, error, /*rejectZeroAreaFaces=*/false, /*validateIndices=*/false);
}

} // namespace detail

bool validateMeshGeometry(const Mesh& mesh, std::string* error) {
    return validateMeshGeometryImpl(mesh, error, /*rejectZeroAreaFaces=*/true, /*validateIndices=*/true);
}

bool validateMeshGeometryLenient(const Mesh& mesh, std::string* error) {
    return validateMeshGeometryImpl(mesh, error, /*rejectZeroAreaFaces=*/false, /*validateIndices=*/true);
}

int countDegenerateFaces(const Mesh& mesh) {
    std::size_t count = 0;
    for (const Face& face : mesh.faces) {
        if (!validFaceIndices(mesh, face)) {
            ++count;
            continue;
        }
        if (face.v[0] == face.v[1] || face.v[1] == face.v[2] || face.v[0] == face.v[2]) {
            ++count;
            continue;
        }
        const Vec3& a = mesh.vertices[face.v[0]];
        const Vec3& b = mesh.vertices[face.v[1]];
        const Vec3& c = mesh.vertices[face.v[2]];
        if (robustTriangleArea(a, b, c) <= kMinTriangleArea) {
            ++count;
        }
    }
    return static_cast<int>(std::min(count, static_cast<std::size_t>(std::numeric_limits<int>::max())));
}

double triangleArea(const Vec3& a, const Vec3& b, const Vec3& c) { return robustTriangleArea(a, b, c); }

Vec3 triangleNormal(const Vec3& a, const Vec3& b, const Vec3& c) { return computeTriangleGeometry(a, b, c).normal; }

std::vector<double> computeFaceAreas(const Mesh& mesh) {
    std::vector<double> areas(mesh.faces.size(), 0.0);
    for (std::size_t faceIndex = 0; faceIndex < mesh.faces.size(); ++faceIndex) {
        const Face& face = mesh.faces[faceIndex];
        if (validFaceIndices(mesh, face) && face.v[0] != face.v[1] && face.v[1] != face.v[2] &&
            face.v[0] != face.v[2]) {
            const Vec3& a = mesh.vertices[face.v[0]];
            const Vec3& b = mesh.vertices[face.v[1]];
            const Vec3& c = mesh.vertices[face.v[2]];
            if (finitePoint(a) && finitePoint(b) && finitePoint(c)) {
                const TriangleGeometry geometry = computeTriangleGeometry(a, b, c);
                if (std::isfinite(geometry.area) && geometry.area > kMinTriangleArea) {
                    areas[faceIndex] = geometry.area;
                }
            }
        }
    }
    return areas;
}

double computeSurfaceArea(const Mesh& mesh) {
    long double sum = 0.0L;
    for (const Face& face : mesh.faces) {
        if (!validFaceIndices(mesh, face) || face.v[0] == face.v[1] || face.v[1] == face.v[2] ||
            face.v[0] == face.v[2]) {
            continue;
        }
        const Vec3& a = mesh.vertices[face.v[0]];
        const Vec3& b = mesh.vertices[face.v[1]];
        const Vec3& c = mesh.vertices[face.v[2]];
        if (!finitePoint(a) || !finitePoint(b) || !finitePoint(c)) {
            continue;
        }
        const double area = computeTriangleGeometry(a, b, c).area;
        if (std::isfinite(area) && area > kMinTriangleArea) {
            sum += static_cast<long double>(area);
        }
    }
    const double result = static_cast<double>(sum);
    return std::isfinite(result) ? result : std::numeric_limits<double>::infinity();
}

double computeSignedVolume(const Mesh& mesh) {
    if (mesh.vertices.empty() || mesh.faces.empty()) {
        return 0.0;
    }

    std::vector<char> usable(mesh.faces.size(), 0);
    bool hasUsableFace = false;
    for (std::size_t faceIndex = 0; faceIndex < mesh.faces.size(); ++faceIndex) {
        usable[faceIndex] = usableFace(mesh, mesh.faces[faceIndex]) ? 1 : 0;
        hasUsableFace = hasUsableFace || usable[faceIndex] != 0;
    }
    if (!hasUsableFace) {
        return 0.0;
    }

    // A single global origin is not stable for disconnected components that
    // are far apart: each component then produces large translation terms
    // which cancel only after most significant digits have already been lost.
    // Build edge-connected face sets and give every set its own local frame.
    const Result<MeshTopology> topologyResult = MeshTopology::build(mesh, false);
    if (!topologyResult.ok()) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    const MeshTopology& topology = topologyResult.value();

    std::vector<int> parent(mesh.faces.size());
    std::vector<unsigned char> rank(mesh.faces.size(), 0);
    std::iota(parent.begin(), parent.end(), 0);
    const auto findRoot = [&](int face) {
        int root = face;
        while (parent[static_cast<std::size_t>(root)] != root) {
            root = parent[static_cast<std::size_t>(root)];
        }
        while (face != root) {
            const int next = parent[static_cast<std::size_t>(face)];
            parent[static_cast<std::size_t>(face)] = root;
            face = next;
        }
        return root;
    };
    const auto join = [&](int lhs, int rhs) {
        int lhsRoot = findRoot(lhs);
        int rhsRoot = findRoot(rhs);
        if (lhsRoot == rhsRoot) {
            return;
        }
        if (rank[static_cast<std::size_t>(lhsRoot)] < rank[static_cast<std::size_t>(rhsRoot)]) {
            std::swap(lhsRoot, rhsRoot);
        }
        parent[static_cast<std::size_t>(rhsRoot)] = lhsRoot;
        if (rank[static_cast<std::size_t>(lhsRoot)] == rank[static_cast<std::size_t>(rhsRoot)]) {
            ++rank[static_cast<std::size_t>(lhsRoot)];
        }
    };
    for (const TopologyEdge& edge : topology.edges()) {
        int firstUsableFace = -1;
        for (int face : edge.faces) {
            if (face < 0 || static_cast<std::size_t>(face) >= usable.size() ||
                !usable[static_cast<std::size_t>(face)]) {
                continue;
            }
            if (firstUsableFace < 0) {
                firstUsableFace = face;
            } else {
                join(firstUsableFace, face);
            }
        }
    }

    struct VolumeComponent {
        VolumeComponent()
            : minimum(
                  std::numeric_limits<double>::infinity(),
                  std::numeric_limits<double>::infinity(),
                  std::numeric_limits<double>::infinity()
              ),
              maximum(
                  -std::numeric_limits<double>::infinity(),
                  -std::numeric_limits<double>::infinity(),
                  -std::numeric_limits<double>::infinity()
              ) {}

        Vec3 minimum;
        Vec3 maximum;
        Vec3 origin = Vec3::Zero();
        double scale = 0.0;
        double scaledSum = 0.0;
        double compensation = 0.0;
    };

    std::vector<int> rootToComponent(mesh.faces.size(), -1);
    std::vector<int> componentByFace(mesh.faces.size(), -1);
    std::vector<VolumeComponent> components;
    for (std::size_t faceIndex = 0; faceIndex < mesh.faces.size(); ++faceIndex) {
        if (!usable[faceIndex]) {
            continue;
        }
        const int root = findRoot(static_cast<int>(faceIndex));
        int componentIndex = rootToComponent[static_cast<std::size_t>(root)];
        if (componentIndex < 0) {
            componentIndex = static_cast<int>(components.size());
            rootToComponent[static_cast<std::size_t>(root)] = componentIndex;
            components.emplace_back();
        }
        componentByFace[faceIndex] = componentIndex;
        VolumeComponent& component = components[static_cast<std::size_t>(componentIndex)];
        for (int id : mesh.faces[faceIndex].v) {
            const Vec3& vertex = mesh.vertices[static_cast<std::size_t>(id)];
            component.minimum = component.minimum.cwiseMin(vertex);
            component.maximum = component.maximum.cwiseMax(vertex);
        }
    }

    for (VolumeComponent& component : components) {
        component.origin = component.minimum / 2.0 + component.maximum / 2.0;
        component.scale = std::max(
            (component.minimum - component.origin).cwiseAbs().maxCoeff(),
            (component.maximum - component.origin).cwiseAbs().maxCoeff()
        );
        if (!finitePoint(component.origin) || !std::isfinite(component.scale) || component.scale <= 0.0) {
            return std::numeric_limits<double>::quiet_NaN();
        }
    }

    // Kahan summation in component-normalized coordinates keeps each
    // tetrahedron sum finite before the final scale^3 conversion.
    for (std::size_t faceIndex = 0; faceIndex < mesh.faces.size(); ++faceIndex) {
        const int componentIndex = componentByFace[faceIndex];
        if (componentIndex < 0) {
            continue;
        }
        VolumeComponent& component = components[static_cast<std::size_t>(componentIndex)];
        const Face& face = mesh.faces[faceIndex];
        const Vec3 a = (mesh.vertices[face.v[0]] - component.origin) / component.scale;
        const Vec3 b = (mesh.vertices[face.v[1]] - component.origin) / component.scale;
        const Vec3 c = (mesh.vertices[face.v[2]] - component.origin) / component.scale;
        const double contribution = a.dot(b.cross(c)) / 6.0;
        if (!std::isfinite(contribution)) {
            return std::numeric_limits<double>::quiet_NaN();
        }
        const double corrected = contribution - component.compensation;
        const double next = component.scaledSum + corrected;
        component.compensation = (next - component.scaledSum) - corrected;
        component.scaledSum = next;
        if (!std::isfinite(component.scaledSum)) {
            return std::copysign(std::numeric_limits<double>::infinity(), component.scaledSum);
        }
    }

    long double totalVolume = 0.0L;
    for (const VolumeComponent& component : components) {
        long double volume = static_cast<long double>(component.scaledSum);
        volume *= static_cast<long double>(component.scale);
        volume *= static_cast<long double>(component.scale);
        volume *= static_cast<long double>(component.scale);
        totalVolume += volume;
    }
    return static_cast<double>(totalVolume);
}

Vec3 computeSurfaceCentroid(const Mesh& mesh) {
    Vec3 centroid = Vec3::Zero();
    // Accumulate weights relative to the largest area seen so far. This keeps
    // both the running total and interpolation factor finite even when the
    // true total surface area exceeds double's range.
    double areaScale = 0.0;
    double scaledAreaSum = 0.0;
    for (const Face& face : mesh.faces) {
        if (!usableFace(mesh, face)) {
            continue;
        }
        const Vec3& a = mesh.vertices[face.v[0]];
        const Vec3& b = mesh.vertices[face.v[1]];
        const Vec3& c = mesh.vertices[face.v[2]];
        const double area = triangleArea(a, b, c);
        const Vec3 faceCentroid = a / 3.0 + b / 3.0 + c / 3.0;
        if (area > areaScale) {
            if (areaScale > 0.0) {
                scaledAreaSum *= areaScale / area;
            }
            areaScale = area;
        }
        const double scaledArea = area / areaScale;
        const double nextScaledAreaSum = scaledAreaSum + scaledArea;
        centroid += (scaledArea / nextScaledAreaSum) * (faceCentroid - centroid);
        scaledAreaSum = nextScaledAreaSum;
    }
    return centroid;
}

std::vector<Vec3> computeFaceNormals(const Mesh& mesh) {
    std::vector<Vec3> normals(mesh.faces.size(), Vec3::Zero());
    for (std::size_t faceIndex = 0; faceIndex < mesh.faces.size(); ++faceIndex) {
        const Face& face = mesh.faces[faceIndex];
        if (validFaceIndices(mesh, face) && face.v[0] != face.v[1] && face.v[1] != face.v[2] &&
            face.v[0] != face.v[2]) {
            const Vec3& a = mesh.vertices[face.v[0]];
            const Vec3& b = mesh.vertices[face.v[1]];
            const Vec3& c = mesh.vertices[face.v[2]];
            if (finitePoint(a) && finitePoint(b) && finitePoint(c)) {
                normals[faceIndex] = computeTriangleGeometry(a, b, c).normal;
            }
        }
    }
    return normals;
}

std::vector<Vec3> computeVertexNormals(const Mesh& mesh) {
    std::vector<Vec3> normals(mesh.vertices.size(), Vec3::Zero());
    std::vector<double> areas(mesh.faces.size(), 0.0);
    std::vector<Vec3> faceNormals(mesh.faces.size(), Vec3::Zero());
    for (std::size_t faceIndex = 0; faceIndex < mesh.faces.size(); ++faceIndex) {
        const Face& face = mesh.faces[faceIndex];
        if (!validFaceIndices(mesh, face) || face.v[0] == face.v[1] || face.v[1] == face.v[2] ||
            face.v[0] == face.v[2]) {
            continue;
        }
        const Vec3& a = mesh.vertices[face.v[0]];
        const Vec3& b = mesh.vertices[face.v[1]];
        const Vec3& c = mesh.vertices[face.v[2]];
        if (!finitePoint(a) || !finitePoint(b) || !finitePoint(c)) {
            continue;
        }
        const TriangleGeometry geometry = computeTriangleGeometry(a, b, c);
        if (std::isfinite(geometry.area) && geometry.area > kMinTriangleArea) {
            areas[faceIndex] = geometry.area;
            faceNormals[faceIndex] = geometry.normal;
        }
    }
    const double maximumArea = areas.empty() ? 0.0 : *std::max_element(areas.begin(), areas.end());
    if (!(maximumArea > 0.0) || !std::isfinite(maximumArea)) {
        return normals;
    }
    for (std::size_t faceIndex = 0; faceIndex < mesh.faces.size(); ++faceIndex) {
        if (areas[faceIndex] <= 0.0) {
            continue;
        }
        const Face& face = mesh.faces[faceIndex];
        const double weight = areas[faceIndex] / maximumArea;
        for (int id : face.v) {
            normals[static_cast<std::size_t>(id)] += weight * faceNormals[faceIndex];
        }
    }
    for (Vec3& normal : normals) {
        const double scale = normal.cwiseAbs().maxCoeff();
        if (!(scale > 0.0) || !std::isfinite(scale)) {
            normal.setZero();
            continue;
        }
        normal /= scale;
        const double length = normal.stableNorm();
        if (std::isfinite(length) && length > kMinNormalLength) {
            normal /= length;
        } else {
            normal.setZero();
        }
    }
    return normals;
}

std::vector<std::pair<int, int>> uniqueEdges(const Mesh& mesh) {
    std::unordered_set<std::uint64_t> seen;
    std::vector<std::pair<int, int>> edges;
    const std::size_t reserveCount = mesh.faces.size() <= std::numeric_limits<std::size_t>::max() / 3
                                         ? mesh.faces.size() * 3 / 2
                                         : mesh.faces.size();
    // 哈希表和结果数组使用同一数量级的预留，减少大网格插入时的多次重哈希。
    seen.reserve(reserveCount);
    edges.reserve(reserveCount);

    for (const Face& face : mesh.faces) {
        if (!validFaceIndices(mesh, face)) {
            continue;
        }
        for (int e = 0; e < 3; ++e) {
            int a = face.v[e];
            int b = face.v[(e + 1) % 3];
            if (a == b)
                continue;
            const auto key = topologyEdgeKey(a, b);
            if (seen.insert(key).second) {
                if (a > b)
                    std::swap(a, b);
                edges.emplace_back(a, b);
            }
        }
    }
    std::sort(edges.begin(), edges.end());
    return edges;
}

void reverseFaceWindings(Mesh& mesh) {
    for (std::size_t faceIndex = 0; faceIndex < mesh.faces.size(); ++faceIndex) {
        std::swap(mesh.faces[faceIndex].v[1], mesh.faces[faceIndex].v[2]);
        if (faceIndex < mesh.faceTexCoords.size()) {
            FaceTexCoords& texcoords = mesh.faceTexCoords[faceIndex];
            if (texcoords.valid) {
                std::swap(texcoords.uv[1], texcoords.uv[2]);
            } else {
                for (Vec2& uv : texcoords.uv) {
                    uv.setZero();
                }
            }
        }
    }
}

int removeDegenerateFaces(Mesh& mesh, bool compactVertices) {
    const bool alignedTexcoords = mesh.faceTexCoords.size() == mesh.faces.size();
    std::vector<Face> retainedFaces;
    std::vector<FaceTexCoords> retainedTexcoords;
    retainedFaces.reserve(mesh.faces.size());
    if (alignedTexcoords) {
        retainedTexcoords.reserve(mesh.faceTexCoords.size());
    }
    std::size_t removed = 0;
    for (std::size_t faceIndex = 0; faceIndex < mesh.faces.size(); ++faceIndex) {
        if (!usableFace(mesh, mesh.faces[faceIndex])) {
            ++removed;
            continue;
        }
        retainedFaces.push_back(mesh.faces[faceIndex]);
        if (alignedTexcoords) {
            if (mesh.faceTexCoords[faceIndex].valid) {
                retainedTexcoords.push_back(mesh.faceTexCoords[faceIndex]);
            } else {
                retainedTexcoords.emplace_back();
            }
        }
    }
    mesh.faces.swap(retainedFaces);
    if (alignedTexcoords) {
        mesh.faceTexCoords.swap(retainedTexcoords);
    } else {
        mesh.faceTexCoords.clear();
    }
    if (compactVertices) {
        mesh.removeUnusedVertices();
    }
    return static_cast<int>(std::min(removed, static_cast<std::size_t>(std::numeric_limits<int>::max())));
}

bool appendMesh(Mesh& destination, const Mesh& source, std::string* error) {
    if (error) {
        error->clear();
    }
    std::string validationError;
    if (!validateMeshGeometryLenient(destination, &validationError) ||
        !validateMeshGeometryLenient(source, &validationError)) {
        if (error)
            *error = validationError.empty() ? "Source or destination mesh is invalid." : validationError;
        return false;
    }
    const std::size_t maxInt = static_cast<std::size_t>(std::numeric_limits<int>::max());
    if (source.vertices.size() > maxInt - destination.vertices.size() ||
        source.faces.size() > maxInt - destination.faces.size()) {
        if (error)
            *error = "Combined mesh exceeds the supported int-index range.";
        return false;
    }

    const Mesh sourceCopy = &destination == &source ? source : Mesh{};
    const Mesh& stableSource = &destination == &source ? sourceCopy : source;
    Mesh combined = destination;
    const int vertexOffset = static_cast<int>(combined.vertices.size());
    const std::size_t originalFaceCount = combined.faces.size();
    combined.vertices.insert(combined.vertices.end(), stableSource.vertices.begin(), stableSource.vertices.end());
    combined.faces.reserve(originalFaceCount + stableSource.faces.size());
    for (Face face : stableSource.faces) {
        for (int& id : face.v) {
            id += vertexOffset;
        }
        combined.faces.push_back(face);
    }

    if (!destination.faceTexCoords.empty() || !stableSource.faceTexCoords.empty()) {
        if (combined.faceTexCoords.empty()) {
            combined.faceTexCoords.resize(originalFaceCount);
        }
        if (stableSource.faceTexCoords.empty()) {
            combined.faceTexCoords.resize(combined.faces.size());
        } else {
            combined.faceTexCoords.insert(
                combined.faceTexCoords.end(), stableSource.faceTexCoords.begin(), stableSource.faceTexCoords.end()
            );
        }
        for (FaceTexCoords& texcoords : combined.faceTexCoords) {
            if (!texcoords.valid) {
                for (Vec2& uv : texcoords.uv) {
                    uv.setZero();
                }
            }
        }
    }
    destination = std::move(combined);
    return true;
}

} // 命名空间 manumesh
