#pragma once

#include <SDL3/SDL.h>
#include <memory>
#include <type_traits>

#include "vgi.hpp"

namespace vgi {
    Uint32 custom_event_type() noexcept;

    template<class T>
    class event_traits {
        constexpr static inline const bool use_small_unique =
                std::is_trivially_destructible_v<T> && std::is_trivially_copyable_v<T> &&
                (sizeof(T) <= sizeof(void*)) && (alignof(T) <= alignof(void*));
    };

    class event_info {
        template<class T>
        static void destroy(void*& ptr) {
            if constexpr (event_traits<T>::use_small_unique) {
                std::destroy_at<T>(reinterpret_cast<T*>(&ptr));
            } else {
                delete reinterpret_cast<T*>(ptr);
            }
        }

        constinit static const event_info info{
                .destructor = &event_info::destroy<T>,
                .type_info = typeid(T),
        };

    public:
        void (*destructor)(void*&);
        const std::type_info& type_info;

        template<class T>
        constexpr const event_info& get() {
            return info;
        }
    };

    template<class T, class... Args>
        requires(std::is_constructible_v<std::remove_cv_t<T>, Args...>)
    static void push_event(Args&&... args) {
        using value_type = std::remove_cv_t<T>;

        SDL_Event user_event;
        SDL_zero(user_event);
        user_event.type = custom_event_type();
        user_event.user.code = 0;
        user_event.user.data2 =
                static_cast<void*>(const_cast<event_info*>(std::addressof(event_info::get<T>())));

        if constexpr (event_traits<T>::use_small_unique) {
            new (&user_event.user.data1) value_type(std::forward<Args>(args)...);
            if (!SDL_PushEvent(&user_event)) throw sdl_error{};
        } else {
            auto ptr = std::make_unique<value_type>(std::forward<Args>(args)...);
            user_event.user.data1 = static_cast<void*>(ptr.get());
            if (!SDL_PushEvent(&user_event)) throw sdl_error{};
            ptr.release();
        }
    }

    template<class T>
    static T* dyn_cast_event(const SDL_Event& event) {
        using value_type = std::remove_cv_t<T>;

        if (event.type != custom_event_type()) return nullptr;
        if (event.user.code != 0) return nullptr;

        const event_info& info = *static_cast<event_info*>(event.user.data2);
        if (info.type_info != typeid(T)) return nullptr;

        if (event.user.code == 0) {
            if constexpr (event_traits<T>::use_small_unique) {
                return reinterpret_cast<T*>(&event.user.data1);
            } else {
                return reinterpret_cast<T*>(event.user.data1);
            }
        } else {
            VGI_UNREACHABLE;
        }
    }

}  // namespace vgi
