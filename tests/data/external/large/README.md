# 大型公共网格验证集

包含超过 10k 个三角面的 STL 网格，用于手动和批量验证简化拓扑、质量及距离指标。

大多数模型来自 Alec Jacobson 的 `common-3d-test-models` 仓库：

https://github.com/alecjacobson/common-3d-test-models

该仓库收集常用的几何处理测试模型，并在已知时记录原始来源。此处文件由其
`data/` 目录中的直接下载内容转换为二进制 STL，仅为保持本地文件名简洁而重命名。

| 本地文件 | 源文件 | 三角面数 | 备注 |
| --- | --- | ---: | --- |
| `armadillo.stl` | `armadillo.obj` | 99976 | Stanford 风格扫描有机模型。 |
| `beast.stl` | `beast.obj` | 64618 | 多边形源网格；STL 转换前由加载器三角化。 |
| `beetle_alt.stl` | `beetle-alt.obj` | 38656 | 带有现存边界环的开放网格。 |
| `cheburashka.stl` | `cheburashka.obj` | 13334 | 此大型集合中最小的模型。 |
| `happy.stl` | `happy.obj` | 98601 | Stanford Happy Buddha 风格模型；已有少量边界。 |
| `horse.stl` | `horse.obj` | 96966 | Cyberware 风格扫描有机模型。 |
| `max_planck.stl` | `max-planck.obj` | 99991 | 带有现存边界边的头像扫描。 |
| `nefertiti.stl` | `nefertiti.obj` | 99938 | 头像扫描。 |
| `rocker_arm.stl` | `rocker-arm.obj` | 20088 | 机械零件。 |
| `stanford_bunny.stl` | `stanford-bunny.obj` | 69451 | Stanford Bunny 源网格；已有边界边。 |

上一级目录中的相关公共 CAD 风格 fixture：

- `../fandisk_2014.stl`
- `../casting_aimshape_2014.stl`

这些文件用于 Tsuchie 和 Higashi 2014 年的 normal-tensor 特征线实验。
