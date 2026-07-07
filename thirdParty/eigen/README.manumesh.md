# Eigen Header Bundle

Eigen is a header-only dependency used by the public C++ API and internal
geometry code.

There is intentionally no Eigen `.dll` or `.lib` in this bundle. Consumers
that use the C ABI do not include Eigen headers. Consumers that use the C++ API
get Eigen through the installed SDK property sheet or exported CMake target.
