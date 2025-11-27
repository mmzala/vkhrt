#pragma once

#include <functional>
#include <vulkan/vulkan.hpp>
#include <memory>
#include "common.hpp"

class VulkanContext;

class SingleTimeCommands
{
public:
    SingleTimeCommands(std::shared_ptr<VulkanContext> context);
    ~SingleTimeCommands();
    NON_MOVABLE(SingleTimeCommands);
    NON_COPYABLE(SingleTimeCommands);

    void Record(const std::function<void(vk::CommandBuffer)>& commands) const;
    void SubmitAndWait();

private:
    std::shared_ptr<VulkanContext> _vulkanContext;
    vk::CommandBuffer _commandBuffer;
    vk::Fence _fence;
    bool _submitted = false;
};