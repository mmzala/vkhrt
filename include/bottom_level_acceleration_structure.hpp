#pragma once
#include "acceleration_structure.hpp"
#include "resources/gpu_resources.hpp"
#include "common.hpp"
#include <glm/mat4x4.hpp>

class VulkanContext;
class BindlessResources;
struct Model;
struct Buffer;

struct BLASInput
{
    glm::mat4 transform {};
    GeometryNodeCreation node {};
    vk::AccelerationStructureGeometryKHR geometry {};
    vk::AccelerationStructureBuildRangeInfoKHR info {};
};

class BottomLevelAccelerationStructure : public AccelerationStructure
{
public:
    BottomLevelAccelerationStructure(const BLASInput& input, const std::shared_ptr<BindlessResources>& resources, const std::shared_ptr<VulkanContext>& vulkanContext);
    ~BottomLevelAccelerationStructure();
    BottomLevelAccelerationStructure(BottomLevelAccelerationStructure&& other) noexcept;
    BottomLevelAccelerationStructure& operator=(BottomLevelAccelerationStructure&& other) = delete;
    NON_COPYABLE(BottomLevelAccelerationStructure);

    [[nodiscard]] vk::AccelerationStructureKHR Structure() const { return _vkStructure; }
    [[nodiscard]] const glm::mat4& Transform() const { return _transform; }

private:
    void InitializeStructure(const BLASInput& input);

    glm::mat4 _transform {};
    std::shared_ptr<VulkanContext> _vulkanContext;
};
