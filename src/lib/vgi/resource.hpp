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
        shared_resource() : state(std::in_place_index<ALIVE>, 1) {}

        /// @brief Destroys the resource
        /// @param parent Window used to create the resource
        virtual void destroy(const window& parent) && = 0;

    private:
        struct alive {
            size_t refs = 1;
#ifndef NDEBUG
            collections::slab<std::source_location> locks;
#endif
        };

        using state_type = std::variant<alive, uint32_t, std::monostate>;
        constexpr static inline const size_t ALIVE = 0;
        constexpr static inline const size_t WAITING = 1;
        constexpr static inline const size_t RELEASED = 2;

        state_type state;

        friend struct window;
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

        /// @brief Provides access to the acquired resource
        /// @return A pointer to the resource
        inline R* operator->() const noexcept { return std::addressof(this->ptr); }
        /// @brief Provides access to the acquired resource
        /// @return A pointer to the resource
        inline R& operator*() const noexcept { return this->ptr; }

        ~res_lock() noexcept {
#ifndef NDEBUG
            shared_resource::alive* alive =
                    std::get_if(&static_cast<shared_resource&>(this->ptr).state);
            VGI_ASSERT(alive != nullptr);
            alive->locks.remove(this->location);
#endif
        }

    private:
        using value_type = std::remove_cv_t<R>;
        value_type& ptr;

#ifndef NDEBUG
        size_t location;
        res_lock(value_type* ptr, size_t location) noexcept : ptr(ptr), location(location) {}
        res_lock(std::nullptr_t) noexcept : ptr(nullptr) {}
#else
        res_lock(value_type* ptr) noexcept : ptr(ptr) {}
#endif

        friend struct res<R>;
    };

    /// @brief A weak reference to a `vgi::shared_resource`
    template<std::derived_from<shared_resource> R>
    struct res {
        /// @brief Copy constructor
        /// @param other Object to be copied
        res(const res& other) noexcept : ptr(other.ptr) {
            if (ptr) {
                if (shared_resource::alive* alive = std::get_if<shared_resource::ALIVE>(
                            &static_cast<shared_resource*>(ptr)->state)) {
                    alive->refs++;
                }
            }
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

        /// @brief Checks wether this value points to a shared resource, whether destroyed or
        /// not.
        inline explicit operator bool() const noexcept { return this->ptr; }

/// @brief Attempts to lock the resource, so that it can be used safely for the
/// remainder of the frame.
/// @return The result of the attempted lock
#ifndef NDEBUG
        res_lock<R> lock(std::source_location&& location = std::source_location::current()) {
#else
        res_lock<R> lock() {
#endif
            if (!this->ptr) throw std::runtime_error{"The resource has already been released"};

            shared_resource* res_ptr = static_cast<shared_resource*>(this->ptr);
            if (shared_resource::alive* alive =
                        std::get_if<shared_resource::ALIVE>(&res_ptr->state)) {
#ifndef NDEBUG
                return res_lock<R>{this->ptr, alive->locks.emplace(std::move(location))};
#else
                return this->ptr;
#endif
            } else {
                throw std::runtime_error{"The resource has already been released"};
            }
        }

        ~res() noexcept {
            if (this->ptr) {
                if (shared_resource::alive* alive = std::get_if<shared_resource::ALIVE>(
                            &static_cast<shared_resource*>(this->ptr)->state)) {
                    VGI_ASSERT(alive->refs > 0);
                    alive->refs--;
                }
            }
        }

    private:
        using value_type = std::remove_cv_t<R>;
        value_type* ptr;

        res(value_type* ptr) noexcept : ptr(ptr) {}
        friend struct window;
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
