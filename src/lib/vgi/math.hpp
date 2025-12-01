/*! \file */
#pragma once

#include <cassert>
#include <chrono>
#include <cmath>
#include <concepts>
#include <limits>
#include <optional>

#include "arch.hpp"
#include "defs.hpp"
#include "math/transf3d.hpp"

#ifdef VGI_ARCH_FAMILY_X86
#include <immintrin.h>
#endif

#ifdef _MSC_VER
#include <intrin.h>
#endif

namespace vgi::math {
    /// @brief Specifies that a type is an integral type
    template<class T>
    concept integral = std::numeric_limits<T>::is_integer;
    /// @brief Specifies that a type is a signed integral type
    template<class T>
    concept signed_integral = integral<T> && std::numeric_limits<T>::is_signed;
    /// @brief Specifies that a type is an unsigned integral type
    template<class T>
    concept unsigned_integral = integral<T> && !std::numeric_limits<T>::is_signed;

    /// @brief Checked integer addition.
    /// @tparam T integral type
    /// @param lhs integer value
    /// @param rhs integer value
    /// @return `std::nullopt` if the operation overflows or underflows, `lhs + rhs` otherwise
    template<integral T>
    constexpr std::optional<T> check_add(T lhs, T rhs) noexcept {
        using L = std::numeric_limits<T>;

        if (!std::is_constant_evaluated()) {
#if VGI_HAS_BUILTIN(__builtin_add_overflow)
            T res;
            if (__builtin_add_overflow(lhs, rhs, &res)) [[unlikely]]
                return std::nullopt;
            return std::make_optional<T>(std::move(res));
#elif VGI_ARCH_FAMILY_X86
            if constexpr (unsigned_integral<T>) {
                if constexpr (L::digits == 64) {
                    unsigned __int64 res;
                    if (_addcarry_u64(0, static_cast<unsigned __int64>(lhs),
                                      static_cast<unsigned __int64>(rhs), &res)) [[unlikely]]
                        return std::nullopt;
                    return std::make_optional<T>(static_cast<T>(res));
                } else if constexpr (L::digits == 32) {
                    unsigned int res;
                    if (_addcarry_u32(0, static_cast<unsigned int>(lhs),
                                      static_cast<unsigned int>(rhs), &res)) [[unlikely]]
                        return std::nullopt;
                    return std::make_optional<T>(static_cast<T>(res));
                }
#ifdef _MSC_VER
                if constexpr (L::digits == 16) {
                    unsigned short res;
                    if (_addcarry_u16(0, static_cast<unsigned short>(lhs),
                                      static_cast<unsigned short>(rhs), &res)) [[unlikely]]
                        return std::nullopt;
                    return std::make_optional<T>(static_cast<T>(res));
                } else if constexpr (L::digits == 8) {
                    unsigned char res;
                    if (_addcarry_u8(0, static_cast<unsigned char>(lhs),
                                     static_cast<unsigned char>(rhs), &res)) [[unlikely]]
                        return std::nullopt;
                    return std::make_optional<T>(static_cast<T>(res));
                }
#endif
            }
#endif
        }

        if constexpr (unsigned_integral<T>) {
            if (lhs > L::max() - rhs) [[unlikely]]
                return std::nullopt;
        } else {
            if ((rhs > 0 && lhs > L::max() - rhs) || (rhs < 0 && lhs < L::min() - rhs)) [[unlikely]]
                return std::nullopt;
        }

        return std::make_optional<T>(lhs + rhs);
    }

    /// @brief Checked integer subtraction.
    /// @tparam T integral type
    /// @param lhs integer value
    /// @param rhs integer value
    /// @return `std::nullopt` if the operation overflows or underflows, `lhs - rhs` otherwise
    template<integral T>
    constexpr std::optional<T> check_sub(T lhs, T rhs) noexcept {
        using L = std::numeric_limits<T>;

        if (!std::is_constant_evaluated()) {
#if VGI_HAS_BUILTIN(__builtin_sub_overflow)
            T res;
            if (__builtin_sub_overflow(lhs, rhs, &res)) [[unlikely]]
                return std::nullopt;
            return std::make_optional<T>(std::move(res));
#elif VGI_ARCH_FAMILY_X86
            if constexpr (unsigned_integral<T>) {
                if constexpr (L::digits == 64) {
                    unsigned __int64 res;
                    if (_subborrow_u64(0, static_cast<unsigned __int64>(lhs),
                                       static_cast<unsigned __int64>(rhs), &res)) [[unlikely]]
                        return std::nullopt;
                    return std::make_optional<T>(static_cast<T>(res));
                } else if constexpr (L::digits == 32) {
                    unsigned int res;
                    if (_subborrow_u32(0, static_cast<unsigned int>(lhs),
                                       static_cast<unsigned int>(rhs), &res)) [[unlikely]]
                        return std::nullopt;
                    return std::make_optional<T>(static_cast<T>(res));
                }
#ifdef _MSC_VER
                if constexpr (L::digits == 16) {
                    unsigned short res;
                    if (_subborrow_u16(0, static_cast<unsigned short>(lhs),
                                       static_cast<unsigned short>(rhs), &res)) [[unlikely]]
                        return std::nullopt;
                    return std::make_optional<T>(static_cast<T>(res));
                } else if constexpr (L::digits == 8) {
                    unsigned char res;
                    if (_subborrow_u8(0, static_cast<unsigned char>(lhs),
                                      static_cast<unsigned char>(rhs), &res)) [[unlikely]]
                        return std::nullopt;
                    return std::make_optional<T>(static_cast<T>(res));
                }
#endif
            }
#endif
        }

        if constexpr (unsigned_integral<T>) {
            if (rhs > lhs) [[unlikely]]
                return std::nullopt;
        } else {
            if ((rhs > 0 && lhs < L::min() + rhs) || (rhs < 0 && lhs > L::max() + rhs)) [[unlikely]]
                return std::nullopt;
        }

        return std::make_optional<T>(lhs + rhs);
    }

    /// @brief Checked integer multiplication.
    /// @tparam T integral type
    /// @param lhs integer value
    /// @param rhs integer value
    /// @return `std::nullopt` if the operation overflows or underflows, `lhs * rhs` otherwise
    template<integral T>
    constexpr std::optional<T> check_mul(T lhs, T rhs) noexcept {
        using L = std::numeric_limits<T>;

        if (!std::is_constant_evaluated()) {
#if VGI_HAS_BUILTIN(__builtin_mul_overflow)
            T res;
            if (__builtin_mul_overflow(lhs, rhs, &res)) [[unlikely]]
                return std::nullopt;
            return std::make_optional<T>(std::move(res));
#endif
        }

        if constexpr (unsigned_integral<T>) {
            if (rhs != 0 && lhs > L::max() / rhs) [[unlikely]]
                return std::nullopt;
        } else {
            if (lhs == 0 || rhs == 0) return std::make_optional<T>(0);
            if (lhs > 0) {
                if (rhs > 0) {
                    if (lhs > L::max() / rhs) [[unlikely]]
                        return std::nullopt;
                } else { /* b < 0 */
                    if (rhs < L::min() / lhs) [[unlikely]]
                        return std::nullopt;
                }
            } else {  // a < 0
                if (rhs > 0) {
                    if (lhs < L::min() / rhs) [[unlikely]]
                        return std::nullopt;
                } else { /* b < 0 */
                    if (lhs < L::max() / rhs) [[unlikely]]
                        return std::nullopt;
                }
            }
        }

        return std::make_optional<T>(lhs * rhs);
    }

    /// @brief Checked integer division.
    /// @tparam T integral type
    /// @param lhs integer value
    /// @param rhs integer value
    /// @return `std::nullopt` if the operation overflows, underflows, or `rhs == 0`. `lhs / rhs`
    /// otherwise
    template<integral T>
    constexpr std::optional<T> check_div(T lhs, T rhs) noexcept {
        using L = std::numeric_limits<T>;

        if (rhs == 0) [[unlikely]]
            return std::nullopt;  // divide by zero

        if constexpr (signed_integral<T>) {
            // only overflow case for two's-complement: min / -1
            if (lhs == L::min() && rhs == static_cast<T>(-1)) [[unlikely]]
                return std::nullopt;
        }

        return std::make_optional<T>(lhs / rhs);  // always safe (b != 0)
    }

    /// @brief Checked integer remainder.
    /// @tparam T integral type
    /// @param lhs integer value
    /// @param rhs integer value
    /// @return `std::nullopt` if the operation overflows, underflows, or `rhs == 0`. `lhs % rhs`
    /// otherwise
    template<integral T>
    constexpr std::optional<T> check_rem(T lhs, T rhs) noexcept {
        using L = std::numeric_limits<T>;

        if (rhs == 0) [[unlikely]]
            return std::nullopt;  // divide by zero

        if constexpr (signed_integral<T>) {
            // only overflow case for two's-complement: min / -1
            if (lhs == L::min() && rhs == static_cast<T>(-1)) [[unlikely]]
                return std::nullopt;
        }

        return std::make_optional<T>(lhs % rhs);  // always safe (b != 0)
    }

    /// @brief Checked integer conversion.
    /// @tparam U destination integral type
    /// @tparam T source integral type
    /// @param lhs integer value
    /// @return `std::nullopt` if `lhs` doesn't fit inside the bounds of `U`, `static_cast<U>(lhs)`
    /// otherwise
    template<integral U, integral T>
    constexpr std::optional<U> check_cast(T lhs) noexcept {
        using LT = std::numeric_limits<T>;
        using LU = std::numeric_limits<U>;

        constexpr int digits_U = LU::digits;
        constexpr int digits_T = LT::digits;
        constexpr U max_res = LU::max();

        if constexpr (LU::is_signed == LT::is_signed) {
            if constexpr (digits_U < digits_T) {
                constexpr U min_res = LU::min();
                if (lhs < static_cast<T>(min_res)) [[unlikely]] {
                    return std::nullopt;
                } else if (lhs > static_cast<T>(max_res)) [[unlikely]] {
                    return std::nullopt;
                }
            }
        } else if constexpr (signed_integral<T>) {
            if (lhs < 0) [[unlikely]] {
                return std::nullopt;
            } else if (static_cast<std::make_unsigned_t<T>>(lhs) > max_res) [[unlikely]] {
                return std::nullopt;
            }
        } else {
            if (lhs > static_cast<std::make_unsigned_t<T>>(max_res)) [[unlikely]] {
                return std::nullopt;
            }
        }

        return std::make_optional<U>(static_cast<U>(lhs));
    }

    /// @brief Saturating integer addition.
    /// @tparam T integral type
    /// @param lhs integer value
    /// @param rhs integer value
    /// @return Saturated `lhs + rhs`
    template<integral T>
    constexpr T sat_add(T lhs, T rhs) noexcept {
        using L = std::numeric_limits<T>;

        std::optional<T> check = check_add<T>(lhs, rhs);
        if (check.has_value()) [[likely]] {
            return std::move(check.value());
        }

        if constexpr (unsigned_integral<T>) {
            return L::max();
        } else if (lhs < 0) {
            return L::min();
        } else {
            return L::max();
        }
    }

    /// @brief Saturating integer subtraction.
    /// @tparam T integral type
    /// @param lhs integer value
    /// @param rhs integer value
    /// @return Saturated `lhs - rhs`
    template<integral T>
    constexpr T sat_sub(T lhs, T rhs) noexcept {
        using L = std::numeric_limits<T>;

        std::optional<T> check = check_sub<T>(lhs, rhs);
        if (check.has_value()) [[likely]] {
            return std::move(check.value());
        }

        if constexpr (unsigned_integral<T>) {
            return L::min();
        } else if (lhs < 0) {
            return L::min();
        } else {
            return L::max();
        }
    }

    /// @brief Saturating integer multiplication.
    /// @tparam T integral type
    /// @param lhs integer value
    /// @param rhs integer value
    /// @return Saturated `lhs * rhs`
    template<integral T>
    constexpr T sat_mul(T lhs, T rhs) noexcept {
        using L = std::numeric_limits<T>;

        std::optional<T> check = check_mul<T>(lhs, rhs);
        if (check.has_value()) [[likely]] {
            return std::move(check.value());
        }

        if constexpr (unsigned_integral<T>) {
            return L::max();
        } else if ((lhs < 0) != (rhs < 0)) {
            return L::min();
        } else {
            return L::max();
        }
    }

    /// @brief Saturating integer division.
    /// @tparam T integral type
    /// @param lhs integer value
    /// @param rhs integer value
    /// @return Saturated `lhs / rhs`
    template<integral T>
    constexpr T sat_div(T lhs, T rhs) noexcept {
        using L = std::numeric_limits<T>;

        VGI_ASSERT(rhs != 0);
        if constexpr (signed_integral<T>) {
            if (lhs == L::min() && rhs == static_cast<T>(-1)) [[unlikely]] {
                return L::max();
            }
        }
        return lhs / rhs;
    }

    /// @brief Saturating integer conversion.
    /// @tparam U destination integral type
    /// @tparam T source integral type
    /// @param lhs integer value
    /// @return `static_cast<U>(lhs)` if `lhs` fits inside the bounds of `U`, or either the largest
    /// or smallest representable value of type `T`, whichever is closer to the value of `lhs`.
    template<integral U, integral T>
    constexpr U sat_cast(T lhs) noexcept {
        using LT = std::numeric_limits<T>;
        using LU = std::numeric_limits<U>;

        constexpr int digits_U = LU::digits;
        constexpr int digits_T = LT::digits;
        constexpr U max_res = LU::max();

        if constexpr (LU::is_signed == LT::is_signed) {
            if constexpr (digits_U < digits_T) {
                constexpr U min_res = LU::min();
                if (lhs < static_cast<T>(min_res)) [[unlikely]] {
                    return min_res;
                } else if (lhs > static_cast<T>(max_res)) [[unlikely]] {
                    return max_res;
                }
            }
        } else if constexpr (signed_integral<T>) {
            if (lhs < 0) [[unlikely]] {
                return 0;
            } else if (static_cast<std::make_unsigned_t<T>>(lhs) > max_res) [[unlikely]] {
                return max_res;
            }
        } else {
            if (lhs > static_cast<std::make_unsigned_t<T>>(max_res)) [[unlikely]] {
                return max_res;
            }
        }

        return static_cast<U>(lhs);
    }

    /// @brief Calculates the distance to the next multiple of `rhs`
    /// @tparam T Integer type
    /// @param lhs Integer value
    /// @param rhs Integer value
    /// @return Distance to the next multiple of `rhs`
    template<unsigned_integral T>
    constexpr std::optional<T> offset_to_next_multiple_of(T lhs, T rhs) noexcept {
        std::optional<T> r = check_rem<T>(lhs, rhs);
        if (!r) return std::nullopt;
        return std::make_optional<T>(r.value() == 0 ? 0 : (rhs - r.value()));
    }

    /// @brief Calculates the next multiple of `rhs`
    /// @tparam T Integer type
    /// @param lhs Integer value
    /// @param rhs Integer value
    /// @return From `lhs`, the next multiple of `rhs`
    template<unsigned_integral T>
    constexpr std::optional<T> next_multiple_of(T lhs, T rhs) noexcept {
        std::optional<T> r = check_rem<T>(lhs, rhs);
        if (!r) return std::nullopt;
        if (r.value() == 0) return std::make_optional<T>(lhs);
        return check_add<T>(lhs, rhs - r.value());
    }

    // For animations, glm's slerp is not precise enough, so this is ported from the implementation
    // of three.js
    // (https://github.com/mrdoob/three.js/blob/4c1941f94391acab665ed73e3c81e80b1e6e3975/src/math/Quaternion.js#L517)
    template<std::floating_point T, glm::qualifier Q = glm::qualifier::defaultp>
    inline glm::qua<T, Q> slerp(glm::qua<T, Q> from, glm::qua<T, Q> to, T t) {
        if (t <= T(0.0)) return from;
        if (t >= T(1.0)) return to;

        T cos_half_theta = glm::dot(from, to);
        if (cos_half_theta < T(0.0)) {
            to = -to;
            cos_half_theta = -cos_half_theta;
        }
        if (cos_half_theta >= T(1.0)) return from;

        T sqr_sin_half_theta = T(1.0) - cos_half_theta * cos_half_theta;
        if (sqr_sin_half_theta <= std::numeric_limits<T>::epsilon()) {
            T s = T(1.0) - t;
            return glm::normalize(from * s + to * t);
        }

        T sin_half_theta = std::sqrt(sqr_sin_half_theta);
        T half_theta = std::atan2(sin_half_theta, cos_half_theta);
        T ratio_a = std::sin((T(1.0) - t) * half_theta) / sin_half_theta;
        T ratio_b = std::sin(t * half_theta) / sin_half_theta;
        return from * ratio_a + to * ratio_b;
    }

    namespace chrono {
        template<class T>
        struct is_duration : public std::bool_constant<false> {};
        template<class Rep, class Period>
        struct is_duration<std::chrono::duration<Rep, Period>> : public std::bool_constant<true> {};
        template<class T>
        constexpr static inline const bool is_duration_v = is_duration<T>::value;

        /// @brief Converts a `std::chrono::duration` to a duration of different type `ToDuration`.
        /// @tparam ToDuration The target duration representation
        /// @tparam Period An `std::ratio` representing the tick period (i.e. the number of second's
        /// fractions per tick)
        /// @tparam Rep Arithmetic type representing the number of ticks.
        /// @param d Duration to convert
        /// @return `std::nullopt` if `d` is outside the bounds of `ToDuration`, `d` converted to a
        /// duration of type `ToDuration` otherwise.
        template<class ToDuration, integral Rep, class Period>
            requires(is_duration_v<ToDuration> && integral<typename ToDuration::rep>)
        constexpr std::optional<ToDuration> check_duration_cast(
                const std::chrono::duration<Rep, Period>& d) noexcept {
            using ToRep = typename ToDuration::rep;
            using ToPeriod = typename ToDuration::period;
            using CF = std::ratio_divide<Period, ToPeriod>;
            using CR = std::common_type_t<Rep, ToRep, std::intmax_t>;

            if constexpr (CF::num == 1 && CF::den == 1) {
                auto count = check_cast<ToRep>(d.count());
                if (!count.has_value()) return std::nullopt;
                return std::make_optional<ToDuration>(count.value());
            }

            const std::optional<CR> raw_cr_count = check_cast<CR>(d.count());
            if (!raw_cr_count.has_value()) return std::nullopt;
            const CR cr_count = raw_cr_count.value();

            constexpr CR cr_num = static_cast<CR>(CF::num);
            constexpr CR cr_den = static_cast<CR>(CF::den);

            if constexpr (CF::num == 1) {
                std::optional<CR> res = check_div<CR>(cr_count, cr_den);
                if (!res.has_value()) return std::nullopt;
                std::optional<ToRep> count = check_cast<ToRep>(res.value());
                if (!count.has_value()) return std::nullopt;
                return std::make_optional<ToDuration>(count.value());
            } else if constexpr (CF::den == 1) {
                std::optional<CR> res = check_mul<CR>(cr_count, cr_num);
                if (!res.has_value()) return std::nullopt;
                std::optional<ToRep> count = check_cast<ToRep>(res.value());
                if (!count.has_value()) return std::nullopt;
                return std::make_optional<ToDuration>(count.value());
            } else {
                std::optional<CR> div = check_div<CR>(cr_count, cr_den);
                if (!div.has_value()) return std::nullopt;
                std::optional<CR> whole = check_mul<CR>(div.value(), cr_num);
                if (!whole.has_value()) return std::nullopt;
                std::optional<CR> part = check_mul<CR>(cr_count % cr_den, cr_num);
                if (!part.has_value()) return std::nullopt;
                part = check_div<CR>(part.value(), cr_den);
                if (!part.has_value()) return std::nullopt;
                std::optional<CR> res = check_add<CR>(whole.value(), part.value());
                if (!res.has_value()) return std::nullopt;
                std::optional<ToRep> count = check_cast<ToRep>(res.value());
                if (!count.has_value()) return std::nullopt;
                return std::make_optional<ToDuration>(count.value());
            }
        }

        /// @brief Converts a `std::chrono::duration` to a duration of different type `ToDuration`.
        /// @tparam ToDuration The target duration representation
        /// @tparam Period An `std::ratio` representing the tick period (i.e. the number of second's
        /// fractions per tick)
        /// @tparam Rep Arithmetic type representing the number of ticks.
        /// @param d Duration to convert
        /// @return `d` converted to a duration of type `ToDuration` if it's inside the bounds of
        /// `ToDuration`, or either the largest
        /// or smallest representable value of type `ToDuration`, whichever is closer to the value
        /// of `d`.
        template<class ToDuration, integral Rep, class Period>
            requires(is_duration_v<ToDuration> && integral<typename ToDuration::rep>)
        constexpr ToDuration sat_duration_cast(
                const std::chrono::duration<Rep, Period>& d) noexcept {
            using ToRep = typename ToDuration::rep;
            using ToPeriod = typename ToDuration::period;
            using CF = std::ratio_divide<Period, ToPeriod>;
            using CR = std::common_type_t<Rep, ToRep, std::intmax_t>;

            if constexpr (CF::num == 1 && CF::den == 1) {
                return ToDuration(sat_cast<ToRep>(d.count()));
            }

            const CR cr_count = sat_cast<CR>(d.count());
            constexpr CR cr_num = static_cast<CR>(CF::num);
            constexpr CR cr_den = static_cast<CR>(CF::den);

            if constexpr (CF::num == 1) {
                static_assert(CF::den != 0);
                return ToDuration(sat_cast<ToRep>(sat_div<CR>(cr_count, cr_den)));
            } else if constexpr (CF::den == 1) {
                return ToDuration(sat_cast<ToRep>(sat_mul<CR>(cr_count, cr_num)));
            } else if constexpr (CF::den == -1) {
                static_assert(false, "Not yet implemented");
            } else {
                static_assert(CF::den != 0);
                CR div = cr_count / cr_den;
                CR whole = sat_mul<CR>(div, cr_num);
                CR part = sat_mul<CR>(cr_count % cr_den, cr_num);
                part = sat_div<CR>(part, cr_den);
                return ToDuration(sat_cast<ToRep>(sat_add<CR>(whole, part)));
            }
        }
    }  // namespace chrono

}  // namespace vgi::math
