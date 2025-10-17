#pragma once

#include <concepts>
#include <optional>
#include <type_traits>
#include <variant>

#include "defs.hpp"
#include "forward.hpp"

namespace vgi {
    /// @brief Specifies that a type is valid as a resource
    template<class T>
    concept resource = requires(T&& self, const window& window) {
        { std::move(self).destroy(window) } -> std::same_as<void>;
    };

    /// @brief Specifies that a type is valid as a resource, but isn't derived from
    /// `vgi::shared_resource`
    template<class T>
    concept local_resource = !std::derived_from<T, shared_resource> && resource<T>;

    /// @brief A resource whose lifetime is handled by a parent `vgi::window`.
    struct shared_resource {
        shared_resource(const shared_resource&) = delete;
        shared_resource& operator=(const shared_resource&) = delete;
        virtual ~shared_resource() = default;

    protected:
        shared_resource() = default;

        /// @brief Destroys the resource
        /// @param parent Window used to create the resource
        virtual void destroy(const window& parent) && = 0;

    private:
        using state_type = std::variant<bool, size_t, std::monostate>;
        constexpr static inline const size_t ALIVE = 0;
        constexpr static inline const size_t WAITING = 1;
        constexpr static inline const size_t RELEASED = 2;

        state_type state;
        std::array<vk::Fence, VGI_MAX_FRAMES_IN_FLIGHT> fence_buffer;

        template<std::derived_from<shared_resource> R>
        friend struct res_lock;
        template<std::derived_from<shared_resource> R>
        friend struct res;
    };

    /// @brief A strong reference to a `vgi::shared_resource`
    template<std::derived_from<shared_resource> R>
    struct [[nodiscard]] res_lock {
        res_lock(const res_lock&) = delete;
        res_lock& operator=(const res_lock&) = delete;

        /// @brief Checks wether the resource locking was successful
        inline explicit operator bool() const noexcept { return this->ptr; }
        /// @brief Provides access to the acquired resource
        /// @return A pointer to the resource
        inline R* operator->() const noexcept { return this->ptr; }
        /// @brief Provides access to the acquired resource
        /// @return A pointer to the resource
        inline R& operator*() const noexcept { return *this->ptr; }

        ~res_lock() noexcept { VGI_ASSERT(--static_cast<shared_resource&>(ptr).strong > 0); }

    private:
        using value_type = std::remove_cv_t<R>;
        value_type* ptr;

        res_lock(value_type* ptr) noexcept : ptr(ptr) {}

        friend struct res<R>;
    };

    /// @brief A weak reference to a `vgi::shared_resource`
    template<std::derived_from<shared_resource> R>
    struct res {
        /// @brief Copy constructor
        /// @param other Object to be copied
        res(const res& other) noexcept : ptr(other.ptr) {
            if (ptr) static_cast<shared_resource*>(ptr)->weak++;
        }

        /// @brief Move constructor
        /// @param other Object to be moved
        res(res&& other) noexcept : ptr(std::exchange(other.ptr, nullptr)) {}

        /// @brief Copy assignment
        /// @param other Object to be copied
        res& operator=(const res& other) noexcept {
            if (this == &other) return *this;
            std::destroy_at(this);
            std::construct_at(this, other);
            return *this;
        }

        /// @brief Move assignment
        /// @param other Object to be moved
        res& operator=(res&& other) noexcept {
            if (this == &other) return *this;
            std::destroy_at(this);
            std::construct_at(this, std::move(other));
            return *this;
        }

        /// @brief Checks wether this value points to a shared resource, whether destroyed or not.
        inline explicit operator bool() const noexcept { return this->ptr; }

        /// @brief Attempts to lock the resource, so that it can be used safely for the remainder of
        /// the frame.
        /// @return The result of the attempted lock
        res_lock<R> lock() noexcept {
            if (!this->ptr) return nullptr;

            shared_resource* res_ptr = static_cast<shared_resource*>(this->ptr);
            if (bool* locked = std::get_if<shared_resource::ALIVE>(&res_ptr->state)) {
                *locked = true;
                return this->ptr;
            } else {
                return nullptr;
            }
        }

        ~res() noexcept(std::is_nothrow_destructible_v<R>) {
            if (this->ptr) {
                if (--static_cast<shared_resource*>(this->ptr)->weak == 0) {
                    delete this->ptr;
                }
            }
        }

    private:
        using value_type = std::remove_cv_t<R>;
        value_type* ptr;
    };

    /// @brief A guard that destroys resources when dropped.
    template<local_resource T>
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
        const window& window;
    };
}  // namespace vgi
