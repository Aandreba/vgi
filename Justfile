# This file is not required to build, but it's a usefull tool to run a list of commands one after each other
# To run this file, you will need 'just' (https://github.com/casey/just)

set windows-powershell := true
set dotenv-load

build build_type="Debug":
    cmake -E make_directory build
    cmake -DCMAKE_BUILD_TYPE={{build_type}} -S . -B build
    cmake --build build -j {{num_cpus()}}

clean build_type="Debug":
    cmake --build build -t clean

[unix]
run build_type="Debug" *ARGS="": (build build_type)
    ./build/vgi_exe {{ARGS}}

[windows]
run build_type="Debug" *ARGS="": (build build_type)
    Start-Process -NoNewWindow -FilePath "build\{{build_type}}\vgi_exe.exe" {{ARGS}}

[windows]
docs: build
    Start-Process "file://{{justfile_directory()}}/build/docs/index.html"
