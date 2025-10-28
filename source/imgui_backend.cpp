#include "imgui_backend.hpp"
#include "renderer.hpp"
#include "vulkan_context.hpp"
#include "swap_chain.hpp"
#include "vk_common.hpp"
#include <backends/imgui_impl_sdl3.h>
#include <backends/imgui_impl_vulkan.h>

ImGuiBackend::ImGuiBackend(const std::shared_ptr<VulkanContext>& vulkanContext, const std::shared_ptr<Renderer>& renderer, SDL_Window& sdlWindow)
{
    // Context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsDark();

    // Platform/Renderer backends
    ImGui_ImplSDL3_InitForVulkan(&sdlWindow);

    ImGui_ImplVulkan_InitInfo initInfo {};
    initInfo.ApiVersion = vk::makeApiVersion(0, 1, 3, 0);
    initInfo.Instance = vulkanContext->Instance();
    initInfo.PhysicalDevice = vulkanContext->PhysicalDevice();
    initInfo.Device = vulkanContext->Device();
    initInfo.QueueFamily = vulkanContext->QueueFamilies().graphicsFamily.value();
    initInfo.Queue = vulkanContext->GraphicsQueue();
    initInfo.DescriptorPool = vulkanContext->DescriptorPool();
    initInfo.MinImageCount = 2;
    initInfo.ImageCount = renderer->GetSwapChain().GetImageCount();
    initInfo.CheckVkResultFn = VkCheckResult;
    initInfo.UseDynamicRendering = false;
    initInfo.PipelineInfoMain.PipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
    initInfo.PipelineInfoMain.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
    VkFormat swapchainFormat = static_cast<VkFormat>(renderer->GetSwapChain().GetFormat());
    initInfo.PipelineInfoMain.PipelineRenderingCreateInfo.pColorAttachmentFormats = &swapchainFormat;
    initInfo.PipelineInfoMain.RenderPass = renderer->GetImGuiRenderPass();
    ImGui_ImplVulkan_Init(&initInfo);
}

ImGuiBackend::~ImGuiBackend()
{
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
}

void ImGuiBackend::NewFrame()
{
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
}

void ImGuiBackend::UpdateEvent(const SDL_Event& event)
{
    ImGui_ImplSDL3_ProcessEvent(&event);
}
