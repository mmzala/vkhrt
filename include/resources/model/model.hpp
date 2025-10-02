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

    [[nodiscard]] glm::mat4 GetWorldMatrix() const;
};

struct Line
{
    glm::vec3 start {};
    glm::vec3 end {};
};

struct Curve
{
    glm::vec3 start {};
    glm::vec3 controlPoint1 {};
    glm::vec3 controlPoint2 {};
    glm::vec3 end {};
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

    [[nodiscard]] uint32_t GetIndicesPerFaceNum() const;
};

struct SceneGraph
{
    SceneGraph() = default;
    NON_COPYABLE(SceneGraph) // If we copied, node parent pointers would be invalidated

    std::string sceneName {};
    std::vector<Node> nodes {}; // TODO: Nodes themselves are still copyable, but we just overlook this for now (maybe make this vector a pointer or just rely on a root node only?)
    std::vector<Mesh> meshes {};
    std::vector<ResourceHandle<Image>> textures {};
    std::vector<ResourceHandle<Material>> materials {};
};

struct ModelCreation
{
    std::vector<Mesh::Vertex> vertexBuffer {};
    std::vector<uint32_t> indexBuffer {};
    std::shared_ptr<SceneGraph> sceneGraph {};
};

struct Model
{
    Model(const ModelCreation& creation, const std::shared_ptr<VulkanContext>& vulkanContext);

    std::unique_ptr<Buffer> vertexBuffer {};
    std::unique_ptr<Buffer> indexBuffer {};
    uint32_t verticesCount {};
    uint32_t indexCount {};
    std::shared_ptr<SceneGraph> sceneGraph {};
};
