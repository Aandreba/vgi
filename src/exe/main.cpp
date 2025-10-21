#include <vgi/device.hpp>
#include <vgi/fs.hpp>
#include <vgi/log.hpp>
#include <vgi/main.hpp>
#include <vgi/vgi.hpp>
#include <vgi/window.hpp>

#include "basic.hpp"
#include "waves.hpp"

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

    vgi::emplace_system<vgi::window>(vgi::device::all().front(), u8"Hello world!", 900, 600)
            .add_layer<waves_scene>();

    vgi::run();
    return 0;
}
