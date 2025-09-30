#pragma once
#include <memory>
#include "common.hpp"

class VulkanContext;
class Renderer;
class Input;
class SDL_Window;

class Application
{
public:
    Application();
    ~Application();
    NON_COPYABLE(Application);
    NON_MOVABLE(Application);

    int Run();

private:
    void MainLoopOnce();

    std::shared_ptr<VulkanContext> _vulkanContext;
    std::unique_ptr<Renderer> _renderer;
    std::shared_ptr<Input> _input;
    SDL_Window* _window = nullptr;
    bool _exitRequested = false;
};
