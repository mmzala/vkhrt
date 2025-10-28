#pragma once
#include <memory>

class VulkanContext;
class Renderer;
struct SDL_Window;
union SDL_Event;

class ImGuiBackend
{
public:
    ImGuiBackend(const std::shared_ptr<VulkanContext>& vulkanContext, SDL_Window& sdlWindow, const Renderer& renderer);
    ~ImGuiBackend();

    void NewFrame();
    void UpdateEvent(const SDL_Event& event);
};
