/*! \file */
#pragma once

#include <concepts>
#include <glm/glm.hpp>
#include <type_traits>
#include <vgi/math.hpp>
#include <vgi/resource.hpp>
#include <vgi/vulkan.hpp>
#include <vgi/window.hpp>

//! @cond Doxygen_Suppress
namespace vgi {
    template<class T>
    concept std140_scalar = same_as_any<T, bool, int32_t, uint32_t, float>;

    /// @details Vulkan uniform buffers follow the [`std140` memory
    /// layout](https://ptgmedia.pearsoncmg.com/images/9780321552624/downloads/0321552628_AppL.pdf),
    /// which has some weird alignment rules. To help with programebility, this struct can be used
    /// to automatically align the fields of a struct to the alignment and size that would be used
    /// with the std140 layout.
    template<class T>
        requires(std::is_trivially_copyable_v<T>)
    struct std140 {
        constexpr std140(const T&) noexcept;
        constexpr operator T() const noexcept;
    };

    template<std140_scalar T>
    class std140<T> {
        using value_type = T;
        using bytes_type = std::array<std::byte, sizeof(T)>;
        bytes_type bytes;

    public:
        constexpr std140(const value_type& value) noexcept :
            bytes(std::bit_cast<bytes_type>(value)) {}

        constexpr operator value_type() const noexcept {
            return std::bit_cast<value_type>(this->bytes);
        }
    };

    template<std140_scalar T>
    class std140<glm::vec<2, T, glm::qualifier::highp>> {
        using value_type = glm::vec<2, T, glm::qualifier::highp>;
        using bytes_type = std::array<std::byte, 2 * sizeof(T)>;
        bytes_type bytes;

    public:
        constexpr std140(const value_type& value) noexcept :
            bytes(std::bit_cast<bytes_type>(value)) {}

        constexpr operator value_type() const noexcept {
            return std::bit_cast<value_type>(this->bytes);
        }
    };

    template<std140_scalar T>
    class std140<glm::vec<3, T, glm::qualifier::highp>> {
        using value_type = glm::vec<3, T, glm::qualifier::highp>;
        using bytes_type = std::array<std::byte, 3 * sizeof(T)>;
        bytes_type bytes;
        std::byte padding[sizeof(T)];

    public:
        constexpr std140(const value_type& value) noexcept :
            bytes(std::bit_cast<bytes_type>(value)) {}

        constexpr operator value_type() const noexcept {
            return std::bit_cast<value_type>(this->bytes);
        }
    };

    template<std140_scalar T>
    class std140<glm::vec<4, T, glm::qualifier::highp>> {
        using value_type = glm::vec<4, T, glm::qualifier::highp>;
        using bytes_type = std::array<std::byte, 4 * sizeof(T)>;
        bytes_type bytes;

    public:
        constexpr std140(const value_type& value) noexcept :
            bytes(std::bit_cast<bytes_type>(value)) {}

        constexpr operator value_type() const noexcept {
            return std::bit_cast<value_type>(this->bytes);
        }
    };

    template<std140_scalar T, size_t N>
    class std140<T[N]> {
        using value_type = std::array<std140<T>, N>;
        constexpr static inline const size_t padding_size =
                math::offset_to_next_multiple_of(sizeof(value_type), sizeof(std140<glm::vec4>))
                        .value();

        value_type data;
        std::byte padding[padding_size];

    public:
        constexpr std140(const value_type& value) noexcept :
            data(std::bit_cast<value_type>(value)) {}

        constexpr operator value_type() const noexcept {
            return std::bit_cast<value_type>(this->data);
        }
    };

    template<std140_scalar T, size_t M, size_t N>
    class std140<glm::vec<M, T, glm::qualifier::highp>[N]> {
        using value_type = std::array<glm::vec<M, T, glm::qualifier::highp>, N>;
        using storage_type = std::array<std140<glm::vec<M, T, glm::qualifier::highp>>, N>;
        constexpr static inline const size_t padding_size =
                math::offset_to_next_multiple_of(sizeof(storage_type), sizeof(std140<glm::vec4>))
                        .value();

        storage_type data;
        std::byte padding[padding_size];

    public:
        constexpr std140(const value_type& value) noexcept {
            for (size_t i = 0; i < N; ++i) {
                this->data[i] = value[i];
            }
        }

        constexpr operator value_type() const noexcept {
            value_type result;
            for (size_t i = 0; i < N; ++i) {
                result[i] = this->data[i];
            }
            return result;
        }
    };

    template<std140_scalar T, size_t C, size_t R>
    class std140<glm::mat<C, R, T, glm::qualifier::highp>> {
        using value_type = glm::mat<C, R, T, glm::qualifier::highp>;
        using storage_type = std140<glm::vec<R, T, glm::qualifier::highp>[C]>;
        storage_type data;

    public:
        constexpr std140(const value_type& value) noexcept {
            for (size_t i = 0; i < C; ++i) {
                this->data[i] = value[i];
            }
        }

        constexpr operator value_type() const noexcept {
            value_type value;
            for (size_t i = 0; i < C; ++i) {
                value[i] = this->data[i];
            }
            return value;
        }
    };

    template<std140_scalar T, size_t C, size_t R, size_t N>
    class std140<glm::mat<C, R, T, glm::qualifier::highp>[N]> {
        using value_type = std::array<glm::mat<C, R, T, glm::qualifier::highp>, N>;
        using storage_type = std140<glm::vec<R, T, glm::qualifier::highp>[N * C]>;
        storage_type data;

    public:
        constexpr std140(const value_type& value) noexcept {
            for (size_t i = 0; i < N; ++i) {
                for (size_t j = 0; j < C; ++j) {
                    this->data[i * C + j] = value[i][j];
                }
            }
        }

        constexpr operator value_type() const noexcept {
            value_type value;
            for (size_t i = 0; i < N; ++i) {
                for (size_t j = 0; j < C; ++j) {
                    value[i][j] = this->data[i * C + j];
                }
            }
            return value;
        }
    };

    struct test_uniform {
        std140<glm::mat4> mat;
        std140<glm::mat3x2> mat2;
        std140<glm::mat4[3]> a;
        std140<glm::mat4[4]> b;
        std140<glm::mat4[5]> c;
    };
}  // namespace vgi
//! @endcond
