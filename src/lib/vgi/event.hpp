#pragma once

#include <SDL3/SDL.h>
#include <memory>
#include <type_traits>

#include "vgi.hpp"

namespace vgi {
    class event {
        void* ptr;
        void (*destroy)(void*);
        const std::type_info& info;

        template<class T>
        static void destroy_impl(void* ptr) {
            delete reinterpret_cast<T*>(ptr);
        }

    public:
        template<class T>
        constexpr static inline const bool use_small_object_opt =
                std::is_trivially_destructible_v<T> && std::is_trivially_copyable_v<T> &&
                (sizeof(T) <= sizeof(void*)) && (alignof(T) <= alignof(void*));

        template<class T, class... Args>
            requires(std::is_constructible_v<T, Args...>)
        event(std::in_place_type_t<T>, Args&&... args) :
            ptr(const_cast<std::remove_cv_t<T>*>(new T(std::forward<Args>(args)...))),
            destroy(&destroy_impl<T>), info(typeid(T)) {}

        event(event&& other) noexcept :
            ptr(std::exchange(other.ptr, nullptr)), destroy(std::exchange(other.destroy, nullptr)),
            info(other.info) {}

        event& operator=(event&& other) noexcept {
            if (this == std::addressof(other)) return *this;
            std::destroy_at(this);
            std::construct_at(this, std::move(other));
            return *this;
        }

        event(const event&) = delete;
        event& operator=(const event&) = delete;

        ~event() {
            if (this->destroy) this->destroy(this->ptr);
        }
    };

    Uint32 custom_event_type() noexcept;
    void push_event(event&& event);

    template<class T, class... Args>
        requires(std::is_constructible_v<T, Args...>)
    inline void push_event(Args&&... args) {
        if constexpr (event::use_small_object_opt<T>) {
            SDL_Event user_event;
            SDL_zero(user_event);
            user_event.type = custom_event_type();
            user_event.user.code = 0;
            new (&user_event.user.data1) T(std::forward<Args>(args)...);
            user_event.user.data2 = std::addressof(typeid(T));
            if (!SDL_PushEvent(&user_event)) throw sdl_error{};
        } else {
            push_event(event{std::in_place_type<T>, std::forward<Args>(args)...});
        }
    }
}  // namespace vgi
