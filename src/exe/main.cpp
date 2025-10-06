#include <chrono>
#include <iostream>
#include <thread>
#include <vgi/log.hpp>
#include <vgi/vgi.hpp>
#include <vgi/window.hpp>

using namespace std::literals;

int run() {
    vgi::window win{u8"Hello world!", 900, 600};
    std::this_thread::sleep_for(5s);
    return 0;
}

int main() {
    vgi::init(u8"Entorn VGI");
    try {
        int ret = run();
        vgi::quit();
        return ret;
    } catch (const std::exception& e) {
        vgi::quit();
        std::cout << e.what() << std::endl;
        return 1;
    } catch (...) {
        vgi::quit();
        std::cout << "Unknown exception" << std::endl;
        return 1;
    }
}
