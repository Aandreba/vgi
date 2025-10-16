// This header file helps us set up the context so that we cen handle thinks like arguments, for
// example
#ifdef __VGI_MAIN_
#error "'vgi/main.hpp' should only be included once!"
#else
#define __VGI_MAIN_
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <filesystem>
#include <string>
#include <vector>

#include "log.hpp"

// TODO WinMain
int main(int argc, char* argv[]) {
    extern int __vgi_main_();
    extern std::vector<std::filesystem::path::string_type> __vgi_arguments_;

    int exit_code;
    try {
        __vgi_arguments_.reserve(argc);
        for (int i = 0; i < argc; ++i) {
            __vgi_arguments_.emplace_back(argv[i]);
        }
        exit_code = __vgi_main_();
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
