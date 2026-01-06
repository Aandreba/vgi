# Usage

The main way VGI is intended to be used is with CMakes's [FetchContent](https://cmake.org/cmake/help/latest/module/FetchContent.html).

## Project Structure
```
my_vgi_app/
    - CMakeLists.txt
    - src/
        - main.cpp
```

### `CMakeLists.txt`

```cmake
cmake_minimum_required(VERSION 3.40)
project(my_vgi_app LANGUAGES CXX)
include(FetchContent)

# VGI is C++20 - keep this consistent in your app too.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---- Options you may want to set BEFORE fetching VGI ----
set(VGI_SHARED OFF)

# ---- Fetch VGI ----
FetchContent_Declare(
  vgi
  GIT_REPOSITORY https://github.com/Aandreba/vgi.git
  GIT_TAG        <vgi branch to import>
)
FetchContent_MakeAvailable(vgi)

# ---- Your executable ----
add_executable(my_vgi_app src/main.cpp)

# Link against VGI's exported CMake target
target_link_libraries(my_vgi_app PRIVATE vgi::vgi)

# On some toolchains you may need this to ensure UTF-8 string literals behave as expected.
if (MSVC)
    target_compile_options(my_vgi_app PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/utf-8>)
endif()
```

### `src/main.cpp`

```cpp
#include <vgi/main.hpp>
#include <vgi/vgi.hpp>
#include <vgi/device.hpp>
#include <vgi/window.hpp>

int main() {
    // Initialize the runtime (Vulkan instance, SDL setup, etc.)
    vgi::init(u8"My VGI App");

    // Get a list of all the available devices
    std::span<const vgi::device> gpus = vgi::device::all();
    // Pick a device (simplest option: first available)
    const vgi::device& gpu = gpu.front();

    // Create a Window (note that whenever we add a new system, this reference will be invalidated)
    vgi::window& window = vgi::emplace_system<vgi::window>(
        gpu,
        u8"Hello VGI",
        1280,
        720,
        SDL_WINDOW_RESIZABLE
    );

    // Start running the application loop
    vgi::run();
    // Cleanup before exiting the application
    vgi::quit();

    return 0;
}
```
