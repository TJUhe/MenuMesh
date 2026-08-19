# CLI 使用说明

`manumesh --help` 是命令和选项的唯一完整清单。本页只固定命令的输入输出形状，避免帮助
文本和手工表格漂移。版本可用 `manumesh --version` 查询。

## 命令

| 命令 | 位置参数 | 作用 |
| --- | --- | --- |
| `generate` | 无 | 用 `--type`、`--n` 生成内置 STL，并用 `--out` 写出 |
| `simplify` | `input.stl output.stl` | 运行 QEM/line-quadrics 受约束简化 |
| `compare` | `original.stl simplified.stl` | 输出统计和确定性双向采样距离 |
| `feature-report` | `input.stl` | 输出特征图、环、primitive、component 摘要；启用 `--surface-patches` 时附带面片 |
| `feature-benchmark` | `input.stl labels.csv` | 对人工标签计算 edge/junction/branch/patch 指标 |
| `feature-compare` | `original.stl simplified.stl` | 检测并匹配简化前后的圆形特征环 |
| `sweep` | `input.stl output_dir` | 扫描 line-quadrics 权重列表 |
| `ratio-sweep` | `input.stl output_dir` | 扫描目标比例列表 |
| `face-sweep` | `input.stl output_dir` | 扫描目标面数列表 |
| `large-import` | `input.stl output.mmpd` | 将二进制 STL 写入有界内存分区数据集 |
| `large-validate` | `input.mmpd` | 流式校验分区目录、记录、checksum 和统计 |
| `demo` | 无 | 运行仓库内演示工作流，可用 `--quick` 缩短规模 |
| `summarize-metrics` | `[output_root] [summary.csv]` | 汇总工作流生成的 CSV |
| `validate-features` | 无 | 运行内置特征验证工作流 |
| `validate-external` | 无 | 运行外部模型验证工作流 |

`manumesh help`、`manumesh -h` 或 `manumesh --help` 显示帮助；`version`/`--version` 显示版本。
所有命令都拒绝未知选项、
缺少值和属于其他命令的选项。

## 常用简化配置

```powershell
manumesh simplify input.stl output.stl `
  --profile cad --method line --ratio 0.25 `
  --line-weight 1e-3 --preserve-feature-curves `
  --prevent-local-intersections --metrics-csv output/metrics.csv
```

当前 profile 为 `default`、`cad`、`scan`、`smooth`；`noisy-scan` 仅作为兼容别名接受，并解析为
`scan`，`smooth-surface` 解析为 `smooth`。`smooth` 面向光滑自由曲面，启用多尺度局部
quadric 脊/谷证据并默认关闭 normal tensor；它是显式 opt-in，默认 profile 不增加这部分计算。
`--method` 接受 `standard`、`qem`
（兼容别名）和 `line`；weight mode 为 `uniform`、`dihedral`、`normal-tensor`、
`smooth-curvature`、`height`、`xband`。`--ratio` 和 `--target-faces` 同时给出时以后者为准。

常见保护开关包括 `--preserve-boundary`、`--preserve-feature-curves`、
`--industrial-safe`、`--prevent-local-intersections`、`--min-triangle-quality`、
`--max-normal-deviation-deg`、`--max-local-error`、`--max-local-error-ratio` 和
`--quality-refinement-iterations`。特征检测还提供 normal-filter、normal-tensor、graph
cleanup/consolidation 和 `--surface-patches` 选项。

启用平滑曲率证据可使用 `--profile smooth`，或显式指定 `--smooth-curvature-features`。
相关阈值和尺度选项包括 `--smooth-curvature-threshold`、`--smooth-curvature-edge-alignment`、
`--smooth-curvature-tangent-consistency`、`--smooth-curvature-base-rings`、
`--smooth-curvature-scales`、`--smooth-curvature-min-persistent-scales` 和
`--smooth-curvature-robust-iterations`；需要按跨尺度稳定性选尺度时，再加
`--smooth-curvature-stable-scale` 与 `--smooth-curvature-min-scale-stability`。这些候选会
进入统一 feature graph、环恢复和简化器的曲线指导；默认 `primitive-curves` 只硬保护已拟合
的几何基元，严格锁定所有检测边时使用 `--feature-protection-mode all-feature-edges`。
在 `simplify` 的 `--method line` 路径中，`--profile smooth` 还默认选择 `smooth-curvature`
weight mode，使通过持久尺度筛选的逐顶点曲率分数参与 line-quadric 特征增益；显式
`--weight-mode` 会覆盖 profile 默认值。`feature-report` 只运行检测流程，不使用简化权重模式。仅使用
`--weight-mode smooth-curvature` 而未启用平滑曲率特征证据时，
`--smooth-curvature-edge-alignment` 不参与逐顶点评分；
`--smooth-curvature-min-scale-stability` 仅在同时指定 `--smooth-curvature-stable-scale` 时生效。

简化指标用 `--metrics-csv`，特征命令用 `--csv`，阶段计时用 `--performance-csv`；
完整字段仍以命令帮助和 CSV 表头为准。

执行前用 `--print-resolved-config` 查看 profile 与显式参数合并后的实际配置；用 `--verbose`
输出诊断。省略 `--threads` 时执行模式为串行；提供 `--threads N` 会请求并行模式，并将
`N` 作为最大并发度（`0` 交给后端选择）。没有 oneTBB 的构建只能使用 `--threads 1`；当前
Release preset 启用 oneTBB 后端。

## 生成器

`generate --type` 当前支持：`plane`、`clustered-plane`、`hole-plane`、`ridge`、
`noisy-plane`、`sine-terrain`、`terrace`、`bump`、`cylinder`、`torus`、`cube`、
`thin-fin`、`stepped-shaft`、`pipe-coupling`、`pulley`。分辨率 `--n` 的有效范围由
`MeshGenerators.h` 校验；未知类型或非法参数不会替换已有输出。

## 输出解释

CSV 和终端报告是诊断表现层，不是内部状态 API。简化未达到目标面数时，先查看
`termination_reason` 与分类拒绝计数，再判断是拓扑、边界、特征、质量、误差还是相交保护
限制。`large-import`/`large-validate` 只保证三角记录级存储完整性，不提供分区拓扑、跨分区
特征图或 out-of-core QEM。
