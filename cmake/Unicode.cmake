function(target_literal_utf8 TARGET_NAME)
    target_compile_options(
        ${TARGET_NAME} PUBLIC
        # MSVC & clang-cl: set both source + execution charsets to UTF-8
        "$<$<OR:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>:/utf-8>"
        "$<$<OR:$<C_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:Clang>>:$<$<BOOL:${WIN32}>:/utf-8>>"
        # GCC/Clang (POSIX/MinGW): ensure UTF-8 input and execution charsets
        "$<$<AND:$<OR:$<C_COMPILER_ID:GNU>,$<C_COMPILER_ID:Clang>>,$<NOT:$<BOOL:${WIN32}>>>:-finput-charset=UTF-8;-fexec-charset=UTF-8>"
        "$<$<AND:$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>,$<NOT:$<BOOL:${WIN32}>>>:-finput-charset=UTF-8;-fexec-charset=UTF-8>"
    )
endfunction()
