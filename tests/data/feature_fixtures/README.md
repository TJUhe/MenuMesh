# 特征测试模型

这里保存特征识别和特征保护简化测试使用的固定 OBJ fixture。它们作为源数据直接维护，仓库不再保留生成脚本。

- `coaxial_hole_plate.obj`：竖直环形板，孔半径为 0.6，包含两个同轴圆形边界 loop。
- `tilted_coaxial_hole_plate.obj`：孔轴方向为 `(0.35, 0.2, 1.0)` 的环形板，用于检查非 Z 轴圆 loop 法向和中心线同轴性。
- `eccentric_hole_plate.obj`：上下表面孔半径相同，但中心故意偏移，用作非同轴负例。
- `elliptical_hole_plate.obj`：带两个真实椭圆 loop 的竖直板，长半径 0.8、短半径 0.45。
- `near_circular_hole_plate.obj`：轻微椭圆孔，长半径 0.62、短半径 0.59，用于近圆分类。
- `boss_pocket_plate.obj`：一个凸起矩形 boss 和一个凹陷 pocket，用于平面 patch、凸/凹硬边测试。
