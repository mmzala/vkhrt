#pragma once
#include <memory>

class VulkanContext;
class SwapChain;
class SDL_Window;

class ImGuiBackend
{
public:
    ImGuiBackend(const std::shared_ptr<VulkanContext>& vulkanContext, SDL_Window* sdlWindow, const SwapChain& swapChain);
    ~ImGuiBackend();

    void NewFrame();
};
