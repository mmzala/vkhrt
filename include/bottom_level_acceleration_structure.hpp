#pragma once
#include <glm/mat4x4.hpp>
#include "acceleration_structure.hpp"
#include "resources/gpu_resources.hpp"
#include "common.hpp"

class VulkanContext;
class BindlessResources;
struct Model;
struct Buffer;

enum class BLASType : uint8_t
{
    eMesh,
    eHair,
    eVoxels,
};

struct BLASInput
{
    BLASType type = BLASType::eMesh;
    glm::mat4 transform {};
    GeometryNodeCreation node {};
    vk::AccelerationStructureGeometryKHR geometry {};
    vk::AccelerationStructureBuildRangeInfoKHR info {};

    // Optional structure used to give lss data as it isn't considered geometry for AccelerationStructureBuildRangeInfoKHR, but as a next structure
    vk::AccelerationStructureGeometryLinearSweptSpheresDataNV lssInfo {};
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
    [[nodiscard]] BLASType Type() const { return _type; }
    [[nodiscard]] const glm::mat4& Transform() const { return _transform; }

private:
    void InitializeStructure(const BLASInput& input);

    BLASType _type = BLASType::eMesh;
    glm::mat4 _transform {};
    std::shared_ptr<VulkanContext> _vulkanContext;
};
