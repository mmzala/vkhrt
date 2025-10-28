#include "vk_common.hpp"
#include <spdlog/spdlog.h>
#include <unordered_map>

void VkCheckResult(vk::Result result, std::string_view message)
{
    if (result == vk::Result::eSuccess)
    {
        return;
    }

    spdlog::error("[VULKAN] {}", message);

    abort();
}

void VkCheckResult(VkResult result, std::string_view message)
{
    VkCheckResult(static_cast<vk::Result>(result), message);
}

void VkCheckResult(VkResult result)
{
    VkCheckResult(result, {});
}

bool VkHasStencilComponent(vk::Format format)
{
    return format == vk::Format::eD32SfloatS8Uint || format == vk::Format::eD24UnormS8Uint;
}

ImageLayoutTransitionState VkGetImageLayoutTransitionSourceState(vk::ImageLayout sourceLayout)
{
    static const std::unordered_map<vk::ImageLayout, ImageLayoutTransitionState> sourceStateMap = {
        { vk::ImageLayout::eUndefined,
            { .pipelineStage = vk::PipelineStageFlagBits2::eTopOfPipe,
                .accessFlags = vk::AccessFlags2 { 0 } } },
        { vk::ImageLayout::eTransferDstOptimal,
            { .pipelineStage = vk::PipelineStageFlagBits2::eTransfer,
                .accessFlags = vk::AccessFlagBits2::eTransferWrite } },
        { vk::ImageLayout::eTransferSrcOptimal,
            { .pipelineStage = vk::PipelineStageFlagBits2::eTransfer,
                .accessFlags = vk::AccessFlagBits2::eTransferWrite } },
        { vk::ImageLayout::eShaderReadOnlyOptimal,
            { .pipelineStage = vk::PipelineStageFlagBits2::eFragmentShader,
                .accessFlags = vk::AccessFlagBits2::eShaderRead } },
        { vk::ImageLayout::eColorAttachmentOptimal,
            { .pipelineStage = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                .accessFlags = vk::AccessFlagBits2::eColorAttachmentWrite } },
        { vk::ImageLayout::eDepthStencilAttachmentOptimal,
            { .pipelineStage = vk::PipelineStageFlagBits2::eLateFragmentTests,
                .accessFlags = vk::AccessFlagBits2::eDepthStencilAttachmentWrite } },
        { vk::ImageLayout::eGeneral,
            { .pipelineStage = vk::PipelineStageFlagBits2::eRayTracingShaderKHR,
                .accessFlags = vk::AccessFlagBits2::eShaderWrite | vk::AccessFlagBits2::eMemoryWrite } }
    };

    auto it = sourceStateMap.find(sourceLayout);
    if (it == sourceStateMap.end())
    {
        spdlog::error("[VULKAN] Unsupported source state for image layout transition!");
        return ImageLayoutTransitionState {};
    }

    return it->second;
}

ImageLayoutTransitionState VkGetImageLayoutTransitionDestinationState(vk::ImageLayout destinationLayout)
{
    static const std::unordered_map<vk::ImageLayout, ImageLayoutTransitionState> destinationStateMap = {
        { vk::ImageLayout::eTransferDstOptimal,
            { .pipelineStage = vk::PipelineStageFlagBits2::eTransfer,
                .accessFlags = vk::AccessFlagBits2::eTransferWrite } },
        { vk::ImageLayout::eTransferSrcOptimal,
            { .pipelineStage = vk::PipelineStageFlagBits2::eTransfer,
                .accessFlags = vk::AccessFlagBits2::eTransferRead } },
        { vk::ImageLayout::eShaderReadOnlyOptimal,
            { .pipelineStage = vk::PipelineStageFlagBits2::eFragmentShader,
                .accessFlags = vk::AccessFlagBits2::eShaderRead } },
        { vk::ImageLayout::eColorAttachmentOptimal,
            { .pipelineStage = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                .accessFlags = vk::AccessFlagBits2::eColorAttachmentWrite } },
        { vk::ImageLayout::eDepthStencilAttachmentOptimal,
            { .pipelineStage = vk::PipelineStageFlagBits2::eEarlyFragmentTests,
                .accessFlags = vk::AccessFlagBits2::eDepthStencilAttachmentRead | vk::AccessFlagBits2::eDepthStencilAttachmentWrite } },
        { vk::ImageLayout::ePresentSrcKHR,
            { .pipelineStage = vk::PipelineStageFlagBits2::eBottomOfPipe,
                .accessFlags = vk::AccessFlags2 { 0 } } },
        { vk::ImageLayout::eDepthStencilReadOnlyOptimal,
            { .pipelineStage = vk::PipelineStageFlagBits2::eEarlyFragmentTests,
                .accessFlags = vk::AccessFlagBits2::eDepthStencilAttachmentRead } },
        { vk::ImageLayout::eGeneral,
            { .pipelineStage = vk::PipelineStageFlagBits2::eRayTracingShaderKHR,
                .accessFlags = vk::AccessFlagBits2::eShaderRead | vk::AccessFlagBits2::eMemoryRead } },
    };

    auto it = destinationStateMap.find(destinationLayout);
    if (it == destinationStateMap.end())
    {
        spdlog::error("[VULKAN] Unsupported destination state for image layout transition!");
        return ImageLayoutTransitionState {};
    }

    return it->second;
}

void VkInitializeImageMemoryBarrier(vk::ImageMemoryBarrier2& barrier, vk::Image image, vk::Format format, vk::ImageLayout oldLayout, vk::ImageLayout newLayout, uint32_t numLayers, uint32_t mipLevel, uint32_t mipCount, vk::ImageAspectFlagBits imageAspect)
{
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = vk::QueueFamilyIgnored;
    barrier.dstQueueFamilyIndex = vk::QueueFamilyIgnored;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = imageAspect;
    barrier.subresourceRange.baseMipLevel = mipLevel;
    barrier.subresourceRange.levelCount = mipCount;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = numLayers;

    if (newLayout == vk::ImageLayout::eDepthStencilAttachmentOptimal)
    {
        barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eDepth;
        if (VkHasStencilComponent(format))
        {
            barrier.subresourceRange.aspectMask |= vk::ImageAspectFlagBits::eStencil;
        }
    }

    const ImageLayoutTransitionState sourceState = VkGetImageLayoutTransitionSourceState(oldLayout);
    const ImageLayoutTransitionState destinationState = VkGetImageLayoutTransitionDestinationState(newLayout);

    barrier.srcStageMask = sourceState.pipelineStage;
    barrier.srcAccessMask = sourceState.accessFlags;
    barrier.dstStageMask = destinationState.pipelineStage;
    barrier.dstAccessMask = destinationState.accessFlags;
}

void VkTransitionImageLayout(vk::CommandBuffer commandBuffer, vk::Image image, vk::Format format, vk::ImageLayout oldLayout, vk::ImageLayout newLayout, uint32_t numLayers, uint32_t mipLevel, uint32_t mipCount, vk::ImageAspectFlagBits imageAspect)
{
    vk::ImageMemoryBarrier2 barrier {};
    VkInitializeImageMemoryBarrier(barrier, image, format, oldLayout, newLayout, numLayers, mipLevel, mipCount, imageAspect);

    vk::DependencyInfo dependencyInfo {};
    dependencyInfo.setImageMemoryBarrierCount(1)
        .setPImageMemoryBarriers(&barrier);

    commandBuffer.pipelineBarrier2(dependencyInfo);
}

void VkCopyImageToImage(vk::CommandBuffer commandBuffer, vk::Image srcImage, vk::Image dstImage, vk::Extent2D srcSize, vk::Extent2D dstSize)
{
    vk::ImageBlit2 region {};

    region.srcOffsets[1].x = srcSize.width;
    region.srcOffsets[1].y = srcSize.height;
    region.srcOffsets[1].z = 1;

    region.dstOffsets[1].x = dstSize.width;
    region.dstOffsets[1].y = dstSize.height;
    region.dstOffsets[1].z = 1;

    region.srcSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
    region.srcSubresource.baseArrayLayer = 0;
    region.srcSubresource.layerCount = 1;
    region.srcSubresource.mipLevel = 0;
    region.dstSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
    region.dstSubresource.baseArrayLayer = 0;
    region.dstSubresource.layerCount = 1;
    region.dstSubresource.mipLevel = 0;

    vk::BlitImageInfo2 blitInfo {};
    blitInfo.sType = vk::StructureType::eBlitImageInfo2;
    blitInfo.pNext = nullptr;
    blitInfo.dstImage = dstImage;
    blitInfo.dstImageLayout = vk::ImageLayout::eTransferDstOptimal;
    blitInfo.srcImage = srcImage;
    blitInfo.srcImageLayout = vk::ImageLayout::eTransferSrcOptimal;
    blitInfo.filter = vk::Filter::eLinear;
    blitInfo.regionCount = 1;
    blitInfo.pRegions = &region;

    commandBuffer.blitImage2(&blitInfo);
}

void VkCopyBufferToImage(vk::CommandBuffer commandBuffer, vk::Buffer buffer, vk::Image image, uint32_t width, uint32_t height)
{
    vk::BufferImageCopy region {};
    region.bufferImageHeight = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = vk::Offset3D { 0, 0, 0 };
    region.imageExtent = vk::Extent3D { width, height, 1 };

    commandBuffer.copyBufferToImage(buffer, image, vk::ImageLayout::eTransferDstOptimal, 1, &region);
}

void VkCopyBufferToBuffer(vk::CommandBuffer commandBuffer, vk::Buffer srcBuffer, vk::Buffer dstBuffer, vk::DeviceSize size, uint32_t offset)
{
    vk::BufferCopy copyRegion {};
    copyRegion.srcOffset = 0;
    copyRegion.dstOffset = offset;
    copyRegion.size = size;
    commandBuffer.copyBuffer(srcBuffer, dstBuffer, 1, &copyRegion);
}

VkTransformMatrixKHR VkGLMToTransformMatrixKHR(const glm::mat4& matrix)
{
    // VkTransformMatrixKHR uses a row-major memory layout, while glm::mat4 uses a column-major memory layout
    const glm::mat4 temp = glm::transpose(matrix);
    VkTransformMatrixKHR out {};
    memcpy(&out, &temp, sizeof(VkTransformMatrixKHR));
    return out;
}
