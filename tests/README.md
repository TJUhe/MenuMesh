# Tests Layout

| Path | Purpose |
| --- | --- |
| `include/` | Shared test helper headers. |
| `src/` | Shared test helper implementation. |
| `data/` | Tracked fixtures and external regression meshes. |
| `output/` | Local validation outputs from CLI validation commands; ignored by git. |

GTest and CTest runtime logs belong to the build tree. CLI validation artifacts,
such as generated STL and CSV files from `validate-features` or
`validate-external`, belong under `tests/output/`.
