# 算法扩展协议（Algorithm Extension Protocol）

状态：生效（2026-07-12，架构蓝图 R7 落地）
适用范围：任何新增算法模块（repair、remeshing、offset、…）与既有模块的新增入口。
目标：给新增算法提供一致的边界和验收路径。实现前仍应阅读相邻模块与真实调用方；
清单用于防止遗漏，不是自动生成目录、类型和诊断字段的模板。

## 1. 新增算法模块的 7 步标准路径

以未来 `repair` 为例，每步一个动作：

1. **目录与登记**：建 `include/algorithms/repair/` 与 `src/repair/` +
   `src/repair/detail/`；在 `tests/support/check_include_boundaries.py` 的
   `MODULE_DEPENDENCIES` 与 `INCLUDE_MODULE_PREFIXES` 登记 `repair`
   （新模块是显式架构决策，boundary checker 会强制执行）。命名空间遵循
   `architecture.md` 的"命名空间约定"表：目录名 = 模块名 = 命名空间。
2. **公共头**：`RepairTypes.h`（紧凑 options/result 类型，不依赖 Eigen）+
   `Repairer.h`（pimpl 对象 + `repairMesh()` 自由函数入口）。每个公共入口的
   doc comment 写明错误机制（见 `error_handling_policy.md`）与线程契约。参数少且
   属于同一职责时保持一个小结构；只有职责已经明显分离时才引入嵌套配置。
3. **options 校验**：提供 `Status validateOptions(const RepairOptions&)` 公共
   自由函数（协议见第 2 节），对象构造与函数入口内部调用它。
4. **结果与诊断**：主结果只包含调用方通常会据此继续处理的稳定信息。调参级计数、
   逐元素误差和事件轨迹按第 3 节进入可选 diagnostics、属性通道或专用分析对象，
   不因实现中新增一个分支就扩张公共结果。
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
  可校验的默认值。少量同职责字段直接放在一个小结构中；当目标、代价、约束、
  质量或输出控制已经形成独立概念时，使用小型嵌套配置，避免继续增长一个平面结构。
- 互斥选择应由枚举、tagged value 或命名工厂表达。例如“目标面数”和“目标比例”
  不应依赖两个同时有效的字段及隐式优先级。
- 主算法入口保持短：输入、一个 options/config，以及真正需要的输出。兼容转换集中在
  一个边界函数中，内部热循环只消费一种已归一化表示。
- 对象构造与自由函数入口内部调用它，并按错误处理决策表把非法 options 转成
  异常（编程错误轨）：`throw std::invalid_argument(status.message())`，错误
  文案与 Status 完全一致。
- CLI 与 C API 先调用 `validateOptions` 做预检并输出 `status.message()`，
  不再靠捕获异常兜底。
- 现状与迁移：simplification 与 feature_detection 目前在
  `SimplificationValidation.cpp` / `FeatureDetector.cpp`（`validateFeatureOptions`）
  中直接 throw；迁移时先生成 Status、在入口处 throw，错误文案保持不变，并以
  "validateOptions 返回的 message 与原异常 message 相等"的单测锁定兼容性。

## 3. 结果与诊断规范

参考 pmp-library 的紧凑分析结果、OpenMesh 的 observer/module 分离和 VTK 的可选
属性输出，新模块按下面顺序决定公开数据：

1. **主结果**：只保留输出网格/映射、实际完成数量、终止原因和调用方常用的质量结论。
   一般应能在一个屏幕内读完；字段变长时先检查是否混入了调参或逐元素数据。
2. **可选 diagnostics**：只有调用方能据此调参、定位失败或做稳定回归时，才公开
   聚合计数。内部状态机的每个分支不自动获得公共字段。
3. **逐元素数据**：逐顶点、边、面或 corner 的误差、标签和来源进入类型化属性通道
   或专用分析对象，不塞入全局 report，也不为它们建立一组镜像 summary 类型。
4. **事件流**：只有存在真实消费方时再增加 visitor/observer；不要为假想调试需求
   预先公开回调和内部队列状态。

命名继续遵循以下约定：

- 计数使用 `<过去分词/名词> + <名词>`，如 `collapsedEdges`、`rejectedCollapses`。
- 终止原因使用模块级 enum，并提供 `toString()`。
- 比率和几何量的注释标明单位与归一化基准。
- 只有分类在语义上互斥且穷尽时，才要求总数等于细分之和并用测试锁定；不要为凑恒等式
  暴露本来属于实现细节的分类。

## 4. mesh_edit 公共化判据（R7-d，仅记录，不实施）

`src/mesh_edit` 保持 internal。升格公共 API 需**同时**满足：

1. 至少两个已发布的公共算法模块需要把编辑操作暴露给调用方（而非仅内部复用）；
2. 出现真实 SDK 用户需求"增量编辑 + 回读映射"；
3. typed handle + generation-aware 生命周期已实现
   （`mesh_edit_foundation.md` 扩展顺序第 1 步），否则公共化会把不稳定索引
   语义写进 ABI。

时机：remeshing 模块落地并稳定一个版本后评估。在此之前公共层继续以
"输入 `Mesh` → 输出 `Mesh` + old-to-new 映射"表达编辑结果。
