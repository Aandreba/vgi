#include <chrono>
#include <iostream>
#include <thread>
#include <vgi/device.hpp>
#include <vgi/log.hpp>
#include <vgi/vgi.hpp>
#include <vgi/window.hpp>

using namespace std::literals;

int run() {
    vgi::window win{u8"Hello world!", 900, 600};

    vgi::log("Detected devices ({}):", vgi::device::all().size());
    for (const vgi::device& device: vgi::device::all()) {
        vgi::log("{}", device.name());
    }

    std::this_thread::sleep_for(5s);
    return 0;
}

static void show_error_box(const char* msg) noexcept {
    try {
        if (SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, reinterpret_cast<const char*>(u8"Error"),
                                     msg, nullptr)) {
            std::cout << msg << std::endl;
        } else {
            std::cout << SDL_GetError() << std::endl;
        }
    } catch (...) {
    }
}

int main() {
    try {
        vgi::init(u8"Entorn VGI");
    } catch (const std::exception& e) {
        show_error_box(e.what());
        return 1;
    } catch (...) {
        show_error_box("Unknown exception");
        return 1;
    }

    try {
        int ret = run();
        vgi::quit();
        return ret;
    } catch (const std::exception& e) {
        show_error_box(e.what());
        vgi::quit();
        return 1;
    } catch (...) {
        show_error_box("Unknown exception");
        vgi::quit();
        return 1;
    }
}
