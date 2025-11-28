#pragma once
#include "common.hpp"
#include <memory>

class Application;
class VulkanContext;
class Renderer;

class Editor
{
public:
    Editor(const Application& application, const std::shared_ptr<VulkanContext>& vulkanContext, const std::shared_ptr<Renderer>& renderer);
    NON_COPYABLE(Editor);
    NON_MOVABLE(Editor);

    void Update();

private:
    struct SceneInformation
    {
        uint32_t trianglePrimitivesCount;
        uint32_t curvePrimitivesCount;
        uint32_t filledVoxelPrimitivesCount;
    } _sceneInformation {};

    const Application& _application;
    std::shared_ptr<VulkanContext> _vulkanContext;
    std::shared_ptr<Renderer> _renderer;
};
