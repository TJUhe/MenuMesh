# ManuMesh 工业网格库定位

ManuMesh 当前是一个面向增材制造三角网格处理的 C++/C SDK 原型，已经具备向工业多边形网格几何内核演进的骨架，但还不是完整工业 CAD/mesh kernel。当前库目标为 `manumesh`，CLI 为 `manumesh.exe`。

当前算法定位见 [`algorithm_essence.md`](algorithm_essence.md)：ManuMesh 的核心不是单一 QEM 公式，而是候选排序、三角网格特征图和硬合法性过滤共同组成的 decimation 模块。

## 当前可作为库交付的内容

- CMake 构建和安装目标。
- `manumesh` 共享库或静态库，作为 ManuMesh 当前 ABI 兼容交付物。
- C++ 公共头：`core`、`io`、`algorithms/feature_detection`、`algorithms/simplification`。
- C ABI：`api/CApi.h`。
- CLI：`manumesh.exe`，用于批处理、验证和示例。
- 示例：C++ SDK 和 C ABI consumer。
- 回归测试：258 个启用的非性能 CTest（其中 `ctest -LE "performance|external"` 快速套件 247 个，external 大网格用例 11 个），另有 4 个 performance 用例的独立构建路径。分层与命令见 [`testing_strategy.md`](testing_strategy.md)。

## 当前核心能力

| 能力 | 当前状态 |
| --- | --- |
| 网格交换 | `Mesh`、`PlainMesh`、STL/OBJ 读写；OBJ 读取支持多边形三角化并保留逐角 `vt`，`faceTexCoords` 携带角拥有的逐面 UV。 |
| 拓扑分析 | boundary、non-manifold、edge/face/vertex 统计。 |
| 特征检测 | boundary、dihedral、normal-tensor、opt-in smooth-curvature、loop、circle/ellipse fitting。 |
| 简化 | standard QEM、line quadrics、ratio/face target、sweep；opt-in 纹理感知排序与 UV chart 保护（仅 C++ API）。 |
| 保护 | boundary、feature curves、topology link condition、triangle quality、normal deviation、local error、local intersections；opt-in UV chart/有符号 UV 面积保护。 |
| 集成 | Eigen-backed C++ API、Eigen-free `PlainMesh` C++ 入口、C ABI、SDK 安装、示例工程。 |
| 内部诊断 | 可选 Debug-only HTML wireframe 辅助工具；仅用于开发排查，不属于 SDK 合约。 |

## 与完整工业几何内核的差距

还缺少：

- 半边/可编辑拓扑内核。
- 完整属性传播和源面/区域映射（UV 已通过 `faceTexCoords` 与 opt-in 纹理保护落地第一步；法线、颜色、source face id 仍缺）。
- 修复、补洞、定向、make-manifold。
- 布尔、切割、offset/thickening。
- 严格全局误差 envelope。
- CAD/B-Rep 语义特征识别。
- 多平台二进制发布和 ABI 兼容矩阵。

## 工程原则

1. 先把 decimation 和 feature_detection 做成可靠 SDK，再扩展 repair/boolean/offset。
2. 公共 API 只暴露稳定概念，私有 helper 留在 `src/.../detail/`。
3. C ABI 不暴露 C++ 类型、异常或 Eigen。
4. C ABI 结构体只能向尾部追加；同一 ABI 版本内旧 `struct_size` 的缺失字段使用默认值。
5. 所有“工业安全”说法必须绑定具体过滤器和测试数据，不做泛化承诺。
6. Debug HTML、临时可视化和本地实验输出只能作为排查辅助；交付验收仍以 API、CTest、CSV 指标和 STL/OBJ 结果为准。

## 推荐对外表述

可以说：ManuMesh 当前提供带 line quadrics、特征曲线保护和保守合法性过滤的增材制造三角网格简化 SDK，并已把特征检测提升为独立算法模块。

不要说：ManuMesh 已经是完整 CAD kernel、能自动修复所有网格、能替代 B-Rep 特征识别或能保证制造级公差。
