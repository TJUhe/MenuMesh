# oneTBB 并行执行基准（2026-08-18）

本文记录 oneTBB 后端的可复现实测，不把某台机器上的固定加速比写成正确性门槛。
测试机为 Intel Core i5-14400F（10 核 / 16 逻辑线程，32 GiB RAM），Windows、MSVC
v142、Release、oneTBB 2021.12.0 shared 构建。

## 架构边界

执行契约位于 [`include/core/ExecutionOptions.h`](../../include/core/ExecutionOptions.h)：

- 旧入口默认 `ExecutionMode::Serial`，保持兼容和确定性；
- 调用方显式选择 `Parallel`，并可设置 `maxConcurrency` 与 `minItemsPerTask`；
- 公共头不暴露 `tbb::*` 类型；
- `src/common/detail/ParallelExecution.h` / `src/common/ParallelExecution.cpp` 是唯一 oneTBB 适配边界；
- 未启用 oneTBB 或请求单线程时，回调使用相同的串行语义。

特征检测仅对不重叠写入的面积、法向滤波、Normal Tensor 顶点范围并行。
诊断归约、边/图排序、环恢复的拓扑遍历与提交、分区和图整理保持确定性串行顺序；独立环候选的
纯几何拟合可在固定槽位中并行。QEM 仅并行初始顶点/面状态、
边 placement/cost 的只读求解，再串行批量建立 priority queue；动态 edge-collapse、版本失效和
拓扑提交仍串行。这是结果等价优先的边界，不是把共享可变拓扑交给线程池。

恢复阶段会把公共的 `minItemsPerTask`（按顶点/面计）按候选组件平均顶点数换算成组件粒度，
并限制为每个 worker 的少量任务；候选少于四个时直接保持调用方的串行/单任务策略。这样
默认粒度不会让真实模型的数百个拟合候选意外合并成一个范围，也不会为小模型制造调度开销。

环恢复也遵循同一边界。五个恢复阶段保持固定调用顺序，因为后续阶段会读取前序阶段写入的
`analysis.vertices`、`analysis.loops` 和圆环归属。`FeaturePrimitiveRecovery` 现在先按原 seed/BFS
顺序收集独立图组件，再用 oneTBB 对每个组件执行只读 `fitPrimitive`，最后按组件顺序串行提交
拟合结果、分配连续 loop ID、更新顶点归属和 `analysis.loops`。因此并行区没有共享拓扑写入，
串行和 0/1/2/4/8 worker 的输出仍保持逐位一致；组件候选的临时内存上界为候选顶点总数加上
每组件一个拟合槽位，最多覆盖当前图中的候选顶点，不复制整张网格。

## 合成 pipeline

`tests/performance/parallel_pipeline_benchmark.cpp` 在同一进程输出 serial、1、2、4、8 worker
的墙钟时间和结果指纹。下表是同一 Release 进程连续 5 次的中位数；指纹在所有配置中保持一致。

| 阶段 | 输入 | serial | 1 thread | 2 threads | 4 threads | 8 threads | 结果指纹 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| feature detection | 66,049 V / 131,072 F | 406.21 ms | 403.35 ms | 355.62 ms | 335.90 ms | 331.19 ms | `9052808876168565309` |
| QEM simplification | 16,641 V / 32,768 F | 190.78 ms | 191.76 ms | 182.46 ms | 178.08 ms | 177.57 ms | `18446020898975846176` |

该结果显示特征检测在 4/8 worker 上分别约 1.21x/1.23x，QEM 约 1.07x；QEM 的动态
拓扑提交和 priority queue 仍然是主要串行段。1 thread 的额外调度成本可能略高或略低，
不能据此承诺固定收益；基准只断言所有线程配置的指纹相同。

本轮新增的基元恢复基准位于 `FeatureDetectionPerf.DISABLED_PrimitiveRecoveryTiming`，使用
1,024 个互不相交圆组件（131,072 个顶点）验证候选级并行和有序提交。5 次独立 Release 诊断
得到 5 次进程中位数串行 26.28 ms、8 worker 21.19 ms（约 1.24x）；该用例保持 disabled，只用于本机/发布前
性能采样，不把固定加速比写入测试断言。

## 真实 STL 负载

输入为仓库已有的 `tests/data/external/large/max_planck.stl`（99,991 面），使用 noisy-scan 特征
配置和 line-QEM 简化。每次都是独立进程，输出通过 SHA-256 比对：

| 操作 | serial 中位数（3 次） | `--threads 4` 中位数（3 次） | 加速 | 输出 SHA-256 是否相同 |
| --- | ---: | ---: | ---: | --- |
| noisy-scan `feature-report` | 438.80 ms | 371.34 ms | **1.18x** | 是（`98e6b824...a6451`） |
| line QEM `simplify --ratio 0.95` | 819.80 ms | 749.59 ms | **1.09x** | 是（`30777550...96c5`） |

运行命令示例：

```text
manumesh feature-report tests/data/external/large/max_planck.stl \
  --profile noisy-scan --threads 4 --csv output/feature.csv
manumesh simplify tests/data/external/large/max_planck.stl output/simplified.stl \
  --method line --line-weight 0.001 --ratio 0.95 \
  --max-normal-deviation-deg 180 --min-triangle-quality 0 --threads 4
```

上表的真实 STL 数据为 2026-08-18 当前机器的三次独立进程中位数；输出文件的完整
SHA-256 在 serial/4 worker 之间一致。这是实测记录，不是性能门槛或对其他 CPU 的固定承诺。

## 超大网格边界

本地 Thingi10K 样本 `thingi10k_120628_2529744_faces.stl`（2,529,744 面）由单块 STL 加载器以
`STL exceeds the supported temporary-memory budget` 拒绝；这是现有 1 GiB 估算工作集保护生效，
不是并行算法失败。相同样本已经通过 `PartitionedMeshDataset` 以 1 MiB 数据层预算完成分区导入和
校验，但当前分区层只提供三角记录 I/O，还没有全局顶点/边 incidence、owner/ghost、halo、
FeatureGraph 合并或 out-of-core QEM。

因此当前可准确宣称的是：中等到约十万面单块网格的特征识别和 QEM 已有 oneTBB 并行收益，且结果
保持逐位一致；多百万到百兆面需要沿 [`large_mesh_architecture_2026_08_18.md`](large_mesh_architecture_2026_08_18.md)
的 global-ID、完整 edge incidence、halo 和全局提交路线继续实现，不能把各分区独立处理后直接拼接。
