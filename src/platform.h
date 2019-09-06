/****************************************************************************
*
*  APPLE //E EMULATOR FOR WINDOWS
*
*  Copyright (C) 1994-96, Michael O'Brien.  All rights reserved.
*
***/

#pragma once

#if defined(_M_X64) || defined(__x86_64__)
#   define CPU_X64
#endif

#if defined(_M_IX86) || defined(__i386__)
#   define CPU_X86
#endif

#if defined(_WIN32) || defined(_WIN64)
#   define OS_WINDOWS
#elif defined(__linux__)
#   define OS_LINUX
#elif defined(__ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__)
#   define OS_OSX
#endif

#if defined(OS_LINUX) || defined(OS_OSX)
#   define OS_POSIX
#endif

#if defined(__clang__)
#   define COMPILER_CLANG
#elif defined(_MSC_VER)
#   define COMPILER_MSVC
#elif defined(__GNUC__)
#   define COMPILER_GCC
#endif

#if !defined(COMPILER_MSVC)

typedef uint32_t    DWORD;
typedef int32_t     BOOL;
typedef uint8_t     BYTE;
typedef uint16_t    WORD;
typedef int32_t     INT;
typedef uint32_t    UINT;
typedef float       FLOAT;
typedef FLOAT       *PFLOAT;
typedef BOOL        *PBOOL;
typedef BOOL        *LPBOOL;
typedef BYTE        *PBYTE;
typedef BYTE        *LPBYTE;
typedef int32_t     *PINT;
typedef int32_t     *LPINT;
typedef uint16_t    *PWORD;
typedef uint16_t    *LPWORD;
typedef uint32_t    *LPLONG;
typedef uint32_t    *PDWORD;
typedef uint32_t    *LPDWORD;
typedef void        *LPVOID;
typedef const void  *LPCVOID;
typedef uint32_t    *PUINT;

#ifndef TRUE
#   define TRUE     1
#endif
#ifndef FALSE
#   define FALSE    0
#endif

#if defined(CPU_X64)
typedef int64_t     INT_PTR;
typedef int64_t     *PINT_PTR;
typedef uint64_t    UINT_PTR;
typedef uint64_t    *PUINT_PTR;
typedef int64_t     LONG_PTR;
typedef int64_t     *PLONG_PTR;
typedef uint64_t    ULONG_PTR;
typedef uint64_t    *PULONG_PTR;
#elif defined(CPU_X86)
typedef int32_t     INT_PTR;
typedef int32_t     *PINT_PTR;
typedef uint32_t    UINT_PTR;
typedef uint32_t    *PUINT_PTR;
typedef int32_t     LONG_PTR;
typedef int32_t     *PLONG_PTR;
typedef uint32_t    ULONG_PTR;
typedef uint32_t    *PULONG_PTR;
#endif

#endif //  !defined(COMPILER_MSVC)
