# GoogleTest Binary Bundle

GoogleTest is used only by this repository's regression tests.

The default bundle is selected by compiler:

```text
prebuilt/msvc-x64-static/
  include/gtest/...
  lib/Debug/gtest.lib
  lib/Debug/gtest_main.lib
  lib/Release/gtest.lib
  lib/Release/gtest_main.lib

prebuilt/mingw-x64-shared/
  include/gtest/...
  lib/libgtest.dll.a
  lib/libgtest_main.dll.a
  bin/libgtest.dll
  bin/libgtest_main.dll
```

Configure with `-DLQ_GOOGLETEST_PROVIDER=prebuilt` to require the selected
bundle. The `auto` provider tries the prebuilt bundle first, then a system
GTest, then FetchContent as a developer fallback.
