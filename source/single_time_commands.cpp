#include "single_time_commands.hpp"
#include "vk_common.hpp"
#include "vulkan_context.hpp"

SingleTimeCommands::SingleTimeCommands(std::shared_ptr<VulkanContext> context)
    : _vulkanContext(context)
{
    vk::CommandBufferAllocateInfo allocateInfo {};
    allocateInfo.level = vk::CommandBufferLevel::ePrimary;
    allocateInfo.commandPool = _vulkanContext->CommandPool();
    allocateInfo.commandBufferCount = 1;

    VkCheckResult(_vulkanContext->Device().allocateCommandBuffers(&allocateInfo, &_commandBuffer), "Failed allocating one time command buffer!");

    vk::FenceCreateInfo fenceInfo {};
    VkCheckResult(_vulkanContext->Device().createFence(&fenceInfo, nullptr, &_fence), "Failed creating single time command fence!");

    vk::CommandBufferBeginInfo beginInfo {};
    beginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;

    VkCheckResult(_commandBuffer.begin(&beginInfo), "Failed beginning one time command buffer!");
}

SingleTimeCommands::~SingleTimeCommands()
{
    SubmitAndWait();

    _vulkanContext->Device().free(_vulkanContext->CommandPool(), _commandBuffer);
    _vulkanContext->Device().destroy(_fence);
}

void SingleTimeCommands::Record(const std::function<void(vk::CommandBuffer)>& commands) const
{
    commands(_commandBuffer);
}

void SingleTimeCommands::SubmitAndWait()
{
    if (_submitted)
    {
        return;
    }
    _submitted = true;

    _commandBuffer.end();

    vk::SubmitInfo submitInfo {};
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &_commandBuffer;

    VkCheckResult(_vulkanContext->GraphicsQueue().submit(1, &submitInfo, _fence), "Failed submitting one time buffer to queue!");
    VkCheckResult(_vulkanContext->Device().waitForFences(1, &_fence, vk::True, std::numeric_limits<uint64_t>::max()), "Failed waiting for fence!");
}
