find_program(GLSLC glslc HINTS /usr/bin /usr/local/bin $ENV{VULKAN_SDK}/Bin/ $ENV{VULKAN_SDK}/Bin32/)

if (NOT GLSLC)
    message(FATAL_ERROR "GLSLC not found at ${GLSLC}")
endif ()

set(SHADER_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

file(GLOB_RECURSE SHADERS CONFIGURE_DEPENDS
        ${SHADER_DIR}/*.rchit
        ${SHADER_DIR}/*.rmiss
        ${SHADER_DIR}/*.rgen
        ${SHADER_DIR}/*.rint
)

file(GLOB_RECURSE GLSL_SHADERS CONFIGURE_DEPENDS
        ${SHADER_DIR}/*.glsl
)

set(SPIRV_BIN "${CMAKE_CURRENT_SOURCE_DIR}/bin")
file(MAKE_DIRECTORY ${SPIRV_BIN})

function(compile_shader shader)
    get_filename_component(FILE_NAME ${shader} NAME)
    set(SPIRV_OUTPUT "${SPIRV_BIN}/${FILE_NAME}.spv")

    add_custom_command(
            OUTPUT ${SPIRV_OUTPUT}
            COMMAND "${GLSLC}" ${shader} -o ${SPIRV_OUTPUT} -g --target-env=vulkan1.3
            COMMAND_EXPAND_LISTS
            COMMENT "Compiling ${shader} to SPIR-V"
            DEPENDS ${shader} ${GLSL_SHADERS}
            VERBATIM
    )

    list(APPEND SPIRV_SHADERS ${SPIRV_OUTPUT})
    set(SPIRV_SHADERS ${SPIRV_SHADERS} PARENT_SCOPE)
endfunction()

foreach (SHADER ${SHADERS})
    compile_shader(${SHADER})
endforeach ()

add_custom_target(Shaders ALL DEPENDS ${SPIRV_SHADERS})
