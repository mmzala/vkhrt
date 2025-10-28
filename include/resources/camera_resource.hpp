#pragma once
#include "vk_common.hpp"
#include "gpu_resources.hpp"
#include <vulkan/vulkan.hpp>
#include <glm\glm.hpp>

struct CameraUniformData
{
    glm::mat4 viewInverse {};
    glm::mat4 projInverse {};
};

class CameraResource
{
public:
    CameraResource(const std::shared_ptr<VulkanContext>& vulkanContext);
    ~CameraResource();
    void Update(uint32_t frameIndex, const glm::mat4& viewInverse, const glm::mat4& projInverse);
    vk::DescriptorSet DescriptorSet(uint32_t frameIndex) { return _descriptorSets[frameIndex]; };
    vk::DescriptorSetLayout DescriptorSetLayout() { return _descriptorSetLayout; };

private:
    void CreateBuffers();
    void CreateDescriptorSetLayout();
    void CreateDescriptorSets();

    std::shared_ptr<VulkanContext> _vulkanContext;
    vk::DescriptorSetLayout _descriptorSetLayout;
    std::array<vk::DescriptorSet, MAX_FRAMES_IN_FLIGHT> _descriptorSets;
    std::array<std::unique_ptr<Buffer>, MAX_FRAMES_IN_FLIGHT> _buffers;
};
