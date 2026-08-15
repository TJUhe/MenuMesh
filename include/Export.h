/**
 * @file include/Export.h
 * @brief 声明 ManuMesh 核心网格模块的导出设施。
 * @ingroup manumesh_core
 *
 * @details `MANUMESH_API` 选择公共符号的链接方式。
 * 编译共享库时，`MANUMESH_BUILDING_DLL` 导出符号；SDK 使用者通过
 * `MANUMESH_USING_DLL` 导入符号。两个宏都未定义时，静态/内部构建不添加注解。
 * 公共头只支持 Visual Studio 2019、MSVC v142 和 x64。
 */

#pragma once

#if defined(__clang__) || !defined(_MSC_VER) || _MSC_VER < 1920 || _MSC_VER >= 1930 || !defined(_M_X64)
#error "ManuMesh requires Visual Studio 2019, MSVC v142, and the x64 target architecture."
#endif

#if defined(MANUMESH_BUILDING_DLL)
#define MANUMESH_API __declspec(dllexport)
#elif defined(MANUMESH_USING_DLL)
#define MANUMESH_API __declspec(dllimport)
#else
#define MANUMESH_API
#endif
