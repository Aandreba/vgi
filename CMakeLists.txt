cmake_minimum_required(VERSION 3.40)
project(vgi CXX)

# Import CMake modules
include(FetchContent)
include("cmake/EnableSVML.cmake")
include("cmake/BuildShaders.cmake")
include("cmake/FindGraphViz.cmake")
include("cmake/Unicode.cmake")

# We configure support to compile C++ code using C++20
set(CMAKE_CXX_STANDARD 20)

# Load environment variables from the '.env' file (if it exists)
if(EXISTS ${CMAKE_HOME_DIRECTORY}/.env)
    file(STRINGS ${CMAKE_HOME_DIRECTORY}/.env ENV_FILE)
    foreach(VAR ${ENV_FILE})
        string(REGEX MATCH "^[^=]*" KEY ${VAR})
        string(REGEX REPLACE "^[^=]*=" "" VALUE ${VAR})
        if(VALUE MATCHES "^\".*\"$")
            string(REGEX REPLACE "^\"(.*)\"$" "\\1" VALUE "${VALUE}")
            string(REGEX REPLACE "^'(.*)'$" "\\1" VALUE "${VALUE}")
        endif()
        set(ENV{${KEY}} ${VALUE})
    endforeach()
endif()

# Define compile options
option(VGI_SHARED "Utilitza la versió dinamica de la llibreria per defecte" OFF)
option(VGI_DOXYGEN "Generar documentacio Doxygen" OFF)

# Define library options
set(GLM_ENABLE_CXX_20 ON)
set(GLM_ENABLE_FAST_MATH ON)
set(GLM_BUILD_TESTS OFF)
set(SDL_STATIC ON)
set(SDL_SHARED ON)
#set(SDL_MAIN_USE_CALLBACKS ON)

# Fetch/Dowload libraries
FetchContent_Declare(
        SDL3
        GIT_REPOSITORY https://github.com/libsdl-org/SDL
        GIT_TAG release-3.2.22
        OVERRIDE_FIND_PACKAGE
)
FetchContent_Declare(
        glm
        GIT_REPOSITORY https://github.com/g-truc/glm
        GIT_TAG 1.0.1
        OVERRIDE_FIND_PACKAGE
)
FetchContent_MakeAvailable(SDL3 glm)

# Create a library for the Vulkan Memory Allocator
add_library(vgi_vma STATIC "src/vma.cpp")

# Create a library for the reusable code
file(GLOB_RECURSE vgi_lib_sources CONFIGURE_DEPENDS "src/lib/vgi/*.cpp")
add_library(vgi_static STATIC ${vgi_lib_sources})
add_library(vgi_shared SHARED ${vgi_lib_sources})

# Don't build the libraries that will not be used, either explicitly or implicitly
if (VGI_SHARED)
    add_library(vgi::vgi ALIAS vgi_shared)
    set_target_properties(vgi_static PROPERTIES EXCLUDE_FROM_ALL)
else()
    add_library(vgi::vgi ALIAS vgi_static)
    set_target_properties(vgi_shared PROPERTIES EXCLUDE_FROM_ALL ON)
endif()

# Force libraries (and dependants) to use UTF-8 string literals
target_literal_utf8(vgi_static)
target_literal_utf8(vgi_shared)

# Create a list with all of the libraries used by vgi (libraries can only be linked all together, at once)
set(vgi_libraries glm::glm vgi_vma)
set(vgi_static_libraries SDL3::SDL3-static)
set(vgi_shared_libraries SDL3::SDL3-shared)

# Declare the 'src/lib' directory as an include target.
target_include_directories(vgi_static PUBLIC "src/lib" PRIVATE "src/private")
target_include_directories(vgi_shared PUBLIC "src/lib" PRIVATE "src/private")

# Specify the macros to be added when compiling the source
set(
    vgi_definitions
    VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1
    VULKAN_HPP_NO_CONSTRUCTORS
    VULKAN_HPP_HANDLES_MOVE_EXCHANGE
    VMA_STATIC_VULKAN_FUNCTIONS=0
    VMA_DYNAMIC_VULKAN_FUNCTIONS=1
    # Adjust glm to use Vulkan's coordinate system
    GLM_FORCE_RADIANS
    GLM_FORCE_DEPTH_ZERO_TO_ONE
    #GLM_FORCE_LEFT_HANDED
)
target_compile_definitions(vgi_vma PUBLIC ${vgi_definitions})
target_compile_definitions(vgi_static PUBLIC ${vgi_definitions})
target_compile_definitions(vgi_shared PUBLIC ${vgi_definitions})

# Find Vulkan SDK
if (DEFINED ENV{VULKAN_SDK})
    set(Vulkan_Include "$ENV{VULKAN_SDK}/include")
    # The Vulkan SDK on Apple devices is actually the MoltenVK library,
    # a translation layer that transforms Vulkan functions into their equivalent Metal ones. Hence, it's better
    # to statically link the SDK, because there is no guarantee that the user running the final executable has MoltenVK
    # installed on their device.
    # More (and better) information at:
    # - https://vulkan.lunarg.com/doc/view/latest/mac/getting_started.html
    # - https://wiki.libsdl.org/SDL3/SDL_Vulkan_LoadLibrary
    if (APPLE)
        list(APPEND vgi_libraries "$ENV{VULKAN_SDK}/lib/libvulkan.dylib")
    endif()
    target_include_directories(vgi_vma PUBLIC ${Vulkan_Include})
    target_include_directories(vgi_static PUBLIC ${Vulkan_Include})
    target_include_directories(vgi_shared PUBLIC ${Vulkan_Include})
    set(GLSLC_PATH "$ENV{VULKAN_SDK}/bin/glslc")
else()
    find_package(Vulkan REQUIRED)
    target_link_libraries(vgi_vma PUBLIC Vulkan::Headers)
    list(APPEND vgi_libraries Vulkan::Vulkan)
    set(GLSLC_PATH Vulkan::glslc)
endif()

# Link the required libraries into VGI
target_link_libraries(vgi_static PUBLIC ${vgi_libraries} ${vgi_static_libraries})
target_link_libraries(vgi_shared PUBLIC ${vgi_libraries} ${vgi_shared_libraries})

# (Windows) Enable preprocessor conformance mode
if (MSVC)
    target_compile_options(vgi_static PUBLIC /Zc:preprocessor)
    target_compile_options(vgi_shared PUBLIC /Zc:preprocessor)
endif()

# (x86) Enable Intel SVML
if (x86)
    target_enable_svml(vgi_static)
    target_enable_svml(vgi_shared)
endif()

# Create the executable
file(GLOB_RECURSE vgi_exe_sources CONFIGURE_DEPENDS "src/exe/*.cpp")
add_executable(vgi_exe ${vgi_exe_sources})
target_link_libraries(vgi_exe PRIVATE vgi::vgi)

# Compile the shaders before compiling the executable
file(GLOB_RECURSE vgi_exe_shaders CONFIGURE_DEPENDS "src/exe/*.vert" "src/exe/*.frag")
add_shaders(vgi_exe ${vgi_exe_shaders})

# (Windows) Copy shared libraries into executable's directory
if (WIN32 AND $<TARGET_RUNTIME_DLLS:vgi_exe>)
    add_custom_command(TARGET vgi_exe POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_RUNTIME_DLLS:vgi_exe> $<TARGET_FILE_DIR:vgi_exe>
        COMMAND_EXPAND_LISTS
    )
endif()

# Generate Doxygen docs (if enabled)
find_package(Doxygen)
if(Doxygen_FOUND)
    set(DOXYGEN_HTML_OUTPUT            ${PROJECT_BINARY_DIR}/docs)
    set(DOXYGEN_GENERATE_HTML          YES)
    set(DOXYGEN_MULTILINE_CPP_IS_BRIEF YES)
    set(DOXYGEN_BUILTIN_STL_SUPPORT    YES)
    set(DOXYGEN_USE_MDFILE_AS_MAINPAGE "README.md")
    set(DOXYGEN_EXCLUDE_PATTERNS "*.cpp")
    if(GRAPHVIZ_FOUND)
        set(DOXYGEN_HAVE_DOT           YES)
    endif()
    cmake_path(ABSOLUTE_PATH DOXYGEN_USE_MDFILE_AS_MAINPAGE OUTPUT_VARIABLE DOXYGEN_USE_MDFILE_AS_MAINPAGE)
    
    doxygen_add_docs(vgi_docs
        "src/lib/vgi" ${DOXYGEN_USE_MDFILE_AS_MAINPAGE}
        COMMENT "Generate HTML documentation"
    )
    
    # Only build the docs when requested, no need to waste time
    if (VGI_DOXYGEN)
        if (VGI_SHARED)
            add_dependencies(vgi_shared vgi_docs)
        else()
            add_dependencies(vgi_static vgi_docs)
        endif()
    endif()
elseif(VGI_DOXYGEN)
    message(WARNING "Doxygen not found. Documentation will not be generated")
endif()
