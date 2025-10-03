#pragma once
#include "common.hpp"
#include "resources/resource_manager.hpp"
#include "model.hpp"
#include <assimp/Importer.hpp>
#include <unordered_map>

class VulkanContext;
class BindlessResources;
struct aiScene;

class ModelLoader
{
public:
    ModelLoader(const std::shared_ptr<BindlessResources>& bindlessResources, const std::shared_ptr<VulkanContext>& vulkanContext);
    ~ModelLoader() = default;
    NON_COPYABLE(ModelLoader);
    NON_MOVABLE(ModelLoader);

    [[nodiscard]] std::shared_ptr<Model> LoadFromFile(std::string_view path);

private:
    [[nodiscard]] ModelCreation LoadModel(const aiScene* scene, const std::string_view directory);
    [[nodiscard]] std::shared_ptr<Model> ProcessModel(const ModelCreation& modelCreation);

    Assimp::Importer _importer {};
    std::unordered_map<std::string_view, ResourceHandle<Image>> _imageCache {};
    std::shared_ptr<VulkanContext> _vulkanContext;
    std::shared_ptr<BindlessResources> _bindlessResources;
};
