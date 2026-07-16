/**
 * @file include/Export.h
 * @brief Declares export facilities for ManuMesh's core-mesh module.
 * @ingroup manumesh_core
 *
 * @details `MANUMESH_API` selects the linkage for public symbols.
 * `MANUMESH_BUILDING_DLL` exports symbols while compiling the shared library;
 * `MANUMESH_USING_DLL` imports them for Windows consumers. With neither macro,
 * the annotation is empty for static/internal builds. Non-Windows shared builds
 * use default ELF visibility when supported.
 */

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
