# 新增功能流程

这份流程只约束真实仓库中的扩展点。示例路径必须在实现落地后才可以写入文档，不能把未来
目录当成当前能力。

## 1. 先定边界

- 复用两个以上模块且不属于某个算法的几何基础，公共实现放 `src/common/`，私有声明/辅助类型放
  `src/common/detail/`；
- 动态面/邻接/compact 放 `src/mesh_edit`；
- 新的统计、特征或简化能力优先判断是否应成为 `include/algorithms/<domain>` 的平级公共模块；
- CLI 工作流只组合已有 SDK，不把算法状态放进 `apps`；
- C ABI 只有在 C++ 契约稳定后再增加，公共结构体必须 size-aware。

## 2. 按现有目录落地

公共头加入 `include/`，实现加入对应的 `src/<domain>/`（`<domain>` 是实际模块名），私有状态放 `detail/`；在
`src/CMakeLists.txt` 更新 production source/header 列表，并在对应的
`apps/CMakeLists.txt` 或 `tests/CMakeLists.txt` 更新应用/测试目标。示例只能 include 公共头，测试按模块放在
`tests/unit/<domain>/`。新增 CLI handler 后同时更新 command registry 和 `CliArguments.cpp`
的 `OptionSpec` 表，这一表同时驱动帮助与命令归属校验。

## 3. 定义契约

每个公共入口至少写清：输入前置条件、输出所有权、失败状态、单位/范围、确定性、线程安全、
复杂度和不实现的边界。按所属 API 边界沿用 `Status`/`Result`、现有 `bool + error` I/O 或 C
状态码；程序员错误用异常。不要复制一套内部候选结构到公共头。

## 4. 验证闭环

依次补输入校验、最小行为、回归 fixture、并行等价（如适用）、CLI/C ABI/安装 consumer（如
适用）测试；运行 `vs2019-debug-unit` 和受影响的 external/full 套件。最后同步头文件注释、
本目录相关设计文档、根 README 和 Doxygen 页面。

## 5. 合入前检查

- `rg` 确认示例和公共头没有 include 私有实现路径（下方 `src/.../detail` 仅为示意写法）；
- `cmake --build ... --target check-src-doxygen` 通过；
- `manumesh --help` 与 CLI 文档的命令形状一致；
- 文档只引用存在的路径和当前符号；
- 未把 roadmap、实验数字或论文结论写成运行时保证。
