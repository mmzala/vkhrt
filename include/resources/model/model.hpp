#pragma once
#include "resources/gpu_resources.hpp"
#include <glm/vec3.hpp>
#include <glm/matrix.hpp>

struct Node
{
    std::string name {};
    const Node* parent = nullptr;
    glm::mat4 localMatrix {};
    std::vector<uint32_t> meshes {};
    std::vector<uint32_t> hairs {};
    std::vector<uint32_t> voxelMeshes {};

    [[nodiscard]] glm::mat4 GetWorldMatrix() const;
};

struct alignas(16) AABB
{
    glm::vec3 min {};
    glm::vec3 max {};
};

struct Line
{
    glm::vec3 start {};
    glm::vec3 end {};
};

struct alignas(16) Curve
{
    glm::vec3 start {};
    glm::vec3 controlPoint1 {};
    glm::vec3 controlPoint2 {};
    glm::vec3 end {};

    [[nodiscard]] glm::vec3 Sample(float t) const;
    [[nodiscard]] glm::vec3 SampleDerivitive(float t) const;
};

struct Mesh
{
    struct Vertex
    {
        glm::vec3 position {};
        glm::vec3 normal {};
        glm::vec2 texCoord {};
    };

    enum class PrimitiveType : uint8_t
    {
        eTriangles,
        eLines,
    };

    PrimitiveType primitiveType = PrimitiveType::eTriangles;
    uint32_t indexCount {};
    uint32_t firstIndex {};
    uint32_t firstVertex {};
    ResourceHandle<Material> material {};
    AABB boundingBox {};

    [[nodiscard]] uint32_t GetIndicesPerFaceNum() const;
};

struct Hair
{
    uint32_t curveCount {};
    uint32_t firstCurve {};
    uint32_t aabbCount {};
    uint32_t firstAabb {};
    ResourceHandle<Material> material {};
};

struct VoxelMesh
{
    std::vector<bool> voxels {}; // TODO: Don't store after uploaded to GPU
    AABB boundingBox {};
    glm::ivec3 gridResolution {};
    ResourceHandle<Material> material {};

    uint32_t aabbCount {};
    uint32_t firstAabb {};
};

struct SceneGraph
{
    SceneGraph() = default;
    NON_COPYABLE(SceneGraph) // If we copied, node parent pointers would be invalidated

    std::string sceneName {};
    std::vector<Node> nodes {}; // TODO: Nodes themselves are still copyable, but we just overlook this for now (maybe make this vector a pointer or just rely on a root node only?)
    std::vector<Mesh> meshes {};
    std::vector<Hair> hairs {};
    std::vector<VoxelMesh> voxelMeshes {};
    std::vector<ResourceHandle<Image>> textures {};
    std::vector<ResourceHandle<Material>> materials {};
};

struct ModelCreation
{
    std::vector<Mesh::Vertex> vertexBuffer {};
    std::vector<uint32_t> indexBuffer {};

    std::vector<Curve> curveBuffer {};
    std::vector<AABB> aabbBuffer {};

    std::shared_ptr<SceneGraph> sceneGraph {};
};

struct Model
{
    Model(const ModelCreation& creation, const std::shared_ptr<VulkanContext>& vulkanContext);

    std::unique_ptr<Buffer> vertexBuffer {};
    std::unique_ptr<Buffer> indexBuffer {};
    uint32_t vertexCount {};
    uint32_t indexCount {};

    std::unique_ptr<Buffer> curveBuffer {};
    std::unique_ptr<Buffer> aabbBuffer {};
    uint32_t curveCount {};
    uint32_t aabbCount {};

    std::shared_ptr<SceneGraph> sceneGraph {};
};
