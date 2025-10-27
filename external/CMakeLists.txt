# Vulkan

find_package(Vulkan REQUIRED)
add_library(VulkanAPI INTERFACE)

target_link_libraries(VulkanAPI INTERFACE ${Vulkan_LIBRARY})
target_include_directories(VulkanAPI INTERFACE Vulkan-Headers/include)

if (WIN32)
    target_compile_options(VulkanAPI
            INTERFACE -DVK_USE_PLATFORM_WIN32_KHR
            INTERFACE -DNOMINMAX)
elseif (LINUX)
    target_compile_options(VulkanAPI
            INTERFACE -DVK_USE_PLATFORM_XLIB_KHR)
endif ()

# Vulkan Memory allocator

FetchContent_Declare(
        VMA
        GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git
        GIT_TAG v3.1.0
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
)

FetchContent_MakeAvailable(vma)

# SpdLog

FetchContent_Declare(
        SpdLog
        GIT_REPOSITORY https://github.com/gabime/spdlog.git
        GIT_TAG v1.15.3
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
)

FetchContent_MakeAvailable(SpdLog)

# SDL

FetchContent_Declare(
        SDL
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG release-3.2.0
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
)

set(SDL_SHARED OFF)
set(SDL_STATIC ON)
FetchContent_MakeAvailable(SDL)

target_compile_options(SDL3-static PRIVATE -Wno-deprecated-declarations)

# GLM

FetchContent_Declare(
        GLM
        GIT_REPOSITORY https://github.com/g-truc/glm.git
        GIT_TAG 1.0.1
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
)

FetchContent_MakeAvailable(GLM)

# Guarantees GLM follows the same perspective convention as Vulkan
target_compile_definitions(glm PUBLIC GLM_FORCE_DEPTH_ZERO_TO_ONE PUBLIC GLM_ENABLE_EXPERIMENTAL)

# Assimp

add_library(Assimp INTERFACE)
FetchContent_Declare(
        assimp
        GIT_REPOSITORY https://github.com/assimp/assimp.git
        GIT_TAG v6.0.1
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
)

set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ASSIMP_INJECT_DEBUG_POSTFIX OFF CACHE BOOL "" FORCE)
set(ASSIMP_INSTALL OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(assimp)

target_compile_options(assimp PRIVATE -Wdeprecated-non-prototype)

target_include_directories(Assimp INTERFACE ${assimp_SOURCE_DIR}/include)
target_include_directories(Assimp INTERFACE ${assimp_BINARY_DIR}/include) # Include for assimp config
target_link_libraries(Assimp INTERFACE assimp)

# STB

add_library(STB INTERFACE)
FetchContent_Declare(
        stb
        GIT_REPOSITORY https://github.com/nothings/stb.git
        GIT_TAG master
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
)

FetchContent_MakeAvailable(stb)
target_include_directories(STB INTERFACE ${stb_SOURCE_DIR})

# ImGui

add_library(ImGui INTERFACE)
FetchContent_Declare(
        imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG v1.92.4
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
)

FetchContent_MakeAvailable(imgui)
target_include_directories(ImGui INTERFACE ${imgui_SOURCE_DIR})
