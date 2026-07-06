# 工业化验证现状

本文记录当前库在工业风格模型上的验证边界。结论基于当前源码和测试，而不是产品化承诺。

## 当前验证入口

```powershell
.\build\mingw-ninja-release\bin\linequadrics.exe validate-features --ratio 0.20 --samples 1000
.\build\mingw-ninja-release\bin\linequadrics.exe validate-external --ratio 0.25 --samples 800
ctest --test-dir build\mingw-ninja-release -LE performance --output-on-failure
```

VS Code 中对应常用任务：

- `run: feature validation`
- `run: external validation`
- `test: mingw+ninja release`

## 当前工业相关测试点

- 圆孔、近圆孔、椭圆孔是否能检测并在 aggressive simplify 后保留。
- Fandisk 等硬边模型是否能达到目标面数并避免 generic crease 过度硬锁。
- Casting/NASA/OpenFOAM 类 STL 是否能保持基本拓扑稳定。
- `industrial-safe` 是否能通过质量、法线、局部误差和自交过滤减少危险 collapse。
- C API 是否能通过 opaque handle 处理真实 STL。

## 重要报告字段

| 字段 | 解读 |
| --- | --- |
| `termination_reason` | 是否达到目标，或因候选耗尽/拒绝上限停止。 |
| `feature_rejected_collapses` | 特征策略拒绝总数。 |
| `primitive_feature_rejected_collapses` | primitive loop 保护拒绝。 |
| `generic_feature_rejected_collapses` | generic feature 拒绝，过高可能说明过度锁边。 |
| `boundary_rejected_collapses` | 边界保护拒绝。 |
| `quality_rejected_collapses` | 三角形质量过滤拒绝。 |
| `normal_flip_rejected_collapses` | 法线偏转过滤拒绝。 |
| `self_intersection_rejected_collapses` | 局部自交过滤拒绝。 |
| `error_rejected_collapses` | 局部误差预算拒绝。 |

## 当前结论

当前算法已经能作为工业三角网格简化模块的基础：它可构建、可测试、可通过 C/C++ 集成，并能对常见孔洞/硬边/边界场景给出诊断。

但它仍不是完整工业几何内核。要进入更严格生产环境，还需要：全局误差 envelope、属性传播、更多真实数据、装配级测试、崩溃/异常恢复、版本化 ABI 和更系统的性能基线。
