function(target_enable_svml TARGET_NAME)
  if (CMAKE_CXX_COMPILER_ID STREQUAL "IntelLLVM")         # oneAPI icx/icpx
    # Use SVML for vector math + allow SVML for scalar calls too
    target_compile_options(${TARGET_NAME} PRIVATE -fveclib=SVML -fimf-use-svml=true)
    # oneAPI toolchain usually auto-links libsvml if the env is set up;
    # add an explicit link just in case:
    target_link_libraries(${TARGET_NAME} PRIVATE svml)

  elseif (CMAKE_CXX_COMPILER_ID STREQUAL "Intel")         # Classic ICC/ICPC
    # ICC’s switch to select SVML
    target_compile_options(${TARGET_NAME} PRIVATE -vec-lib=svml)
    target_link_libraries(${TARGET_NAME} PRIVATE svml)

  elseif (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    # Upstream Clang can use SVML if present
    target_compile_options(${TARGET_NAME} PRIVATE -fveclib=SVML)
    # You typically must link SVML explicitly and point the linker to it:
    # (see the find_library block below)
    find_library(SVML_LIB NAMES svml
      HINTS
        $ENV{ONEAPI_ROOT}/compiler/latest/lib
        $ENV{ONEAPI_ROOT}/compiler/*/lib
        $ENV{INTEL_ONEAPI_ROOT}/compiler/latest/lib
        /opt/intel/oneapi/compiler/*/lib
        /opt/intel/oneapi/*/lib
        "C:/Program Files (x86)/Intel/oneAPI/compiler/latest/lib"
    )
    if (SVML_LIB)
      target_link_libraries(${TARGET_NAME} PRIVATE ${SVML_LIB})
      # Optional: add an rpath on Unix so the binary finds libsvml at runtime
      if (UNIX AND NOT APPLE)
        get_filename_component(_svml_dir "${SVML_LIB}" DIRECTORY)
        target_link_options(${TARGET_NAME} PRIVATE "-Wl,-rpath,${_svml_dir}")
      endif()
    endif()

  elseif (MSVC)
    # If you’re using Intel’s Clang in MSVC/CL driver mode,
    # the compatible flag is:
    #   /Qimf-use-svml:true   (and SVML gets pulled automatically)
    # If you are *pure* MSVC, there’s no portable switch to “use SVML”;
    # you need Intel’s compilers installed/invoked.
    target_compile_options(${TARGET_NAME} PRIVATE /Qimf-use-svml:true)
    target_link_libraries(${TARGET_NAME} PRIVATE svml)
  endif()
endfunction()
