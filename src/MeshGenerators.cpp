#include "MeshGenerators.h"

#include <algorithm>
#include <cmath>
#include <random>
#include <utility>
#include <vector>

namespace lq {
namespace {

void addGridFaces(int n, int base, Mesh& mesh, bool flip = false) {
  for (int y = 0; y < n; ++y) {
    for (int x = 0; x < n; ++x) {
      const int v00 = base + y * (n + 1) + x;
      const int v10 = v00 + 1;
      const int v01 = v00 + (n + 1);
      const int v11 = v01 + 1;
      if (!flip) {
        mesh.faces.push_back({{v00, v10, v11}});
        mesh.faces.push_back({{v00, v11, v01}});
      } else {
        mesh.faces.push_back({{v00, v11, v10}});
        mesh.faces.push_back({{v00, v01, v11}});
      }
    }
  }
}

void addGridCellFaces(int n, int base, int x, int y, Mesh& mesh,
                      bool flip = false) {
  const int v00 = base + y * (n + 1) + x;
  const int v10 = v00 + 1;
  const int v01 = v00 + (n + 1);
  const int v11 = v01 + 1;
  if (!flip) {
    mesh.faces.push_back({{v00, v10, v11}});
    mesh.faces.push_back({{v00, v11, v01}});
  } else {
    mesh.faces.push_back({{v00, v11, v10}});
    mesh.faces.push_back({{v00, v01, v11}});
  }
}

double coordFromIndex(int i, int n, double size, bool clustered) {
  const double t = static_cast<double>(i) / static_cast<double>(n);
  double u = t;
  if (clustered) {
    u = 0.5 * (std::tanh((t - 0.5) * 2.4) / std::tanh(1.2) + 1.0);
  }
  return (u - 0.5) * size;
}

void appendFaceGrid(Mesh& mesh, int n, double size, const Vec3& origin,
                    const Vec3& ux, const Vec3& uy, bool flip = false) {
  const int base = static_cast<int>(mesh.vertices.size());
  for (int y = 0; y <= n; ++y) {
    for (int x = 0; x <= n; ++x) {
      const double sx = (static_cast<double>(x) / n - 0.5) * size;
      const double sy = (static_cast<double>(y) / n - 0.5) * size;
      mesh.vertices.push_back(origin + sx * ux + sy * uy);
    }
  }
  addGridFaces(n, base, mesh, flip);
}

void appendRectGrid(Mesh& mesh, int nx, int ny, const Vec3& origin,
                    const Vec3& uxFull, const Vec3& uyFull, bool flip = false) {
  const int base = static_cast<int>(mesh.vertices.size());
  for (int y = 0; y <= ny; ++y) {
    for (int x = 0; x <= nx; ++x) {
      const double sx = static_cast<double>(x) / nx - 0.5;
      const double sy = static_cast<double>(y) / ny - 0.5;
      mesh.vertices.push_back(origin + sx * uxFull + sy * uyFull);
    }
  }
  for (int y = 0; y < ny; ++y) {
    for (int x = 0; x < nx; ++x) {
      const int v00 = base + y * (nx + 1) + x;
      const int v10 = v00 + 1;
      const int v01 = v00 + (nx + 1);
      const int v11 = v01 + 1;
      if (!flip) {
        mesh.faces.push_back({{v00, v10, v11}});
        mesh.faces.push_back({{v00, v11, v01}});
      } else {
        mesh.faces.push_back({{v00, v11, v10}});
        mesh.faces.push_back({{v00, v01, v11}});
      }
    }
  }
}

int addVertex(Mesh& mesh, const Vec3& p) {
  const int index = static_cast<int>(mesh.vertices.size());
  mesh.vertices.push_back(p);
  return index;
}

void addQuad(Mesh& mesh, int a, int b, int c, int d) {
  mesh.faces.push_back({{a, b, c}});
  mesh.faces.push_back({{a, c, d}});
}

void addCylinderWall(Mesh& mesh, const Vec3& center, double radius, double z0,
                     double z1, int segments, bool inward) {
  constexpr double pi = 3.141592653589793238462643383279502884;
  std::vector<int> lower(segments);
  std::vector<int> upper(segments);
  for (int i = 0; i < segments; ++i) {
    const double t = 2.0 * pi * static_cast<double>(i) / segments;
    lower[i] = addVertex(mesh, Vec3(center.x() + radius * std::cos(t),
                                    center.y() + radius * std::sin(t), z0));
    upper[i] = addVertex(mesh, Vec3(center.x() + radius * std::cos(t),
                                    center.y() + radius * std::sin(t), z1));
  }

  for (int i = 0; i < segments; ++i) {
    const int next = (i + 1) % segments;
    if (!inward) {
      addQuad(mesh, lower[i], lower[next], upper[next], upper[i]);
    } else {
      addQuad(mesh, lower[i], upper[i], upper[next], lower[next]);
    }
  }
}

bool insideAnyBoltHole(const Vec3& p, int holes, double boltRadius,
                       double holeRadius) {
  constexpr double pi = 3.141592653589793238462643383279502884;
  for (int i = 0; i < holes; ++i) {
    const double theta = 2.0 * pi * static_cast<double>(i) / holes;
    const Vec3 c(boltRadius * std::cos(theta), boltRadius * std::sin(theta), p.z());
    if ((p.head<2>() - c.head<2>()).norm() < holeRadius * 0.98) {
      return true;
    }
  }
  return false;
}

void addAnnularDisk(Mesh& mesh, double r0, double r1, double z, int rings,
                    int segments, bool top, int boltHoles = 0,
                    double boltRadius = 0.0, double boltHoleRadius = 0.0) {
  constexpr double pi = 3.141592653589793238462643383279502884;
  std::vector<std::vector<int>> index(
      rings + 1, std::vector<int>(static_cast<std::size_t>(segments), -1));
  for (int r = 0; r <= rings; ++r) {
    const double a = static_cast<double>(r) / rings;
    const double rr = r0 + (r1 - r0) * a;
    for (int i = 0; i < segments; ++i) {
      const double t = 2.0 * pi * static_cast<double>(i) / segments;
      index[r][i] = addVertex(mesh, Vec3(rr * std::cos(t), rr * std::sin(t), z));
    }
  }

  for (int r = 0; r < rings; ++r) {
    const double a0 = static_cast<double>(r) / rings;
    const double a1 = static_cast<double>(r + 1) / rings;
    const double rr0 = r0 + (r1 - r0) * a0;
    const double rr1 = r0 + (r1 - r0) * a1;
    for (int i = 0; i < segments; ++i) {
      const double t0 = 2.0 * pi * static_cast<double>(i) / segments;
      const double t1 = 2.0 * pi * static_cast<double>(i + 1) / segments;
      const Vec3 p00(rr0 * std::cos(t0), rr0 * std::sin(t0), z);
      const Vec3 p10(rr1 * std::cos(t0), rr1 * std::sin(t0), z);
      const Vec3 p01(rr0 * std::cos(t1), rr0 * std::sin(t1), z);
      const Vec3 p11(rr1 * std::cos(t1), rr1 * std::sin(t1), z);
      const Vec3 center = 0.25 * (p00 + p10 + p01 + p11);
      if (boltHoles > 0 &&
          insideAnyBoltHole(center, boltHoles, boltRadius, boltHoleRadius)) {
        continue;
      }
      const int v00 = index[r][i];
      const int v10 = index[r + 1][i];
      const int v01 = index[r][(i + 1) % segments];
      const int v11 = index[r + 1][(i + 1) % segments];
      if (top) {
        addQuad(mesh, v00, v10, v11, v01);
      } else {
        addQuad(mesh, v00, v01, v11, v10);
      }
    }
  }
}

Mesh generateLatheProfile(const std::vector<std::pair<double, double>>& rz,
                          int segments) {
  constexpr double pi = 3.141592653589793238462643383279502884;
  Mesh mesh;
  if (rz.size() < 2) {
    return mesh;
  }

  segments = std::max(16, segments);
  std::vector<std::vector<int>> index(
      rz.size(), std::vector<int>(static_cast<std::size_t>(segments), -1));
  for (int j = 0; j < static_cast<int>(rz.size()); ++j) {
    const double r = rz[j].first;
    const double z = rz[j].second;
    for (int i = 0; i < segments; ++i) {
      const double t = 2.0 * pi * static_cast<double>(i) / segments;
      index[j][i] = addVertex(mesh, Vec3(r * std::cos(t), r * std::sin(t), z));
    }
  }

  for (int j = 0; j + 1 < static_cast<int>(rz.size()); ++j) {
    for (int i = 0; i < segments; ++i) {
      const int next = (i + 1) % segments;
      addQuad(mesh, index[j][i], index[j][next], index[j + 1][next],
              index[j + 1][i]);
    }
  }

  mesh.removeUnusedVertices();
  return mesh;
}

}  // namespace

Mesh generatePlaneGrid(int n, double size, bool clustered) {
  n = std::max(2, n);
  Mesh mesh;
  mesh.vertices.reserve(static_cast<std::size_t>((n + 1) * (n + 1)));

  for (int y = 0; y <= n; ++y) {
    for (int x = 0; x <= n; ++x) {
      mesh.vertices.emplace_back(coordFromIndex(x, n, size, clustered),
                                 coordFromIndex(y, n, size, clustered), 0.0);
    }
  }
  addGridFaces(n, 0, mesh);
  return mesh;
}

Mesh generateHolePlaneGrid(int n, double size, double radius) {
  n = std::max(4, n);
  Mesh mesh;
  mesh.vertices.reserve(static_cast<std::size_t>((n + 1) * (n + 1)));

  for (int y = 0; y <= n; ++y) {
    for (int x = 0; x <= n; ++x) {
      mesh.vertices.emplace_back((static_cast<double>(x) / n - 0.5) * size,
                                 (static_cast<double>(y) / n - 0.5) * size,
                                 0.0);
    }
  }

  for (int y = 0; y < n; ++y) {
    for (int x = 0; x < n; ++x) {
      const double cx = (static_cast<double>(x) + 0.5) / n - 0.5;
      const double cy = (static_cast<double>(y) + 0.5) / n - 0.5;
      if (std::sqrt(cx * cx + cy * cy) * size < radius) {
        continue;
      }
      addGridCellFaces(n, 0, x, y, mesh);
    }
  }
  mesh.removeUnusedVertices();
  return mesh;
}

Mesh generateRidgeGrid(int n, double size, double height) {
  n = std::max(2, n);
  Mesh mesh;
  mesh.vertices.reserve(static_cast<std::size_t>((n + 1) * (n + 1)));

  for (int y = 0; y <= n; ++y) {
    for (int x = 0; x <= n; ++x) {
      const double px = (static_cast<double>(x) / n - 0.5) * size;
      const double py = (static_cast<double>(y) / n - 0.5) * size;
      const double z = height * (1.0 - std::abs(px) / (size * 0.5));
      mesh.vertices.emplace_back(px, py, z);
    }
  }
  addGridFaces(n, 0, mesh);
  return mesh;
}

Mesh generateNoisyPlaneGrid(int n, double size, double noiseAmplitude) {
  n = std::max(2, n);
  Mesh mesh;
  mesh.vertices.reserve(static_cast<std::size_t>((n + 1) * (n + 1)));
  std::mt19937 rng(42);
  std::normal_distribution<double> noise(0.0, noiseAmplitude);

  for (int y = 0; y <= n; ++y) {
    for (int x = 0; x <= n; ++x) {
      const double px = (static_cast<double>(x) / n - 0.5) * size;
      const double py = (static_cast<double>(y) / n - 0.5) * size;
      const bool boundary = x == 0 || y == 0 || x == n || y == n;
      mesh.vertices.emplace_back(px, py, boundary ? 0.0 : noise(rng));
    }
  }
  addGridFaces(n, 0, mesh);
  return mesh;
}

Mesh generateSineTerrainGrid(int n, double size) {
  n = std::max(4, n);
  Mesh mesh;
  mesh.vertices.reserve(static_cast<std::size_t>((n + 1) * (n + 1)));
  constexpr double pi = 3.141592653589793238462643383279502884;

  for (int y = 0; y <= n; ++y) {
    for (int x = 0; x <= n; ++x) {
      const double px = (static_cast<double>(x) / n - 0.5) * size;
      const double py = (static_cast<double>(y) / n - 0.5) * size;
      const double u = px / size + 0.5;
      const double v = py / size + 0.5;
      const double z = 0.18 * std::sin(4.0 * pi * u) * std::sin(3.0 * pi * v) +
                       0.05 * std::sin(11.0 * pi * u);
      mesh.vertices.emplace_back(px, py, z);
    }
  }
  addGridFaces(n, 0, mesh);
  return mesh;
}

Mesh generateTerraceGrid(int n, double size) {
  n = std::max(4, n);
  Mesh mesh;
  mesh.vertices.reserve(static_cast<std::size_t>((n + 1) * (n + 1)));

  for (int y = 0; y <= n; ++y) {
    for (int x = 0; x <= n; ++x) {
      const double px = (static_cast<double>(x) / n - 0.5) * size;
      const double py = (static_cast<double>(y) / n - 0.5) * size;
      const double t = std::floor((px / size + 0.5) * 7.0) / 7.0;
      const double z = 0.55 * t + 0.04 * std::sin(8.0 * py);
      mesh.vertices.emplace_back(px, py, z);
    }
  }
  addGridFaces(n, 0, mesh);
  return mesh;
}

Mesh generateBumpGrid(int n, double size) {
  n = std::max(4, n);
  Mesh mesh;
  mesh.vertices.reserve(static_cast<std::size_t>((n + 1) * (n + 1)));

  for (int y = 0; y <= n; ++y) {
    for (int x = 0; x <= n; ++x) {
      const double px = (static_cast<double>(x) / n - 0.5) * size;
      const double py = (static_cast<double>(y) / n - 0.5) * size;
      const double r2 = px * px + py * py;
      const double dome = 0.38 * std::exp(-4.0 * r2);
      const double cross = 0.08 * std::exp(-180.0 * px * px) +
                           0.08 * std::exp(-180.0 * py * py);
      mesh.vertices.emplace_back(px, py, dome + cross);
    }
  }
  addGridFaces(n, 0, mesh);
  return mesh;
}

Mesh generateCylinderGrid(int radialSegments, int heightSegments, double radius,
                          double height) {
  radialSegments = std::max(8, radialSegments);
  heightSegments = std::max(2, heightSegments);
  Mesh mesh;
  constexpr double pi = 3.141592653589793238462643383279502884;

  for (int z = 0; z <= heightSegments; ++z) {
    const double pz = (static_cast<double>(z) / heightSegments - 0.5) * height;
    for (int i = 0; i < radialSegments; ++i) {
      const double theta = 2.0 * pi * static_cast<double>(i) / radialSegments;
      mesh.vertices.emplace_back(radius * std::cos(theta), radius * std::sin(theta),
                                 pz);
    }
  }

  for (int z = 0; z < heightSegments; ++z) {
    for (int i = 0; i < radialSegments; ++i) {
      const int j = (i + 1) % radialSegments;
      const int v00 = z * radialSegments + i;
      const int v10 = z * radialSegments + j;
      const int v01 = (z + 1) * radialSegments + i;
      const int v11 = (z + 1) * radialSegments + j;
      mesh.faces.push_back({{v00, v10, v11}});
      mesh.faces.push_back({{v00, v11, v01}});
    }
  }

  const int bottomCenter = static_cast<int>(mesh.vertices.size());
  mesh.vertices.emplace_back(0.0, 0.0, -0.5 * height);
  const int topCenter = static_cast<int>(mesh.vertices.size());
  mesh.vertices.emplace_back(0.0, 0.0, 0.5 * height);
  for (int i = 0; i < radialSegments; ++i) {
    const int j = (i + 1) % radialSegments;
    mesh.faces.push_back({{bottomCenter, j, i}});
    mesh.faces.push_back({{topCenter, heightSegments * radialSegments + i,
                           heightSegments * radialSegments + j}});
  }
  return mesh;
}

Mesh generateTorusGrid(int majorSegments, int minorSegments, double majorRadius,
                       double minorRadius) {
  majorSegments = std::max(8, majorSegments);
  minorSegments = std::max(6, minorSegments);
  Mesh mesh;
  constexpr double pi = 3.141592653589793238462643383279502884;

  for (int i = 0; i < majorSegments; ++i) {
    const double u = 2.0 * pi * static_cast<double>(i) / majorSegments;
    for (int j = 0; j < minorSegments; ++j) {
      const double v = 2.0 * pi * static_cast<double>(j) / minorSegments;
      const double r = majorRadius + minorRadius * std::cos(v);
      mesh.vertices.emplace_back(r * std::cos(u), r * std::sin(u),
                                 minorRadius * std::sin(v));
    }
  }

  for (int i = 0; i < majorSegments; ++i) {
    const int in = (i + 1) % majorSegments;
    for (int j = 0; j < minorSegments; ++j) {
      const int jn = (j + 1) % minorSegments;
      const int v00 = i * minorSegments + j;
      const int v10 = in * minorSegments + j;
      const int v01 = i * minorSegments + jn;
      const int v11 = in * minorSegments + jn;
      mesh.faces.push_back({{v00, v10, v11}});
      mesh.faces.push_back({{v00, v11, v01}});
    }
  }
  return mesh;
}

Mesh generateCubeGrid(int n, double size) {
  n = std::max(1, n);
  Mesh mesh;
  const double half = 0.5 * size;
  appendFaceGrid(mesh, n, size, Vec3(0, 0, half), Vec3(1, 0, 0), Vec3(0, 1, 0));
  appendFaceGrid(mesh, n, size, Vec3(0, 0, -half), Vec3(1, 0, 0),
                 Vec3(0, -1, 0));
  appendFaceGrid(mesh, n, size, Vec3(half, 0, 0), Vec3(0, 1, 0),
                 Vec3(0, 0, 1));
  appendFaceGrid(mesh, n, size, Vec3(-half, 0, 0), Vec3(0, -1, 0),
                 Vec3(0, 0, 1));
  appendFaceGrid(mesh, n, size, Vec3(0, half, 0), Vec3(-1, 0, 0),
                 Vec3(0, 0, 1));
  appendFaceGrid(mesh, n, size, Vec3(0, -half, 0), Vec3(1, 0, 0),
                 Vec3(0, 0, 1));
  return mesh;
}

Mesh generateThinFinGrid(int n, double size) {
  n = std::max(4, n);
  Mesh mesh = generatePlaneGrid(n, size, false);

  const double finHeight = 0.75 * size;
  const double finLength = 0.85 * size;
  const double finThickness = 0.035 * size;
  const int nx = std::max(4, n / 2);
  const int ny = std::max(4, n / 3);
  const int nt = 1;

  appendRectGrid(mesh, nx, ny, Vec3(0.0, 0.0, 0.5 * finHeight),
                 Vec3(0.0, finLength, 0.0), Vec3(0.0, 0.0, finHeight));
  appendRectGrid(mesh, nx, ny, Vec3(finThickness, 0.0, 0.5 * finHeight),
                 Vec3(0.0, finLength, 0.0), Vec3(0.0, 0.0, finHeight), true);
  appendRectGrid(mesh, nt, ny, Vec3(0.5 * finThickness, -0.5 * finLength,
                                    0.5 * finHeight),
                 Vec3(finThickness, 0.0, 0.0), Vec3(0.0, 0.0, finHeight));
  appendRectGrid(mesh, nt, ny, Vec3(0.5 * finThickness, 0.5 * finLength,
                                    0.5 * finHeight),
                 Vec3(finThickness, 0.0, 0.0), Vec3(0.0, 0.0, finHeight),
                 true);
  appendRectGrid(mesh, nt, nx, Vec3(0.5 * finThickness, 0.0, finHeight),
                 Vec3(finThickness, 0.0, 0.0), Vec3(0.0, finLength, 0.0));
  return mesh;
}

Mesh generateFlangedBossGrid(int n) {
  const int segments = std::max(48, n);
  const int boltSegments = std::max(18, n / 3);
  const int radialRings = std::max(10, n / 5);
  constexpr int boltHoles = 6;

  const double flangeRadius = 1.15;
  const double flangeThickness = 0.18;
  const double bossRadius = 0.42;
  const double bossHeight = 0.58;
  const double boreRadius = 0.16;
  const double boltRadius = 0.78;
  const double boltHoleRadius = 0.105;

  Mesh mesh;
  addAnnularDisk(mesh, bossRadius, flangeRadius, 0.0, radialRings, segments, true,
                 boltHoles, boltRadius, boltHoleRadius);
  addAnnularDisk(mesh, boreRadius, flangeRadius, -flangeThickness, radialRings,
                 segments, false, boltHoles, boltRadius, boltHoleRadius);
  addAnnularDisk(mesh, boreRadius, bossRadius, bossHeight, std::max(4, n / 10),
                 segments, true);

  addCylinderWall(mesh, Vec3(0, 0, 0), flangeRadius, -flangeThickness, 0.0,
                  segments, false);
  addCylinderWall(mesh, Vec3(0, 0, 0), bossRadius, 0.0, bossHeight, segments,
                  false);
  addCylinderWall(mesh, Vec3(0, 0, 0), boreRadius, -flangeThickness, bossHeight,
                  segments, true);

  constexpr double pi = 3.141592653589793238462643383279502884;
  for (int i = 0; i < boltHoles; ++i) {
    const double theta = 2.0 * pi * static_cast<double>(i) / boltHoles;
    const Vec3 center(boltRadius * std::cos(theta), boltRadius * std::sin(theta),
                      0.0);
    addCylinderWall(mesh, center, boltHoleRadius, -flangeThickness, 0.0,
                    boltSegments, true);
  }

  mesh.removeUnusedVertices();
  return mesh;
}

Mesh generateSteppedShaftGrid(int n) {
  const int segments = std::max(64, n);
  const std::vector<std::pair<double, double>> profile = {
      {0.32, -1.15}, {0.32, -0.72}, {0.48, -0.72}, {0.48, -0.20},
      {0.36, -0.20}, {0.36, 0.32},  {0.58, 0.32},  {0.58, 0.78},
      {0.42, 0.78},  {0.42, 1.08},
  };
  return generateLatheProfile(profile, segments);
}

Mesh generatePipeCouplingGrid(int n) {
  const int segments = std::max(72, n);
  const std::vector<std::pair<double, double>> profile = {
      {0.38, -1.00}, {0.72, -1.00}, {0.72, -0.42},
      {0.58, -0.42}, {0.58, 0.42},  {0.72, 0.42},
      {0.72, 1.00},  {0.38, 1.00},  {0.38, -1.00},
  };
  return generateLatheProfile(profile, segments);
}

Mesh generatePulleyGrid(int n) {
  const int segments = std::max(72, n);
  const std::vector<std::pair<double, double>> profile = {
      {0.26, -0.58}, {0.62, -0.58}, {0.76, -0.26},
      {0.60, 0.0},   {0.76, 0.26},  {0.62, 0.58},
      {0.26, 0.58},  {0.26, -0.58},
  };
  return generateLatheProfile(profile, segments);
}

bool generateMeshByName(const std::string& type, int n, Mesh& mesh,
                        std::string* error) {
  if (type == "plane") {
    mesh = generatePlaneGrid(n, 2.0, false);
  } else if (type == "clustered-plane") {
    mesh = generatePlaneGrid(n, 2.0, true);
  } else if (type == "hole-plane") {
    mesh = generateHolePlaneGrid(n, 2.0, 0.34);
  } else if (type == "ridge") {
    mesh = generateRidgeGrid(n, 2.0, 0.5);
  } else if (type == "noisy-plane") {
    mesh = generateNoisyPlaneGrid(n, 2.0, 0.035);
  } else if (type == "sine-terrain") {
    mesh = generateSineTerrainGrid(n, 2.0);
  } else if (type == "terrace") {
    mesh = generateTerraceGrid(n, 2.0);
  } else if (type == "bump") {
    mesh = generateBumpGrid(n, 2.0);
  } else if (type == "cylinder") {
    mesh = generateCylinderGrid(std::max(16, n), std::max(4, n / 3), 0.65, 1.7);
  } else if (type == "torus") {
    mesh = generateTorusGrid(std::max(16, n), std::max(8, n / 3), 0.7, 0.23);
  } else if (type == "cube") {
    mesh = generateCubeGrid(std::max(1, n / 3), 2.0);
  } else if (type == "thin-fin") {
    mesh = generateThinFinGrid(n, 2.0);
  } else if (type == "flange" || type == "flanged-boss") {
    mesh = generateFlangedBossGrid(std::max(48, n));
  } else if (type == "stepped-shaft") {
    mesh = generateSteppedShaftGrid(std::max(64, n));
  } else if (type == "pipe-coupling") {
    mesh = generatePipeCouplingGrid(std::max(72, n));
  } else if (type == "pulley") {
    mesh = generatePulleyGrid(std::max(72, n));
  } else {
    if (error) {
      *error =
          "Unknown generator type. Use plane, clustered-plane, hole-plane, "
          "ridge, noisy-plane, sine-terrain, terrace, bump, cylinder, torus, "
          "cube, thin-fin, flange, stepped-shaft, pipe-coupling, or pulley.";
    }
    return false;
  }
  return true;
}

}  // namespace lq
