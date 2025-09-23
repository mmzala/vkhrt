#include "model_loader.hpp"
#include "resources/bindless_resources.hpp"
#include "resources/gpu_resources.hpp"
#include "single_time_commands.hpp"
#include "vk_common.hpp"
#include <assimp/GltfMaterial.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <filesystem>
#include <glm/gtc/type_ptr.hpp>
#include <spdlog/spdlog.h>
#include <stb_image.h>

ResourceHandle<Image> ProcessImage(const std::string_view localPath, const std::string_view directory, const std::shared_ptr<BindlessResources>& resources)
{
    ImageCreation imageCreation {};
    imageCreation.SetName(localPath)
        .SetFormat(vk::Format::eR8G8B8A8Unorm)
        .SetUsageFlags(vk::ImageUsageFlagBits::eSampled);

    int32_t width {}, height {}, nrChannels {};

    const std::string fullPath = std::string(directory) + "/" + std::string { localPath.begin(), localPath.end() };

    unsigned char* stbiData = stbi_load(fullPath.c_str(), &width, &height, &nrChannels, 4);

    if (!stbiData)
    {
        spdlog::error("[MODEL LOADING] Failed to load data from image [{}] from path [{}]", imageCreation.name, fullPath);
        return ResourceHandle<Image> {};
    }

    std::vector<std::byte> data = std::vector<std::byte>(width * height * 4);
    std::memcpy(data.data(), std::bit_cast<std::byte*>(stbiData), data.size());
    stbi_image_free(stbiData);

    imageCreation.SetSize(width, height)
        .SetData(data);

    return resources->Images().Create(imageCreation);
}

ResourceHandle<Image> LoadTexture(const std::string_view localPath, const std::string_view directory, const std::shared_ptr<BindlessResources>& resources, std::vector<ResourceHandle<Image>>& textures, std::unordered_map<std::string_view, ResourceHandle<Image>>& imageCache)
{
    const auto it = imageCache.find(localPath);

    if (it == imageCache.end())
    {
        ResourceHandle<Image> image = ProcessImage(localPath, directory, resources);
        textures.push_back(image);
        return image;
    }

    return it->second;
}

ResourceHandle<Material> ProcessMaterial(const aiMaterial* aiMaterial, const std::string_view directory, const std::shared_ptr<BindlessResources>& resources, std::vector<ResourceHandle<Image>>& textures, std::unordered_map<std::string_view, ResourceHandle<Image>>& imageCache)
{
    MaterialCreation materialCreation {};

    // Textures

    aiString texturePath {};

    if (aiMaterial->GetTexture(aiTextureType_DIFFUSE, 0, &texturePath) == AI_SUCCESS)
    {
        materialCreation.albedoMap = LoadTexture(texturePath.C_Str(), directory, resources, textures, imageCache);
    }

    if (aiMaterial->GetTexture(aiTextureType_GLTF_METALLIC_ROUGHNESS, 0, &texturePath) == AI_SUCCESS)
    {
        materialCreation.metallicRoughnessMap = LoadTexture(texturePath.C_Str(), directory, resources, textures, imageCache);
    }

    if (aiMaterial->GetTexture(aiTextureType_NORMALS, 0, &texturePath) == AI_SUCCESS)
    {
        materialCreation.normalMap = LoadTexture(texturePath.C_Str(), directory, resources, textures, imageCache);
    }

    if (aiMaterial->GetTexture(aiTextureType_AMBIENT_OCCLUSION, 0, &texturePath) == AI_SUCCESS)
    {
        materialCreation.occlusionMap = LoadTexture(texturePath.C_Str(), directory, resources, textures, imageCache);
    }

    if (aiMaterial->GetTexture(aiTextureType_EMISSIVE, 0, &texturePath) == AI_SUCCESS)
    {
        materialCreation.emissiveMap = LoadTexture(texturePath.C_Str(), directory, resources, textures, imageCache);
    }

    // Properties

    aiColor4D color {};
    float factor {};

    if (aiMaterial->Get(AI_MATKEY_BASE_COLOR, color) == AI_SUCCESS)
    {
        materialCreation.albedoFactor = glm::vec4(color.r, color.g, color.b, color.a);
    }

    if (aiMaterial->Get(AI_MATKEY_METALLIC_FACTOR, factor) == AI_SUCCESS)
    {
        materialCreation.metallicFactor = factor;
    }

    if (aiMaterial->Get(AI_MATKEY_ROUGHNESS_FACTOR, factor) == AI_SUCCESS)
    {
        materialCreation.roughnessFactor = factor;
    }

    if (aiMaterial->Get(AI_MATKEY_GLTF_TEXTURE_SCALE(aiTextureType_NORMALS, 0), factor) == AI_SUCCESS)
    {
        materialCreation.normalScale = factor;
    }

    if (aiMaterial->Get(AI_MATKEY_COLOR_EMISSIVE, color) == AI_SUCCESS)
    {
        materialCreation.emissiveFactor = glm::vec3(color.r, color.g, color.b);
    }

    if (aiMaterial->Get(AI_MATKEY_GLTF_TEXTURE_SCALE(aiTextureType_AMBIENT_OCCLUSION, 0), factor) == AI_SUCCESS)
    {
        materialCreation.occlusionStrength = factor;
    }

    if (aiMaterial->Get(AI_MATKEY_TRANSMISSION_FACTOR, factor) == AI_SUCCESS)
    {
        materialCreation.transparency = factor;
    }

    if (aiMaterial->Get(AI_MATKEY_REFRACTI, factor) == AI_SUCCESS)
    {
        materialCreation.ior = factor;
    }

    return resources->Materials().Create(materialCreation);
}

Mesh ProcessMesh(const aiScene* aiScene, const aiMesh* aiMesh, const std::vector<ResourceHandle<Material>>& materials, std::vector<Model::Vertex>& vertices, std::vector<uint32_t>& indices)
{
    Mesh mesh {};
    mesh.firstIndex = static_cast<uint32_t>(indices.size());
    mesh.firstVertex = static_cast<uint32_t>(vertices.size());

    if (aiMesh->HasFaces())
    {
        // Using aiProcess_Triangulate, so we know that each face has 3 indices
        mesh.indexCount = aiMesh->mNumFaces * 3;
        indices.resize(indices.size() + mesh.indexCount);
        uint32_t indexOffset = 0;

        for (uint32_t i = 0; i < aiMesh->mNumFaces; ++i)
        {
            const aiFace face = aiMesh->mFaces[i];
            for (uint32_t j = 0; j < face.mNumIndices; ++j)
            {
                indices[mesh.firstIndex + indexOffset] = mesh.firstVertex + face.mIndices[j];
                indexOffset++;
            }
        }
    }
    else
    {
        spdlog::error("[MODEL LOADING] Mesh \"{}\" doesn't have any indices!", aiMesh->mName.C_Str());
    }

    // Positions
    {
        vertices.resize(vertices.size() + aiMesh->mNumVertices);

        for (uint32_t i = 0; i < aiMesh->mNumVertices; ++i)
        {
            vertices[mesh.firstVertex + i].position = glm::vec3(aiMesh->mVertices[i].x, aiMesh->mVertices[i].y, aiMesh->mVertices[i].z);
        }
    }

    // Normals
    if (aiMesh->HasNormals())
    {
        for (uint32_t i = 0; i < aiMesh->mNumVertices; ++i)
        {
            vertices[mesh.firstVertex + i].normal = glm::vec3(aiMesh->mNormals[i].x, aiMesh->mNormals[i].y, aiMesh->mNormals[i].z);
        }
    }

    // UVs
    if (aiMesh->HasTextureCoords(0))
    {
        for (uint32_t i = 0; i < aiMesh->mNumVertices; ++i)
        {
            vertices[mesh.firstVertex + i].texCoord = glm::vec2(aiMesh->mTextureCoords[0][i].x, aiMesh->mTextureCoords[0][i].y);
        }
    }

    // Material
    if (aiMesh->mMaterialIndex < aiScene->mNumMaterials)
    {
        mesh.material = materials[aiMesh->mMaterialIndex]; // Order of materials is the same as assimp loads them, so we get the correct one from our vector with the same index
    }

    return mesh;
}

void ProcessNode(const aiNode* aiNode, const Node* parent, std::vector<Node>& nodes)
{
    static const auto aiMatrixToGlm = [](const aiMatrix4x4& from)
    {
        glm::mat4 to {};
        to[0][0] = from.a1;
        to[1][0] = from.a2;
        to[2][0] = from.a3;
        to[3][0] = from.a4;
        to[0][1] = from.b1;
        to[1][1] = from.b2;
        to[2][1] = from.b3;
        to[3][1] = from.b4;
        to[0][2] = from.c1;
        to[1][2] = from.c2;
        to[2][2] = from.c3;
        to[3][2] = from.c4;
        to[0][3] = from.d1;
        to[1][3] = from.d2;
        to[2][3] = from.d3;
        to[3][3] = from.d4;
        return to;
    };

    Node& node = nodes.emplace_back();
    node.name = aiNode->mName.C_Str();
    node.parent = parent;
    node.localMatrix = aiMatrixToGlm(aiNode->mTransformation);

    for (uint32_t i = 0; i < aiNode->mNumMeshes; ++i)
    {
        node.meshes.push_back(aiNode->mMeshes[i]); // We have the same order of meshes as assimp, so we can use the same index to access it
    }

    for (uint32_t i = 0; i < aiNode->mNumChildren; ++i)
    {
        ProcessNode(aiNode->mChildren[i], &node, nodes);
    }
}

size_t CountNodes(const aiNode* aiNode)
{
    size_t count = 1;
    for (unsigned int i = 0; i < aiNode->mNumChildren; ++i)
    {
        count += CountNodes(aiNode->mChildren[i]);
    }
    return count;
}

std::vector<Node> ProcessNodes(const aiScene* aiScene)
{
    const size_t nodeCount = CountNodes(aiScene->mRootNode);
    std::vector<Node> nodes {};
    nodes.reserve(nodeCount);

    ProcessNode(aiScene->mRootNode, nullptr, nodes);
    return nodes;
}

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

ModelLoader::ModelLoader(const std::shared_ptr<BindlessResources>& bindlessResources, const std::shared_ptr<VulkanContext>& vulkanContext)
    : _vulkanContext(vulkanContext)
    , _bindlessResources(bindlessResources)
{
}

std::shared_ptr<Model> ModelLoader::LoadFromFile(std::string_view path)
{
    spdlog::info("[FILE] Loading model file {}", path);

    const aiScene* aiScene = _importer.ReadFile({ path.begin(), path.end() }, aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_GenNormals);

    if (!aiScene || aiScene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !aiScene->mRootNode)
    {
        spdlog::error("[FILE] Failed to load model file {} with error: {}", path, _importer.GetErrorString());
        return nullptr;
    }

    _imageCache.clear(); // Clear image cache for a new load
    std::string_view directory = path.substr(0, path.find_last_of('/'));
    return ProcessModel(aiScene, directory);
}

std::shared_ptr<Model> ModelLoader::ProcessModel(const aiScene* aiScene, const std::string_view directory)
{
    std::shared_ptr<Model> model = std::make_shared<Model>();

    for (uint32_t i = 0; i < aiScene->mNumMaterials; ++i)
    {
        model->materials.push_back(ProcessMaterial(aiScene->mMaterials[i], directory, _bindlessResources, model->textures, _imageCache));
    }

    std::vector<Model::Vertex> vertices {};
    std::vector<uint32_t> indices {};

    for (uint32_t i = 0; i < aiScene->mNumMeshes; ++i)
    {
        model->meshes.push_back(ProcessMesh(aiScene, aiScene->mMeshes[i], model->materials, vertices, indices));
    }

    // Process vertex and index data
    {
        std::string sceneName = aiScene->mName.C_Str();
        model->verticesCount = vertices.size();
        model->indexCount = indices.size();

        // Staging buffers
        BufferCreation vertexStagingBufferCreation {};
        vertexStagingBufferCreation.SetName(sceneName + " - Vertex Staging Buffer")
            .SetUsageFlags(vk::BufferUsageFlagBits::eTransferSrc)
            .SetMemoryUsage(VMA_MEMORY_USAGE_CPU_ONLY)
            .SetIsMappable(true)
            .SetSize(sizeof(Model::Vertex) * vertices.size());
        Buffer vertexStagingBuffer(vertexStagingBufferCreation, _vulkanContext);
        memcpy(vertexStagingBuffer.mappedPtr, vertices.data(), sizeof(Model::Vertex) * vertices.size());

        BufferCreation indexStagingBufferCreation {};
        indexStagingBufferCreation.SetName(sceneName + " - Index Staging Buffer")
            .SetUsageFlags(vk::BufferUsageFlagBits::eTransferSrc)
            .SetMemoryUsage(VMA_MEMORY_USAGE_CPU_ONLY)
            .SetIsMappable(true)
            .SetSize(sizeof(uint32_t) * indices.size());
        Buffer indexStagingBuffer(indexStagingBufferCreation, _vulkanContext);
        memcpy(indexStagingBuffer.mappedPtr, indices.data(), sizeof(uint32_t) * indices.size());

        // GPU buffers
        vk::BufferUsageFlags bufferUsage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress;

        BufferCreation vertexBufferCreation {};
        vertexBufferCreation.SetName(sceneName + " - Vertex Buffer")
            .SetUsageFlags(vk::BufferUsageFlagBits::eVertexBuffer | bufferUsage)
            .SetMemoryUsage(VMA_MEMORY_USAGE_GPU_ONLY)
            .SetIsMappable(false)
            .SetSize(sizeof(Model::Vertex) * vertices.size());
        model->vertexBuffer = std::make_unique<Buffer>(vertexBufferCreation, _vulkanContext);

        BufferCreation indexBufferCreation {};
        indexBufferCreation.SetName(sceneName + " - Index Buffer")
            .SetUsageFlags(vk::BufferUsageFlagBits::eIndexBuffer | bufferUsage)
            .SetMemoryUsage(VMA_MEMORY_USAGE_GPU_ONLY)
            .SetIsMappable(false)
            .SetSize(sizeof(uint32_t) * indices.size());
        model->indexBuffer = std::make_unique<Buffer>(indexBufferCreation, _vulkanContext);

        SingleTimeCommands commands(_vulkanContext);
        commands.Record([&](vk::CommandBuffer commandBuffer)
            {
                VkCopyBufferToBuffer(commandBuffer, vertexStagingBuffer.buffer, model->vertexBuffer->buffer, sizeof(Model::Vertex) * vertices.size());
                VkCopyBufferToBuffer(commandBuffer, indexStagingBuffer.buffer, model->indexBuffer->buffer, sizeof(uint32_t) * indices.size()); });
        commands.SubmitAndWait();
    }

    model->nodes = ProcessNodes(aiScene);

    return model;
}
