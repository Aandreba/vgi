#include <vgi/device.hpp>
#include <vgi/fs.hpp>
#include <vgi/log.hpp>
#include <vgi/main.hpp>
#include <vgi/texture.hpp>
#include <vgi/vgi.hpp>
#include <vgi/window.hpp>

#include "basic.hpp"
#include "skeleton.hpp"

using namespace std::literals;

int main() {
    vgi::init(u8"Entorn VGI");

    vgi::log("Arguments ({}):", vgi::argc());
    for (auto arg: vgi::argv()) {
        vgi::log(VGI_OS("- {}"), arg);
    }

    vgi::log("Detected devices ({}):", vgi::device::all().size());
    for (const vgi::device& device: vgi::device::all()) {
        vgi::log("- {}", device.name());
    }

    vgi::push_event<std::string>("Hello world!");
    throw "A!!";

    vgi::emplace_system<vgi::window>(vgi::device::all().front(), u8"Hello world!", 900, 600,
                                     SDL_WINDOW_RESIZABLE)
            .add_layer<skeleton::scene>();

    vgi::run();
    return 0;
}
