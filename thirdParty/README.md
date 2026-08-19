# Third-Party Binary And Header Bundles

This directory stores vendored dependency bundles and tool runtimes that the
repository can consume offline.

Current policy:

- `eigen/` is an Eigen header bundle. Eigen is header-only, so there is no
  `.dll`, `.lib`, `.so`, or `.a` to link.
- `googletest/` contains repository test dependencies for the supported
  Visual Studio 2019, MSVC v142, x64 baseline. The `source/` subtree is the
  only bundled provider; GoogleTest is built with the active C++14 and `/MD`
  settings instead of using repository-hosted prebuilt libraries.
- `oneTBB/` contains the oneTBB 2021.12.0 source used by the optional
  deterministic parallel backend. It is built with the active C++14 and `/MD`
  settings, so Release builds do not depend on a network fetch or a prebuilt
  binary ABI. SDK installs retain oneTBB's `LICENSE.txt` and
  `third-party-programs.txt` notices under `share/doc/ManuMesh/oneTBB`.
- `graphviz/` vendors the `dot` runtime and plugin/configuration files used by
  Doxygen graphs.
- Build scripts should prefer supported vendored libraries and tools here when
  present. Doxygen itself is discovered from the developer environment.

Preferred bundle shape:

```text
thirdParty/<name>/
  README.manumesh.md
  prebuilt/<toolchain-platform-variant>/
    include/
    lib/
    bin/
```

Notes:

- Tool bundles may carry additional runtime folders such as `share/` when the
  upstream program requires plugins, configuration, fonts, or sample assets.
- The main library should not require downstream users to build third-party
  source trees just to consume the SDK.
