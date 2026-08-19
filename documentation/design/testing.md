# 测试与验收

测试目录反映代码边界：

| 位置/标签 | 内容 |
| --- | --- |
| `tests/unit/core`, `common`, `mesh_edit`, `io` | 数据模型、拓扑、几何、I/O 和统计 |
| `tests/unit/feature_detection` | 证据、图、loop、primitive、patch、并行等价 |
| `tests/unit/simplification` | QEM、placement、合法性、保护和报告 |
| `tests/unit/api` | C ABI、容量/ABI、生命周期和并发边界 |
| `tests/unit/apps` | CLI 选项和绑定契约 |
| `tests/data/external/`、`tests/unit/<module>/*_external_tests.cpp` 或 `external` 标签 | 外部 STL/OBJ 和 Thingi10K 数据；由独立的 external 测试可执行文件运行 |
| `tests/performance` 或 performance 标签 | 可选性能与大模型门禁 |
| `tests/memory`、`tests/support` | 生命周期压力、fixture 和架构检查 |

测试由 CMake/GoogleTest 动态发现；数量随源码变化，使用 `ctest -N` 查询，不在文档里写死。
除 `unit`、`external` 和 `performance` 外，仓库还使用 `cli`、`architecture`、`abi`、`memory`
标签标识命令行、模块边界、C ABI 和生命周期压力测试。
大型 Thingi10K fixture 由内网制品预置到 `output/thingi10k_large/`；CTest 只校验本地
manifest 和哈希，缺少该可选包时跳过对应大模型用例，不执行公网获取。

## 最小验证闭环

```powershell
cmake --preset vs2019-debug-full
cmake --build --preset vs2019-debug-full-tests --parallel
ctest --preset vs2019-debug-full-unit
```

修改公共 API、CMake 或安装布局后，再跑：

```powershell
cmake --preset vs2019-release-sdk
cmake --build --preset vs2019-release-sdk --parallel
ctest --preset vs2019-release-sdk
```

算法改动应至少覆盖一个最小解析 fixture、一个退化/非法输入和一个真实外部 fixture；并行
阶段还要比较 Serial/Parallel 的结构化结果，而不是只比较墙钟。C ABI 改动必须覆盖当前
结构体、旧前缀大小、容量不足和失败时不替换输出。

## 结果解释

简化测试同时检查目标面数、拓扑、质量、局部误差、自交保护、特征漂移和终止原因。特征
benchmark 的 precision/recall 是标签比较指标，不是所有输入的产品保证。外部模型测试用于
捕捉回归，不把单机固定耗时写成正确性门槛。
