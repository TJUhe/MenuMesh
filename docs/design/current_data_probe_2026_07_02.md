# 当前数据探针记录

本文记录当前仓库中用于验证算法的主要数据来源和它们在测试中的角色。内容已按当前目录复核。

## 内置生成器

CLI `generate` 支持以下类型：

```text
plane, clustered-plane, hole-plane, ridge, noisy-plane,
sine-terrain, terrace, bump, cylinder, torus, cube, thin-fin,
stepped-shaft, pipe-coupling, pulley
```

这些生成器主要用于快速构造平面退化、孔洞、边界、硬边、薄片和简单工业形状。它们适合回归测试和参数演示，不代表真实工业数据分布。

## 特征 fixture

`tests/data/feature_fixtures/` 包含：

| 文件 | 用途 |
| --- | --- |
| `coaxial_hole_plate.obj` | 圆孔和同轴环检测。 |
| `tilted_coaxial_hole_plate.obj` | 倾斜孔轴线检测。 |
| `eccentric_hole_plate.obj` | 非同轴/偏心孔检测。 |
| `elliptical_hole_plate.obj` | 椭圆和近圆分类。 |
| `near_circular_hole_plate.obj` | near-circle 容差验证。 |
| `boss_pocket_plate.obj` | boss/pocket 平面和硬边验证。 |

这些 fixture 是当前 `FeatureDetection.*`、`ManuMeshParameters.*` 和独立 `FeatureDetector` 对象 API 测试的重要输入。

## 外部模型

`tests/data/external/` 当前包含：

- `fandisk_2014.stl`
- `casting_aimshape_2014.stl`
- NASA / OpenFOAM 工业相关 STL
- `common_3d_test_models/*.obj`
- `large/*.stl` 的 10 个较大公开模型
- `thingi10k/*.stl` 的 97 个真实世界模型子集

这些数据用于验证边界、非流形、质量退化、特征保持和性能。

## 当前测试覆盖

`cmake -E chdir build/mingw-ninja-release ctest -N` 在本文成文时（2026-07-02）显示 CTest 共 80 个；2026-07-13 复核为启用的非性能 CTest 共 236 个（快速套件 `-LE "performance|external"` 225 个 + external 大网格 11 个），新增部分主要是解析真值测试、性能护栏、纹理/光滑曲率/architecture 守卫用例，分层说明见 [`testing_strategy.md`](testing_strategy.md)。覆盖：

- CLI 基本行为和错误拒绝。
- 示例程序构建/运行。
- 参数解析、权重模式和终止原因。
- normal tensor 特征证据。
- 三角形质量、法线偏转、局部误差和自交过滤。
- 边界、拓扑和 feature protection 策略。
- C++ 对象 API、PlainMesh 入口、copy/move 和报告。
- C ABI 输入校验、结构体初始化、旧尾部 `struct_size` 兼容和诊断字段。

## 当前数据结论

当前数据集足以捕捉常见回归：目标面数未达成、边界异常、新增非流形边、三角形严重退化、特征环消失、C ABI 未初始化结构体。它还不足以证明全局工业安全性，因为还没有覆盖复杂 CAD 装配、扫描噪声、多材料属性、B-Rep 对齐或严格误差 envelope。
