function(add_shaders TARGET_NAME)
    set(SHADER_SOURCE_FILES ${ARGN}) # the rest of arguments to this function will be assigned as shader source files

    # Validate that source files have been passed
    list(LENGTH SHADER_SOURCE_FILES FILE_COUNT)
    if (FILE_COUNT EQUAL 0)
        message(WARNING "No shaders found")
        return()
    endif ()

    foreach (SHADER_SOURCE IN LISTS SHADER_SOURCE_FILES)
        cmake_path(ABSOLUTE_PATH SHADER_SOURCE NORMALIZE)
        cmake_path(GET SHADER_SOURCE FILENAME SHADER_NAME)
        set(SHADER_TARGET_NAME "${TARGET_NAME}_${SHADER_NAME}")
        add_custom_target(${SHADER_TARGET_NAME}
                COMMENT "Compiling shader '${SHADER_NAME}' [${TARGET_NAME}]"
                COMMAND "${CMAKE_COMMAND}" "-E" "make_directory" "$<TARGET_FILE_DIR:${TARGET_NAME}>/shaders"
                COMMAND "${GLSLC_PATH}" "${SHADER_SOURCE}" "-o" "$<TARGET_FILE_DIR:${TARGET_NAME}>/shaders/${SHADER_NAME}.spv"
                DEPENDS ${SHADER_SOURCE}
                BYPRODUCTS "${CMAKE_CURRENT_BINARY_DIR}/shaders/${SHADER_NAME}.spv"
        )
        add_dependencies(${TARGET_NAME} ${SHADER_TARGET_NAME})
    endforeach()

    add_custom_target("${TARGET_NAME}_shaders" ALL
            ${SHADER_COMMANDS}
            COMMENT "Compiling Shaders [${TARGET_NAME}]"
            SOURCES ${SHADER_SOURCE_FILES}
            BYPRODUCTS ${SHADER_PRODUCTS}
    )
    add_dependencies(${TARGET_NAME} "${TARGET_NAME}_shaders")
endfunction()
