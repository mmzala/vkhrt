#include "editor.hpp"
#include "application.hpp"
#include "renderer.hpp"
#include "resources/model/model.hpp"
#include <imgui.h>

Editor::Editor(const Application& application, const std::shared_ptr<VulkanContext>& vulkanContext, const std::shared_ptr<Renderer>& renderer)
    : _application(application), _vulkanContext(vulkanContext), _renderer(renderer)
{
    _sceneInformation = {};
    for (const std::shared_ptr<Model>& model : _renderer->GetModels())
    {
        _sceneInformation.trianglePrimitivesCount += model->vertexCount;
        _sceneInformation.curvePrimitivesCount += model->curveCount;
    }
}

void Editor::Update()
{
    static constexpr float INDENT_SPACING = 20.0f;

    ImGui::Begin("Debug Information", nullptr, ImGuiWindowFlags_NoDecoration);
    ImGui::SetWindowSize(ImVec2(200.0f, 300.0f));

    ImGui::Text("Frame Time: %fms", _application.GetFrameTime());

    ImGui::Separator();

    ImGui::Text("Scene Information");
    ImGui::Indent(INDENT_SPACING);
    ImGui::Text("Triangle Count: %u", _sceneInformation.trianglePrimitivesCount);
    ImGui::Text("Curve Count: %u", _sceneInformation.curvePrimitivesCount);
    ImGui::Indent(-INDENT_SPACING);

    ImGui::End();
}
