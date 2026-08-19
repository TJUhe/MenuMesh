# oneTBB Dependency

This directory vendors oneTBB 2021.12.0 from the upstream release source
archive:

```text
https://github.com/oneapi-src/oneTBB/archive/refs/tags/v2021.12.0.tar.gz
SHA-256: C7BB7AA69C254D91B8F0041A71C5BCC3936ACB64408A1719AEC0B2B7639DD84F
```

`source/` is the unmodified upstream source tree. Its `LICENSE.txt` contains
the Apache License 2.0 notice.

`MANUMESH_ONETBB_PROVIDER=vendored` is the default for oneTBB-enabled Release
presets. It builds the shared oneTBB runtime using the active Visual Studio
2019, MSVC v142, x64 and `/MD` configuration. `system` remains available as an
explicit alternative; `auto` tries the system package and then this vendored
source tree. No provider invokes a network download.

The SDK installs oneTBB's `README.md`, `LICENSE.txt`, and
`third-party-programs.txt` under `share/doc/ManuMesh/oneTBB`. ManuMesh keeps
the oneTBB shared-runtime choice local to the dependency even when the SDK
itself is built as a static archive.
