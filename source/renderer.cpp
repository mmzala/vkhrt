#include "renderer.hpp"
#include "model_loader.hpp"
#include "resources/bindless_resources.hpp"
#include "shader.hpp"
#include "single_time_commands.hpp"
#include "swap_chain.hpp"
#include "top_level_acceleration_structure.hpp"
#include "vulkan_context.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/matrix_decompose.hpp>

Renderer::Renderer(const VulkanInitInfo& initInfo, const std::shared_ptr<VulkanContext>& vulkanContext)
    : _vulkanContext(vulkanContext)
    , _windowWidth(initInfo.width)
    , _windowHeight(initInfo.height)
{
    _swapChain = std::make_unique<SwapChain>(vulkanContext, glm::uvec2 { initInfo.width, initInfo.height });
    InitializeCommandBuffers();
    InitializeSynchronizationObjects();
    InitializeRenderTarget();

    _bindlessResources = std::make_shared<BindlessResources>(_vulkanContext);
    _modelLoader = std::make_unique<ModelLoader>(_bindlessResources, _vulkanContext);

    const std::vector<std::string> scene = {
        "assets/claire/Claire_HairMain_HQ.gltf",
    };
    for (const auto& modelPath : scene)
    {
        _models.emplace_back(_modelLoader->LoadFromFile(modelPath));
    }
    InitializeBLAS();

    _tlas = std::make_unique<TopLevelAccelerationStructure>(_blases, _bindlessResources, _vulkanContext);
    _bindlessResources->UpdateDescriptorSet();

    InitializeCamera();
    InitializeDescriptorSets();
    InitializePipeline();
    InitializeShaderBindingTable();
}

Renderer::~Renderer()
{
    _vulkanContext->Device().destroyPipeline(_pipeline);
    _vulkanContext->Device().destroyPipelineLayout(_pipelineLayout);

    _vulkanContext->Device().destroyDescriptorSetLayout(_descriptorSetLayout);
    _vulkanContext->Device().destroyDescriptorPool(_descriptorPool);

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
    RecordCommands(commandBuffer, swapChainImageIndex);
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

void Renderer::RecordCommands(const vk::CommandBuffer& commandBuffer, uint32_t swapChainImageIndex)
{
    VkTransitionImageLayout(commandBuffer, _renderTarget->image, _renderTarget->format,
        vk::ImageLayout::eUndefined, vk::ImageLayout::eGeneral);

    commandBuffer.bindPipeline(vk::PipelineBindPoint::eRayTracingKHR, _pipeline);
    commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eRayTracingKHR, _pipelineLayout, 0, _bindlessResources->DescriptorSet(), nullptr);
    commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eRayTracingKHR, _pipelineLayout, 1, _descriptorSet, nullptr);

    vk::StridedDeviceAddressRegionKHR callableShaderSbtEntry {};
    commandBuffer.traceRaysKHR(_raygenAddressRegion, _missAddressRegion, _hitAddressRegion, callableShaderSbtEntry, _windowWidth, _windowHeight, 1, _vulkanContext->Dldi());

    VkTransitionImageLayout(commandBuffer, _swapChain->GetImage(swapChainImageIndex), _swapChain->GetFormat(),
        vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);
    VkTransitionImageLayout(commandBuffer, _renderTarget->image, _renderTarget->format,
        vk::ImageLayout::eGeneral, vk::ImageLayout::eTransferSrcOptimal);

    vk::Extent2D extent = { _windowWidth, _windowHeight };
    VkCopyImageToImage(commandBuffer, _renderTarget->image, _swapChain->GetImage(swapChainImageIndex), extent, extent);

    VkTransitionImageLayout(commandBuffer, _swapChain->GetImage(swapChainImageIndex), _swapChain->GetFormat(),
        vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::ePresentSrcKHR);
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
        .SetUsageFlags(vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eStorage);

    _renderTarget = std::make_unique<Image>(imageCreation, _vulkanContext);
}

void Renderer::InitializeCamera()
{
    constexpr float fov = glm::radians(60.0f);
    const float aspectRatio = _windowWidth / static_cast<float>(_windowHeight);

    glm::mat4 projection = glm::perspectiveRH_ZO(fov, aspectRatio, 0.1f, 1000.0f);
    projection[1][1] *= -1; // Inverting Y for Vulkan (not needed with perspectiveVK)

    CameraUniformData cameraData {};
    cameraData.projInverse = glm::inverse(projection);
    cameraData.viewInverse = glm::inverse(glm::lookAt(glm::vec3(15.0f, 150.0f, 25.0f), glm::vec3(-6.0f, 151.0f, -1.2f), glm::vec3(0.0f, 1.0f, 0.0f)));

    constexpr vk::DeviceSize uniformBufferSize = sizeof(CameraUniformData);
    BufferCreation uniformBufferCreation {};
    uniformBufferCreation.SetName("Camera Uniform Buffer")
        .SetUsageFlags(vk::BufferUsageFlagBits::eUniformBuffer)
        .SetMemoryUsage(VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE)
        .SetIsMappable(true)
        .SetSize(uniformBufferSize);
    _uniformBuffer = std::make_unique<Buffer>(uniformBufferCreation, _vulkanContext);
    memcpy(_uniformBuffer->mappedPtr, &cameraData, uniformBufferSize);
}

void Renderer::InitializeDescriptorSets()
{
    std::array<vk::DescriptorSetLayoutBinding, 3> bindingLayouts {};

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

    vk::DescriptorSetLayoutBinding& cameraLayout = bindingLayouts.at(2);
    cameraLayout.binding = 2;
    cameraLayout.descriptorType = vk::DescriptorType::eUniformBuffer;
    cameraLayout.descriptorCount = 1;
    cameraLayout.stageFlags = vk::ShaderStageFlagBits::eRaygenKHR;

    vk::DescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo {};
    descriptorSetLayoutCreateInfo.bindingCount = bindingLayouts.size();
    descriptorSetLayoutCreateInfo.pBindings = bindingLayouts.data();
    _descriptorSetLayout = _vulkanContext->Device().createDescriptorSetLayout(descriptorSetLayoutCreateInfo);

    std::array<vk::DescriptorPoolSize, 3> poolSizes {};

    vk::DescriptorPoolSize& imagePoolSize = poolSizes.at(0);
    imagePoolSize.type = vk::DescriptorType::eStorageImage;
    imagePoolSize.descriptorCount = 1;

    vk::DescriptorPoolSize& accelerationStructureSize = poolSizes.at(1);
    accelerationStructureSize.type = vk::DescriptorType::eAccelerationStructureKHR;
    accelerationStructureSize.descriptorCount = 1;

    vk::DescriptorPoolSize& cameraSize = poolSizes.at(2);
    cameraSize.type = vk::DescriptorType::eUniformBuffer;
    cameraSize.descriptorCount = 1;

    vk::DescriptorPoolCreateInfo descriptorPoolCreateInfo {};
    descriptorPoolCreateInfo.maxSets = 1;
    descriptorPoolCreateInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    descriptorPoolCreateInfo.pPoolSizes = poolSizes.data();
    _descriptorPool = _vulkanContext->Device().createDescriptorPool(descriptorPoolCreateInfo);

    vk::DescriptorSetAllocateInfo descriptorSetAllocateInfo {};
    descriptorSetAllocateInfo.descriptorPool = _descriptorPool;
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

    vk::DescriptorBufferInfo descriptorBufferInfo {};
    descriptorBufferInfo.buffer = _uniformBuffer->buffer;
    descriptorBufferInfo.offset = 0;
    descriptorBufferInfo.range = sizeof(CameraUniformData);

    std::array<vk::WriteDescriptorSet, 3> descriptorWrites {};

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

    vk::WriteDescriptorSet& uniformBufferWrite = descriptorWrites.at(2);
    uniformBufferWrite.dstSet = _descriptorSet;
    uniformBufferWrite.dstBinding = 2;
    uniformBufferWrite.dstArrayElement = 0;
    uniformBufferWrite.descriptorCount = 1;
    uniformBufferWrite.descriptorType = vk::DescriptorType::eUniformBuffer;
    uniformBufferWrite.pBufferInfo = &descriptorBufferInfo;

    _vulkanContext->Device().updateDescriptorSets(static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
}

void Renderer::InitializePipeline()
{
    vk::ShaderModule raygenModule = Shader::CreateShaderModule("shaders/bin/ray_gen.rgen.spv", _vulkanContext->Device());
    vk::ShaderModule missModule = Shader::CreateShaderModule("shaders/bin/miss.rmiss.spv", _vulkanContext->Device());
    vk::ShaderModule chitModule = Shader::CreateShaderModule("shaders/bin/closest_hit.rchit.spv", _vulkanContext->Device());

    std::array<vk::PipelineShaderStageCreateInfo, 3> shaderStagesCreateInfo {};

    vk::PipelineShaderStageCreateInfo& raygenStage = shaderStagesCreateInfo.at(0);
    raygenStage.stage = vk::ShaderStageFlagBits::eRaygenKHR;
    raygenStage.module = raygenModule;
    raygenStage.pName = "main";

    vk::PipelineShaderStageCreateInfo& missStage = shaderStagesCreateInfo.at(1);
    missStage.stage = vk::ShaderStageFlagBits::eMissKHR;
    missStage.module = missModule;
    missStage.pName = "main";

    vk::PipelineShaderStageCreateInfo& chitStage = shaderStagesCreateInfo.at(2);
    chitStage.stage = vk::ShaderStageFlagBits::eClosestHitKHR;
    chitStage.module = chitModule;
    chitStage.pName = "main";

    std::array<vk::RayTracingShaderGroupCreateInfoKHR, 3> shaderGroupsCreateInfo {};

    vk::RayTracingShaderGroupCreateInfoKHR& group1 = shaderGroupsCreateInfo.at(0);
    group1.type = vk::RayTracingShaderGroupTypeKHR::eGeneral;
    group1.generalShader = 0;
    group1.closestHitShader = vk::ShaderUnusedKHR;
    group1.anyHitShader = vk::ShaderUnusedKHR;
    group1.intersectionShader = vk::ShaderUnusedKHR;

    vk::RayTracingShaderGroupCreateInfoKHR& group2 = shaderGroupsCreateInfo.at(1);
    group2.type = vk::RayTracingShaderGroupTypeKHR::eGeneral;
    group2.generalShader = 1;
    group2.closestHitShader = vk::ShaderUnusedKHR;
    group2.anyHitShader = vk::ShaderUnusedKHR;
    group2.intersectionShader = vk::ShaderUnusedKHR;

    vk::RayTracingShaderGroupCreateInfoKHR& group3 = shaderGroupsCreateInfo.at(2);
    group3.type = vk::RayTracingShaderGroupTypeKHR::eTrianglesHitGroup;
    group3.generalShader = vk::ShaderUnusedKHR;
    group3.closestHitShader = 2;
    group3.anyHitShader = vk::ShaderUnusedKHR;
    group3.intersectionShader = vk::ShaderUnusedKHR;

    std::array<vk::DescriptorSetLayout, 2> descriptorSetLayouts { _bindlessResources->DescriptorSetLayout(), _descriptorSetLayout };

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
}

void Renderer::InitializeShaderBindingTable()
{
    auto AlignedSize = [](uint32_t value, uint32_t alignment)
    { return (value + alignment - 1) & ~(alignment - 1); };

    const vk::PhysicalDeviceRayTracingPipelinePropertiesKHR rayTracingPipelineProperties = _vulkanContext->RayTracingPipelineProperties();
    const uint32_t handleSize = rayTracingPipelineProperties.shaderGroupHandleSize;
    const uint32_t handleSizeAligned = AlignedSize(rayTracingPipelineProperties.shaderGroupHandleSize, rayTracingPipelineProperties.shaderGroupHandleAlignment);
    const uint32_t shaderGroupCount = 3; // TODO: Get this from somewhere
    vk::DeviceSize sbtSize = shaderGroupCount * handleSizeAligned;

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
    memcpy(_hitSBT->mappedPtr, handles.data() + handleSizeAligned * 2, handleSize);

    _raygenAddressRegion.deviceAddress = _vulkanContext->GetBufferDeviceAddress(_raygenSBT->buffer);
    _raygenAddressRegion.stride = handleSizeAligned;
    _raygenAddressRegion.size = handleSizeAligned;

    _missAddressRegion.deviceAddress = _vulkanContext->GetBufferDeviceAddress(_missSBT->buffer);
    _missAddressRegion.stride = handleSizeAligned;
    _missAddressRegion.size = handleSizeAligned;

    _hitAddressRegion.deviceAddress = _vulkanContext->GetBufferDeviceAddress(_hitSBT->buffer);
    _hitAddressRegion.stride = handleSizeAligned;
    _hitAddressRegion.size = handleSizeAligned;
}

BLASInput InitializeBLASInput(const std::shared_ptr<Model>& model, const Node& node, const Mesh& mesh, const std::shared_ptr<VulkanContext>& vulkanContext)
{
    BLASInput output {};
    output.transform = node.GetWorldMatrix();

    vk::DeviceOrHostAddressConstKHR vertexBufferDeviceAddress {};
    vk::DeviceOrHostAddressConstKHR indexBufferDeviceAddress {};
    vertexBufferDeviceAddress.deviceAddress = vulkanContext->GetBufferDeviceAddress(model->vertexBuffer->buffer);
    indexBufferDeviceAddress.deviceAddress = vulkanContext->GetBufferDeviceAddress(model->indexBuffer->buffer) + mesh.firstIndex * sizeof(uint32_t);

    vk::AccelerationStructureGeometryTrianglesDataKHR trianglesData {};
    trianglesData.vertexFormat = vk::Format::eR32G32B32Sfloat;
    trianglesData.vertexData = vertexBufferDeviceAddress;
    trianglesData.maxVertex = model->verticesCount - 1;
    trianglesData.vertexStride = sizeof(Mesh::Vertex);
    trianglesData.indexType = vk::IndexType::eUint32;
    trianglesData.indexData = indexBufferDeviceAddress;
    trianglesData.transformData = {}; // Identity transform

    vk::AccelerationStructureGeometryKHR& accelerationStructureGeometry = output.geometry;
    accelerationStructureGeometry.flags = vk::GeometryFlagBitsKHR::eOpaque;
    accelerationStructureGeometry.geometryType = vk::GeometryTypeKHR::eTriangles;
    accelerationStructureGeometry.geometry.triangles = trianglesData;

    uint32_t primitiveCount = mesh.indexCount / 3;

    vk::AccelerationStructureBuildRangeInfoKHR& buildRangeInfo = output.info;
    buildRangeInfo.primitiveCount = primitiveCount;
    buildRangeInfo.primitiveOffset = 0;
    buildRangeInfo.firstVertex = 0;
    buildRangeInfo.transformOffset = 0;

    GeometryNodeCreation& nodeCreation = output.node;
    nodeCreation.vertexBufferDeviceAddress = vertexBufferDeviceAddress.deviceAddress;
    nodeCreation.indexBufferDeviceAddress = indexBufferDeviceAddress.deviceAddress;
    nodeCreation.material = mesh.material;

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
        }
    }
}
