#include "top_level_acceleration_structure.hpp"
#include "bottom_level_acceleration_structure.hpp"
#include "resources/bindless_resources.hpp"
#include "single_time_commands.hpp"
#include "vk_common.hpp"
#include "vulkan_context.hpp"

TopLevelAccelerationStructure::TopLevelAccelerationStructure(const std::vector<BottomLevelAccelerationStructure>& blases, const std::shared_ptr<BindlessResources>& resources, const std::shared_ptr<VulkanContext>& vulkanContext)
    : _vulkanContext(vulkanContext)
{
    InitializeStructure(blases, resources);
}

TopLevelAccelerationStructure::~TopLevelAccelerationStructure()
{
    _vulkanContext->Device().destroyAccelerationStructureKHR(_vkStructure, nullptr, _vulkanContext->Dldi());
}

void TopLevelAccelerationStructure::InitializeStructure(const std::vector<BottomLevelAccelerationStructure>& blases, const std::shared_ptr<BindlessResources>& resources)
{
    uint32_t geometryCount = 0;
    std::vector<vk::AccelerationStructureInstanceKHR> accelerationStructureInstances {};
    for (const auto& blas : blases)
    {
        vk::TransformMatrixKHR transform = VkGLMToTransformMatrixKHR(blas.Transform());

        vk::AccelerationStructureInstanceKHR& accelerationStructureInstance = accelerationStructureInstances.emplace_back();
        accelerationStructureInstance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR; // vk::GeometryInstanceFlagBitsKHR::eTriangleFacingCullDisable
        accelerationStructureInstance.transform = transform;
        accelerationStructureInstance.instanceCustomIndex = accelerationStructureInstances.size() - 1;
        accelerationStructureInstance.mask = 0xFF;
        accelerationStructureInstance.instanceShaderBindingTableRecordOffset = static_cast<uint32_t>(blas.Type()); // 0 for triangle meshes and 1 for hair strands

        vk::AccelerationStructureDeviceAddressInfoKHR blasDeviceAddress {};
        blasDeviceAddress.accelerationStructure = blas.Structure();
        accelerationStructureInstance.accelerationStructureReference = _vulkanContext->Device().getAccelerationStructureAddressKHR(blasDeviceAddress, _vulkanContext->Dldi());

        BLASInstanceCreation blasInstanceCreation {};
        blasInstanceCreation.firstGeometryIndex = geometryCount;
        resources->BLASInstances().Create(blasInstanceCreation);

        geometryCount++;
    }

    BufferCreation instancesBufferCreation {};
    instancesBufferCreation.SetName("TLAS Instances Buffer")
        .SetUsageFlags(vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress)
        .SetMemoryUsage(VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE)
        .SetIsMappable(true)
        .SetSize(accelerationStructureInstances.size() * sizeof(vk::AccelerationStructureInstanceKHR));
    _instancesBuffer = std::make_unique<Buffer>(instancesBufferCreation, _vulkanContext);
    memcpy(_instancesBuffer->mappedPtr, accelerationStructureInstances.data(), accelerationStructureInstances.size() * sizeof(vk::AccelerationStructureInstanceKHR));

    vk::DeviceOrHostAddressConstKHR instanceDataDeviceAddress {};
    instanceDataDeviceAddress.deviceAddress = _vulkanContext->GetBufferDeviceAddress(_instancesBuffer->buffer);

    vk::AccelerationStructureGeometryKHR accelerationStructureGeometry {};
    accelerationStructureGeometry.flags = vk::GeometryFlagBitsKHR::eOpaque;
    accelerationStructureGeometry.geometryType = vk::GeometryTypeKHR::eInstances;
    accelerationStructureGeometry.geometry.instances = vk::AccelerationStructureGeometryInstancesDataKHR {};
    accelerationStructureGeometry.geometry.instances.arrayOfPointers = false;
    accelerationStructureGeometry.geometry.instances.data = instanceDataDeviceAddress;

    vk::AccelerationStructureBuildGeometryInfoKHR buildGeometryInfo {};
    buildGeometryInfo.type = vk::AccelerationStructureTypeKHR::eTopLevel;
    buildGeometryInfo.flags = vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace;
    buildGeometryInfo.mode = vk::BuildAccelerationStructureModeKHR::eBuild;
    buildGeometryInfo.geometryCount = 1;
    buildGeometryInfo.pGeometries = &accelerationStructureGeometry;

    const uint32_t primitiveCount = accelerationStructureInstances.size();
    vk::AccelerationStructureBuildSizesInfoKHR buildSizesInfo = _vulkanContext->Device().getAccelerationStructureBuildSizesKHR(
        vk::AccelerationStructureBuildTypeKHR::eDevice, buildGeometryInfo, primitiveCount, _vulkanContext->Dldi());

    BufferCreation structureBufferCreation {};
    structureBufferCreation.SetName("TLAS Structure Buffer")
        .SetUsageFlags(vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress)
        .SetMemoryUsage(VMA_MEMORY_USAGE_GPU_ONLY)
        .SetIsMappable(false)
        .SetSize(buildSizesInfo.accelerationStructureSize);
    _structureBuffer = std::make_unique<Buffer>(structureBufferCreation, _vulkanContext);

    vk::AccelerationStructureCreateInfoKHR createInfo {};
    createInfo.buffer = _structureBuffer->buffer;
    createInfo.offset = 0;
    createInfo.size = buildSizesInfo.accelerationStructureSize;
    createInfo.type = vk::AccelerationStructureTypeKHR::eTopLevel;
    _vkStructure = _vulkanContext->Device().createAccelerationStructureKHR(createInfo, nullptr, _vulkanContext->Dldi());

    BufferCreation scratchBufferCreation {};
    scratchBufferCreation.SetName("TLAS Scratch Buffer")
        .SetUsageFlags(vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress)
        .SetMemoryUsage(VMA_MEMORY_USAGE_GPU_ONLY)
        .SetIsMappable(false)
        .SetSize(buildSizesInfo.buildScratchSize);
    _scratchBuffer = std::make_unique<Buffer>(scratchBufferCreation, _vulkanContext);

    // Fill remaining data
    buildGeometryInfo.dstAccelerationStructure = _vkStructure;
    buildGeometryInfo.scratchData.deviceAddress = _vulkanContext->GetBufferDeviceAddress(_scratchBuffer->buffer);

    vk::AccelerationStructureBuildRangeInfoKHR buildRangeInfo {};
    buildRangeInfo.primitiveCount = primitiveCount;
    buildRangeInfo.primitiveOffset = 0;
    buildRangeInfo.firstVertex = 0;
    buildRangeInfo.transformOffset = 0;
    std::vector<vk::AccelerationStructureBuildRangeInfoKHR*> pBuildRangeInfos = { &buildRangeInfo };

    SingleTimeCommands singleTimeCommands { _vulkanContext };
    singleTimeCommands.Record([&](vk::CommandBuffer commandBuffer)
        { commandBuffer.buildAccelerationStructuresKHR(1, &buildGeometryInfo, pBuildRangeInfos.data(), _vulkanContext->Dldi()); });
    singleTimeCommands.SubmitAndWait();
}
