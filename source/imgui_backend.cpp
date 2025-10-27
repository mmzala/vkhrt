#include "imgui_backend.hpp"
#include "vulkan_context.hpp"
#include "swap_chain.hpp"
#include <imgui/backends/imgui_impl_sdl3.h>
#include <imgui/backends/imgui_impl_vulkan.h>

ImGuiBackend::ImGuiBackend(const std::shared_ptr<VulkanContext>& vulkanContext, SDL_Window* sdlWindow, const SwapChain& swapChain)
{
    // Context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsDark();

    // Platform/Renderer backends
    ImGui_ImplSDL3_InitForVulkan(sdlWindow);

    vk::PipelineRenderingCreateInfoKHR pipelineCreationInfo {};
    pipelineCreationInfo.colorAttachmentCount = 1;

    vk::Format swapChainFormat = swapChain.GetFormat();
    pipelineCreationInfo.pColorAttachmentFormats = &swapChainFormat;

    ImGui_ImplVulkan_InitInfo initInfo {};
    initInfo.ApiVersion = vk::makeApiVersion(0, 1, 3, 0);
    initInfo.Instance = vulkanContext->Instance();
    initInfo.PhysicalDevice = vulkanContext->PhysicalDevice();
    initInfo.Device = vulkanContext->Device();
    initInfo.QueueFamily = vulkanContext->QueueFamilies().graphicsFamily.value();
    initInfo.Queue = vulkanContext->GraphicsQueue();
    initInfo.DescriptorPool = vulkanContext->DescriptorPool();
    initInfo.MinImageCount = 2;
    initInfo.ImageCount = swapChain.GetImageCount();
    initInfo.Allocator = nullptr;
    initInfo.PipelineInfoMain.PipelineRenderingCreateInfo = pipelineCreationInfo;
    // initInfo.CheckVkResultFn = VkCheckResult;
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
