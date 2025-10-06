#pragma once

#include <cassert>

// In Debug mode, use regular asserts
// On Release mode, use asserts to help the compiler with optimization
#ifndef VGI_ASSERT
#ifndef NDEBUG
#define VGI_ASSERT(cond) assert(cond)
#else
#define VGI_ASSERT(cond) [[assume(cond)]]
#endif
#endif

// Restricted pointers cannot have overlapping memory, helping the compiler better optimize the code
#ifndef VGI_RESTRICT
#if defined(_MSC_VER) && !defined(__clang__)
#define VGI_RESTRICT __restrict
#elif defined(__clang__) || defined(__GNUC__)
#define VGI_RESTRICT __restrict__
#else
#define VGI_RESTRICT
#endif
#endif

// This macro creates random names for variables and types, usefull for macros
#define VGI_ANON SLOT_CONCAT(__vgi_anon_, __COUNTER__)
#define VGI_CONCAT(x, y) VGI_CONCAT_(x, y)
#define VGI_CONCAT_(x, y) x##y
