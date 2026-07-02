#pragma once

#if defined(_WIN32) || defined(__CYGWIN__)
#if defined(LINE_QUADRICS_QEM_BUILDING_DLL)
#define LQ_API __declspec(dllexport)
#elif defined(LINE_QUADRICS_QEM_USING_DLL)
#define LQ_API __declspec(dllimport)
#else
#define LQ_API
#endif
#else
#if defined(LINE_QUADRICS_QEM_BUILDING_DLL) && __GNUC__ >= 4
#define LQ_API __attribute__((visibility("default")))
#else
#define LQ_API
#endif
#endif
