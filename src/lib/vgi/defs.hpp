/*! \file */
#pragma once

#include <cassert>
#include <concepts>

#ifndef VGI_HAS_BUILTIN
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

/// @brief Tells the compiler that the code is unlikely to take this branch, helping it optimize
#ifndef VGI_IF_UNLIKELY
#if defined(__has_cpp_attribute) && __has_cpp_attribute(unlikely)
#define VGI_IF_UNLIKELY(cond) if (cond) [[unlikely]]
#elif VGI_HAS_BUILTIN(__builtin_expect)
#define VGI_IF_UNLIKELY(cond) if (__builtin_expect(static_cast<bool>(cond), 0))
#else
#define VGI_IF_UNLIKELY(cond) if (cond)
#endif
#endif

namespace vgi {
    //! @cond Doxygen_Suppress
    template<class T, class A, class... Args>
    struct is_same_as_any : is_same_as_any<T, Args...> {};
    template<class T, std::same_as<T> A, class... Args>
    struct is_same_as_any<T, A, Args...> : std::true_type {};
    template<class T, class A>
    struct is_same_as_any<T, A> : std::bool_constant<std::same_as<T, A>> {};
    template<class T, class A, class... Args>
    constexpr inline const bool is_same_as_any_v = is_same_as_any<T, A, Args...>::value;
    template<class T, class A, class... Args>
    concept same_as_any = is_same_as_any_v<T, A, Args...>;
    //! @endcond
}  // namespace vgi
