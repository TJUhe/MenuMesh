# 算法扩展协议（Algorithm Extension Protocol）

状态：生效（2026-07-12，架构蓝图 R7 落地）
适用范围：任何新增算法模块（repair、remeshing、offset、…）与既有模块的新增入口。
目标：新增一个算法的路径是机械化的——不需要读旧算法源码，只需要照本清单执行。

## 1. 新增算法模块的 7 步标准路径

以未来 `repair` 为例，每步一个动作：

1. **目录与登记**：建 `include/algorithms/repair/` 与 `src/repair/` +
   `src/repair/detail/`；在 `tests/support/check_include_boundaries.py` 的
   `MODULE_DEPENDENCIES` 与 `INCLUDE_MODULE_PREFIXES` 登记 `repair`
   （新模块是显式架构决策，boundary checker 会强制执行）。命名空间遵循
   `architecture.md` 的"命名空间约定"表：目录名 = 模块名 = 命名空间。
2. **公共头**：`RepairTypes.h`（扁平 options/report struct，不依赖 Eigen）+
   `Repairer.h`（pimpl 对象 + `repairMesh()` 自由函数入口）。每个公共入口的
   doc comment 写明错误机制（见 `error_handling_policy.md`）与线程契约。
3. **options 校验**：提供 `Status validateOptions(const RepairOptions&)` 公共
   自由函数（协议见第 2 节），对象构造与函数入口内部调用它。
4. **诊断**：report 字段按第 3 节命名规范填写；每个拒绝/降级路径必须有对应
   计数字段（"诊断跟着分支走"）。
5. **CLI 注册**：`apps/` 新增 `commandRepair()` 并注册到 command
   registry；CLI 只绑定公共 options 与格式化 report，禁止出现算法逻辑
   （R3 圆环匹配下沉的教训：算法进库并配单测，CLI 只做 load→run→格式化）。
6. **测试**：`tests/unit/repair/` 按行为拆文件；黑盒与白盒用例都进
   `manumesh_tests`（它链接内部 STATIC 聚合库 `manumesh_internal`，include
   detail 头是安全的）；使用 `tests/data/external` 数据或运行超过约 30 秒的
   用例放入 `manumesh_external_tests`（`external` 标签），保持
   `ctest -LE "performance|external"` 是秒级快速套件。
7. **文档**：更新 `architecture.md` 依赖图与命名空间表、
   `source_organization.md` 目录契约、`CHANGELOG.md`；CMake 按
   `adding_new_algorithm.md` 清单新增 object library 并接入
   `MANUMESH_LIBRARY_OBJECTS`（DLL 与内部静态聚合库自动同时获得该模块，
   源码只编译一次）。

## 2. options 统一校验协议

- 每个算法模块的公共头提供
  `MANUMESH_API Status validateOptions(const XOptions&)` 自由函数；全部字段有
  可校验的默认值（对齐 CGAL named parameters 解决的同一需求，但用扁平 struct）。
- 对象构造与自由函数入口内部调用它，并按错误处理决策表把非法 options 转成
  异常（编程错误轨）：`throw std::invalid_argument(status.message())`，错误
  文案与 Status 完全一致。
- CLI 与 C API 先调用 `validateOptions` 做预检并输出 `status.message()`，
  不再靠捕获异常兜底。
- 现状与迁移：simplification 与 feature_detection 目前在
  `SimplificationValidation.cpp` / `FeatureDetector.cpp`（`validateFeatureOptions`）
  中直接 throw；迁移时先生成 Status、在入口处 throw，错误文案保持不变，并以
  "validateOptions 返回的 message 与原异常 message 相等"的单测锁定兼容性。

## 3. 诊断（report）字段命名规范

基于 `SimplifyReport` 现状归纳，新模块 report 按此评审：

- **计数字段**：`<过去分词/名词> + <名词>` 的 `int`，如 `collapsedEdges`、
  `rejectedCollapses`、`solverFallbacks`、`graphCleanupBridgedGaps`。
- **拒绝细分**：总拒绝数等于各细分之和（如 `rejectedCollapses` =
  feature/boundary/topology/normalFlip/quality/selfIntersection/curveBudget/
  error 各细分之和），该恒等式必须有单测。
- **终止原因**：模块级 enum（`SimplifyTerminationReason` 样式）+ `toString()`。
- **比率/几何量**：`double`，doc comment 标注单位与归一化基准（如
  "相对 bbox 对角线"）。
- **"诊断跟着分支走"**：每新增一个拒绝路径/降级路径，必须同时新增对应计数
  字段；没有计数字段的分支不允许合入。

## 4. mesh_edit 公共化判据（R7-d，仅记录，不实施）

`src/mesh_edit` 保持 internal。升格公共 API 需**同时**满足：

1. 至少两个已发布的公共算法模块需要把编辑操作暴露给调用方（而非仅内部复用）；
2. 出现真实 SDK 用户需求"增量编辑 + 回读映射"；
3. typed handle + generation-aware 生命周期已实现
   （`mesh_edit_foundation.md` 扩展顺序第 1 步），否则公共化会把不稳定索引
   语义写进 ABI。

时机：remeshing 模块落地并稳定一个版本后评估。在此之前公共层继续以
"输入 `Mesh` → 输出 `Mesh` + old-to-new 映射"表达编辑结果。
