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
    verticesCount = creation.vertexBuffer.size();
    indexCount = creation.indexBuffer.size();
    sceneGraph = creation.sceneGraph;

    // Upload to GPU
    {
        // Staging buffers
        BufferCreation vertexStagingBufferCreation {};
        vertexStagingBufferCreation.SetName(sceneGraph->sceneName + " - Vertex Staging Buffer")
            .SetUsageFlags(vk::BufferUsageFlagBits::eTransferSrc)
            .SetMemoryUsage(VMA_MEMORY_USAGE_CPU_ONLY)
            .SetIsMappable(true)
            .SetSize(sizeof(Mesh::Vertex) * creation.vertexBuffer.size());
        Buffer vertexStagingBuffer(vertexStagingBufferCreation, vulkanContext);
        memcpy(vertexStagingBuffer.mappedPtr, creation.vertexBuffer.data(), sizeof(Mesh::Vertex) * creation.vertexBuffer.size());

        BufferCreation indexStagingBufferCreation {};
        indexStagingBufferCreation.SetName(sceneGraph->sceneName + " - Index Staging Buffer")
            .SetUsageFlags(vk::BufferUsageFlagBits::eTransferSrc)
            .SetMemoryUsage(VMA_MEMORY_USAGE_CPU_ONLY)
            .SetIsMappable(true)
            .SetSize(sizeof(uint32_t) * creation.indexBuffer.size());
        Buffer indexStagingBuffer(indexStagingBufferCreation, vulkanContext);
        memcpy(indexStagingBuffer.mappedPtr, creation.indexBuffer.data(), sizeof(uint32_t) * creation.indexBuffer.size());

        // GPU buffers
        vk::BufferUsageFlags bufferUsage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress;

        BufferCreation vertexBufferCreation {};
        vertexBufferCreation.SetName(sceneGraph->sceneName + " - Vertex Buffer")
            .SetUsageFlags(vk::BufferUsageFlagBits::eVertexBuffer | bufferUsage)
            .SetMemoryUsage(VMA_MEMORY_USAGE_GPU_ONLY)
            .SetIsMappable(false)
            .SetSize(sizeof(Mesh::Vertex) * creation.vertexBuffer.size());
        vertexBuffer = std::make_unique<Buffer>(vertexBufferCreation, vulkanContext);

        BufferCreation indexBufferCreation {};
        indexBufferCreation.SetName(sceneGraph->sceneName + " - Index Buffer")
            .SetUsageFlags(vk::BufferUsageFlagBits::eIndexBuffer | bufferUsage)
            .SetMemoryUsage(VMA_MEMORY_USAGE_GPU_ONLY)
            .SetIsMappable(false)
            .SetSize(sizeof(uint32_t) * creation.indexBuffer.size());
        indexBuffer = std::make_unique<Buffer>(indexBufferCreation, vulkanContext);

        SingleTimeCommands commands(vulkanContext);
        commands.Record([&](vk::CommandBuffer commandBuffer)
            {
                VkCopyBufferToBuffer(commandBuffer, vertexStagingBuffer.buffer, vertexBuffer->buffer, sizeof(Mesh::Vertex) * creation.vertexBuffer.size());
                VkCopyBufferToBuffer(commandBuffer, indexStagingBuffer.buffer, indexBuffer->buffer, sizeof(uint32_t) * creation.indexBuffer.size()); });
        commands.SubmitAndWait();
    }
}
