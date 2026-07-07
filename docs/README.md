# 文档目录

这里是 **ManuMesh** 的项目文档根目录。ManuMesh 定位为面向增材制造的多边形网格几何内核；文档必须跟随当前程序状态更新，不能描述已经移除的任务、旧 CLI 参数或尚未实现的能力。

## 当前代码事实

- 构建系统是 CMake，主路径为 MinGW + Ninja，VS Code 任务保存在 `.vscode/tasks.json`。
- 产品名是 ManuMesh；C++ API 命名空间已统一为 `manumesh`。核心网格类型位于根命名空间，功能入口使用 `manumesh::feature` 与 `manumesh::simplification`，不再提供旧根命名空间别名。当前 CMake 目标、CLI 名称和 include 根路径仍沿用 `manumesh` / `manumesh`；C ABI 名称仍为 `ManuMesh*` / `manumesh_*`。
- 公共 C++ SDK 位于 `include/manumesh/`，C ABI 位于 `include/manumesh/api/CApi.h`。
- 当前已实现能力包括 STL/OBJ 读写、内置网格生成、QEM/line-quadrics 简化、特征检测、特征曲线保护、质量/拓扑/局部误差过滤、CLI 批处理、C API 和回归测试。
- 当前没有实现通用布尔运算、offset/thickening、完整 B-Rep 特征识别或通用去噪器；这些只能作为 ManuMesh 路线图描述。

## 文档维护规则

- CLI 示例必须和 `apps/manumesh/main.cpp` 的实际解析逻辑一致，并至少用 `manumesh --help` 核对。
- 算法说明必须对应 `src/common/`、`src/feature_detection/`、`src/simplification/` 和 `include/manumesh/algorithms/` 的当前实现。
- 论文笔记必须区分“论文提出的思路”和“本仓库已经实现的行为”。未实现的技术要明确写“未实现”。
- `docs/generated/notes/` 下的 HTML/PDF 是历史导出资料；若 CLI、选项、测试、源码结构或产品命名变化，受影响的导出说明应重新生成或标注为历史资料。
- 路径、命令、函数名、枚举名、CSV 字段名和论文文件名保留英文；说明文字使用中文。

## 目录说明

| 路径 | 作用 |
| --- | --- |
| `design/` | 架构、算法设计、验证记录、路线图。 |
| `guide/` | 使用者和开发者操作指南。 |
| `papers/` | 论文 PDF 归档和论文索引。 |
| `generated/notes/` | 导出的 HTML/PDF/ZIP 笔记和报告，属于参考资料。 |
| `Doxyfile.in` | Doxygen API 文档生成模板。 |

长期设计决策放在 `design/`，操作步骤放在 `guide/`，论文 PDF 放在 `papers/`，测试生成物放在 `tests/output/` 或构建目录，不提交到文档目录。
