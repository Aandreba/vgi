#include "tray.hpp"

namespace vgi {
    tray::tray(const char8_t* tooltip) :
        handle(sdl::tri(SDL_CreateTray(nullptr, reinterpret_cast<const char*>(tooltip)))),
        menu(sdl::tri(SDL_CreateTrayMenu(this->handle))) {
        SDL_Surface* surface = sdl::tri(SDL_CreateSurface(22, 22, SDL_PIXELFORMAT_RGBA32));
        SDL_Rect rect{0, 0, 22, 22};
        SDL_FillSurfaceRect(surface, &rect, 0XFFFFFFFF);
        SDL_SetTrayIcon(this->handle, surface);
        SDL_DestroySurface(surface);
    }

    tray::~tray() noexcept {
        if (this->handle) SDL_DestroyTray(this->handle);
    }
}  // namespace vgi
