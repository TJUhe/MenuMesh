#pragma once

#include <Eigen/Dense>

#include <array>
#include <string>
#include <utility>
#include <vector>

namespace lq {

using Vec3 = Eigen::Vector3d;
using Mat4 = Eigen::Matrix4d;

struct Face {
  std::array<int, 3> v{};
};

struct Mesh {
  std::vector<Vec3> vertices;
  std::vector<Face> faces;

  bool empty() const { return vertices.empty() || faces.empty(); }
  Vec3 bboxMin() const;
  Vec3 bboxMax() const;
  double bboxDiag() const;
  void removeUnusedVertices();
};

bool loadStl(const std::string& path, Mesh& mesh, std::string* error = nullptr,
             double weldRelativeEpsilon = 1e-9);
bool saveAsciiStl(const std::string& path, const Mesh& mesh,
                  const std::string& solidName = "mesh",
                  std::string* error = nullptr);

double triangleArea(const Vec3& a, const Vec3& b, const Vec3& c);
Vec3 triangleNormal(const Vec3& a, const Vec3& b, const Vec3& c);
std::vector<std::pair<int, int>> uniqueEdges(const Mesh& mesh);

}  // namespace lq
