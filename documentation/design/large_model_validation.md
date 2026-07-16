# 大模型验证记录

本文按 ManuMesh 当前仓库数据和任务更新。大模型验证的目的，是在 10k 面以上的公开 STL 上捕捉新增孔洞、非流形边、严重质量退化和性能回归。

大模型验证主要覆盖 [`algorithm_essence.md`](algorithm_essence.md) 中的工程层问题：候选队列是否会被 stale candidate 拖慢，动态拓扑是否能稳定更新，局部自交空间索引是否有效，硬过滤器是否让目标面数无法达到。

## 数据来源

| 路径 | 内容 |
| --- | --- |
| `tests/data/external/large/*.stl` | 10 个较大公开模型，二进制 STL。 |
| `tests/data/external/thingi10k/*.stl` | 97 个真实世界模型子集。 |
| `tests/data/external/casting_aimshape_2014.stl` | 工业 Casting 风格模型。 |
| `tests/data/external/fandisk_2014.stl` | 硬边 Fandisk 模型。 |

性能测试会检查二进制 STL 布局是否满足 `84 + 50 * triangle_count` 字节格式，然后再运行简化。

## 构建和测试

非性能回归：

```powershell
$buildDir = "build/mingw-ninja-release"
cmake -S . -B $buildDir -G Ninja `
  -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_C_COMPILER=gcc `
  -DCMAKE_CXX_COMPILER=g++ `
  -DMANUMESH_BUILD_PERFORMANCE_TESTS=OFF
cmake --build $buildDir --parallel
cmake -E chdir $buildDir ctest -LE performance --output-on-failure
```

性能测试需要单独配置：

```powershell
$perfBuildDir = "build/mingw-ninja-debug-performance"
cmake -S . -B $perfBuildDir -G Ninja `
  -DCMAKE_BUILD_TYPE=Debug `
  -DCMAKE_C_COMPILER=gcc `
  -DCMAKE_CXX_COMPILER=g++ `
  -DMANUMESH_BUILD_PERFORMANCE_TESTS=ON
cmake --build $perfBuildDir --target manumesh_performance_tests --parallel
cmake -E chdir $perfBuildDir ctest -L performance --output-on-failure
```

## 手动抽样命令

90% 保守简化：

```powershell
$exe = "build/mingw-ninja-release/bin/manumesh.exe"
& $exe simplify `
  tests\data\external\large\<model>.stl `
  tests\output\large_validation\<model>_line_090.stl `
  --method line --ratio 0.9 `
  --line-weight 1e-3 --weight-mode dihedral --feature-boost 0.08 `
  --feature-angle-deg 25 --samples 120 `
  --metrics-csv tests\output\large_validation\<model>_line_090_metrics.csv
```

50% 更深简化：

```powershell
$exe = "build/mingw-ninja-release/bin/manumesh.exe"
& $exe simplify `
  tests\data\external\large\<model>.stl `
  tests\output\large_validation\<model>_line_050.stl `
  --method line --ratio 0.5 `
  --line-weight 1e-3 --weight-mode dihedral --feature-boost 0.08 `
  --feature-angle-deg 25 --samples 160 `
  --metrics-csv tests\output\large_validation\<model>_line_050_metrics.csv
```

## 历史观察

早期验证中，10 个 large 模型的 90% 和代表性 50% 简化没有新增非流形边；已有开放边界一般保持稳定或减少。加入增量 incident-face 邻接后，大模型运行时间显著下降。

## 当前注意事项

- ManuMesh 当前 VS Code 任务没有 `run: large validation 100 stl` 这类旧入口；大模型验证应使用 `test: mingw+ninja release performance`、`test: mingw+ninja release full` 或手动 CLI 批处理。
- 部分公开模型输入本身就有边界，验证时应比较 boundary delta，而不是要求输出边界为 0。
- 下一步更严格的质量保护应继续关注 normal flip、极差三角形和全局误差 envelope。
