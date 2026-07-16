# ManuMesh 新增功能维护流程

本文说明在当前目录级 `CMakeLists.txt` 结构下，如何给 ManuMesh 新增功能，并让代码、测试、文档、示例和 SDK 安装保持可维护。这里的“功能”包括：

- 新算法模块，例如 `repair`、`remeshing`、`validation`。
- 现有算法的新能力，例如 QEM 新的 collapse 过滤器、feature detection 新的证据来源。
- 面向用户的组合能力，例如“简化后质量门禁”“批处理报告”“CLI 子命令”。
- C API 或 SDK 示例层面的新入口。

核心原则很简单：先决定功能属于哪一层，再把代码放到对应目录。不要把所有新增逻辑塞回顶层 `CMakeLists.txt`、`QEMSimplifier.cpp` 或 `ManuMeshCommands.cpp`。

## 当前分层

```text
CMakeLists.txt                    全局选项、Eigen 解析、通用 warning/runtime helper、add_subdirectory
src/CMakeLists.txt                manumesh_core、库源码列表、库安装规则
apps/CMakeLists.txt      CLI target、CLI 测试、CLI 安装规则
examples/CMakeLists.txt           SDK 示例 target 和示例测试
tests/CMakeLists.txt              GoogleTest 解析、单元测试、性能测试
adm/CMakeLists.txt                format/check-format、docs-api/docs-internal、SDK install/export/consumer test

include/core/                     稳定 core SDK 头
include/algorithms/<domain>/      稳定算法 SDK 头
include/api/                      C ABI
src/core/                         core 实现
src/common/detail/                跨算法私有基础设施
src/<domain>/                     算法实现
src/<domain>/detail/              算法私有状态、阶段和 helper
tests/unit/<domain>/              单元测试
examples/                         外部用户视角的 SDK demo
documentation/                             当前设计事实和使用指南
```

以后新增功能时，优先维护这些 `CMakeLists.txt`，不要再新增项目自有的 `.cmake` 模块，除非某段逻辑已经被多个完全不同的目录复用，且继续放在目录级 `CMakeLists.txt` 会造成明显重复。

## 第一步：判断功能类型

新增功能前先问四个问题。

### 1. 它是库能力，还是应用层组合？

如果只是把现有 API 组合成一个工作流，例如：

- 生成一个网格。
- 检测特征。
- 简化。
- 计算质量指标。
- 判断是否通过阈值。

它可以先放在 `examples/` 或 CLI 中，不一定立刻变成库 API。本文附带的 `examples/feature_workflow_demo.cpp` 就是这种方式。

如果调用方会反复复用这个能力，并且它有稳定的输入、输出、选项和报告，再考虑提升为 `include/algorithms/<domain>/` 下的公共模块。

### 2. 它属于已有模块，还是新平级模块？

属于已有模块的例子：

- QEM 新的边折叠合法性检查：放到 `src/simplification/CollapseLegality.cpp` 或相邻新文件。
- QEM 新的代价项或 placement 求解：放到 `src/simplification/Quadrics.cpp` 或相邻新文件。
- 特征检测新证据来源：放到 `src/feature_detection/FeatureEvidence.cpp` 或相邻新文件。
- 特征 loop 恢复策略：放到 `src/feature_detection/FeatureLoopRecovery.cpp` 或专门 recovery 文件。

当前 feature detection 已给出更细的拆分范例：normal-domain 预处理、graph compatibility、component consolidation、segmentation 和 benchmark 分别位于独立 translation unit。新增 graph recovery 规则必须复用 `FeatureGraphCompatibility`，新增 recovery edge 必须声明它是否为真实 mesh edge，避免 surface patch segmentation 把空间 bridge 错当 face barrier。

应该新建平级模块的例子：

- `repair`：修洞、去重、非流形处理。
- `remeshing`：重采样、边长均匀化、曲率自适应。
- `validation`：制造约束、几何质量门禁、报告生成。
- `offset` 或 `boolean`：独立几何算法。

判断标准是：它是否有自己的 `Options`、`Report`、`Result`，是否会被 CLI、SDK 示例、C API 或其他算法独立消费。如果是，就新建平级模块。

现成范例是 `manumesh::analysis`（`include/algorithms/analysis/MeshAnalysis.h` + `src/analysis/`）：通用网格统计与采样距离比较原来住在 simplification 里，因为会被 CLI、示例和未来 repair/remeshing 独立消费，被提升为平级公共模块；CSV 拼装这类表现层代码则同步下放到 CLI（`apps/CliCsv.{h,cpp}`），不进 SDK。

### 3. 它是否需要公开 API？

公开 API 必须稳定。只有调用方确实需要直接读写的数据才放到公共头里：

- `Options`：用户配置。
- `Report`：诊断和统计。
- `Result`：输出网格和报告。
- facade class：需要保存配置或缓存时使用，例如 `QEMSimplifier`、`FeatureDetector`。

临时队列、图结构、空间索引、内部候选集、阶段上下文不要进公共头。它们应放到 `src/<domain>/detail/` 或 `.cpp` 匿名命名空间。

### 4. 它是否需要跨 ABI？

先实现 C++ SDK。C API 晚一点加更稳妥，因为 C ABI 一旦公开，后续字段扩展要遵守：

- struct 带 `struct_size` 和 `abi_version`。
- 提供 `*_init()` 初始化函数。
- 新字段只追加在尾部。
- 输出写入必须按调用方提供的结构体大小做边界保护。
- C API 测试要覆盖旧结构体大小和未初始化结构体。

## 推荐开发顺序

### 1. 先写设计落点

在编码前写清楚功能边界。**新增算法模块前先读两份生效政策**：
[`../design/algorithm_extension_protocol.md`](../design/algorithm_extension_protocol.md)
给出 7 步机械化落地路径、`validateOptions` 协议和诊断字段命名规范；
[`../design/error_handling_policy.md`](../design/error_handling_policy.md)
的一页决策表决定新入口用 Status/Result 还是异常（数据错误→Status/Result、编程错误→异常、C 边界→状态码）。小功能可以写在 PR 描述里；中大型功能建议新增：

```text
documentation/design/<domain>_design.md
```

最少回答：

- 解决什么几何问题。
- 输入和输出是什么。
- 失败或退化输入怎么处理。
- 是否依赖 feature detection、simplification 或 common helper。
- 哪些行为必须测试。
- 是否需要 CLI、C API、SDK 示例。

### 2. 定公共契约

新模块建议形态：

```cpp
namespace manumesh::<domain> {

struct XxxOptions {
  double tolerance = 1e-8;
};

struct XxxReport {
  int changedFaces = 0;
};

struct XxxResult {
  Mesh mesh;
  XxxReport report;
};

class MANUMESH_API XxxProcessor {
public:
  XxxProcessor();
  explicit XxxProcessor(XxxOptions options);
  XxxResult run(const Mesh& input) const;

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

MANUMESH_API XxxResult runXxx(const Mesh& input, const XxxOptions& options);

} // namespace manumesh::<domain>
```

如果暂时只是 demo 或 CLI 组合能力，可以先在 `.cpp` 内部使用同样的 `Options / Report / Result` 形态，不公开到 `include/`。这样未来提升为正式 API 时迁移成本很低。

### 3. 拆实现文件

不要等一个 `.cpp` 长到几百行才拆。推荐从第一天就按职责拆：

```text
src/<domain>/XxxProcessor.cpp       public facade、pimpl、薄包装函数
src/<domain>/XxxValidation.cpp      options 和输入 mesh 校验
src/<domain>/XxxRun.cpp             单次运行编排
src/<domain>/XxxStageA.cpp          一个明确算法阶段
src/<domain>/XxxStageB.cpp          另一个明确算法阶段
src/<domain>/detail/XxxTypes.h      内部状态和策略类型
src/<domain>/detail/XxxValidation.h 内部校验声明
src/<domain>/detail/XxxRun.h        内部运行声明
```

如果某个 helper 被两个以上算法稳定复用，且语义不是某个算法专属，再考虑移动到：

```text
src/common/detail/
```

例如边 key、面 key、局部边长、面法向、边界顶点标记、通用索引重映射适合 common。某个 repair 阶段的 patch 候选、某个 simplification 阶段的 collapse state 不适合 common。

### 4. 接入 CMake

当前没有项目自有 `.cmake` 模块。新增功能时按目录改对应 `CMakeLists.txt`：

#### 新增库源码

改：

```text
src/CMakeLists.txt
```

需要加：

- 公共头：`MANUMESH_PUBLIC_HEADERS`
- 实现源：`MANUMESH_LIBRARY_SOURCES`
- 私有头分组：新增 `MANUMESH_<DOMAIN>_PRIVATE_HEADERS`
- 汇总到 `MANUMESH_PRIVATE_HEADERS`
- `source_group()`，保持 IDE 分组清晰

#### 新增 CLI 命令

改：

```text
apps/CMakeLists.txt
apps/ManuMeshCommands.cpp
apps/CliArguments.cpp
```

一般不需要动 `main.cpp`。新增 handler 后注册到 command registry。CLI 选项现在由 `CliArguments.cpp` 中共享的 `OptionSpec` 选项表单一来源驱动：把新选项加进对应命令族的 spec 列表（或新建列表并挂到 `commandOptionSets()` / `helpGroups()`），help 文本（`optionsHelpText()`）与逐命令参数校验（`validateArgsForCommand()`，含"未知选项/属于其他命令的选项"报错）会自动跟随，不要在 handler 里手写选项校验。

#### 新增示例

改：

```text
examples/CMakeLists.txt
examples/<demo>.cpp
```

示例只 include 公共头，模拟外部用户，不能 include `src/.../detail/...`。

#### 新增测试

改：

```text
tests/CMakeLists.txt
tests/unit/<domain>/*.cpp
```

如果是性能或大模型测试，放到 `tests/performance/`，并受 `MANUMESH_BUILD_PERFORMANCE_TESTS` 控制。

#### 新增文档或安装样例

改：

```text
adm/CMakeLists.txt
```

这里维护 `docs-api` / `docs-internal` 输入、SDK 安装文档、安装示例、`sdk-consumer-test`。

### 5. 测试策略

新增功能至少要覆盖四层。

1. 输入校验测试

空 mesh、非法 face、退化 face、非有限坐标、非法 options。

2. 核心行为测试

用最小人工 mesh 验证一个明确行为，不依赖复杂 fixture。

3. 回归测试

用现有 generator 或 fixture 验证真实-ish 数据上的行为。

4. API 或集成测试

如果暴露到 CLI、C API、examples 或 SDK install，要加对应测试。

建议命名：

```text
tests/unit/<domain>/<domain>_api_tests.cpp
tests/unit/<domain>/<domain>_validation_tests.cpp
tests/unit/<domain>/<domain>_<stage>_tests.cpp
tests/unit/<domain>/<domain>_fixture_tests.cpp
```

测试名写行为，不写实现细节：

```cpp
TEST(Repair, RejectsDegenerateFacesWithoutReplacingMesh)
TEST(Validation, ReportsBoundaryEdgesForOpenMesh)
TEST(Simplification, PreservesCircularFeatureLoopWithPrimitiveProtection)
```

## Demo：先把组合功能做成 SDK 示例

本次新增了：

```text
examples/feature_workflow_demo.cpp
```

它演示一种推荐做法：当你还不确定某个“新功能”是否应该成为正式库 API 时，先在 examples 里用 `Options / Report / Result` 形态做一个可运行原型。

这个 demo 实现了一个本地的 `runManufacturingQualityGate()`：

```text
输入：内置 cylinder mesh
阶段 1：检测 CAD feature loops
阶段 2：启用 feature-preserving QEM 简化
阶段 3：用 manumesh::analysis 计算简化前后 MeshStats 和采样距离
输出：QualityGateResult，包含简化 mesh、报告和 accepted 标志
```

它只 include 公共 SDK 头：

```cpp
#include "algorithms/analysis/MeshAnalysis.h"
#include "algorithms/feature_detection/FeatureDetector.h"
#include "algorithms/simplification/QEMSimplifier.h"
#include "core/MeshGenerators.h"
```

（统计与采样距离比较来自跨算法公共模块 `manumesh::analysis`；旧的 `algorithms/simplification/Metrics.h` 已是弃用转发头，将在下一 minor 版本删除，新代码不要再 include。）

这点很关键：示例必须像外部用户一样使用 ManuMesh。如果 demo 需要 include `src/.../detail/...`，说明这个能力还没有稳定的 SDK 边界，应该回到设计阶段。

运行方式：

```powershell
cmake --build build\mingw-ninja-release --target manumesh_feature_workflow_demo
build\mingw-ninja-release\bin\manumesh_feature_workflow_demo.exe
```

或通过 CTest：

```powershell
ctest --test-dir build\mingw-ninja-release -R manumesh_example_feature_workflow --output-on-failure
```

如果未来这个质量门禁要升级为正式功能，可以按下面方式落地：

```text
include/algorithms/validation/QualityGate.h
include/algorithms/validation/ValidationTypes.h
src/validation/QualityGate.cpp
src/validation/ValidationRun.cpp
src/validation/ValidationPolicies.cpp
src/validation/detail/ValidationTypes.h
tests/unit/validation/validation_api_tests.cpp
tests/unit/validation/validation_quality_gate_tests.cpp
examples/quality_gate.cpp
```

然后把 demo 里的 `QualityGateOptions / QualityGateReport / QualityGateResult` 搬到公共头，把 `runManufacturingQualityGate()` 拆成 public facade 和内部 run/policy 文件。

## 合入前检查清单

每次新增功能合入前至少跑：

```powershell
cmake --build build\mingw-ninja-release --parallel
cmake --build build\mingw-ninja-release --target check-format
ctest --test-dir build\mingw-ninja-release --output-on-failure
```

如果改了 SDK 公开头、安装规则、示例或 CMake package：

```powershell
cmake -S . -B build\install-check -G Ninja `
  -DCMAKE_BUILD_TYPE=Release `
  -DMANUMESH_ENABLE_INSTALL=ON `
  -DMANUMESH_INSTALL_CMAKE_CONFIG=ON

cmake --build build\install-check --target sdk-consumer-test --parallel
```

最终确认：

- 新公共头不 include `src/...`。
- 新示例不 include `src/.../detail/...`。
- 新 target 在对应目录的 `CMakeLists.txt` 中维护。
- 没有新增项目自有 `.cmake` 模块。
- C API struct 如果新增字段，只追加在尾部，并有 ABI 测试。
- feature detection 新 options 同步覆盖 C++ validation、CLI binding、simplify mapping、size-aware C ABI 和非法值测试。
- 新 evidence/recovery 同步更新 graph source flags、component summary、CLI/CSV、benchmark 和文档；新增 patch 语义同时测试 patch id 重编号不影响 adjacency accuracy。
- 文档描述的是当前代码事实，不是未来愿景。
