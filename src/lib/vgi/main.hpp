// This header file helps us set up the context so that we can handle thinks like arguments, for
// example
#ifdef __VGI_MAIN_
#error "'vgi/main.hpp' should only be included once!"
#else
#define __VGI_MAIN_
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <filesystem>
#include <span>
#include <string>

#ifdef _MSC_VER
#include <windows.h>
#endif

#include "event.hpp"
#include "log.hpp"
#include "vgi.hpp"

int main(int argc, char* argv[]) {
    auto destroy_remaining_events = []() {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type != vgi::custom_event_type()) continue;
            const vgi::event_info* info = reinterpret_cast<vgi::event_info*>(event.user.data2);
            if (event.user.code == 0) {
                try {
                    info->destructor(event.user.data1);
                } catch (...) {
                    // TODO: What should we do here?
                }
            } else {
                VGI_UNREACHABLE;
            }
        }
    };

    extern int __vgi_main_();
    extern std::span<const std::filesystem::path::value_type* const> __vgi_arguments_;

#ifdef _MSC_VER
    const auto free_wargv = [](LPWSTR* ptr) noexcept { LocalFree(ptr); };
    std::unique_ptr<LPWSTR, decltype(free_wargv)> wargv{nullptr, free_wargv};
#endif

    int exit_code;
    try {
        // Parse command arguments
#ifdef _MSC_VER
        wargv.reset(CommandLineToArgvW(GetCommandLineW(), &argc));
        if (wargv == nullptr) {
            __vgi_arguments_ = std::span<const wchar_t* const>{};
        } else {
            __vgi_arguments_ =
                    std::span<const wchar_t* const>{wargv.get(), static_cast<size_t>(argc)};
        }
#else
        __vgi_arguments_ = std::span<const char* const>{static_cast<const char* const*>(argv),
                                                        static_cast<size_t>(argc)};
#endif

        try {
            exit_code = __vgi_main_();
        } catch (...) {
            destroy_remaining_events();
            throw;
        }
        destroy_remaining_events();
    } catch (const vgi::vgi_error& e) {
        vgi::log_err("error: {}({}:{}): {}", e.location.file_name(), e.location.line(),
                     e.location.column(), e.what());
        std::ignore = SDL_ShowSimpleMessageBox(
                SDL_MESSAGEBOX_ERROR, reinterpret_cast<const char*>(u8"Error"), e.what(), nullptr);
        exit_code = EXIT_FAILURE;
    } catch (const std::exception& e) {
        vgi::log_err("{}", e.what());
        std::ignore = SDL_ShowSimpleMessageBox(
                SDL_MESSAGEBOX_ERROR, reinterpret_cast<const char*>(u8"Error"), e.what(), nullptr);
        exit_code = EXIT_FAILURE;
    } catch (...) {
        vgi::log_err("Unhandled exception");
        std::ignore = SDL_ShowSimpleMessageBox(
                SDL_MESSAGEBOX_ERROR, reinterpret_cast<const char*>(u8"Error"),
                reinterpret_cast<const char*>(u8"Unhandled exception"), nullptr);
        exit_code = EXIT_FAILURE;
    }

    vgi::quit();
    return exit_code;
}

#undef main
#define main __vgi_main_

#endif
