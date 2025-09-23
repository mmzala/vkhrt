### PROJECT
cmake_minimum_required(VERSION 3.22.1)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
set(CMAKE_BUILD_RPATH "$ORIGIN")

project(VKHRT)

### HELPERS AND INCLUDES

include(FetchContent)

### COMPILATION SETTINGS

add_executable(VKHRT "source/main.cpp")

option(WARNINGS_AS_ERRORS "Enable warnings as errors" ON)
option(COMPILE_SHADERS "Compile all GLSL shaders as part of build step" ON)

target_compile_features(VKHRT INTERFACE cxx_std_20)
target_compile_options(VKHRT
		INTERFACE -Wall INTERFACE -Wextra INTERFACE -Wno-unknown-pragmas)

if (WARNINGS_AS_ERRORS)
	message(STATUS "### Warnings are enabled as Errors")
	target_compile_options(VKHRT INTERFACE -Werror)
endif ()

# Add external dependencies
add_subdirectory(external)
target_link_libraries(VKHRT
        PUBLIC VulkanAPI
        PUBLIC VulkanMemoryAllocator
		PUBLIC spdlog::spdlog
		PUBLIC SDL3::SDL3-static
		PUBLIC glm::glm
		PUBLIC Assimp
		PUBLIC STB
)

# Add sources and includes
file(GLOB_RECURSE sources CONFIGURE_DEPENDS "source/*.cpp")
file(GLOB_RECURSE headers CONFIGURE_DEPENDS "include/*.hpp")

target_sources(VKHRT PUBLIC ${headers} PRIVATE ${sources})
target_include_directories(VKHRT PUBLIC "include" "external")

### SHADER COMPILATION

if (COMPILE_SHADERS)
	message(STATUS "### Shaders will be compiled on build")
	add_subdirectory(shaders)
	add_dependencies(VKHRT Shaders)
endif ()
