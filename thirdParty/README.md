# Third-Party Binary And Header Bundles

This directory stores vendored dependency bundles and tool runtimes that the
repository can consume offline.

Current policy:

- `eigen/` is an Eigen header bundle. Eigen is header-only, so there is no
  `.dll`, `.lib`, `.so`, or `.a` to link.
- `googletest/` contains repository test dependencies. The `source/` subtree is
  used to build GoogleTest from source when prebuilt binaries are undesirable.
- `doxygen/` vendors the documentation generator used by `docs-api` and
  `docs-internal`.
- `graphviz/` vendors the `dot` runtime and plugin/configuration files used by
  Doxygen graphs.
- Build scripts should prefer vendored bundles here and only fall back to
  machine-global tools when a vendored bundle is absent.

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
