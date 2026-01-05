#include "event.hpp"

#include <concepts>
#include <optional>
#include <typeinfo>
#include <variant>

#include "collections/slab.hpp"
#include "math.hpp"

namespace vgi {
    constinit collections::slab<event> events;
    constinit static inline std::optional<Uint32> custom_type = std::nullopt;

    Uint32 custom_event_type() noexcept {
        std::optional<Uint32>& type = custom_type;
        if (!type) [[unlikely]]
            type.emplace(SDL_RegisterEvents(1));
        return *type;
    }

    void push_event(event&& event) {
        SDL_Event user_event;
        SDL_zero(user_event);
        user_event.type = custom_event_type();
        user_event.user.code = 0;
        user_event.user.data1 = reinterpret_cast<void*>(events.emplace(std::move(event)));
        user_event.user.data2 = nullptr;
        if (!SDL_PushEvent(&user_event)) throw sdl_error{};
    }
}  // namespace vgi
