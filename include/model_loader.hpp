#pragma once
#include "common.hpp"
#include "resources/resource_manager.hpp"
#include <assimp/Importer.hpp>
#include <glm/vec3.hpp>
#include <glm/matrix.hpp>
#include <unordered_map>

class VulkanContext;
class BindlessResources;
struct aiScene;
struct Buffer;
struct Image;
struct Material;

struct Node
{
    std::string name {};
    const Node* parent = nullptr;
    glm::mat4 localMatrix {};
    std::vector<uint32_t> meshes {};

    [[nodiscard]] glm::mat4 GetWorldMatrix() const;
};

struct Mesh
{
    uint32_t indexCount {};
    uint32_t firstIndex {};
    uint32_t firstVertex {};
    ResourceHandle<Material> material {};
};

struct Model
{
    struct Vertex
    {
        glm::vec3 position {};
        glm::vec3 normal {};
        glm::vec2 texCoord {};
    };

    std::unique_ptr<Buffer> vertexBuffer;
    std::unique_ptr<Buffer> indexBuffer;
    uint32_t verticesCount {};
    uint32_t indexCount {};

    std::vector<Node> nodes {};
    std::vector<Mesh> meshes {};
    std::vector<ResourceHandle<Image>> textures {};
    std::vector<ResourceHandle<Material>> materials {};
};

class ModelLoader
{
public:
    ModelLoader(const std::shared_ptr<BindlessResources>& bindlessResources, const std::shared_ptr<VulkanContext>& vulkanContext);
    ~ModelLoader() = default;
    NON_COPYABLE(ModelLoader);
    NON_MOVABLE(ModelLoader);

    [[nodiscard]] std::shared_ptr<Model> LoadFromFile(std::string_view path);

private:
    [[nodiscard]] std::shared_ptr<Model> ProcessModel(const aiScene* scene, const std::string_view directory);

    Assimp::Importer _importer {};
    std::unordered_map<std::string_view, ResourceHandle<Image>> _imageCache {};
    std::shared_ptr<VulkanContext> _vulkanContext;
    std::shared_ptr<BindlessResources> _bindlessResources;
};
