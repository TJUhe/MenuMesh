# MeshCore 库工作区

`vs2019-debug-meshcore` 是独立于 `vs2019-debug` 和 `vs2019-debug-full` 的固定配置。它不修改完整
库、SDK 或测试行为，只编译面向日常网格处理的功能实现集。

```powershell
cmake --preset vs2019-debug-meshcore
cmake --build --preset vs2019-debug-meshcore --parallel
```

生成的解决方案位于 `build/vs2019-debug-meshcore/MeshCore.sln`，其中只有以下功能项目：

| 项目 | 内容 |
| --- | --- |
| `Core` | 唯一的共享库项目；汇总全部对象，生成 `MeshCore.dll` 和导入库。 |
| `Common` | 私有几何谓词、邻域查询和空间加速基础。 |
| `MeshEdit` | 动态拓扑和局部编辑基础。 |
| `IO` | STL/OBJ 导入、三角化和 STL/OBJ 写出。 |
| `Analysis` | 网格统计和质量分析。 |
| `FeatureDetection` | 边界、非流形边、锐边和特征分析。 |
| `Simplification` | 特征保护 QEM 边坍缩。 |

`ALL_BUILD` 是 CMake 的聚合构建入口。此 preset 关闭自动重生成，所以它不创建 `ZERO_CHECK`；修改
CMake 或增删 MeshCore 文件后，必须重新运行前述 `cmake --preset` 命令。

## 产物

构建只交付以下库文件：

```text
build/vs2019-debug-meshcore/bin/Debug/MeshCore.dll
build/vs2019-debug-meshcore/lib/Debug/MeshCore.lib
```

只有 `Core` 是共享库目标。`Common`、`MeshEdit`、`IO`、`Analysis`、`FeatureDetection` 和
`Simplification` 是 CMake `OBJECT` 目标，用于在 Visual Studio 中按源码职责浏览和组织；它们的
对象文件会并入 `MeshCore`，不会作为独立 DLL 或 `.lib` 交付。该解决方案不包含 CLI、测试、示例或
第三方库的 Visual Studio 项目。

## 范围

精简版本保留 STL/OBJ 读写、网格分析、特征检测和特征保护 QEM 简化等日常处理能力，但不作为完整
SDK 或命令行产品交付。需要 C/C++ API、测试、示例、MMPD 超大网格、纹理坐标简化或并行后端时，使用
`vs2019-debug` 或 `vs2019-debug-full`。
