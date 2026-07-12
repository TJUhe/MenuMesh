# 错误处理策略（Error Handling Policy）

状态：生效（2026-07-12，架构蓝图 R5 落地）
适用范围：全部公共 API 与内部模块。新增任何公共入口前先查本表，不再逐案讨论
"抛不抛异常"。`include/core/Status.h` 顶部注释引用本文件。

## 决策表

| 错误类别 | 判据 | 机制 | 现状与迁移 |
| --- | --- | --- | --- |
| 数据错误 | 合法调用 + 不合规输入（非流形、非有限坐标、空 mesh 等"世界的错"） | 返回 `Status` / `Result<T>`，不抛异常 | 新算法模块一律采用；`simplifyMesh`/`detectFeatureCurves` 对"输入 mesh 不合规"当前抛 `std::invalid_argument`，在 v-next 增加 `Result` 姊妹入口（如 `trySimplifyMesh`）后改判为数据错误轨 |
| 编程错误 | 调用方违反 API 契约（options 越界、句柄越界、未 init 的 ABI struct） | 抛异常（`std::invalid_argument` / `std::out_of_range`）或 assert | 保留现状：`SimplificationValidation.cpp`、`FeatureDetector.cpp` 的 options 校验、`MeshTopology.cpp` 的越界访问即此类，语义正确，不迁移 |
| C ABI 边界 | 任何跨 `api/CApi.h` 的失败 | 状态码（`ManuMeshStatus`）+ 可查询的 last-error 文本；`CApi.cpp` 捕获全部 C++ 异常并映射为状态码，异常永不穿越 C 边界 | 保留现状（`CApi.cpp` 已映射 `invalid_argument` → `MANUMESH_STATUS_INVALID_ARGUMENT`）；规则：每新增一条异常→状态码映射，必须补双向测试（C++ 侧抛出 + C 侧断言状态码与 last-error 文本） |
| IO 错误 | 文件不存在 / 解析失败 | 目标形态 `Result<Mesh> loadMesh(path, options)`；现有 `bool + std::string* error`（`include/io/MeshIo.h`）保留为薄包装 | 渐进迁移：新增 `Result` 重载（旧签名内部转调）→ CLI/examples 切换 → 旧签名标 deprecated，一个 minor 版本后评估删除 |

## 判据速记

- 问一句"输入数据换一个文件就能触发吗？"——能，就是数据错误（Status/Result 轨）。
- 问一句"修好调用方代码后还会发生吗？"——不会，就是编程错误（异常轨）。
- 跨 C ABI 的一切失败都是状态码，无例外。

## 迁移路径（三步，均可独立合入）

1. 本决策表文档化并纳入 code review checklist（已完成）。
2. `io` 增加 `Result<Mesh>` 重载，旧 `bool + error*` 签名内部转调新入口。
3. 为 `simplifyMesh` / `detectFeatureCurves` 增加 `Result` 形态姊妹入口
   （`trySimplifyMesh` 或 `simplifyMeshChecked`），把"输入 mesh 不合规"从异常轨
   改走 `Status`；现有对象入口行为不变。

## 公共头文档要求

每个公共入口的 doc comment 必须写明其错误机制（返回 Status / 抛何种异常 /
状态码），以及线程契约（对齐 CGAL 的逐算法声明纪律）。示例见
`include/algorithms/analysis/MeshAnalysis.h`。
