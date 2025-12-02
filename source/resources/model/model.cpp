#include "resources/model/model.hpp"
#include "single_time_commands.hpp"
#include "vk_common.hpp"
#include <spdlog/spdlog.h>

glm::mat4 Node::GetWorldMatrix() const
{
    glm::mat4 matrix = localMatrix;
    const Node* p = parent;

    while (p)
    {
        matrix = p->localMatrix * matrix;
        p = p->parent;
    }

    return matrix;
}

glm::vec3 Curve::Sample(float t) const
{
    float u = 1.0f - t;
    float tt = t * t;
    float uu = u * u;
    float uuu = uu * u;
    float ttt = tt * t;

    return uuu * start + 3.0f * uu * t * controlPoint1 + 3.0f * u * tt * controlPoint2 + ttt * end;
}

glm::vec3 Curve::SampleDerivitive(float t) const
{
    float u = 1.0f - t;
    return 3.0f * u * u * (controlPoint1 - start) + 6.0f * u * t * (controlPoint2 - controlPoint1) + 3.0f * t * t * (end - controlPoint2);
}

uint32_t Mesh::GetIndicesPerFaceNum() const
{
    switch (primitiveType)
    {
    case PrimitiveType::eTriangles:
        return 3;
    case PrimitiveType::eLines:
        return 2;
    default:
        spdlog::error("[MODEL LOADING] Trying to get number of indices per face using unsupported mesh primitive type");
    }

    return 0;
}

Model::Model(const ModelCreation& creation, const std::shared_ptr<VulkanContext>& vulkanContext)
{
    sceneGraph = creation.sceneGraph;

    vertexCount = creation.vertexBuffer.size();
    indexCount = creation.indexBuffer.size();

    // Upload to GPU
    if (vertexCount != 0 && indexCount != 0)
    {
        const size_t vertexBufferSize = sizeof(Mesh::Vertex) * vertexCount;
        const size_t indexBufferSize = sizeof(uint32_t) * indexCount;

        // Staging buffers
        BufferCreation vertexStagingBufferCreation {};
        vertexStagingBufferCreation.SetName(sceneGraph->sceneName + " - Vertex Staging Buffer")
            .SetUsageFlags(vk::BufferUsageFlagBits::eTransferSrc)
            .SetMemoryUsage(VMA_MEMORY_USAGE_CPU_ONLY)
            .SetIsMappable(true)
            .SetSize(vertexBufferSize);
        Buffer vertexStagingBuffer(vertexStagingBufferCreation, vulkanContext);
        memcpy(vertexStagingBuffer.mappedPtr, creation.vertexBuffer.data(), vertexBufferSize);

        BufferCreation indexStagingBufferCreation {};
        indexStagingBufferCreation.SetName(sceneGraph->sceneName + " - Index Staging Buffer")
            .SetUsageFlags(vk::BufferUsageFlagBits::eTransferSrc)
            .SetMemoryUsage(VMA_MEMORY_USAGE_CPU_ONLY)
            .SetIsMappable(true)
            .SetSize(indexBufferSize);
        Buffer indexStagingBuffer(indexStagingBufferCreation, vulkanContext);
        memcpy(indexStagingBuffer.mappedPtr, creation.indexBuffer.data(), indexBufferSize);

        // GPU buffers
        vk::BufferUsageFlags bufferUsage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress;

        BufferCreation vertexBufferCreation {};
        vertexBufferCreation.SetName(sceneGraph->sceneName + " - Vertex Buffer")
            .SetUsageFlags(vk::BufferUsageFlagBits::eVertexBuffer | bufferUsage)
            .SetMemoryUsage(VMA_MEMORY_USAGE_GPU_ONLY)
            .SetIsMappable(false)
            .SetSize(vertexBufferSize);
        vertexBuffer = std::make_unique<Buffer>(vertexBufferCreation, vulkanContext);

        BufferCreation indexBufferCreation {};
        indexBufferCreation.SetName(sceneGraph->sceneName + " - Index Buffer")
            .SetUsageFlags(vk::BufferUsageFlagBits::eIndexBuffer | bufferUsage)
            .SetMemoryUsage(VMA_MEMORY_USAGE_GPU_ONLY)
            .SetIsMappable(false)
            .SetSize(indexBufferSize);
        indexBuffer = std::make_unique<Buffer>(indexBufferCreation, vulkanContext);

        SingleTimeCommands commands(vulkanContext);
        commands.Record([&](vk::CommandBuffer commandBuffer)
            {
                VkCopyBufferToBuffer(commandBuffer, vertexStagingBuffer.buffer, vertexBuffer->buffer, vertexBufferSize);
                VkCopyBufferToBuffer(commandBuffer, indexStagingBuffer.buffer, indexBuffer->buffer, indexBufferSize); });
        commands.SubmitAndWait();
    }

    curveCount = creation.curveBuffer.size();
    aabbCount = creation.aabbBuffer.size();

    if (curveCount != 0 && aabbCount != 0)
    {
        const size_t curveBufferSize = sizeof(Curve) * curveCount;
        const size_t aabbBufferSize = sizeof(AABB) * aabbCount;

        // Staging buffers
        BufferCreation curveStagingBufferCreation {};
        curveStagingBufferCreation.SetName(sceneGraph->sceneName + " - Curve Staging Buffer")
            .SetUsageFlags(vk::BufferUsageFlagBits::eTransferSrc)
            .SetMemoryUsage(VMA_MEMORY_USAGE_CPU_ONLY)
            .SetIsMappable(true)
            .SetSize(curveBufferSize);
        Buffer curveStagingBuffer(curveStagingBufferCreation, vulkanContext);
        memcpy(curveStagingBuffer.mappedPtr, creation.curveBuffer.data(), curveBufferSize);

        BufferCreation aabbStagingBufferCreation {};
        aabbStagingBufferCreation.SetName(sceneGraph->sceneName + " - AABB Staging Buffer")
            .SetUsageFlags(vk::BufferUsageFlagBits::eTransferSrc)
            .SetMemoryUsage(VMA_MEMORY_USAGE_CPU_ONLY)
            .SetIsMappable(true)
            .SetSize(aabbBufferSize);
        Buffer aabbStagingBuffer(aabbStagingBufferCreation, vulkanContext);
        memcpy(aabbStagingBuffer.mappedPtr, creation.aabbBuffer.data(), aabbBufferSize);

        // GPU buffers
        vk::BufferUsageFlags bufferUsage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress;

        BufferCreation curveBufferCreation {};
        curveBufferCreation.SetName(sceneGraph->sceneName + " - Curve Buffer")
            .SetUsageFlags(bufferUsage)
            .SetMemoryUsage(VMA_MEMORY_USAGE_GPU_ONLY)
            .SetIsMappable(false)
            .SetSize(curveBufferSize);
        curveBuffer = std::make_unique<Buffer>(curveBufferCreation, vulkanContext);

        BufferCreation aabbBufferCreation {};
        aabbBufferCreation.SetName(sceneGraph->sceneName + " - AABB Buffer")
            .SetUsageFlags(bufferUsage)
            .SetMemoryUsage(VMA_MEMORY_USAGE_GPU_ONLY)
            .SetIsMappable(false)
            .SetSize(aabbBufferSize);
        aabbBuffer = std::make_unique<Buffer>(aabbBufferCreation, vulkanContext);

        SingleTimeCommands commands(vulkanContext);
        commands.Record([&](vk::CommandBuffer commandBuffer)
            {
                VkCopyBufferToBuffer(commandBuffer, curveStagingBuffer.buffer, curveBuffer->buffer, curveBufferSize);
                VkCopyBufferToBuffer(commandBuffer, aabbStagingBuffer.buffer, aabbBuffer->buffer, aabbBufferSize); });
        commands.SubmitAndWait();
    }

    lssPositionCount = creation.lssPositionBuffer.size();
    lssRadiusCount = creation.lssRadiusBuffer.size();

    if (lssPositionCount != 0 && lssRadiusCount != 0)
    {
        const size_t positionBufferSize = sizeof(glm::vec3) * curveCount;
        const size_t radiusBufferSize = sizeof(float) * aabbCount;

        // Staging buffers
        BufferCreation positionStagingBufferCreation {};
        positionStagingBufferCreation.SetName(sceneGraph->sceneName + " - LSS Position Staging Buffer")
            .SetUsageFlags(vk::BufferUsageFlagBits::eTransferSrc)
            .SetMemoryUsage(VMA_MEMORY_USAGE_CPU_ONLY)
            .SetIsMappable(true)
            .SetSize(positionBufferSize);
        Buffer positionStagingBuffer(positionStagingBufferCreation, vulkanContext);
        memcpy(positionStagingBuffer.mappedPtr, creation.curveBuffer.data(), positionBufferSize);

        BufferCreation radiusStagingBufferCreation {};
        radiusStagingBufferCreation.SetName(sceneGraph->sceneName + " - LSS Radius Staging Buffer")
            .SetUsageFlags(vk::BufferUsageFlagBits::eTransferSrc)
            .SetMemoryUsage(VMA_MEMORY_USAGE_CPU_ONLY)
            .SetIsMappable(true)
            .SetSize(radiusBufferSize);
        Buffer radiusStagingBuffer(radiusStagingBufferCreation, vulkanContext);
        memcpy(radiusStagingBuffer.mappedPtr, creation.aabbBuffer.data(), radiusBufferSize);

        // GPU buffers
        vk::BufferUsageFlags bufferUsage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress;

        BufferCreation positionBufferCreation {};
        positionBufferCreation.SetName(sceneGraph->sceneName + " - LSS Position Buffer")
            .SetUsageFlags(bufferUsage)
            .SetMemoryUsage(VMA_MEMORY_USAGE_GPU_ONLY)
            .SetIsMappable(false)
            .SetSize(positionBufferSize);
        lssPositionBuffer = std::make_unique<Buffer>(positionBufferCreation, vulkanContext);

        BufferCreation radiusBufferCreation {};
        radiusBufferCreation.SetName(sceneGraph->sceneName + " - LSS Radius Buffer")
            .SetUsageFlags(bufferUsage)
            .SetMemoryUsage(VMA_MEMORY_USAGE_GPU_ONLY)
            .SetIsMappable(false)
            .SetSize(radiusBufferSize);
        lssRadiusBuffer = std::make_unique<Buffer>(radiusBufferCreation, vulkanContext);

        SingleTimeCommands commands(vulkanContext);
        commands.Record([&](vk::CommandBuffer commandBuffer)
            {
                VkCopyBufferToBuffer(commandBuffer, positionStagingBuffer.buffer, lssPositionBuffer->buffer, positionBufferSize);
                VkCopyBufferToBuffer(commandBuffer, radiusStagingBuffer.buffer, lssRadiusBuffer->buffer, radiusBufferSize); });
        commands.SubmitAndWait();
    }
}
