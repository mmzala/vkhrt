#pragma once
#include "common.hpp"
#include <memory>

class VulkanContext;
class Renderer;
struct SDL_Window;
union SDL_Event;

class ImGuiBackend
{
public:
    ImGuiBackend(const std::shared_ptr<VulkanContext>& vulkanContext, const std::shared_ptr<Renderer>& renderer, SDL_Window& sdlWindow);
    ~ImGuiBackend();
    NON_COPYABLE(ImGuiBackend);
    NON_MOVABLE(ImGuiBackend);

    void NewFrame();
    void UpdateEvent(const SDL_Event& event);
};
