#pragma once

#include <memory>
#include <optional>
#include <vulkan/vulkan.hpp>
#include <vk_mem_alloc.h>
#include <glm/glm.hpp>
#include "common.hpp"
#include "resource_manager.hpp"

class VulkanContext;

struct BufferCreation
{
    vk::DeviceSize size {};
    vk::BufferUsageFlags usage {};
    bool isMappable = true;
    VmaMemoryUsage memoryUsage = VMA_MEMORY_USAGE_CPU_ONLY;
    std::string name {};

    BufferCreation& SetSize(vk::DeviceSize size);
    BufferCreation& SetUsageFlags(vk::BufferUsageFlags usage);
    BufferCreation& SetIsMappable(bool isMappable);
    BufferCreation& SetMemoryUsage(VmaMemoryUsage memoryUsage);
    BufferCreation& SetName(std::string_view name);
};

struct Buffer
{
    Buffer(const BufferCreation& creation, const std::shared_ptr<VulkanContext>& vulkanContext);
    ~Buffer();
    NON_COPYABLE(Buffer);
    Buffer(Buffer&& other) noexcept;
    Buffer& operator=(Buffer&& other) noexcept;

    vk::Buffer buffer {};
    VmaAllocation allocation {};
    void* mappedPtr = nullptr;

private:
    std::shared_ptr<VulkanContext> _vulkanContext;
};

struct SamplerCreation
{
    vk::SamplerAddressMode addressModeU = vk::SamplerAddressMode::eRepeat;
    vk::SamplerAddressMode addressModeW = vk::SamplerAddressMode::eRepeat;
    vk::SamplerAddressMode addressModeV = vk::SamplerAddressMode::eRepeat;
    vk::Filter minFilter = vk::Filter::eLinear;
    vk::Filter magFilter = vk::Filter::eLinear;
    bool useMaxAnisotropy = true;
    bool anisotropyEnable = true;
    vk::BorderColor borderColor = vk::BorderColor::eIntOpaqueBlack;
    bool unnormalizedCoordinates = false;
    bool compareEnable = false;
    vk::CompareOp compareOp = vk::CompareOp::eAlways;
    vk::SamplerMipmapMode mipmapMode = vk::SamplerMipmapMode::eLinear;
    float mipLodBias = 0.0f;
    float minLod = 0.0f;
    float maxLod = 1.0f;
    vk::SamplerReductionMode reductionMode = vk::SamplerReductionMode::eWeightedAverage;
    std::string name {};
};

struct Sampler
{
    Sampler(const SamplerCreation& creation, const std::shared_ptr<VulkanContext>& vulkanContext);
    ~Sampler();
    NON_COPYABLE(Sampler);
    Sampler(Sampler&& other) noexcept;
    Sampler& operator=(Sampler&& other) noexcept;

    vk::Sampler sampler;

private:
    std::shared_ptr<VulkanContext> _vulkanContext;
};

struct ImageCreation
{
    std::vector<std::byte> data {};
    uint32_t width {};
    uint32_t height {};
    vk::Format format = vk::Format::eUndefined;
    vk::ImageUsageFlags usage { 0 };
    std::string name {};

    ImageCreation& SetData(const std::vector<std::byte>& data);
    ImageCreation& SetSize(uint32_t width, uint32_t height);
    ImageCreation& SetFormat(vk::Format format);
    ImageCreation& SetUsageFlags(vk::ImageUsageFlags usage);
    ImageCreation& SetName(std::string_view name);
};

struct Image
{
    Image(const ImageCreation& creation, const std::shared_ptr<VulkanContext>& vulkanContext);
    ~Image();
    NON_COPYABLE(Image);
    Image(Image&& other) noexcept;
    Image& operator=(Image&& other) noexcept;

    vk::Image image {};
    vk::ImageView view {};
    VmaAllocation allocation {};
    vk::Format format {};

private:
    std::shared_ptr<VulkanContext> _vulkanContext;
};

struct MaterialCreation
{
    ResourceHandle<Image> albedoMap = ResourceHandle<Image>::Null();
    glm::vec4 albedoFactor { 1.0f };
    uint32_t albedoUVChannel = 0;

    ResourceHandle<Image> metallicRoughnessMap = ResourceHandle<Image>::Null();
    float metallicFactor = 0.0f;
    float roughnessFactor = 0.0f;
    std::optional<uint32_t> metallicRoughnessUVChannel {};

    ResourceHandle<Image> normalMap = ResourceHandle<Image>::Null();
    float normalScale = 0.0f;
    uint32_t normalUVChannel = 0;

    ResourceHandle<Image> occlusionMap = ResourceHandle<Image>::Null();
    float occlusionStrength = 0.0f;
    uint32_t occlusionUVChannel = 0;

    ResourceHandle<Image> emissiveMap = ResourceHandle<Image>::Null();
    glm::vec3 emissiveFactor { 0.0f };
    uint32_t emissiveUVChannel = 0;

    float transparency = 0.0f;
    float ior = 1.5f;

    MaterialCreation& SetAlbedoMap(ResourceHandle<Image> albedoMap);
    MaterialCreation& SetAlbedoFactor(const glm::vec4& albedoFactor);
    MaterialCreation& SetAlbedoUVChannel(uint32_t albedoUVChannel);

    MaterialCreation& SetMetallicRoughnessMap(ResourceHandle<Image> metallicRoughnessMap);
    MaterialCreation& SetMetallicFactor(float metallicFactor);
    MaterialCreation& SetRoughnessFactor(float roughnessFactor);
    MaterialCreation& SetMetallicRoughnessUVChannel(uint32_t metallicRoughnessUVChannel);

    MaterialCreation& SetNormalMap(ResourceHandle<Image> normalMap);
    MaterialCreation& SetNormalScale(float normalScale);
    MaterialCreation& SetNormalUVChannel(uint32_t normalUVChannel);

    MaterialCreation& SetOcclusionMap(ResourceHandle<Image> occlusionMap);
    MaterialCreation& SetOcclusionStrength(float occlusionStrength);
    MaterialCreation& SetOcclusionUVChannel(uint32_t occlusionUVChannel);

    MaterialCreation& SetEmissiveMap(ResourceHandle<Image> emissiveMap);
    MaterialCreation& SetEmissiveFactor(const glm::vec3& emissiveFactor);
    MaterialCreation& SetEmissiveUVChannel(uint32_t emissiveUVChannel);
};

struct Material
{
    explicit Material(const MaterialCreation& creation);

    glm::vec4 albedoFactor { 1.0f };

    float metallicFactor = 0.0f;
    float roughnessFactor = 0.0f;
    float normalScale = 1.0f;
    float occlusionStrength = 0.0f;

    glm::vec3 emissiveFactor { 0.0f };
    int32_t useEmissiveMap = false;

    int32_t useAlbedoMap = false;
    int32_t useMetallicRoughnessMap = false;
    int32_t useNormalMap = false;
    int32_t useOcclusionMap = false;

    uint32_t albedoMapIndex = NULL_RESOURCE_INDEX_VALUE;
    uint32_t metallicRoughnessMapIndex = NULL_RESOURCE_INDEX_VALUE;
    uint32_t normalMapIndex = NULL_RESOURCE_INDEX_VALUE;
    uint32_t occlusionMapIndex = NULL_RESOURCE_INDEX_VALUE;

    uint32_t emissiveMapIndex = NULL_RESOURCE_INDEX_VALUE;
    float transparency = 0.0f;
    float ior = 1.5f;
    uint32_t _PADDING_{};
};

struct GeometryNodeCreation
{
    vk::DeviceAddress vertexBufferDeviceAddress = 0;
    vk::DeviceAddress indexBufferDeviceAddress = 0;
    ResourceHandle<Material> material = ResourceHandle<Material>::Null();
};

struct GeometryNode
{
    explicit GeometryNode(const GeometryNodeCreation& creation);

    uint64_t vertexBufferDeviceAddress = 0;
    uint64_t indexBufferDeviceAddress = 0;
    uint32_t materialIndex = NULL_RESOURCE_INDEX_VALUE;
    glm::vec3 _PADDING_{};
};

struct BLASInstance
{
    uint32_t firstGeometryIndex = 0;
};

using BLASInstanceCreation = BLASInstance;
