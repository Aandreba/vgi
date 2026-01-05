#include "event.hpp"

#include <concepts>
#include <optional>
#include <typeinfo>
#include <variant>

#include "collections/slab.hpp"
#include "math.hpp"

namespace vgi {
    constinit static inline std::optional<Uint32> custom_type = std::nullopt;
    Uint32 custom_event_type() noexcept {
        std::optional<Uint32>& type = custom_type;
        if (!type) [[unlikely]]
            type.emplace(SDL_RegisterEvents(1));
        return *type;
    }

    constinit static inline collections::slab<void> event;
}  // namespace vgi
