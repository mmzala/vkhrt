#pragma once
#include "acceleration_structure.hpp"
#include "common.hpp"

class VulkanContext;
class BottomLevelAccelerationStructure;
class BindlessResources;

class TopLevelAccelerationStructure : public AccelerationStructure
{
public:
    TopLevelAccelerationStructure(const std::vector<BottomLevelAccelerationStructure>& blases, const std::shared_ptr<BindlessResources>& resources, const std::shared_ptr<VulkanContext>& vulkanContext);
    ~TopLevelAccelerationStructure();
    NON_COPYABLE(TopLevelAccelerationStructure);
    NON_MOVABLE(TopLevelAccelerationStructure);

    [[nodiscard]] vk::AccelerationStructureKHR Structure() const { return _vkStructure; }

private:
    void InitializeStructure(const std::vector<BottomLevelAccelerationStructure>& blases, const std::shared_ptr<BindlessResources>& resources);

    std::shared_ptr<VulkanContext> _vulkanContext;
};
