# Examples Layout

Small examples for using the library.

| Path | Purpose |
| --- | --- |
| `basic_simplify.cpp` | Minimal C++ API example. |
| `c_api_basic.c` | Minimal C API example. |
| `sdk_consumer/` | Independent downstream project that builds only against an installed SDK root. |
| `CMakeLists.txt` | Example build integration. |

`basic_simplify.cpp` and `c_api_basic.c` are source-tree smoke tests linked to
the in-tree target. `sdk_consumer/` is the stronger integration check: install
the SDK first, then configure that project with `-DMANUMESH_SDK_ROOT=<install-prefix>`
so it sees only the released `include/`, `lib/`, `bin/`, and `share/` layout.

Demo outputs belong under `output/`. Validation inputs belong under
`tests/data/`; validation outputs belong under `tests/output/`.
