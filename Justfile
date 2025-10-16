# This file is not required to build, but it's a usefull tool to run a list of commands one after each other
# To run this file, you will need 'just' (https://github.com/casey/just)

set windows-powershell := true
set dotenv-load

DOCS_URL := "file://" + justfile_directory() + "/build/docs/index.html"
COMMON_CMAKE_ARGS := "-DCMAKE_OSX_ARCHITECTURES=\"x86_64;arm64\""

# Builds the libraries and the example executable
build build_type="Debug" docs="OFF" *ARGS="":
    cmake -E make_directory build
    cmake -DCMAKE_BUILD_TYPE={{build_type}} -DVGI_DOXYGEN={{docs}} {{COMMON_CMAKE_ARGS}} {{ARGS}} -S . -B build
    cmake --build build -j {{num_cpus()}}

# Runs the program in Debug mode
[unix]
run *ARGS="": (build "Debug" "OFF")
    ./build/vgi_exe {{ARGS}}

# Runs the program in Release mode
[unix]
exe *ARGS="": (build "Release" "OFF")
    ./build/vgi_exe {{ARGS}}

# Runs the program in Debug mode
[windows]
run *ARGS="": (build "Debug" "OFF")
    Start-Process -NoNewWindow -FilePath "build\Debug\vgi_exe.exe" {{ARGS}}

# Runs the program in Release mode
[windows]
run *ARGS="": (build "Release" "OFF")
    Start-Process -NoNewWindow -FilePath "build\Release\vgi_exe.exe" {{ARGS}}

[windows]
docs: (build "Debug" "ON")
    Start-Process "{{DOCS_URL}}"

[unix]
docs: (build "Debug" "ON")
    open "{{DOCS_URL}}"

clean:
    cmake --build build -t clean
