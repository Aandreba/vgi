/*! \file */
#pragma once

#include <concepts>
#include <glm/glm.hpp>
#include <type_traits>
#include <vgi/math.hpp>
#include <vgi/resource.hpp>
#include <vgi/vulkan.hpp>
#include <vgi/window.hpp>

namespace vgi {
    /// @details Vulkan uniform buffers follow the [`std140` memory
    /// layout](https://ptgmedia.pearsoncmg.com/images/9780321552624/downloads/0321552628_AppL.pdf),
    /// which has some weird alignment rules. To help with programebility, this struct can be used
    /// to automatically align the fields of a struct to the alignment and size that would be used
    /// with the std140 layout.
    template<class T>
        requires(std::is_trivially_copyable_v<T>)
    struct std140 {
        template<class... Args>
            requires(std::is_constructible_v<T, Args...>)
        std140(Args&&... args) noexcept(std::is_nothrow_constructible_v<T, Args...>);

        constexpr operator T() const noexcept;
    };

    template<class T>
        requires(std::numeric_limits<T>::is_specialized && std::is_trivially_copyable_v<T>)
    struct std140<T> {
        template<class... Args>
            requires(std::is_constructible_v<bool, Args...>)
        constexpr std140(Args&&... args) noexcept(std::is_nothrow_constructible_v<bool, Args...>) :
            bytes(std::bit_cast<std::array<std::byte, sizeof(T)>>(T{std::forward<Args>(args)...})) {
        }

        constexpr operator T() const noexcept { return std::bit_cast<T>(this->bytes); }

    private:
        std::array<std::byte, sizeof(T)> bytes;
    };
}  // namespace vgi
