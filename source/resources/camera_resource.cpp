#include "resources/camera_resource.hpp"

CameraResource::CameraResource(const std::shared_ptr<VulkanContext>& vulkanContext)
    : _vulkanContext(vulkanContext)
{
    CreateBuffers();
    CreateDescriptorSetLayout();
    CreateDescriptorSets();
}

CameraResource::~CameraResource()
{
    _vulkanContext->Device().destroyDescriptorSetLayout(_descriptorSetLayout);
    _vulkanContext->Device().destroyDescriptorPool(_descriptorPool);
}

void CameraResource::Update(uint32_t frameIndex, const glm::mat4& viewInverse, const glm::mat4& projInverse)
{
    CameraUniformData data {};
    data.projInverse = projInverse;
    data.viewInverse = viewInverse;
    memcpy(_buffers[frameIndex]->mappedPtr, &data, sizeof(CameraUniformData));
}

void CameraResource::CreateBuffers()
{
    constexpr vk::DeviceSize uniformBufferSize = sizeof(CameraUniformData);
    uint32_t bufferIndex = 0;

    for (auto& buffer : _buffers)
    {
        std::string name = "Camera Uniform Buffer " + std::to_string(bufferIndex);
        bufferIndex++;

        BufferCreation uniformBufferCreation {};
        uniformBufferCreation.SetName(name)
            .SetUsageFlags(vk::BufferUsageFlagBits::eUniformBuffer)
            .SetMemoryUsage(VMA_MEMORY_USAGE_AUTO)
            .SetIsMappable(true)
            .SetSize(uniformBufferSize);
        buffer = std::make_unique<Buffer>(uniformBufferCreation, _vulkanContext);
    }
}

void CameraResource::CreateDescriptorSetLayout()
{
    vk::DescriptorSetLayoutBinding cameraLayout {};
    cameraLayout.binding = 0;
    cameraLayout.descriptorType = vk::DescriptorType::eUniformBuffer;
    cameraLayout.descriptorCount = 1;
    cameraLayout.stageFlags = vk::ShaderStageFlagBits::eRaygenKHR;

    vk::DescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo {};
    descriptorSetLayoutCreateInfo.bindingCount = 1;
    descriptorSetLayoutCreateInfo.pBindings = &cameraLayout;
    _descriptorSetLayout = _vulkanContext->Device().createDescriptorSetLayout(descriptorSetLayoutCreateInfo);
}

void CameraResource::CreateDescriptorSets()
{
    vk::DescriptorPoolSize cameraSize {};
    cameraSize.type = vk::DescriptorType::eUniformBuffer;
    cameraSize.descriptorCount = 1;

    vk::DescriptorPoolCreateInfo descriptorPoolCreateInfo {};
    descriptorPoolCreateInfo.maxSets = _descriptorSets.size();
    descriptorPoolCreateInfo.poolSizeCount = 1;
    descriptorPoolCreateInfo.pPoolSizes = &cameraSize;
    _descriptorPool = _vulkanContext->Device().createDescriptorPool(descriptorPoolCreateInfo);

    std::array<vk::DescriptorSetLayout, MAX_FRAMES_IN_FLIGHT> layouts {};
    std::for_each(layouts.begin(), layouts.end(), [this](auto& l)
        { l = _descriptorSetLayout; });

    vk::DescriptorSetAllocateInfo descriptorSetAllocateInfo {};
    descriptorSetAllocateInfo.descriptorPool = _descriptorPool;
    descriptorSetAllocateInfo.descriptorSetCount = _descriptorSets.size();
    descriptorSetAllocateInfo.pSetLayouts = layouts.data();
    VkCheckResult(_vulkanContext->Device().allocateDescriptorSets(&descriptorSetAllocateInfo, _descriptorSets.data()), "Failed to allocate camera descriptor sets");

    for (uint32_t i = 0; i < _descriptorSets.size(); ++i)
    {
        vk::DescriptorBufferInfo descriptorBufferInfo {};
        descriptorBufferInfo.buffer = _buffers[i]->buffer;
        descriptorBufferInfo.offset = 0;
        descriptorBufferInfo.range = sizeof(CameraUniformData);

        vk::WriteDescriptorSet uniformBufferWrite {};
        uniformBufferWrite.dstSet = _descriptorSets[i];
        uniformBufferWrite.dstBinding = 0;
        uniformBufferWrite.dstArrayElement = 0;
        uniformBufferWrite.descriptorCount = 1;
        uniformBufferWrite.descriptorType = vk::DescriptorType::eUniformBuffer;
        uniformBufferWrite.pBufferInfo = &descriptorBufferInfo;

        _vulkanContext->Device().updateDescriptorSets(1, &uniformBufferWrite, 0, nullptr);
    }
}
