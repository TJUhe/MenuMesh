# 特征标签 Fixture

这些 CSV 文件是 `manumesh feature-benchmark` 使用的小型真实值标签。

- 边行使用 `edge,a,b`，或向后兼容的简写 `a,b`；fixture 由 ManuMesh 加载后，顶点索引从 0 开始。OBJ fixture 可能被 `Mesh::removeUnusedVertices()` 重映射，因此这些索引不一定等于原始 OBJ 行号减一。
- 可选的 junction 行使用 `junction,id`。
- 可选的 continuation 行使用 `branch,junction,neighborA,neighborB`。
- 可选的面标签使用 `face_patch,faceId,patchId`。Patch ID 可任意指定；基准测试比较带标签的相邻面是否属于相同/不同 patch，因此检测器重新编号 patch 不会受罚。
- `coaxial_hole_plate_inner_top_edges.csv` 标记 `tests/data/feature_fixtures/coaxial_hole_plate.obj` 中顶部内侧圆孔的环。
- `FeatureDetection.FixtureBenchmarkUsesCoaxialHoleGroundTruthLabels` 使用此文件执行带标签的精度测试。数据集冒烟测试不会取代这项 precision/recall 断言。
- `elliptical_hole_plate_inner_top_edges.csv` 标记顶部内侧椭圆的 40 条边，用于非圆形 primitive 基准测试。
- `boss_pocket_primary_edges.csv` 标记轴对齐 boss/pocket fixture 中全部 60 条非共面流形边，覆盖凸面和凹面 CAD 边。
- `multi_junction_polygon_edges.csv` 标记确定性合成 fixture 中一个八边形环和三个已知分支 junction。
