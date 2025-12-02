#include "renderer.hpp"
#include "fly_camera.hpp"
#include "resources/bindless_resources.hpp"
#include "resources/camera_resource.hpp"
#include "resources/model/model_loader.hpp"
#include "shader.hpp"
#include "swap_chain.hpp"
#include "top_level_acceleration_structure.hpp"
#include "vulkan_context.hpp"
#include <backends/imgui_impl_vulkan.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/matrix_decompose.hpp>

Renderer::Renderer(const VulkanInitInfo& initInfo, const std::shared_ptr<VulkanContext>& vulkanContext, const std::shared_ptr<FlyCamera>& flyCamera)
    : _vulkanContext(vulkanContext)
    , _flyCamera(flyCamera)
    , _windowWidth(initInfo.width)
    , _windowHeight(initInfo.height)
{
    _swapChain = std::make_unique<SwapChain>(vulkanContext, glm::uvec2 { initInfo.width, initInfo.height });
    InitializeCommandBuffers();
    InitializeSynchronizationObjects();
    InitializeRenderTarget();

    _bindlessResources = std::make_shared<BindlessResources>(_vulkanContext);
    _modelLoader = std::make_unique<ModelLoader>(_bindlessResources, _vulkanContext);
    _cameraResource = std::make_unique<CameraResource>(_vulkanContext);

    const std::vector<std::string> scene = {
        "assets/claire/Claire_HairMain_less_strands.gltf",
        "assets/claire/Claire_PonyTail.gltf",
        "assets/claire/hairtie/hairtie.gltf",
    };
    for (const auto& modelPath : scene)
    {
        _models.emplace_back(_modelLoader->LoadFromFile(modelPath));
    }
    InitializeBLAS();

    _tlas = std::make_unique<TopLevelAccelerationStructure>(_blases, _bindlessResources, _vulkanContext);
    _bindlessResources->UpdateDescriptorSet();

    InitializeDescriptorSets();
    InitializeRayTracingPipeline();
    InitializeImGuiPipeline();
}

Renderer::~Renderer()
{
    _vulkanContext->Device().destroyRenderPass(_imguiRenderPass);
    _vulkanContext->Device().destroyFramebuffer(_imguiFramebuffer);

    _vulkanContext->Device().destroyPipeline(_pipeline);
    _vulkanContext->Device().destroyPipelineLayout(_pipelineLayout);

    _vulkanContext->Device().destroyDescriptorSetLayout(_descriptorSetLayout);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
    {
        _vulkanContext->Device().destroy(_inFlightFences.at(i));
        _vulkanContext->Device().destroy(_renderFinishedSemaphores.at(i));
        _vulkanContext->Device().destroy(_imageAvailableSemaphores.at(i));
    }
}

void Renderer::Render()
{
    uint32_t currentResourcesFrame = _renderedFrames % MAX_FRAMES_IN_FLIGHT;
    UpdateCameraResource(currentResourcesFrame);

    VkCheckResult(_vulkanContext->Device().waitForFences(1, &_inFlightFences.at(currentResourcesFrame), vk::True,
                      std::numeric_limits<uint64_t>::max()),
        "Failed waiting on in flight fence!");

    uint32_t swapChainImageIndex {};
    VkCheckResult(_vulkanContext->Device().acquireNextImageKHR(_swapChain->GetSwapChain(), std::numeric_limits<uint64_t>::max(),
                      _imageAvailableSemaphores.at(currentResourcesFrame), nullptr, &swapChainImageIndex),
        "Failed to acquire swap chain image!");

    VkCheckResult(_vulkanContext->Device().resetFences(1, &_inFlightFences.at(currentResourcesFrame)), "Failed resetting fences!");

    vk::CommandBuffer commandBuffer = _commandBuffers.at(currentResourcesFrame);
    commandBuffer.reset();

    vk::CommandBufferBeginInfo commandBufferBeginInfo {};
    VkCheckResult(commandBuffer.begin(&commandBufferBeginInfo), "Failed to begin recording command buffer!");
    RecordCommands(commandBuffer, swapChainImageIndex, currentResourcesFrame);
    commandBuffer.end();

    vk::Semaphore waitSemaphore = _imageAvailableSemaphores.at(currentResourcesFrame);
    vk::PipelineStageFlags waitStage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    vk::Semaphore signalSemaphore = _renderFinishedSemaphores.at(currentResourcesFrame);

    vk::SubmitInfo submitInfo {};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &waitSemaphore;
    submitInfo.pWaitDstStageMask = &waitStage;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &signalSemaphore;
    VkCheckResult(_vulkanContext->GraphicsQueue().submit(1, &submitInfo, _inFlightFences.at(currentResourcesFrame)), "Failed submitting to graphics queue!");

    vk::SwapchainKHR swapchain = _swapChain->GetSwapChain();
    vk::PresentInfoKHR presentInfo {};
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &signalSemaphore;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &swapchain;
    presentInfo.pImageIndices = &swapChainImageIndex;
    VkCheckResult(_vulkanContext->PresentQueue().presentKHR(&presentInfo), "Failed to present swap chain image!");

    _renderedFrames++;
}

void Renderer::RecordCommands(const vk::CommandBuffer& commandBuffer, uint32_t swapChainImageIndex, uint32_t currentResourceFrame)
{
    VkTransitionImageLayout(commandBuffer, _renderTarget->image, _renderTarget->format,
        vk::ImageLayout::eUndefined, vk::ImageLayout::eGeneral);

    RecordRayTracingCommands(commandBuffer, currentResourceFrame);

    VkTransitionImageLayout(commandBuffer, _renderTarget->image, _renderTarget->format,
        vk::ImageLayout::eGeneral, vk::ImageLayout::eColorAttachmentOptimal);

    RecordImGuiCommands(commandBuffer);

    // No need to transition _renderTarget, as ImGui render pass does it for us
    VkTransitionImageLayout(commandBuffer, _swapChain->GetImage(swapChainImageIndex), _swapChain->GetFormat(),
        vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);

    vk::Extent2D extent = { _windowWidth, _windowHeight };
    VkCopyImageToImage(commandBuffer, _renderTarget->image, _swapChain->GetImage(swapChainImageIndex), extent, extent);

    VkTransitionImageLayout(commandBuffer, _swapChain->GetImage(swapChainImageIndex), _swapChain->GetFormat(),
        vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::ePresentSrcKHR);
}

void Renderer::RecordRayTracingCommands(const vk::CommandBuffer& commandBuffer, uint32_t currentResourceFrame)
{
    commandBuffer.bindPipeline(vk::PipelineBindPoint::eRayTracingKHR, _pipeline);
    commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eRayTracingKHR, _pipelineLayout, 0, _bindlessResources->DescriptorSet(), nullptr);
    commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eRayTracingKHR, _pipelineLayout, 1, _descriptorSet, nullptr);
    commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eRayTracingKHR, _pipelineLayout, 2, _cameraResource->DescriptorSet(currentResourceFrame), nullptr);

    vk::StridedDeviceAddressRegionKHR callableShaderSbtEntry {};
    commandBuffer.traceRaysKHR(_raygenAddressRegion, _missAddressRegion, _hitAddressRegion, callableShaderSbtEntry, _windowWidth, _windowHeight, 1, _vulkanContext->Dldi());
}

void Renderer::RecordImGuiCommands(const vk::CommandBuffer& commandBuffer)
{
    vk::ClearValue clearColor = vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f);

    vk::RenderPassBeginInfo rpInfo = {};
    rpInfo.renderPass = _imguiRenderPass;
    rpInfo.framebuffer = _imguiFramebuffer;
    rpInfo.renderArea.offset = vk::Offset2D { 0, 0 };
    rpInfo.renderArea.extent = _swapChain->GetExtent();
    rpInfo.clearValueCount = 1;
    rpInfo.pClearValues = &clearColor;

    // Begin the render pass
    commandBuffer.beginRenderPass(rpInfo, vk::SubpassContents::eInline);

    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);

    commandBuffer.endRenderPass();
}

void Renderer::UpdateCameraResource(uint32_t currentResourceFrame)
{
    glm::mat4 inverseView = glm::inverse(_flyCamera->ViewMatrix());
    glm::mat4 inverseProjection = glm::inverse(_flyCamera->ProjectionMatrix());

    _cameraResource->Update(currentResourceFrame, inverseView, inverseProjection);
}

void Renderer::InitializeCommandBuffers()
{
    vk::CommandBufferAllocateInfo commandBufferAllocateInfo {};
    commandBufferAllocateInfo.commandPool = _vulkanContext->CommandPool();
    commandBufferAllocateInfo.level = vk::CommandBufferLevel::ePrimary;
    commandBufferAllocateInfo.commandBufferCount = _commandBuffers.size();

    VkCheckResult(_vulkanContext->Device().allocateCommandBuffers(&commandBufferAllocateInfo, _commandBuffers.data()), "Failed allocating command buffer!");
}

void Renderer::InitializeSynchronizationObjects()
{
    vk::SemaphoreCreateInfo semaphoreCreateInfo {};
    vk::FenceCreateInfo fenceCreateInfo {};
    fenceCreateInfo.flags = vk::FenceCreateFlagBits::eSignaled;

    std::string errorMsg { "Failed creating sync object!" };
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
    {
        VkCheckResult(_vulkanContext->Device().createSemaphore(&semaphoreCreateInfo, nullptr, &_imageAvailableSemaphores.at(i)), errorMsg);
        VkCheckResult(_vulkanContext->Device().createSemaphore(&semaphoreCreateInfo, nullptr, &_renderFinishedSemaphores.at(i)), errorMsg);
        VkCheckResult(_vulkanContext->Device().createFence(&fenceCreateInfo, nullptr, &_inFlightFences.at(i)), errorMsg);
    }
}

void Renderer::InitializeRenderTarget()
{
    ImageCreation imageCreation {};
    imageCreation.SetName("Render Target")
        .SetSize(_windowWidth, _windowHeight)
        .SetFormat(_swapChain->GetFormat())
        .SetUsageFlags(vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eColorAttachment);

    _renderTarget = std::make_unique<Image>(imageCreation, _vulkanContext);
}

void Renderer::InitializeDescriptorSets()
{
    std::array<vk::DescriptorSetLayoutBinding, 2> bindingLayouts {};

    vk::DescriptorSetLayoutBinding& imageLayout = bindingLayouts.at(0);
    imageLayout.binding = 0;
    imageLayout.descriptorType = vk::DescriptorType::eStorageImage;
    imageLayout.descriptorCount = 1;
    imageLayout.stageFlags = vk::ShaderStageFlagBits::eRaygenKHR;

    vk::DescriptorSetLayoutBinding& accelerationStructureLayout = bindingLayouts.at(1);
    accelerationStructureLayout.binding = 1;
    accelerationStructureLayout.descriptorType = vk::DescriptorType::eAccelerationStructureKHR;
    accelerationStructureLayout.descriptorCount = 1;
    accelerationStructureLayout.stageFlags = vk::ShaderStageFlagBits::eRaygenKHR;

    vk::DescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo {};
    descriptorSetLayoutCreateInfo.bindingCount = bindingLayouts.size();
    descriptorSetLayoutCreateInfo.pBindings = bindingLayouts.data();
    _descriptorSetLayout = _vulkanContext->Device().createDescriptorSetLayout(descriptorSetLayoutCreateInfo);

    vk::DescriptorSetAllocateInfo descriptorSetAllocateInfo {};
    descriptorSetAllocateInfo.descriptorPool = _vulkanContext->DescriptorPool();
    descriptorSetAllocateInfo.descriptorSetCount = 1;
    descriptorSetAllocateInfo.pSetLayouts = &_descriptorSetLayout;
    _descriptorSet = _vulkanContext->Device().allocateDescriptorSets(descriptorSetAllocateInfo).front();

    vk::DescriptorImageInfo descriptorImageInfo {};
    descriptorImageInfo.imageView = _renderTarget->view;
    descriptorImageInfo.imageLayout = vk::ImageLayout::eGeneral;

    vk::WriteDescriptorSetAccelerationStructureKHR descriptorAccelerationStructureInfo {};
    descriptorAccelerationStructureInfo.accelerationStructureCount = 1;
    const vk::AccelerationStructureKHR tlas = _tlas->Structure();
    descriptorAccelerationStructureInfo.pAccelerationStructures = &tlas;

    std::array<vk::WriteDescriptorSet, 2> descriptorWrites {};

    vk::WriteDescriptorSet& imageWrite = descriptorWrites.at(0);
    imageWrite.dstSet = _descriptorSet;
    imageWrite.dstBinding = 0;
    imageWrite.dstArrayElement = 0;
    imageWrite.descriptorCount = 1;
    imageWrite.descriptorType = vk::DescriptorType::eStorageImage;
    imageWrite.pImageInfo = &descriptorImageInfo;

    vk::WriteDescriptorSet& accelerationStructureWrite = descriptorWrites.at(1);
    accelerationStructureWrite.pNext = &descriptorAccelerationStructureInfo;
    accelerationStructureWrite.dstSet = _descriptorSet;
    accelerationStructureWrite.dstBinding = 1;
    accelerationStructureWrite.dstArrayElement = 0;
    accelerationStructureWrite.descriptorCount = 1;
    accelerationStructureWrite.descriptorType = vk::DescriptorType::eAccelerationStructureKHR;

    _vulkanContext->Device().updateDescriptorSets(static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
}

void Renderer::InitializeRayTracingPipeline()
{
    vk::ShaderModule raygenModule = Shader::CreateShaderModule("shaders/bin/ray_gen.rgen.spv", _vulkanContext->Device());
    vk::ShaderModule missModule = Shader::CreateShaderModule("shaders/bin/miss.rmiss.spv", _vulkanContext->Device());
    vk::ShaderModule chitModule = Shader::CreateShaderModule("shaders/bin/triangle_closest_hit.rchit.spv", _vulkanContext->Device());

    vk::ShaderModule chit2Module = Shader::CreateShaderModule("shaders/bin/hair_closest_hit.rchit.spv", _vulkanContext->Device());
    vk::ShaderModule intModule = Shader::CreateShaderModule("shaders/bin/hair_intersection.rint.spv", _vulkanContext->Device());

    enum StageIndices
    {
        eRaygen,
        eMiss,
        eClosestHit,
        eClosestHit2,
        eIntersection,
        eShaderGroupCount
    };

    std::array<vk::PipelineShaderStageCreateInfo, eShaderGroupCount> shaderStagesCreateInfo {};

    vk::PipelineShaderStageCreateInfo& raygenStage = shaderStagesCreateInfo.at(eRaygen);
    raygenStage.stage = vk::ShaderStageFlagBits::eRaygenKHR;
    raygenStage.module = raygenModule;
    raygenStage.pName = "main";

    vk::PipelineShaderStageCreateInfo& missStage = shaderStagesCreateInfo.at(eMiss);
    missStage.stage = vk::ShaderStageFlagBits::eMissKHR;
    missStage.module = missModule;
    missStage.pName = "main";

    vk::PipelineShaderStageCreateInfo& chitStage = shaderStagesCreateInfo.at(eClosestHit);
    chitStage.stage = vk::ShaderStageFlagBits::eClosestHitKHR;
    chitStage.module = chitModule;
    chitStage.pName = "main";

    vk::PipelineShaderStageCreateInfo& intStage = shaderStagesCreateInfo.at(eClosestHit2);
    intStage.stage = vk::ShaderStageFlagBits::eClosestHitKHR;
    intStage.module = chit2Module;
    intStage.pName = "main";

    vk::PipelineShaderStageCreateInfo& chit2Stage = shaderStagesCreateInfo.at(eIntersection);
    chit2Stage.stage = vk::ShaderStageFlagBits::eIntersectionKHR;
    chit2Stage.module = intModule;
    chit2Stage.pName = "main";

    std::array<vk::RayTracingShaderGroupCreateInfoKHR, 4> shaderGroupsCreateInfo {};

    vk::RayTracingShaderGroupCreateInfoKHR& group1 = shaderGroupsCreateInfo.at(0);
    group1.type = vk::RayTracingShaderGroupTypeKHR::eGeneral;
    group1.generalShader = eRaygen;
    group1.closestHitShader = vk::ShaderUnusedKHR;
    group1.anyHitShader = vk::ShaderUnusedKHR;
    group1.intersectionShader = vk::ShaderUnusedKHR;

    vk::RayTracingShaderGroupCreateInfoKHR& group2 = shaderGroupsCreateInfo.at(1);
    group2.type = vk::RayTracingShaderGroupTypeKHR::eGeneral;
    group2.generalShader = eMiss;
    group2.closestHitShader = vk::ShaderUnusedKHR;
    group2.anyHitShader = vk::ShaderUnusedKHR;
    group2.intersectionShader = vk::ShaderUnusedKHR;

    vk::RayTracingShaderGroupCreateInfoKHR& group3 = shaderGroupsCreateInfo.at(2);
    group3.type = vk::RayTracingShaderGroupTypeKHR::eTrianglesHitGroup;
    group3.generalShader = vk::ShaderUnusedKHR;
    group3.closestHitShader = eClosestHit;
    group3.anyHitShader = vk::ShaderUnusedKHR;
    group3.intersectionShader = vk::ShaderUnusedKHR;

    vk::RayTracingShaderGroupCreateInfoKHR& group4 = shaderGroupsCreateInfo.at(3);
    group4.type = vk::RayTracingShaderGroupTypeKHR::eProceduralHitGroup;
    group4.generalShader = vk::ShaderUnusedKHR;
    group4.closestHitShader = eClosestHit2;
    group4.anyHitShader = vk::ShaderUnusedKHR;
    group4.intersectionShader = eIntersection;

    std::array<vk::DescriptorSetLayout, 3> descriptorSetLayouts { _bindlessResources->DescriptorSetLayout(), _descriptorSetLayout, _cameraResource->DescriptorSetLayout() };

    vk::PipelineLayoutCreateInfo pipelineLayoutCreateInfo {};
    pipelineLayoutCreateInfo.setLayoutCount = descriptorSetLayouts.size();
    pipelineLayoutCreateInfo.pSetLayouts = descriptorSetLayouts.data();
    pipelineLayoutCreateInfo.pushConstantRangeCount = 0;
    pipelineLayoutCreateInfo.pPushConstantRanges = nullptr;
    _pipelineLayout = _vulkanContext->Device().createPipelineLayout(pipelineLayoutCreateInfo);

    vk::PipelineLibraryCreateInfoKHR libraryCreateInfo {};
    libraryCreateInfo.libraryCount = 0;

    vk::RayTracingPipelineCreateInfoKHR pipelineCreateInfo {};
    pipelineCreateInfo.stageCount = static_cast<uint32_t>(shaderStagesCreateInfo.size());
    pipelineCreateInfo.pStages = shaderStagesCreateInfo.data();
    pipelineCreateInfo.groupCount = static_cast<uint32_t>(shaderGroupsCreateInfo.size());
    pipelineCreateInfo.pGroups = shaderGroupsCreateInfo.data();
    pipelineCreateInfo.maxPipelineRayRecursionDepth = _vulkanContext->RayTracingPipelineProperties().maxRayRecursionDepth;
    pipelineCreateInfo.pLibraryInfo = &libraryCreateInfo;
    pipelineCreateInfo.pLibraryInterface = nullptr;
    pipelineCreateInfo.layout = _pipelineLayout;
    pipelineCreateInfo.basePipelineHandle = nullptr;
    pipelineCreateInfo.basePipelineIndex = 0;

    _pipeline = _vulkanContext->Device().createRayTracingPipelineKHR(nullptr, nullptr, pipelineCreateInfo, nullptr, _vulkanContext->Dldi()).value;

    _vulkanContext->Device().destroyShaderModule(raygenModule);
    _vulkanContext->Device().destroyShaderModule(missModule);
    _vulkanContext->Device().destroyShaderModule(chitModule);
    _vulkanContext->Device().destroyShaderModule(chit2Module);
    _vulkanContext->Device().destroyShaderModule(intModule);

    InitializeShaderBindingTable(pipelineCreateInfo);
}

void Renderer::InitializeShaderBindingTable(const vk::RayTracingPipelineCreateInfoKHR& pipelineInfo)
{
    auto AlignedSize = [](uint32_t value, uint32_t alignment)
    { return (value + alignment - 1) & ~(alignment - 1); };

    const vk::PhysicalDeviceRayTracingPipelinePropertiesKHR rayTracingPipelineProperties = _vulkanContext->RayTracingPipelineProperties();
    const uint32_t handleSize = rayTracingPipelineProperties.shaderGroupHandleSize;
    const uint32_t handleSizeAligned = AlignedSize(rayTracingPipelineProperties.shaderGroupHandleSize, rayTracingPipelineProperties.shaderGroupHandleAlignment);
    const uint32_t shaderGroupCount = pipelineInfo.groupCount;
    const vk::DeviceSize sbtSize = shaderGroupCount * handleSizeAligned;

    BufferCreation shaderBindingTableBufferCreation {};
    shaderBindingTableBufferCreation.SetName("Ray Gen Shader Binding Table")
        .SetSize(sbtSize)
        .SetUsageFlags(vk::BufferUsageFlagBits::eShaderBindingTableKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress)
        .SetMemoryUsage(VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE)
        .SetIsMappable(true);
    _raygenSBT = std::make_unique<Buffer>(shaderBindingTableBufferCreation, _vulkanContext);

    shaderBindingTableBufferCreation.SetName("Miss Shader Binding Table");
    _missSBT = std::make_unique<Buffer>(shaderBindingTableBufferCreation, _vulkanContext);

    shaderBindingTableBufferCreation.SetName("Hit Shader Binding Table");
    _hitSBT = std::make_unique<Buffer>(shaderBindingTableBufferCreation, _vulkanContext);

    std::vector<uint8_t> handles = _vulkanContext->Device().getRayTracingShaderGroupHandlesKHR<uint8_t>(_pipeline, 0, shaderGroupCount, sbtSize, _vulkanContext->Dldi());

    memcpy(_raygenSBT->mappedPtr, handles.data(), handleSize);
    memcpy(_missSBT->mappedPtr, handles.data() + handleSizeAligned, handleSize);
    memcpy(_hitSBT->mappedPtr, handles.data() + handleSizeAligned * 2, handleSize * 2);

    _raygenAddressRegion.deviceAddress = _vulkanContext->GetBufferDeviceAddress(_raygenSBT->buffer);
    _raygenAddressRegion.stride = handleSizeAligned;
    _raygenAddressRegion.size = handleSizeAligned;

    _missAddressRegion.deviceAddress = _vulkanContext->GetBufferDeviceAddress(_missSBT->buffer);
    _missAddressRegion.stride = handleSizeAligned;
    _missAddressRegion.size = handleSizeAligned;

    _hitAddressRegion.deviceAddress = _vulkanContext->GetBufferDeviceAddress(_hitSBT->buffer);
    _hitAddressRegion.stride = handleSizeAligned;
    _hitAddressRegion.size = handleSizeAligned * 2;
}

void Renderer::InitializeImGuiPipeline()
{
    InitializeImGuiRenderPass();
    InitializeImGuiFrameBuffer();
}

void Renderer::InitializeImGuiRenderPass()
{
    vk::AttachmentDescription colorAttachment = {};
    colorAttachment.format = _renderTarget->format;
    colorAttachment.samples = vk::SampleCountFlagBits::e1;
    colorAttachment.loadOp = vk::AttachmentLoadOp::eLoad;
    colorAttachment.storeOp = vk::AttachmentStoreOp::eStore;
    colorAttachment.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
    colorAttachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
    colorAttachment.initialLayout = vk::ImageLayout::eColorAttachmentOptimal;
    colorAttachment.finalLayout = vk::ImageLayout::eTransferSrcOptimal;

    vk::AttachmentReference colorAttachmentRef = {};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = vk::ImageLayout::eColorAttachmentOptimal;

    vk::SubpassDescription subpass = {};
    subpass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;

    vk::SubpassDependency dependency = {};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    dependency.dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    dependency.srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
    dependency.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite | vk::AccessFlagBits::eColorAttachmentRead;

    vk::RenderPassCreateInfo renderPassInfo = {};
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    _imguiRenderPass = _vulkanContext->Device().createRenderPass(renderPassInfo);
}

void Renderer::InitializeImGuiFrameBuffer()
{
    vk::FramebufferCreateInfo framebufferInfo = {};
    framebufferInfo.renderPass = _imguiRenderPass;
    framebufferInfo.attachmentCount = 1;
    framebufferInfo.pAttachments = &_renderTarget->view;
    framebufferInfo.width = _swapChain->GetExtent().width;
    framebufferInfo.height = _swapChain->GetExtent().height;
    framebufferInfo.layers = 1;

    _imguiFramebuffer = _vulkanContext->Device().createFramebuffer(framebufferInfo);
}

BLASInput InitializeBLASInput(const std::shared_ptr<Model>& model, const Node& node, const Mesh& mesh, const std::shared_ptr<VulkanContext>& vulkanContext)
{
    BLASInput output {};
    output.type = BLASType::eMesh;
    output.transform = node.GetWorldMatrix();

    vk::DeviceOrHostAddressConstKHR vertexBufferDeviceAddress {};
    vk::DeviceOrHostAddressConstKHR indexBufferDeviceAddress {};
    vertexBufferDeviceAddress.deviceAddress = vulkanContext->GetBufferDeviceAddress(model->vertexBuffer->buffer);
    indexBufferDeviceAddress.deviceAddress = vulkanContext->GetBufferDeviceAddress(model->indexBuffer->buffer) + mesh.firstIndex * sizeof(uint32_t);

    vk::AccelerationStructureGeometryTrianglesDataKHR trianglesData {};
    trianglesData.vertexFormat = vk::Format::eR32G32B32Sfloat;
    trianglesData.vertexData = vertexBufferDeviceAddress;
    trianglesData.maxVertex = model->vertexCount - 1;
    trianglesData.vertexStride = sizeof(Mesh::Vertex);
    trianglesData.indexType = vk::IndexType::eUint32;
    trianglesData.indexData = indexBufferDeviceAddress;
    trianglesData.transformData = {}; // Identity transform

    vk::AccelerationStructureGeometryKHR& accelerationStructureGeometry = output.geometry;
    accelerationStructureGeometry.flags = vk::GeometryFlagBitsKHR::eOpaque;
    accelerationStructureGeometry.geometryType = vk::GeometryTypeKHR::eTriangles;
    accelerationStructureGeometry.geometry.triangles = trianglesData;

    const uint32_t primitiveCount = mesh.indexCount / 3;

    vk::AccelerationStructureBuildRangeInfoKHR& buildRangeInfo = output.info;
    buildRangeInfo.primitiveCount = primitiveCount;
    buildRangeInfo.primitiveOffset = 0;
    buildRangeInfo.firstVertex = 0;
    buildRangeInfo.transformOffset = 0;

    GeometryNodeCreation& nodeCreation = output.node;
    nodeCreation.primitiveBufferDeviceAddress = vertexBufferDeviceAddress.deviceAddress;
    nodeCreation.indexBufferDeviceAddress = indexBufferDeviceAddress.deviceAddress;
    nodeCreation.material = mesh.material;

    return output;
}

BLASInput InitializeBLASInput(const std::shared_ptr<Model>& model, const Node& node, const Hair& hair, const std::shared_ptr<VulkanContext>& vulkanContext)
{
    BLASInput output {};
    output.type = BLASType::eHair;
    output.transform = node.GetWorldMatrix();

    vk::DeviceOrHostAddressConstKHR aabbBufferDeviceAddress {};
    aabbBufferDeviceAddress.deviceAddress = vulkanContext->GetBufferDeviceAddress(model->aabbBuffer->buffer) + hair.firstAabb * sizeof(AABB);

    vk::AccelerationStructureGeometryAabbsDataKHR aabbData {};
    aabbData.data = aabbBufferDeviceAddress;
    aabbData.stride = sizeof(AABB);

    vk::AccelerationStructureGeometryKHR& accelerationStructureGeometry = output.geometry;
    accelerationStructureGeometry.flags = vk::GeometryFlagBitsKHR::eOpaque;
    accelerationStructureGeometry.geometryType = vk::GeometryTypeKHR::eAabbs;
    accelerationStructureGeometry.geometry.aabbs = aabbData;

    const uint32_t primitiveCount = hair.aabbCount;

    vk::AccelerationStructureBuildRangeInfoKHR& buildRangeInfo = output.info;
    buildRangeInfo.primitiveCount = primitiveCount;
    buildRangeInfo.primitiveOffset = 0;
    buildRangeInfo.firstVertex = 0;
    buildRangeInfo.transformOffset = 0;

    vk::DeviceOrHostAddressConstKHR curvePrimitiveBufferDeviceAddress {};
    curvePrimitiveBufferDeviceAddress.deviceAddress = vulkanContext->GetBufferDeviceAddress(model->curveBuffer->buffer);

    GeometryNodeCreation& nodeCreation = output.node;
    nodeCreation.primitiveBufferDeviceAddress = curvePrimitiveBufferDeviceAddress.deviceAddress;
    nodeCreation.material = hair.material;

    return output;
}

BLASInput InitializeBLASInput(const std::shared_ptr<Model>& model, const Node& node, const LSSMesh& lssMesh, const std::shared_ptr<VulkanContext>& vulkanContext)
{
    BLASInput output {};
    output.type = BLASType::eMesh;
    output.transform = node.GetWorldMatrix();

    vk::DeviceOrHostAddressConstKHR positionBufferDeviceAddress {};
    vk::DeviceOrHostAddressConstKHR radiusBufferDeviceAddress {};
    positionBufferDeviceAddress.deviceAddress = vulkanContext->GetBufferDeviceAddress(model->lssPositionBuffer->buffer) + lssMesh.firstVertex * sizeof(glm::vec3);
    radiusBufferDeviceAddress.deviceAddress = vulkanContext->GetBufferDeviceAddress(model->lssRadiusBuffer->buffer) + lssMesh.firstVertex * sizeof(float);

    vk::AccelerationStructureGeometryLinearSweptSpheresDataNV& lssData = output.lssInfo;
    lssData.vertexFormat = vk::Format::eR32G32B32Sfloat;
    lssData.vertexData = positionBufferDeviceAddress;
    lssData.vertexStride = sizeof(glm::vec3);
    lssData.radiusFormat = vk::Format::eR32Sfloat;
    lssData.radiusData = radiusBufferDeviceAddress;
    lssData.radiusStride = sizeof(float);
    lssData.indexType = vk::IndexType::eNoneNV;
    lssData.indexingMode = vk::RayTracingLssIndexingModeNV::eList;
    lssData.endCapsMode = vk::RayTracingLssPrimitiveEndCapsModeNV::eNone;

    vk::AccelerationStructureGeometryKHR& accelerationStructureGeometry = output.geometry;
    accelerationStructureGeometry.flags = vk::GeometryFlagBitsKHR::eOpaque;
    accelerationStructureGeometry.geometryType = vk::GeometryTypeKHR::eLinearSweptSpheresNV;
    accelerationStructureGeometry.pNext = &output.lssInfo;

    const uint32_t primitiveCount = lssMesh.vertexCount / 2;

    vk::AccelerationStructureBuildRangeInfoKHR& buildRangeInfo = output.info;
    buildRangeInfo.primitiveCount = primitiveCount;
    buildRangeInfo.primitiveOffset = 0;
    buildRangeInfo.firstVertex = 0;
    buildRangeInfo.transformOffset = 0;

    GeometryNodeCreation& nodeCreation = output.node;
    nodeCreation.material = lssMesh.material;

    return output;
}

void Renderer::InitializeBLAS()
{
    for (const auto& model : _models)
    {
        std::shared_ptr<SceneGraph> sceneGraph = model->sceneGraph;

        for (const auto& node : sceneGraph->nodes)
        {
            for (const auto mesh : node.meshes)
            {
                BLASInput input = InitializeBLASInput(model, node, sceneGraph->meshes[mesh], _vulkanContext);
                _blases.emplace_back(input, _bindlessResources, _vulkanContext);
            }

            for (const auto hair : node.hairs)
            {
                BLASInput input = InitializeBLASInput(model, node, sceneGraph->hairs[hair], _vulkanContext);
                _blases.emplace_back(input, _bindlessResources, _vulkanContext);
            }

            for (const auto lssMesh : node.lssMeshes)
            {
                BLASInput input = InitializeBLASInput(model, node, sceneGraph->lssMeshes[lssMesh], _vulkanContext);
                _blases.emplace_back(input, _bindlessResources, _vulkanContext);
            }
        }
    }
}
