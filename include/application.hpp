#pragma once
#include <memory>
#include "common.hpp"

class VulkanContext;
class Renderer;
class Input;
class FlyCamera;
class ImGuiBackend;
class Editor;
class Timer;
struct SDL_Window;

class Application
{
public:
    Application();
    ~Application();
    NON_COPYABLE(Application);
    NON_MOVABLE(Application);

    int Run();
    [[nodiscard]] float GetFrameTime() const { return _frameTime; }

private:
    void MainLoopOnce();

    std::unique_ptr<Timer> _timer;
    std::shared_ptr<Input> _input;
    std::shared_ptr<FlyCamera> _flyCamera;
    std::shared_ptr<VulkanContext> _vulkanContext;
    std::shared_ptr<Renderer> _renderer;
    std::unique_ptr<ImGuiBackend> _imguiBackend;
    std::unique_ptr<Editor> _editor;

    SDL_Window* _window = nullptr;
    bool _exitRequested = false;
    float _frameTime {};
};
