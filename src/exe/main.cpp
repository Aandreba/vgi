#include <chrono>
#include <iostream>
#include <thread>
#include <vgi/buffer/uniform.hpp>
#include <vgi/buffer/vertex.hpp>
#include <vgi/device.hpp>
#include <vgi/log.hpp>
#include <vgi/tray.hpp>
#include <vgi/vgi.hpp>
#include <vgi/window.hpp>

using namespace std::literals;

struct uniform {
    vgi::std140<glm::mat4> mvp;
};

int run() {
    vgi::log("Detected devices ({}):", vgi::device::all().size());
    for (const vgi::device& device: vgi::device::all()) {
        vgi::log("{}", device.name());
    }

    vgi::window win{vgi::device::all().front(), u8"Hello world!", 900, 600};
    vgi::vertex_buffer_guard vbo{win, 3};
    vgi::uniform_buffer_guard<uniform> ubo{win};
    ubo.write(win, uniform{.mvp = glm::mat4(1.0f)});

    while (true) {
        SDL_Event event;
        // while (SDL_PollEvent(&event)) {
        while (SDL_WaitEvent(&event)) {
            switch (event.type) {
                case SDL_EVENT_QUIT:
                    return 0;
                case SDL_EVENT_WINDOW_CLOSE_REQUESTED: {
                    if (event.window.windowID == win) return 0;
                    break;
                }
                default:
                    break;
            }
        }

        // TODO Render loop
    }
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
