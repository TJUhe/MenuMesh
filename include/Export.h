#pragma once

// MANUMESH_API selects the linkage for public symbols:
// - MANUMESH_BUILDING_DLL: defined while compiling the shared library
//   (dllexport / default visibility).
// - MANUMESH_USING_DLL: defined for consumers of the shared library on
//   Windows (dllimport). Propagated by the ManuMesh::manumesh target.
// - Neither macro (static branch): MANUMESH_API expands to nothing. This is
//   the path used when linking the internal static aggregate
//   (manumesh_internal) or a static manumesh_core build.
#if defined(_WIN32) || defined(__CYGWIN__)
#if defined(MANUMESH_BUILDING_DLL)
#define MANUMESH_API __declspec(dllexport)
#elif defined(MANUMESH_USING_DLL)
#define MANUMESH_API __declspec(dllimport)
#else
#define MANUMESH_API
#endif
#else
#if defined(MANUMESH_BUILDING_DLL) && __GNUC__ >= 4
#define MANUMESH_API __attribute__((visibility("default")))
#else
#define MANUMESH_API
#endif
#endif
