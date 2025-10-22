#pragma once

#include <SDL3/SDL.h>
#include <span>

#include "defs.hpp"

namespace vgi {
    /// @brief A list with the current state for each key of the keyboard
    extern std::span<const bool, SDL_SCANCODE_COUNT> keyboard_state;

    /// @brief Checks wether a key is currently pressed
    /// @param code The scancode of the key
    /// @return `true` if the key is down, `false` otherwise
    VGI_FORCEINLINE bool is_key_down(SDL_Scancode code) noexcept { return keyboard_state[code]; }

    /// @brief Checks wether a key is currently released
    /// @param code The scancode of the key
    /// @return `true` if the key is up, `false` otherwise
    VGI_FORCEINLINE bool is_key_up(SDL_Scancode code) noexcept { return !keyboard_state[code]; }
}  // namespace vgi
