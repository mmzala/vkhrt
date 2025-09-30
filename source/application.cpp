#include "application.hpp"

// SDL throws some weird errors when parsed with clang-analyzer (used in clang-tidy checks)
// This definition fixes the issues and does not change the final build output
#define SDL_DISABLE_ANALYZE_MACROS

#include "vulkan_context.hpp"
#include "renderer.hpp"
#include "input/input.hpp"
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <spdlog/spdlog.h>

Application::Application()
{
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        spdlog::error("[SDL] Failed initializing SDL: {0}", SDL_GetError());
        return;
    }

    int32_t displayCount {};
    SDL_DisplayID* displayIds = SDL_GetDisplays(&displayCount);
    const SDL_DisplayMode* displayMode = SDL_GetCurrentDisplayMode(*displayIds);

    if (displayMode == nullptr)
    {
        spdlog::error("[SDL] Failed retrieving DisplayMode: {0}", SDL_GetError());
        return;
    }

    SDL_WindowFlags flags = SDL_WINDOW_VULKAN | SDL_WINDOW_FULLSCREEN;
    _window = SDL_CreateWindow("RayTracer", displayMode->w, displayMode->h, flags);

    if (_window == nullptr)
    {
        spdlog::error("[SDL] Failed creating SDL window: {}", SDL_GetError());
        SDL_Quit();
        return;
    }

    VulkanInitInfo vulkanInfo {};
    uint32_t sdlExtensionsCount = 0;
    vulkanInfo.extensions = SDL_Vulkan_GetInstanceExtensions(&sdlExtensionsCount);
    vulkanInfo.extensionCount = sdlExtensionsCount;
    vulkanInfo.width = displayMode->w;
    vulkanInfo.height = displayMode->h;
    vulkanInfo.retrieveSurface = [this](vk::Instance instance)
    {
        VkSurfaceKHR surface {};
        if (!SDL_Vulkan_CreateSurface(_window, instance, nullptr, &surface))
        {
            spdlog::error("[SDL] Failed creating SDL vk::Surface: {}", SDL_GetError());
        }
        return vk::SurfaceKHR(surface);
    };

    _vulkanContext = std::make_shared<VulkanContext>(vulkanInfo);
    _renderer = std::make_unique<Renderer>(vulkanInfo, _vulkanContext);
    _input = std::make_shared<Input>();
}

Application::~Application()
{
    SDL_DestroyWindow(_window);
    SDL_Quit();
}

int Application::Run()
{
    while (!_exitRequested)
    {
        MainLoopOnce();
    }

    return 0;
}

void Application::MainLoopOnce()
{
    _input->Update();

    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        if (event.type == SDL_EventType::SDL_EVENT_QUIT)
        {
            _exitRequested = true;
            break;
        }

        _input->UpdateEvent(event);
    }

    _renderer->Render();
}
