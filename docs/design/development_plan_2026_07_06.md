# Line Quadrics QEM Development Plan

Date: 2026-07-06

## 1. Background And Goal

This repository is currently a C++17 mesh simplification SDK prototype oriented
toward industrial use. The implemented core is QEM plus line quadrics, with
feature detection and feature-preserving edge-collapse behavior. The project is
not just a demo executable: it already has a public C++ API, a C ABI surface, a
CLI, examples, tests, validation data, generated notes, and SDK installation
support.

The practical development goal is to turn the existing prototype into a stable
and auditable mesh simplification kernel that can be integrated by downstream
applications. The near-term deliverable should be a version that can:

- simplify STL/OBJ triangle meshes with stable QEM/line-quadric behavior;
- preserve important industrial features such as circular holes, near-circular
  loops, ellipse loops, hard creases, and open boundaries;
- expose clear C++ API, C API, CLI, report, and SDK integration paths;
- produce measurable validation outputs such as STL, CSV metrics, gtest
  results, and feature-comparison reports;
- document what is implemented, what is experimental, and what remains risky.

## 2. Current Code Baseline

The current source tree already has the following useful foundations:

| Area | Current implementation | Main paths |
| --- | --- | --- |
| Mesh exchange model | Dense vertex array plus triangle faces, STL/OBJ loading, mesh validation, STL writing | `include/line_quadrics_qem/core/`, `src/core/` |
| Simplification API | `SimplifyOptions`, `SimplifyReport`, `QEMSimplifier`, `simplifyMesh` | `include/line_quadrics_qem/algorithms/simplification/QEMSimplifier.h` |
| QEM and line quadrics | Plane quadrics, line quadrics, adaptive/spatial line weights, optimal/fallback placement candidates | `src/simplification/Quadrics.cpp` |
| Collapse loop | Candidate priority queue, dynamic topology, stale-candidate filtering, repeated edge collapse | `src/simplification/SimplificationRun.cpp`, `src/simplification/CandidateQueue.cpp` |
| Legality filters | Link condition, boundary policy, normal flip, triangle quality, local error, local self-intersection guard | `src/simplification/CollapseLegality.cpp` |
| Feature protection | Feature loops, circle/near-circle/ellipse primitive data, projection and hard protection policies | `include/line_quadrics_qem/features/`, `src/features/`, `src/simplification/FeatureConstraints.cpp` |
| External surfaces | CLI, C API, C++ examples, C ABI examples, SDK consumer project | `apps/linequadrics/`, `src/api/`, `examples/` |
| Verification | Unit tests, parameter tests, external data fixtures, industrial validation notes | `tests/`, `docs/design/industrial_validation.md` |

The current algorithm is already beyond a minimal QEM demo. The next work should
therefore focus less on inventing a new algorithm from scratch and more on
stabilizing behavior, improving feature preservation, hardening validation, and
making the SDK boundary easier to consume.

## 3. Development Strategy

The recommended order is:

1. Freeze and document the current baseline.
2. Stabilize the core edge-collapse pipeline.
3. Improve industrial feature detection and preservation.
4. Borrow proven design patterns from third-party mesh libraries.
5. Harden validation against real model sets.
6. Polish API, C ABI, CLI, and SDK integration.
7. Prepare release-quality documentation and examples.

This order matters because feature and SDK work depends on a reliable collapse
core. If the collapse loop is unstable, later API and documentation work will
only describe moving behavior.

## 4. Three-Week Development Plan

The development cycle is fixed at 3 weeks for the first verifiable version. The
plan is organized by week, not by scattered day-level estimates. Each week has a
clear theme, implementation scope, deliverables, and acceptance criteria.

### Week 1: Baseline And Core Simplification Stabilization

Theme: make the existing codebase build, run, and simplify predictably before
changing feature behavior.

Main goals:

- Confirm the current build, tests, CLI smoke path, and SDK example baseline.
- Stabilize the core QEM/line-quadric edge-collapse flow.
- Make termination reasons and rejection counters clear enough for debugging.

Development tasks:

- Build the project in a clean debug directory and run existing tests.
- Run CLI smoke tests on generated meshes and several existing STL/OBJ fixtures.
- Review `SimplificationRun::execute`, `collapseUntilTarget`, `tryCollapse`,
  and `applyCollapse` as the main algorithm path.
- Verify candidate queue behavior, stale-candidate handling, queue rebuild
  policy, and dynamic topology updates.
- Check quadric construction and fallback placement behavior for degenerate or
  nearly singular cases.
- Strengthen focused tests around topology rejection, normal deviation,
  triangle quality, local error, fallback placement, and termination reason.

Recommended commands:

```powershell
cmake -S . -B build/dev-week1 -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++
cmake --build build/dev-week1 --parallel
cmake -E chdir build/dev-week1 ctest --output-on-failure
```

Primary files:

- `src/simplification/SimplificationRun.cpp`
- `src/simplification/Quadrics.cpp`
- `src/simplification/CandidateQueue.cpp`
- `src/simplification/DynamicTopology.cpp`
- `src/simplification/CollapseLegality.cpp`
- `tests/simplifier_tests.cpp`
- `tests/qem_parameter_tests.cpp`

Week 1 deliverables:

- Baseline build and test summary.
- A short list of known current risks or failing cases.
- Stable simplification behavior at representative target ratios such as 0.8,
  0.5, 0.25, and 0.1.
- Focused tests for the main collapse rejection paths.
- Clearer interpretation of `SimplifyReport` counters.

Week 1 acceptance criteria:

- Project builds successfully in the selected primary toolchain.
- Core tests pass, or remaining failures are documented with exact follow-up
  tasks.
- Built-in generated meshes simplify consistently.
- The simplifier reaches target faces or reports a clear termination reason.

### Week 2: Industrial Feature Detection And Protection

Theme: make the simplified results preserve industrial features that matter in
real meshes, while using mature third-party libraries as design references.

Main goals:

- Improve and validate circle, near-circle, ellipse, hard-edge, boundary, and
  junction feature handling.
- Tune feature-protection options so they are useful without blocking all
  simplification.
- Make feature behavior measurable through tests and reports.
- Decide which third-party design patterns should be borrowed directly and which
  should remain reference-only.

Development tasks:

- Review `FeatureDetection` output on coaxial holes, near-circular holes,
  elliptical holes, boss/pocket plates, flange-like models, NASA examples, and
  Thingi10K examples already present under `tests/data`.
- Confirm `FeatureProtectionMode::PrimitiveCurves` as the practical default:
  hard-protect fitted primitive loops while keeping generic polygonal creases
  mostly soft unless strict mode is requested.
- Improve feature-loop conflict handling where loops pass through junctions.
- Tune `featureAngleDeg`, `featureCurveWeight`,
  `circleFitRelativeThreshold`, `ellipseFitRelativeThreshold`,
  `nearCircleAxisRatioTolerance`, `minFeatureLoopVertices`, and
  `maxFeatureCurveDeviationRatio`.
- Strengthen circular and elliptical projection behavior during placement.
- Add feature drift checks or feature-comparison output where practical.
- Compare the current implementation against CGAL Surface Mesh Simplification,
  OpenMesh decimation modules, libigl/qslim-style QEM examples,
  MeshLab/VCGLib filters, and OCCT-style industrial API boundaries.
- Extract practical upgrade items from those libraries, especially constrained
  edge collapse policies, stop predicates, placement policies, module-style
  decimation, validation metrics, and SDK separation.

Primary files:

- `include/line_quadrics_qem/features/FeatureDetection.h`
- `src/features/FeatureDetection.cpp`
- `src/simplification/FeatureConstraints.cpp`
- `tests/feature_detection_tests.cpp`
- `tests/qem_parameter_tests.cpp`
- `tests/qem_dataset_tests.cpp`

Week 2 deliverables:

- Test coverage for circle, near-circle, ellipse, polygonal loop, junction, and
  boundary feature cases.
- Parameter recommendations for common industrial feature-preserving runs.
- A comparison of standard QEM, line quadrics, and protected feature mode on
  representative fixtures.
- A third-party library comparison note that maps what we should borrow, what we
  should avoid, and what level the current library should reach.
- A matrix-level explanation of how borrowed ideas become QEM terms, placement
  solves, hard predicates, feature projections, and validation gates.
- Observable feature-related rejection and projection counters in
  `SimplifyReport`.

Week 2 acceptance criteria:

- Circular and elliptical fixture loops remain measurable after simplification.
- Protected feature mode reduces visible or measured feature drift compared with
  unprotected simplification on representative meshes.
- Feature protection does not routinely prevent the simplifier from making
  progress on normal inputs.
- The third-party comparison produces concrete engineering tasks rather than a
  general literature summary.

### Week 3: Validation, Integration, And Delivery

Theme: turn the stabilized implementation into something that can be reviewed,
called by downstream code, and handed over.

Main goals:

- Produce repeatable validation commands and CSV/STL outputs.
- Verify public C++ API, C ABI, CLI, examples, and SDK installation behavior.
- Update documentation so the delivered version is understandable and auditable.

Development tasks:

- Consolidate validation commands from `docs/design/industrial_validation.md`.
- Ensure CLI validation commands produce stable CSV columns.
- Build a standard validation set from existing `tests/data` fixtures.
- Track metrics for face count, triangle quality, edge-length distribution,
  approximate distance error, boundary changes, non-manifold edges, feature
  loops, and rejection counters.
- Verify C++ API examples and C ABI examples.
- Verify SDK consumer build against installed SDK artifacts.
- Review public headers to keep private simplification details out of the SDK
  surface.
- Apply the selected third-party-inspired upgrades that fit inside the three-week
  scope, such as clearer policy boundaries, better validation gates, and cleaner
  SDK packaging.
- Update README, SDK integration guide, algorithm roadmap, validation docs, and
  known limitations.

Primary paths:

- `apps/linequadrics/main.cpp`
- `src/simplification/Metrics.cpp`
- `include/line_quadrics_qem/`
- `include/line_quadrics_qem/api/CApi.h`
- `src/api/CApi.cpp`
- `examples/`
- `examples/sdk_consumer/`
- `adm/templates/`
- `docs/design/industrial_validation.md`
- `docs/guide/sdk_integration.md`

Week 3 deliverables:

- Repeatable validation command list.
- CSV summary files and STL outputs for selected representative models.
- Verified C++ API example, C ABI example, and SDK consumer example.
- Updated docs and release checklist.
- Known-limitations section that distinguishes implemented behavior from future
  roadmap items.
- A clear statement of the target upgrade level: from prototype simplifier to
  small industrial SDK kernel, not yet a full CGAL/OpenMesh/OCCT replacement.

Week 3 acceptance criteria:

- Fast tests remain practical for normal development.
- Slower validation has documented commands and expected outputs.
- Downstream examples build against installed SDK artifacts, not private source
  files.
- CLI options map cleanly to documented `SimplifyOptions`.
- A new developer can build, test, run a simplification, and understand the main
  API without reading all source files.

## 5. Weekly Milestone Summary

| Week | Focus | Concrete output |
| --- | --- | --- |
| Week 1 | Baseline and core simplification | Clean build/test result, stable collapse loop, core rejection tests, baseline STL/CSV outputs |
| Week 2 | Feature detection, protection, and third-party borrowing | Feature fixtures covered, primitive loop protection tuned, third-party comparison note, parameter recommendations |
| Week 3 | Validation, integration, and delivery | Validation matrix outputs, selected third-party-inspired upgrades, C++/C API examples, SDK consumer verification, updated docs and release checklist |

## 6. Third-Party Library Borrowing Strategy

The goal is not to wrap a third-party library blindly. The goal is to learn from
the stable parts of mature geometry libraries and upgrade this repository in the
places where it is still prototype-like.

| Reference library | What to study | What to borrow for this project | Upgrade target |
| --- | --- | --- | --- |
| CGAL Surface Mesh Simplification | Stop predicates, edge constraints, placement policies, cost policies | Make collapse policies explicit and testable; separate "can collapse", "where to place", and "when to stop" | From monolithic simplifier flow to policy-oriented simplification kernel |
| OpenMesh decimation | Module-based decimation, binary/non-binary module decisions, mesh-status tracking | Keep rejection reasons modular and make future legality/cost modules easier to add | From one-off checks to composable decimation modules |
| libigl qslim examples | Minimal QEM data flow, compact implementation, easy experimentation | Preserve a small readable QEM path for debugging and teaching | Keep the algorithm understandable while adding industrial guards |
| MeshLab/VCGLib filters | Practical mesh cleanup, quality metrics, batch validation mindset | Improve validation metrics and pre/post simplification checks | From "runs on examples" to measurable batch validation |
| OCCT-style SDK boundaries | Industrial API separation, installed headers/libs, long-term binary integration | Keep private algorithm helpers out of public headers; strengthen C ABI and install layout | From research-style codebase to consumable SDK layout |

### 6.1 Third-Party Strengths And Concrete Borrowing

The distilled literature suggests treating feature-preserving simplification as
constrained optimization rather than plain decimation. In this repository, the
best borrowing strategy is to keep QEM/line quadrics as the cost backbone, then
borrow policy structure, validation discipline, and SDK boundaries from mature
libraries.

CGAL Surface Mesh Simplification:

- Strength: explicit separation of cost, placement, stop predicate, and
  constrained-edge behavior.
- Borrowing plan: split our mental model and tests into the same decisions:
  `cost(edge)`, `placement(edge)`, `legal(edge, placement)`, and
  `stop(report/state)`.
- Matrix meaning: CGAL-style cost and placement can be represented in our code
  by the merged edge quadric
  \[
    Q_{ab}=Q_a+Q_b,
  \]
  while constrained edges remain hard predicates instead of just large weights.

OpenMesh decimation modules:

- Strength: modular binary and non-binary decimation rules.
- Borrowing plan: keep topology, boundary, normal, triangle-quality,
  self-intersection, feature, and local-error checks independently observable.
- Matrix meaning: not every decision should be folded into one matrix. Soft
  geometric preferences belong in \(Q\), but hard validity checks should remain
  predicates:
  \[
    \operatorname{accept}(a,b,x)=
    P_{\text{topology}}\land P_{\text{boundary}}\land P_{\text{normal}}\land
    P_{\text{quality}}\land P_{\text{feature}}\land P_{\text{error}}.
  \]

libigl qslim-style examples:

- Strength: compact QEM data flow that is easy to debug and teach.
- Borrowing plan: keep a readable standard-QEM path in the code and tests even
  as industrial guards are added.
- Matrix meaning: every vertex owns one symmetric error matrix, and edge
  collapse is easy to reason about:
  \[
    E_i(x)=\tilde{x}^{T}Q_i\tilde{x},\quad
    \tilde{x}=(x,y,z,1)^T.
  \]

MeshLab/VCGLib filters:

- Strength: practical mesh cleanup, batch processing, and quality metrics.
- Borrowing plan: improve pre/post validation, CSV metrics, and regression
  checks instead of relying only on visual STL inspection.
- Matrix meaning: matrix cost alone is not a full quality guarantee. We still
  need post-collapse measurements such as triangle quality, normal deviation,
  approximate distance, boundary change, and non-manifold edges.

OCCT-style industrial boundaries:

- Strength: stable SDK boundary, strict public/private separation, and long-term
  integration expectations.
- Borrowing plan: expose only `Mesh`, `SimplifyOptions`, `SimplifyReport`,
  C API handles, examples, and installed SDK artifacts; keep topology and
  collapse internals private.
- Matrix meaning: downstream users should configure weights and constraints,
  but should not depend on the internal matrix assembly layout.

### 6.2 Matrix-Level Explanation

Current QEM behavior can be explained as a sum of symmetric quadratic forms.
For a plane with unit normal \(n\) and offset \(d=-n\cdot p\), write

\[
  \pi=(n_x,n_y,n_z,d)^T.
\]

The plane quadric is

\[
  K_{\text{plane}}=\pi\pi^T,
\]

and the squared plane error of a homogeneous vertex \(\tilde{x}=(x,y,z,1)^T\)
is

\[
  E_{\text{plane}}(x)=\tilde{x}^{T}K_{\text{plane}}\tilde{x}.
\]

For one mesh vertex, all incident face contributions are accumulated:

\[
  Q_i=\sum_{f\in N(i)} w_f K_f.
\]

Line quadrics add a soft penalty that reduces tangential drift in
underconstrained flat regions. For a line through point \(p\) with unit direction
\(t\), define

\[
  A=I-tt^T,\quad b=-Ap,\quad c=p^TAp.
\]

The line-distance quadric is

\[
  K_{\text{line}}=
  \begin{bmatrix}
    A & b \\
    b^T & c
  \end{bmatrix},
\]

so the line error is

\[
  E_{\text{line}}(x)=\tilde{x}^{T}K_{\text{line}}\tilde{x}.
\]

This matches the implementation idea in `lineQuadric`: choose two planes whose
normals span the subspace orthogonal to the line direction, then add those two
plane quadrics.

Feature protection can be represented as a combination of soft matrix terms and
hard predicates. A feature tangent line contributes

\[
  Q_i \leftarrow Q_i+\lambda_{\text{curve}}K_{\text{line}},
\]

while primitive loops such as circles and ellipses are also protected by
collapse rejection and placement projection. For an edge collapse \(a\rightarrow
b\), the merged matrix is

\[
  Q_{ab}=Q_a+Q_b.
\]

Partition it as

\[
  Q_{ab}=
  \begin{bmatrix}
    A & b \\
    b^T & c
  \end{bmatrix}.
\]

The unconstrained optimal placement solves

\[
  \min_x \begin{bmatrix}x\\1\end{bmatrix}^T
  Q_{ab}
  \begin{bmatrix}x\\1\end{bmatrix},
  \quad\Rightarrow\quad
  Ax=-b.
\]

When \(A\) is singular or poorly conditioned, the robust engineering behavior is
to test fallback candidates such as endpoint \(a\), endpoint \(b\), and midpoint
\((a+b)/2\). This is the same practical direction recommended by edge-collapse
engineering guides: matrix optimum first, guarded fallbacks second.

For a hard feature curve \(C\), the mathematically ideal form is constrained
optimization:

\[
  \min_x \tilde{x}^{T}Q_{ab}\tilde{x}
  \quad\text{s.t.}\quad x\in C.
\]

The current implementation uses a practical approximation:

1. solve or choose a candidate position \(x^\*\);
2. if the edge belongs to a protected primitive feature, project it back:
   \[
     x_C=\Pi_C(x^\*);
   \]
3. run hard legality predicates on \(x_C\);
4. accept the collapse only if topology, boundary, normals, triangle quality,
   local error, and feature budgets all pass.

This matrix view gives a clear upgrade target:

- borrow CGAL-style policies to separate matrix cost, placement, constraints,
  and stop conditions;
- borrow OpenMesh-style modules to keep hard predicates independent and
  reportable;
- borrow libigl-style clarity so the base QEM path remains easy to debug;
- borrow MeshLab/VCGLib-style validation so matrix error is checked against
  real mesh-quality and feature-quality metrics;
- borrow OCCT-style SDK boundaries so downstream users see stable options and
  reports, not private matrix assembly internals.

The target library level after three weeks is therefore a matrix-aware,
policy-oriented simplification SDK: QEM and line quadrics rank candidate
collapses, feature and boundary logic constrain or project placements, and
validation metrics prove whether the output is acceptable.

Concrete borrowing tasks inside the three-week cycle:

- Week 1: compare the current collapse loop with CGAL/OpenMesh-style policy
  separation and identify where the code should stay as-is versus where naming
  or tests should be improved.
- Week 2: use third-party patterns to refine feature constraints, stop
  conditions, rejection categories, and feature validation metrics.
- Week 3: reflect the selected patterns in docs, examples, SDK boundaries, and
  validation gates.

Target upgrade level after three weeks:

- Core simplifier is stable enough to be treated as a small SDK component.
- Collapse decisions are observable through reports and tests.
- Feature protection covers common industrial primitive loops with measurable
  behavior.
- CLI and API examples demonstrate real downstream integration.
- Validation is repeatable through commands and CSV outputs.

Target level not claimed after three weeks:

- It is not a full replacement for CGAL, OpenMesh, MeshLab, VCGLib, or OCCT.
- It does not yet provide complete mesh repair, Boolean operations, exact
  predicate guarantees, or full CAD/B-Rep semantic preservation.
- It remains a focused QEM/line-quadric simplification kernel with an industrial
  SDK direction.

## 7. Priority Ranking

P0, must finish first:

- Clean build and test baseline.
- Stable QEM/line-quadric edge collapse.
- Deterministic termination reason and report counters.
- Core topology, normal, quality, and local-error guards.

P1, needed for practical industrial value:

- Circle, near-circle, and ellipse feature preservation.
- Boundary-preserving simplification.
- Third-party library comparison translated into concrete upgrade tasks.
- Repeatable CLI validation outputs.
- C++ API and C ABI examples.
- SDK consumer verification.

P2, valuable but can follow after first validation:

- Broader real-model validation set.
- More robust feature graph ownership across complex junctions.
- Stronger exact or filtered geometric predicates.
- Richer attribute preservation, such as normals, UVs, colors, and source face
  IDs.
- Additional mesh repair and healing preflight steps inspired by libraries such
  as MeshLab/VCGLib and CGAL.

## 8. Main Risks And Mitigation

| Risk | Impact | Mitigation |
| --- | --- | --- |
| Feature detection is sensitive to tessellation quality | Circular or ellipse loops may be missed or misclassified | Use multiple fixtures, expose thresholds, report detection confidence |
| Aggressive protection blocks simplification | The algorithm may stop before target face count | Prefer primitive-only hard protection by default; keep generic creases as soft costs where possible |
| Local self-intersection checks are not full global proof | Some invalid outputs may still pass local checks | Document limitation; use spatial local checks plus external validation and visual STL review |
| Metrics are approximate | Distance and feature drift metrics may not fully represent CAD intent | Keep metrics as engineering signals, not formal CAD equivalence proof |
| SDK surface changes during algorithm work | Downstream code may churn | Keep public headers small; hide private helpers in `src/simplification/detail/` |
| Large models may expose performance limits | Validation may become slow | Separate fast unit tests from slower local validation; use representative subsets |
| Third-party comparison becomes too broad | Time may be spent reading libraries instead of upgrading this codebase | Limit comparison to directly borrowable patterns: policies, metrics, tests, API boundaries |
| Direct dependency adoption creates integration risk | Adding heavy dependencies may complicate SDK packaging | Prefer borrowing patterns first; add dependencies only with a clear build and licensing reason |

## 9. Proposed Reply For Stakeholder Confirmation

The following can be used as the concise communication version:

```text
我先按这个库当前情况整理一版实际开发方案。现在这个项目已经有 C++17 的
QEM + Line Quadrics 简化内核、特征检测、C++ API、C API、CLI、测试和 SDK
集成基础。开发周期我们先按 3 周规划，并且以周为单位拆解：

第 1 周先做现有代码和核心简化流程稳定，包括构建测试、QEM/line quadric、
候选边队列、边折叠、拓扑/法向/质量/局部误差检查。

第 2 周重点做工业特征保护，包括圆孔、近圆、椭圆、硬边、边界环等特征的检测、
保护、投影和参数调优。同时会重点对标 CGAL、OpenMesh、libigl、MeshLab/VCGLib
和 OCCT 这类第三方库，明确它们的优点、哪些设计可以借鉴到我们的库里，并且
会从 QEM 矩阵、line quadric 矩阵、约束投影和合法性检查的角度把原理讲清楚。

第 3 周做验证、接口和交付，包括真实 STL/OBJ 数据集验证、CSV 指标、C++/C API
示例、SDK 集成、CLI 命令和文档。最终目标不是简单跑通 demo，而是把当前库从
原型级简化程序升级到一个小型工业 SDK 内核：有稳定 API、可验证指标、清楚的
特征保护策略和可交付的集成方式。

我会把每一周的目标、开发内容、交付物、验证方式和风险都写清楚。初稿出来后，
我们再基于实际优先级确认每周边界和交付标准。
```

## 10. Immediate Next Actions

1. Confirm the primary toolchain and build directory for this cycle.
2. Run the Week 1 baseline build and tests.
3. Pick 3 to 5 representative meshes for ongoing validation.
4. Open issues or checklist items for Week 1 P0 stabilization tasks.
5. Create the Week 2 third-party comparison checklist so the borrowing scope is
   explicit before implementation starts.
6. Review this plan with stakeholders and adjust timeline based on delivery
   priority.
