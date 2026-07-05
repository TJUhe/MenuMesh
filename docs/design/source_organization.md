# Source Organization

This repository uses a small geometry-kernel layout: public SDK headers are
separated from implementation files, while private helper headers stay beside
their implementation module under `detail/`.

The intent is to keep the code easy to browse today, but still leave a path
toward a larger OpenCascade-style geometry kernel later.

## Reference Model

OpenCascade OCCT is the main structural reference: public headers,
implementation code, tests, samples, tools, administrative files, and
documentation are clearly separated. The current project should stay compact at
its present size while preserving that boundary discipline.

## Directory Contract

| Path | Role | Rule |
| --- | --- | --- |
| `include/line_quadrics_qem/` | Installed SDK root | Keep only global headers and domain directories here. |
| `include/line_quadrics_qem/core/` | Mesh exchange, handles, status, topology cache | Safe for external applications to include. |
| `include/line_quadrics_qem/features/` | Feature detection API and public feature data | Safe for external analysis and algorithm users. |
| `include/line_quadrics_qem/algorithms/` | Public algorithm modules | New algorithms should live below this level. |
| `include/line_quadrics_qem/algorithms/simplification/` | QEM simplification options, reports, metrics, entry points | Current simplification module; safe for external decimation users. |
| `include/line_quadrics_qem/api/` | Binary-stable C ABI | Should not expose STL, Eigen, or C++ exceptions. |
| `src/<domain>/*.cpp` | Library implementation entry points | One `.cpp` should correspond to a visible module responsibility. |
| `src/<domain>/detail/*.h` | Private implementation helpers | May be included only by sources in that implementation module; never installed. |
| `apps/` | CLI and app-layer orchestration | Consumes the library like an external user. |
| `examples/` | Minimal external-consumer examples | Should include only public headers. |
| `tests/` | Regression and validation tests | Prefer public API; use internals only for narrowly justified white-box tests. |
| `docs/` | Design, guide, paper, and generated notes | Must describe the current code, not a desired future layout. |

## Current Simplification Module

The simplification implementation is now split like this:

```text
include/line_quadrics_qem/algorithms/simplification/
  QEMSimplifier.h       public options, reports, and simplification entry points
  Metrics.h             public quality metrics

src/simplification/
  QEMSimplifier.cpp            thin public API implementation
  SimplificationRun.cpp        per-run orchestration and collapse loop
  Quadrics.cpp                 QEM/line-quadric construction and placement solves
  FeatureConstraints.cpp       feature-curve collapse policy and projections
  CandidateQueue.cpp           collapse priority queue
  ResultBuilder.cpp            active-state to compact mesh conversion
  MeshEdges.cpp                local edge incidence utilities
  SimplificationValidation.cpp option and input validation
  Metrics.cpp                  metrics implementation
  CollapseLegality.cpp         legality checks that need local geometry/topology
  DynamicTopology.cpp          incremental incident-face topology
  GeometryPredicates.cpp
  SpatialFaceIndex.cpp         spatial hash for local intersection queries

src/simplification/detail/
  SimplificationRun.h          private run object
  SimplificationTypes.h        per-run vertex/face/candidate state
  Quadrics.h                   private quadric API
  FeatureConstraints.h         private feature policy API
  CandidateQueue.h             private queue API
  ResultBuilder.h              private result compaction API
  MeshEdges.h                  private edge incidence API
  SimplificationValidation.h   private validation API
  CollapseLegality.h           private legality API
  DynamicTopology.h            private incremental topology API
  GeometryPredicates.h         private local predicate API
  SpatialFaceIndex.h           private spatial-index API
```

`QEMSimplifier.h` remains the consumer contract. The `detail` headers are
allowed to change whenever the implementation changes, because they are not
installed and are not part of the SDK.

## Include Policy

Use explicit include paths that show ownership:

```cpp
#include "line_quadrics_qem/algorithms/simplification/QEMSimplifier.h" // public
#include "detail/DynamicTopology.h"                                    // private
```

Do not include `detail/...` from public headers, examples, apps, or external
tests. If a concept is needed outside the library, first decide whether it is a
stable API. If yes, promote a small public type to `include/`; if not, keep it
private and expose behavior through options, reports, or a public algorithm
function.

## Growth Rule

When a module grows, split by responsibility rather than by formula fragments:

- Public data contracts go to `include/line_quadrics_qem/<domain>/` or
  `include/line_quadrics_qem/algorithms/<algorithm>/`.
- Long-lived implementation helpers go to the owning module's `detail/`
  directory.
- Heavy algorithms get one or more `src/<domain>/*.cpp` files with matching
  private headers.
- Cross-domain utilities should first be justified in `core/` or a future
  `analysis/`, `repair/`, `remesh/`, or `boolean/` domain, not copied between
  modules.

This preserves an OCCT-like habit: a header's location should answer whether it
is public API, private implementation, or application/test code.
