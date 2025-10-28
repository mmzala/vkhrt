#pragma once
#include <vulkan/vulkan.hpp>
#include <glm/mat4x4.hpp>
#include "vulkan_context.hpp"

constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 3;

struct ImageLayoutTransitionState
{
    vk::PipelineStageFlags2 pipelineStage {};
    vk::AccessFlags2 accessFlags {};
};

void VkCheckResult(vk::Result result, std::string_view message);
void VkCheckResult(VkResult result, std::string_view message);
void VkCheckResult(VkResult result);

[[nodiscard]] bool VkHasStencilComponent(vk::Format format);
[[nodiscard]] ImageLayoutTransitionState VkGetImageLayoutTransitionSourceState(vk::ImageLayout sourceLayout);
[[nodiscard]] ImageLayoutTransitionState VkGetImageLayoutTransitionDestinationState(vk::ImageLayout destinationLayout);
void VkInitializeImageMemoryBarrier(vk::ImageMemoryBarrier2& barrier, vk::Image image, vk::Format format, vk::ImageLayout oldLayout, vk::ImageLayout newLayout, uint32_t numLayers = 1, uint32_t mipLevel = 0, uint32_t mipCount = 1, vk::ImageAspectFlagBits imageAspect = vk::ImageAspectFlagBits::eColor);
void VkTransitionImageLayout(vk::CommandBuffer commandBuffer, vk::Image image, vk::Format format, vk::ImageLayout oldLayout, vk::ImageLayout newLayout, uint32_t numLayers = 1, uint32_t mipLevel = 0, uint32_t mipCount = 1, vk::ImageAspectFlagBits imageAspect = vk::ImageAspectFlagBits::eColor);
void VkCopyImageToImage(vk::CommandBuffer commandBuffer, vk::Image srcImage, vk::Image dstImage, vk::Extent2D srcSize, vk::Extent2D dstSize);
void VkCopyBufferToImage(vk::CommandBuffer commandBuffer, vk::Buffer buffer, vk::Image image, uint32_t width, uint32_t height);
void VkCopyBufferToBuffer(vk::CommandBuffer commandBuffer, vk::Buffer srcBuffer, vk::Buffer dstBuffer, vk::DeviceSize size, uint32_t offset = 0);
VkTransformMatrixKHR VkGLMToTransformMatrixKHR(const glm::mat4& matrix);

template <typename T>
static void VkNameObject(T object, std::string_view name, const std::shared_ptr<VulkanContext>& context)
{
#if defined(NDEBUG)
    return;
#endif

    vk::DebugUtilsObjectNameInfoEXT nameInfo {};
    nameInfo.pObjectName = name.data();
    nameInfo.objectType = object.objectType;
    nameInfo.objectHandle = reinterpret_cast<uint64_t>(static_cast<typename T::CType>(object));

    VkCheckResult(context->Device().setDebugUtilsObjectNameEXT(&nameInfo, context->Dldi()), "Failed debug naming object!");
}
