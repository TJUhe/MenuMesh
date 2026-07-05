# Tests Layout

| Path | Purpose |
| --- | --- |
| `include/` | Shared test helper headers. |
| `src/` | Shared test helper implementation. |
| `data/` | Tracked fixtures and external regression meshes. |
| `output/` | Local validation outputs from CLI validation commands; ignored by git. |

GTest and CTest runtime logs belong to the build tree. CLI validation artifacts,
such as copied external validation STL files and CSV files from
`validate-features` or generated reports from `validate-external`, belong under
`tests/output/`.
