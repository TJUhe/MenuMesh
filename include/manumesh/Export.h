#pragma once

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
