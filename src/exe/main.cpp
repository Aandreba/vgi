#include <iostream>
#include <vgi/vgi.hpp>

int run() {
    std::cout << "Hi!!" << std::endl;
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
