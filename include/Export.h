/**
 * @file include/Export.h
 * @brief 声明 ManuMesh 核心网格模块的导出设施。
 * @ingroup manumesh_core
 *
 * @details `MANUMESH_API` 选择公共符号的链接方式。
 * 编译共享库时，`MANUMESH_BUILDING_DLL` 导出符号；Windows 使用者通过
 * `MANUMESH_USING_DLL` 导入符号。两个宏都未定义时，静态/内部构建不添加注解。
 * 非 Windows 的共享构建在支持时使用默认 ELF 可见性。
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
