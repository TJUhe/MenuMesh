# 精简工作区

`vs2019-debug-slim` 是独立于 `vs2019-debug` 和 `vs2019-debug-full` 的固定配置。它不修改完整
库、SDK 或测试行为，只编译面向日常网格处理的最小实现集。

```powershell
cmake --preset vs2019-debug-slim
cmake --build --preset vs2019-debug-slim --parallel
```

生成的解决方案位于 `build/vs2019-debug-slim/ManuMesh.sln`，其中只有以下可构建项目：

| 项目 | 内容 |
| --- | --- |
| `Core` | `Mesh`、拓扑、几何校验和基础几何计算。 |
| `IO` | STL/OBJ 导入、三角化和 STL/OBJ 写出。 |
| `Analysis` | 顶点数、面数、面积和包围盒统计。 |
| `FeatureDetection` | 边界、非流形边和二面角锐边检测。 |
| `Simplification` | 边界/锐边保护的标准平面 QEM 边坍缩。 |
| `CLI` | `mesh-slim.exe` 命令行入口。 |

`ALL_BUILD` 是 CMake 的聚合构建入口。此 preset 关闭自动重生成，所以它不创建 `ZERO_CHECK`；修改
CMake 或增删 slim 文件后，必须重新运行前述 `cmake --preset` 命令。

## 命令

```powershell
mesh-slim version
mesh-slim stats input.obj
mesh-slim feature-report input.obj --angle 45
mesh-slim simplify input.obj output.stl --ratio 0.5 --feature-angle 45
```

`simplify` 默认冻结所有边界顶点与锐边顶点。若约束使目标面数不可达，输出的 `stop=constraints-blocked`
是正常的安全终止。可显式使用 `--no-preserve-boundary` 或 `--no-preserve-features` 放宽对应限制。

## 范围

精简版本支持干净的三角形 STL/OBJ 网格。它不接受带有效逐角纹理坐标的 OBJ 简化，也会在发现非流形
边时停止简化。完整版本保留纹理保护、法向张量、特征图/曲线/基元恢复、质量细化、并行、超大网格和
C/C++ SDK；需要这些能力时使用 `vs2019-debug` 或 `vs2019-debug-full`。
