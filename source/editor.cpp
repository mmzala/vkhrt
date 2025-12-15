#include "editor.hpp"
#include "application.hpp"
#include "renderer.hpp"
#include "resources/model/model.hpp"
#include <imgui.h>

Editor::Editor(const Application& application, const std::shared_ptr<VulkanContext>& vulkanContext, const std::shared_ptr<Renderer>& renderer)
    : _application(application)
    , _vulkanContext(vulkanContext)
    , _renderer(renderer)
{
    for (const std::shared_ptr<Model>& model : _renderer->GetModels())
    {
        _sceneInformation.trianglePrimitivesCount += model->vertexCount;
        _sceneInformation.curvePrimitivesCount += model->curveCount;
        _sceneInformation.lssPrimitivesCount += model->lssPositionCount / 2;
    }
    _lssSupported = _vulkanContext->IsExtensionSupported(VK_NV_RAY_TRACING_LINEAR_SWEPT_SPHERES_EXTENSION_NAME);
}

void Editor::Update()
{
    static constexpr float INDENT_SPACING = 20.0f;

    ImGui::Begin("Debug Information", nullptr, ImGuiWindowFlags_NoDecoration);
    ImGui::SetWindowSize(ImVec2(250.0f, 325.0f));

    vk::PhysicalDeviceProperties physicalDeviceProperties = _vulkanContext->PhysicalDevice().getProperties();
    ImGui::Text("GPU: %s", physicalDeviceProperties.deviceName.data());
    ImGui::Text("Frame Time: %fms", _application.GetFrameTime());
    ImGui::Text("LSS Supported: %s", _lssSupported ? "Yes" : "No");

    ImGui::Separator();

    ImGui::Text("Scene Information");
    ImGui::Indent(INDENT_SPACING);
    ImGui::Text("Triangle Count: %u", _sceneInformation.trianglePrimitivesCount);
    ImGui::Text("Curve Count: %u", _sceneInformation.curvePrimitivesCount);
    ImGui::Text("LSS Count: %u", _sceneInformation.lssPrimitivesCount);
    ImGui::Indent(-INDENT_SPACING);

    ImGui::End();
}
