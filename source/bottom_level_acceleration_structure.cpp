#include "bottom_level_acceleration_structure.hpp"
#include "resources/bindless_resources.hpp"
#include "resources/model/model.hpp"
#include "single_time_commands.hpp"
#include "vulkan_context.hpp"

BottomLevelAccelerationStructure::BottomLevelAccelerationStructure(const BLASInput& input, const std::shared_ptr<BindlessResources>& resources, const std::shared_ptr<VulkanContext>& vulkanContext)
    : _type(input.type)
    , _transform(input.transform)
    , _vulkanContext(vulkanContext)
{
    InitializeStructure(input);
    resources->GeometryNodes().Create(input.node);
}

BottomLevelAccelerationStructure::~BottomLevelAccelerationStructure()
{
    _vulkanContext->Device().destroyAccelerationStructureKHR(_vkStructure, nullptr, _vulkanContext->Dldi());
}

BottomLevelAccelerationStructure::BottomLevelAccelerationStructure(BottomLevelAccelerationStructure&& other) noexcept
    : _type(other._type)
    , _transform(other._transform)
    , _vulkanContext(other._vulkanContext)
{
    _vkStructure = other._vkStructure;
    _structureBuffer = std::move(other._structureBuffer);
    _scratchBuffer = std::move(other._scratchBuffer);
    _instancesBuffer = std::move(other._instancesBuffer);

    other._vkStructure = nullptr;
}

void BottomLevelAccelerationStructure::InitializeStructure(const BLASInput& input)
{
    vk::AccelerationStructureBuildGeometryInfoKHR buildGeometryInfo {};
    buildGeometryInfo.type = vk::AccelerationStructureTypeKHR::eBottomLevel;
    buildGeometryInfo.flags = vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace;
    buildGeometryInfo.mode = vk::BuildAccelerationStructureModeKHR::eBuild;
    buildGeometryInfo.geometryCount = 1;
    buildGeometryInfo.pGeometries = &input.geometry;

    vk::AccelerationStructureBuildSizesInfoKHR buildSizesInfo = _vulkanContext->Device().getAccelerationStructureBuildSizesKHR(
        vk::AccelerationStructureBuildTypeKHR::eDevice, buildGeometryInfo, input.info.primitiveCount, _vulkanContext->Dldi());

    BufferCreation structureBufferCreation {};
    structureBufferCreation.SetName("BLAS Structure Buffer")
        .SetUsageFlags(vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress)
        .SetMemoryUsage(VMA_MEMORY_USAGE_GPU_ONLY)
        .SetIsMappable(false)
        .SetSize(buildSizesInfo.accelerationStructureSize);
    _structureBuffer = std::make_unique<Buffer>(structureBufferCreation, _vulkanContext);

    vk::AccelerationStructureCreateInfoKHR createInfo {};
    createInfo.type = vk::AccelerationStructureTypeKHR::eBottomLevel;
    createInfo.buffer = _structureBuffer->buffer;
    createInfo.size = buildSizesInfo.accelerationStructureSize;
    _vkStructure = _vulkanContext->Device().createAccelerationStructureKHR(createInfo, nullptr, _vulkanContext->Dldi());

    BufferCreation scratchBufferCreation {};
    scratchBufferCreation.SetName("BLAS Scratch Buffer")
        .SetUsageFlags(vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress)
        .SetMemoryUsage(VMA_MEMORY_USAGE_GPU_ONLY)
        .SetIsMappable(false)
        .SetSize(buildSizesInfo.buildScratchSize);
    _scratchBuffer = std::make_unique<Buffer>(scratchBufferCreation, _vulkanContext);

    // Fill in remaining data
    buildGeometryInfo.dstAccelerationStructure = _vkStructure;
    buildGeometryInfo.scratchData.deviceAddress = _vulkanContext->GetBufferDeviceAddress(_scratchBuffer->buffer);

    std::array<const vk::AccelerationStructureBuildRangeInfoKHR*, 1> pBuildRangeInfos = { &input.info };

    SingleTimeCommands singleTimeCommands { _vulkanContext };
    singleTimeCommands.Record([&](vk::CommandBuffer commandBuffer)
        { commandBuffer.buildAccelerationStructuresKHR(1, &buildGeometryInfo, pBuildRangeInfos.data(), _vulkanContext->Dldi()); });
    singleTimeCommands.SubmitAndWait();
}
