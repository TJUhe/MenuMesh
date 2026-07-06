# 工业网格库定位

本项目当前是一个面向三角网格简化的 C++/C SDK 原型，已经具备向工业网格库演进的骨架，但还不是完整工业 CAD/mesh kernel。

## 当前可作为库交付的内容

- CMake 构建和安装目标。
- `line_quadrics_qem` 共享库或静态库。
- C++ 公共头：`core`、`features`、`algorithms/simplification`。
- C ABI：`api/CApi.h`。
- CLI：`linequadrics.exe`，用于批处理、验证和示例。
- 示例：C++ SDK 和 C ABI consumer。
- 回归测试：72 个非性能测试，另有 performance 构建路径。

## 当前核心能力

| 能力 | 当前状态 |
| --- | --- |
| 网格交换 | `Mesh`、`PlainMesh`、STL/OBJ 读写。 |
| 拓扑分析 | boundary、non-manifold、edge/face/vertex 统计。 |
| 特征检测 | boundary、dihedral、normal-tensor、loop、circle/ellipse fitting。 |
| 简化 | standard QEM、line quadrics、ratio/face target、sweep。 |
| 保护 | boundary、feature curves、triangle quality、normal deviation、local error、local intersections。 |
| 集成 | C++ API、C ABI、SDK 安装、示例工程。 |

## 与完整工业库的差距

还缺少：

- 半边/可编辑拓扑内核。
- 属性传播和源面/区域映射。
- 修复、补洞、定向、make-manifold。
- 布尔、切割、offset/thickening。
- 严格全局误差 envelope。
- CAD/B-Rep 语义特征识别。
- 多平台二进制发布和 ABI 兼容矩阵。

## 工程原则

1. 先把 decimation 模块做成可靠 SDK，再扩展 repair/boolean/offset。
2. 公共 API 只暴露稳定概念，私有 helper 留在 `src/.../detail/`。
3. C ABI 不暴露 C++ 类型、异常或 Eigen。
4. 所有“工业安全”说法必须绑定具体过滤器和测试数据，不做泛化承诺。

## 推荐对外表述

可以说：本库提供带 line quadrics、特征曲线保护和保守合法性过滤的三角网格简化 SDK。

不要说：本库已经是完整 CAD kernel、能自动修复所有网格、能替代 B-Rep 特征识别或能保证制造级公差。
