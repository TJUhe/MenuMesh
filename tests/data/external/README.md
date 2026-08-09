# 外部工业网格 Fixture

这些 STL 文件是从公共模型仓库下载的二进制 STL 回归 fixture。它们让 GoogleTest
能够在不需要网络访问的情况下测试真实、非合成的工业风格几何体。

## NASA 3D 资源

NASA 文件下载自 NASA 3D Resources：

https://github.com/nasa/NASA-3D-Resources

NASA 仓库声明这些资源可免费下载和使用，并且免费且不受版权限制。

已下载的 fixture：

| 文件 | NASA 源路径 | 用途 |
| --- | --- | --- |
| `nasa_antenna_azimuth_track.stl` | `3D Printing/Beam Waveguide Deep Space Station Antenna/Azimuth track.stl` | 紧凑的机械环/轨道模型，直接用于方法比较测试，并作为 `external_ring_track` validate-features 案例的默认输入。 |
| `nasa_cubesat_middle.stl` | `3D Printing/CubeSat/CubeSat middle.stl` | 较大的盒状航天器部件，用于负载、统计和特征拓扑覆盖。 |
| `nasa_mars2020_wheel.stl` | `3D Printing/Mars 2020 5 inch wheel/Mars 2020 5 inch wheel.stl` | 带圆形和薄壁特征的较大轮模型，用于负载、统计和特征拓扑覆盖。 |

## OpenFOAM 教程法兰

`openfoam_flange.stl` 从 OpenFOAM 公共的
`tutorials/resources/geometry/flange.stl.gz` fixture 解压得到：

https://github.com/OpenFOAM/OpenFOAM-6/blob/master/tutorials/resources/geometry/flange.stl.gz

OpenFOAM 按其仓库 `COPYING` 文件所述的 GNU General Public License 发布。此 fixture
保存在 `tests/data/external/` 下，使法兰验证路径使用完成的外部模型，而不是本项目
生成的程序化网格。

## 默认特征验证输入

`manumesh validate-features` 在运行线/曲线简化前，会将完成的外部 STL fixture 复制到
`tests/output/generated_inputs/`：

- `thingi10k/thingi10k_79361_zheng3_tinkeriffic_40mm_spool_spindle.stl` as
  `external_spindle.stl`.
- `nasa_antenna_azimuth_track.stl` as `external_ring_track.stl`.
- `thingi10k/thingi10k_318045_moko_mini_pulley.stl` as
  `external_pulley.stl`.
- `openfoam_flange.stl` as `external_flange.stl`.

这些案例有意替换早期的程序化验证网格，使 SDK 验证路径能够测试导入的几何体。

## 公共 2014 CAD 模型 Fixture

这些 STL 文件是公共 CAD 风格模型，用于复现 Tsuchie 和 Higashi 的论文
"Extraction of Surface-feature Lines on Meshes Using Normal Tensor Framework"（CAD&A
2014）中的模型范围。它们在保持三角化几何完整的同时转换为二进制 STL，以便紧凑地
存储在仓库中：

| 文件 | 来源 | 用途 |
| --- | --- | --- |
| `fandisk_2014.stl` | Common 3D Test Models 的 Fandisk 镜像 | 对应论文图 9 使用的 Fandisk CAD 模型。 |
| `casting_aimshape_2014.stl` | LIRIS Mesh Benchmark 的 AIM@SHAPE Casting 镜像 | 对应论文图 10 使用的 Casting CAD 模型，并重点覆盖圆形/圆角类特征环。 |
