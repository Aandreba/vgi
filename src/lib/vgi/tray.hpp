#pragma once

#include <SDL3/SDL.h>
#include <concepts>
#include <functional>
#include <vector>

#include "defs.hpp"
#include "log.hpp"
#include "vgi.hpp"

namespace vgi {
    class tray {
        struct tray_entry {
            SDL_TrayEntry* VGI_RESTRICT handle;
            void* VGI_RESTRICT cb_userdata;
            void (*cb_destructor)(void*);

            template<class F>
                requires(std::is_move_constructible_v<F>)
            tray_entry(SDL_TrayMenu* menu, const char8_t* label, SDL_TrayEntryFlags flags, F&& f) :
                handle(sdl::tri(SDL_InsertTrayEntryAt(
                        menu, -1, reinterpret_cast<const char*>(label), flags))),
                cb_userdata(new F(std::move(f))), cb_destructor(tray_destructor<F>) {}

            ~tray_entry() {
                SDL_RemoveTrayEntry(this->handle);
                if (this->cb_destructor) (this->cb_destructor)(this->cb_userdata);
            }
        };

        SDL_Tray* VGI_RESTRICT handle;
        SDL_TrayMenu* VGI_RESTRICT menu;
        std::vector<tray_entry> entries;

    public:
        tray(const char8_t* tooltip = nullptr);
        ~tray() noexcept;

        tray(const tray&) = delete;
        tray& operator=(const tray&) = delete;

        template<class F>
            requires(std::is_invocable_r_v<void, F&> && std::is_move_constructible_v<F>)
        void button(const char8_t* label, F&& on_click, bool disabled = false) {
            tray_entry& entry = this->entries.emplace_back(
                    this->menu, label,
                    SDL_TRAYENTRY_BUTTON | (disabled ? SDL_TRAYENTRY_DISABLED : 0),
                    std::move(on_click));
            SDL_SetTrayEntryCallback(entry.handle, button_callback<F>, entry.cb_userdata);
        }

        template<class F>
            requires(std::is_invocable_r_v<void, F&, bool> && std::is_move_constructible_v<F>)
        void checkbox(const char8_t* label, F&& on_change, bool checked = false,
                      bool disabled = false) {
            tray_entry& entry = this->entries.emplace_back(
                    this->menu, label,
                    SDL_TRAYENTRY_CHECKBOX | (checked ? SDL_TRAYENTRY_CHECKED : 0) |
                            (disabled ? SDL_TRAYENTRY_DISABLED : 0),
                    std::move(on_change));
            SDL_SetTrayEntryCallback(entry.handle, checkbox_callback<F>, entry.cb_userdata);
        }

    private:
        template<class F>
            requires(std::is_invocable_r_v<void, F&>)
        static void SDLCALL button_callback(void* userdata, SDL_TrayEntry* entry) {
            try {
                (*static_cast<F*>(userdata))();
            } catch (const std::exception& e) {
                log_err("{}", e.what());
            } catch (...) {
                log_err("Unknown exception");
            }
        }

        template<class F>
            requires(std::is_invocable_r_v<void, F&, bool>)
        static void SDLCALL checkbox_callback(void* userdata, SDL_TrayEntry* entry) {
            try {
                (*static_cast<F*>(userdata))(SDL_GetTrayEntryChecked(entry));
            } catch (const std::exception& e) {
                log_err("{}", e.what());
            } catch (...) {
                log_err("Unknown exception");
            }
        }

        template<class F>
        static void tray_destructor(void* userdata) {
            delete static_cast<F*>(userdata);
        }
    };
}  // namespace vgi
