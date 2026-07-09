# ManuMesh 生成笔记说明

`docs/generated/notes/` 保存 ManuMesh 的 HTML/PDF/ZIP 导出笔记和报告。它们是历史参考产物，不是当前 C++ 库的源代码布局说明；当前产品名为 ManuMesh，当前 C++ 命名空间为 `manumesh`；CMake 目标、include 路径和 CLI 名称仍沿用 `manumesh`、`manumesh`。

## 当前维护规则

- 如果 CLI、`SimplifyOptions`、特征检测、测试数据、验证输出或产品命名发生变化，相关 HTML/PDF 应重新生成或在索引中标注为历史资料。
- 本目录中的 `*.html` 可用于浏览算法解释、代码阅读笔记和实验报告。
- `*.pdf` 是阅读版导出文件；当前未在本次文档更新中重写 PDF 二进制内容。
- `eye-care.css` 是导出 HTML 共用样式。

当前最权威的可维护文档仍然是：

| 路径 | 说明 |
| --- | --- |
| `docs/design/` | 架构、算法设计和验证记录。 |
| `docs/guide/` | 使用和集成指南。 |
| `docs/papers/` | 论文归档与索引。 |
| `README.md` | 项目入口说明。 |

如果发现 HTML 中的任务名、命令、源码路径或结论与当前 `.vscode/tasks.json`、`apps/manumesh/main.cpp`、`include/algorithms/` 或测试结果不一致，应以当前源码为准更新。
