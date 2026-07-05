# Third-Party Binary And Header Bundles

This directory is for SDK-style dependency bundles, not for expanded upstream
source repositories.

Current policy:

- `eigen/` is an Eigen header bundle. Eigen is header-only, so there is no
  `.dll`, `.lib`, `.so`, or `.a` to link.
- `googletest/` contains prebuilt GoogleTest binaries for repository tests.
  GoogleTest is not an SDK runtime dependency.
- Development tools, install templates, and editor extension files live under
  `adm/`, not here.

Preferred binary bundle shape:

```text
thirdParty/<name>/prebuilt/<toolchain-platform-variant>/
  include/
  lib/
  bin/
```

The main library should not require downstream users to build third-party
source trees just to consume the SDK.
