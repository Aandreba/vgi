# This file is not required to build, but it's a usefull tool to run a list of commands one after each other
# To run this file, you will need 'just' (https://github.com/casey/just)

set windows-powershell := true
set dotenv-load

DOCS_URL := "file://" + justfile_directory() + "/build/docs/index.html"

build build_type="Debug" docs="OFF" *ARGS="":
    cmake -E make_directory build
    cmake -DCMAKE_BUILD_TYPE={{build_type}} -DVGI_DOXYGEN={{docs}} {{ARGS}} -S . -B build
    cmake --build build -j {{num_cpus()}}

[unix]
run build_type="Debug" *ARGS="": (build build_type "OFF")
    ./build/vgi_exe {{ARGS}}

[windows]
run build_type="Debug" *ARGS="": (build build_type "OFF")
    Start-Process -NoNewWindow -FilePath "build\{{build_type}}\vgi_exe.exe" {{ARGS}}

[windows]
docs: (build "Debug" "ON")
    Start-Process "{{DOCS_URL}}"

[unix]
docs: (build "Debug" "ON")
    open "{{DOCS_URL}}"

clean:
    cmake --build build -t clean
