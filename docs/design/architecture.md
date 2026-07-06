# 网格内核架构现状

本项目已经从单文件 QEM 原型演进为一个小型 C++ 多边形网格内核。当前重点仍是 QEM 和 line quadrics 简化，但代码边界已经按 SDK 方式组织：公共头、实现、CLI、示例、测试和文档相互分离。

## 当前已落地的层次

```text
core                    Mesh、PlainMesh、Status/Result、拓扑统计和基础生成器
algorithms/feature_detection
                        边界、非流形边、二面角硬边、normal-tensor 弱特征、特征环和圆/椭圆拟合
algorithms/simplification
                        QEM、line quadrics、特征曲线保护、合法性过滤、局部空间索引
api                     稳定 C ABI，不暴露 STL/Eigen/异常
apps                    linequadrics CLI，用于批处理、演示和调试
examples                C/C++ SDK 消费示例
tests                   GoogleTest 和 CTest 回归验证
```

尚未实现但在长期路线中保留的位置：`repair`、`boolean`、`offset`、更完整的 `remesh`、属性传播和可编辑半边拓扑。

## 物理目录

```text
include/line_quadrics_qem/      安装级公共 SDK 头文件
src/<domain>/*.cpp              库实现入口
src/<domain>/detail/*.h         私有实现辅助头，不安装
apps/                           CLI 或应用层消费者
examples/                       外部使用者示例
tests/                          回归测试和验证数据
docs/                           设计、指南、论文和生成笔记
```

`src/feature_detection/` 是独立特征检测模块，只依赖 core，不依赖 QEM。`src/simplification/detail/` 当前包含每次简化运行所需的内部状态、优先队列、二次误差构造、特征约束、动态拓扑、局部几何谓词和空间索引。这些不是 SDK 合约，可以随实现变化。

## 数据结构策略

`Mesh` 仍是交换格式：稠密顶点数组加三角面数组。它适合 C++ API、C ABI、文件读写和测试数据。需要重复邻接查询的算法应先构建 `MeshTopology` 或运行内的动态邻接，不应在每个模块反复临时扫面。

未来如果加入拓扑编辑，应使用 `VertexId`、`EdgeId`、`HalfedgeId`、`FaceId` 等 typed handle，配合稠密存储、generation-aware free list 和显式 compaction。属性不应塞进基础顶点结构，应以类型化数组挂在拓扑旁边，便于重映射和导出。

## API 形态

当前 C++ 简化主入口是：

```cpp
lq::Mesh simplifyMesh(const lq::Mesh& input,
                      const lq::SimplifyOptions& options,
                      lq::SimplifyReport* report = nullptr);
```

需要多次运行或保存配置时使用 `lq::QEMSimplifier`。特征检测同样提供平级对象入口：

```cpp
lq::FeatureDetector detector(options);
lq::FeatureAnalysis features = detector.analyze(mesh);
```

C API 使用 `LqContext`、`LqMeshHandle`、`LqSimplifyOptions`、`LqSimplifyReport` 和 `LqMeshStats`，调用前必须用对应 `*_init` 初始化带 ABI 版本的结构体。

## 当前算法边界

- line quadrics 是 QEM 的候选排序成本补充，不是通用去噪器。
- 特征检测是独立算法模块，QEM 只消费 `FeatureAnalysis`，特征检测不反向依赖简化。
- `--preserve-feature-curves` 会启用特征检测、特征曲线 quadric、placement 投影和硬保护策略。
- `--feature-protection-mode primitive-curves` 是默认硬保护策略；它只硬保护圆、近圆和椭圆等 primitive loops，普通折线/硬边更多依靠软成本。
- `--industrial-safe` 会启用更保守的边界、质量、法线、局部误差和自交保护，但仍不能替代严格 CAD/B-Rep 验证。
