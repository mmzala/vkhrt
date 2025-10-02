#include "resources/model/model_loader.hpp"
#include "resources/bindless_resources.hpp"
#include "resources/model/geometry_processor.hpp"
#include "single_time_commands.hpp"
#include "vk_common.hpp"
#include <assimp/GltfMaterial.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <filesystem>
#include <glm/gtc/type_ptr.hpp>
#include <spdlog/spdlog.h>
#include <stb_image.h>

Mesh::PrimitiveType GetPrimitiveType(const aiMesh* aiMesh)
{
    switch (aiMesh->mPrimitiveTypes)
    {
    case aiPrimitiveType_TRIANGLE:
        return Mesh::PrimitiveType::eTriangles;
    case aiPrimitiveType_LINE:
        return Mesh::PrimitiveType::eLines;
    default:
        spdlog::error("[MODEL LOADING] Using unsupported mesh primitive type: {}", aiMesh->mPrimitiveTypes);
    }

    return Mesh::PrimitiveType::eTriangles;
}

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

Mesh ProcessMesh(const aiScene* aiScene, const aiMesh* aiMesh, const std::vector<ResourceHandle<Material>>& materials, std::vector<Mesh::Vertex>& vertices, std::vector<uint32_t>& indices)
{
    Mesh mesh {};
    mesh.primitiveType = GetPrimitiveType(aiMesh);
    mesh.firstIndex = static_cast<uint32_t>(indices.size());
    mesh.firstVertex = static_cast<uint32_t>(vertices.size());

    if (aiMesh->HasFaces())
    {
        mesh.indexCount = aiMesh->mNumFaces * mesh.GetIndicesPerFaceNum();
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

    ModelCreation modelCreation = LoadModel(aiScene, directory);
    return ProcessModel(modelCreation);
}

ModelCreation ModelLoader::LoadModel(const aiScene* aiScene, const std::string_view directory)
{
    ModelCreation modelCreation {};
    modelCreation.sceneGraph = std::make_shared<SceneGraph>();
    SceneGraph& sceneGraph = *modelCreation.sceneGraph;

    for (uint32_t i = 0; i < aiScene->mNumMaterials; ++i)
    {
        sceneGraph.materials.push_back(ProcessMaterial(aiScene->mMaterials[i], directory, _bindlessResources, sceneGraph.textures, _imageCache));
    }

    for (uint32_t i = 0; i < aiScene->mNumMeshes; ++i)
    {
        sceneGraph.meshes.push_back(ProcessMesh(aiScene, aiScene->mMeshes[i], sceneGraph.materials, modelCreation.vertexBuffer, modelCreation.indexBuffer));
    }

    sceneGraph.sceneName = aiScene->mName.C_Str();
    sceneGraph.nodes = ProcessNodes(aiScene);
    return modelCreation;
}

std::shared_ptr<Model> ModelLoader::ProcessModel(const ModelCreation& modelCreation)
{
    // We don't support pre-processing models with multiple different mesh types
    Mesh::PrimitiveType firstPrimitiveType = modelCreation.sceneGraph->meshes[0].primitiveType;
    for (const Mesh& mesh : modelCreation.sceneGraph->meshes)
    {
        if (mesh.primitiveType != firstPrimitiveType)
        {
            spdlog::error("[MODEL LOADING] Model \"{}\" contains multiple different mesh primitive types which is not supported!", modelCreation.sceneGraph->sceneName);
            return nullptr;
        }
    }

    // If we have a normal triangle mesh, we can just return
    if (firstPrimitiveType == Mesh::PrimitiveType::eTriangles)
    {
        return std::make_unique<Model>(modelCreation, _vulkanContext);
    }

    // Create mesh from hair strands
    ModelCreation newModelCreation = GenerateHairMeshesFromHairModel(modelCreation);
    return std::make_unique<Model>(newModelCreation, _vulkanContext);
}
