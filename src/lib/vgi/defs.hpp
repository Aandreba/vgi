/*! \file */
#pragma once

#include <cassert>

#ifndef VGI_HAS_BUILTI
/// @brief Checks whether the compiler has support for the specified builtin.
#ifdef __has_builtin
#define VGI_HAS_BUILTIN(x) __has_builtin(x)
#else
#define VGI_HAS_BUILTIN(x) 0
#endif
#endif

#ifndef VGI_ASSERT
/// @brief Invokes detectable illegal behavior when `cond` is `false`.
/// @details In Debug mode, use regular C asserts. On Release mode, use asserts to help the compiler
/// with optimization
#ifndef NDEBUG
#define VGI_ASSERT(cond) assert(cond)
#else
#define VGI_ASSERT(cond) [[assume(cond)]]
#endif
#endif

#ifndef VGI_RESTRICT
/// @brief Restricted pointers cannot have overlapping memory, helping it optimize the code.
#if defined(_MSC_VER) && !defined(__clang__)
#define VGI_RESTRICT __restrict
#elif defined(__clang__) || defined(__GNUC__)
#define VGI_RESTRICT __restrict__
#else
#define VGI_RESTRICT
#endif
#endif

#ifndef VGI_FORCEINLINE
/// @brief Forces the compiler to inline calls to this function whenever possible
#if defined(_MSC_VER) && !defined(__clang__)
#define VGI_FORCEINLINE inline __forceinline
#elif defined(__has_attribute) && __has_attribute(always_inline)
#define VGI_FORCEINLINE inline __attribute__((always_inline))
#else
#define VGI_FORCEINLINE inline
#endif
#endif
