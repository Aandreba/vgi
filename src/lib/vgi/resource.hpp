#pragma once

#include <concepts>
#include <optional>
#include <type_traits>
#include <variant>

#ifndef NDEBUG
#include <source_location>

#include "collections/slab.hpp"
#endif

#include "defs.hpp"
#include "forward.hpp"
#include "vgi.hpp"

namespace vgi {
    /// @brief Specifies that a type is valid as a resource
    template<class T>
    concept resource = requires(T&& self, const window& window) {
        { std::move(self).destroy(window) } -> std::same_as<void>;
    };

    /// @brief A guard that destroys resources when dropped.
    template<resource T>
    struct resource_guard final : public T {
        /// @brief Creates a resource_guard in place
        /// @tparam ...Args Argument types
        /// @param window Window used to construct the resource
        /// @param ...args Arguments used to construct the resource
        template<class... Args>
            requires(std::is_constructible_v<T, const window&, Args...>)
        inline explicit resource_guard(const window& window, Args&&... args) noexcept(
                std::is_nothrow_constructible_v<T, const struct window&, Args...>) :
            T(window, std::forward<Args>(args)...), window(window) {}

        resource_guard(const resource_guard&) = delete;
        resource_guard& operator=(const resource_guard&) = delete;

        /// @brief Move constructor operator
        /// @param other Value to be moved
        resource_guard(resource_guard&& other) noexcept(std::is_nothrow_move_constructible_v<T>)
            requires(std::is_move_constructible_v<T>)
            : T(std::move(static_cast<T&&>(other))), window(other.window) {}

        /// @brief Move assignment operator
        /// @param other Value to be moved
        resource_guard& operator=(resource_guard&& other) noexcept
            requires(std::is_nothrow_move_assignable_v<T>)
        {
            if (this == std::addressof(other)) [[unlikely]]
                return *this;
            std::destroy_at(this);
            std::construct_at(this, std::move(other));
            return *this;
        }

        // clang-format off
        
        /// @brief Releases ownership of the resource
        /// @return The resource previously owned by the guard
        inline T release() && requires(std::is_move_constructible_v<T>) {
            return T{std::move(static_cast<T&>(*this))};
        }

        inline ~resource_guard() noexcept(std::is_nothrow_destructible_v<T>) {
            std::move(static_cast<T&>(*this)).destroy(this->window);
        }
        // clang-format on

    private:
        const struct window& window;
    };
}  // namespace vgi
