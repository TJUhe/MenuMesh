# 生成笔记说明

`docs/generated/notes/` 保存导出的 HTML/PDF/ZIP 笔记和报告。它们是参考产物，不是当前 C++ 库的源代码布局说明。

## 当前维护规则

- 如果 CLI、`SimplifyOptions`、特征检测、测试数据或验证输出发生变化，相关 HTML/PDF 应重新生成或在索引中标注为历史资料。
- 本目录中的 `*.html` 可用于浏览算法解释、代码阅读笔记和实验报告。
- `*.pdf` 是阅读版导出文件。
- `eye-care.css` 是导出 HTML 共用样式。

当前最权威的可维护文档仍然是：

| 路径 | 说明 |
| --- | --- |
| `docs/design/` | 架构、算法设计和验证记录。 |
| `docs/guide/` | 使用和集成指南。 |
| `docs/papers/` | 论文归档与索引。 |
| `README.md` | 项目入口说明。 |

如果发现 HTML 中的任务名、命令或结论与当前 `.vscode/tasks.json`、`apps/linequadrics/main.cpp` 或测试结果不一致，应以当前源码为准更新。
