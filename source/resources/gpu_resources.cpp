#include "resources/gpu_resources.hpp"
#include "single_time_commands.hpp"
#include "vk_common.hpp"

BufferCreation& BufferCreation::SetSize(vk::DeviceSize size)
{
    this->size = size;
    return *this;
}

BufferCreation& BufferCreation::SetUsageFlags(vk::BufferUsageFlags usage)
{
    this->usage = usage;
    return *this;
}

BufferCreation& BufferCreation::SetIsMappable(bool isMappable)
{
    this->isMappable = isMappable;
    return *this;
}

BufferCreation& BufferCreation::SetMemoryUsage(VmaMemoryUsage memoryUsage)
{
    this->memoryUsage = memoryUsage;
    return *this;
}

BufferCreation& BufferCreation::SetName(std::string_view name)
{
    this->name = name;
    return *this;
}

Buffer::Buffer(const BufferCreation& creation, const std::shared_ptr<VulkanContext>& vulkanContext)
    : _vulkanContext(vulkanContext)
{
    vk::BufferCreateInfo bufferInfo {};
    bufferInfo.size = creation.size;
    bufferInfo.usage = creation.usage;
    bufferInfo.sharingMode = vk::SharingMode::eExclusive;
    bufferInfo.queueFamilyIndexCount = 1;
    bufferInfo.pQueueFamilyIndices = &_vulkanContext->QueueFamilies().graphicsFamily.value();

    VmaAllocationCreateInfo allocationInfo {};
    allocationInfo.usage = creation.memoryUsage;
    if (creation.isMappable)
    {
        allocationInfo.flags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    }

    VkCheckResult(vmaCreateBuffer(_vulkanContext->MemoryAllocator(), reinterpret_cast<VkBufferCreateInfo*>(&bufferInfo), &allocationInfo, reinterpret_cast<VkBuffer*>(&buffer), &allocation, nullptr), "Failed creating buffer!");
    vmaSetAllocationName(_vulkanContext->MemoryAllocator(), allocation, creation.name.data());
    VkNameObject(buffer, creation.name, _vulkanContext);

    if (creation.isMappable)
    {
        VkCheckResult(vmaMapMemory(_vulkanContext->MemoryAllocator(), allocation, &mappedPtr),
            "Failed mapping memory for buffer: " + creation.name);
    }
}

Buffer::~Buffer()
{
    if (!_vulkanContext)
    {
        return;
    }

    if (mappedPtr)
    {
        vmaUnmapMemory(_vulkanContext->MemoryAllocator(), allocation);
    }

    vmaDestroyBuffer(_vulkanContext->MemoryAllocator(), buffer, allocation);
}

Buffer::Buffer(Buffer&& other) noexcept
    : buffer(other.buffer)
    , allocation(other.allocation)
    , mappedPtr(other.mappedPtr)
    , _vulkanContext(other._vulkanContext)
{
    other.buffer = nullptr;
    other.allocation = nullptr;
    other.mappedPtr = nullptr;
    other._vulkanContext = nullptr;
}

Buffer& Buffer::operator=(Buffer&& other) noexcept
{
    if (this == &other)
    {
        return *this;
    }

    buffer = other.buffer;
    allocation = other.allocation;
    mappedPtr = other.mappedPtr;
    _vulkanContext = other._vulkanContext;

    other.buffer = nullptr;
    other.allocation = nullptr;
    other.mappedPtr = nullptr;
    other._vulkanContext = nullptr;

    return *this;
}

Sampler::Sampler(const SamplerCreation& creation, const std::shared_ptr<VulkanContext>& vulkanContext)
    : _vulkanContext(vulkanContext)
{
    vk::StructureChain<vk::SamplerCreateInfo, vk::SamplerReductionModeCreateInfo> structureChain {};
    vk::SamplerCreateInfo& createInfo = structureChain.get<vk::SamplerCreateInfo>();
    if (creation.useMaxAnisotropy)
    {
        auto properties = _vulkanContext->PhysicalDevice().getProperties();
        createInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
    }

    vk::SamplerReductionModeCreateInfo& reductionModeCreateInfo { structureChain.get<vk::SamplerReductionModeCreateInfo>() };
    reductionModeCreateInfo.reductionMode = creation.reductionMode;

    createInfo.addressModeU = creation.addressModeU;
    createInfo.addressModeV = creation.addressModeV;
    createInfo.addressModeW = creation.addressModeW;
    createInfo.mipmapMode = creation.mipmapMode;
    createInfo.minLod = creation.minLod;
    createInfo.maxLod = creation.maxLod;
    createInfo.compareOp = creation.compareOp;
    createInfo.compareEnable = creation.compareEnable;
    createInfo.unnormalizedCoordinates = creation.unnormalizedCoordinates;
    createInfo.mipLodBias = creation.mipLodBias;
    createInfo.borderColor = creation.borderColor;
    createInfo.minFilter = creation.minFilter;
    createInfo.magFilter = creation.magFilter;

    sampler = _vulkanContext->Device().createSampler(createInfo);
    VkNameObject(sampler, creation.name, _vulkanContext);
}

Sampler::~Sampler()
{
    if (!_vulkanContext)
    {
        return;
    }

    _vulkanContext->Device().destroy(sampler);
}

Sampler::Sampler(Sampler&& other) noexcept
    : sampler(other.sampler)
    , _vulkanContext(other._vulkanContext)
{
    other.sampler = nullptr;
    other._vulkanContext = nullptr;
}

Sampler& Sampler::operator=(Sampler&& other) noexcept
{
    if (this == &other)
    {
        return *this;
    }

    sampler = other.sampler;
    _vulkanContext = other._vulkanContext;

    other.sampler = nullptr;
    other._vulkanContext = nullptr;

    return *this;
}

ImageCreation& ImageCreation::SetData(const std::vector<std::byte>& data)
{
    this->data = data;
    return *this;
}

ImageCreation& ImageCreation::SetSize(uint32_t width, uint32_t height)
{
    this->width = width;
    this->height = height;
    return *this;
}

ImageCreation& ImageCreation::SetFormat(vk::Format format)
{
    this->format = format;
    return *this;
}

ImageCreation& ImageCreation::SetUsageFlags(vk::ImageUsageFlags usage)
{
    this->usage = usage;
    return *this;
}

ImageCreation& ImageCreation::SetName(std::string_view name)
{
    this->name = name;
    return *this;
}

Image::Image(const ImageCreation& creation, const std::shared_ptr<VulkanContext>& vulkanContext)
    : format(creation.format)
    , _vulkanContext(vulkanContext)
{
    vk::ImageCreateInfo imageCreateInfo {};
    imageCreateInfo.imageType = vk::ImageType::e2D;
    imageCreateInfo.extent.width = creation.width;
    imageCreateInfo.extent.height = creation.height;
    imageCreateInfo.extent.depth = 1;
    imageCreateInfo.mipLevels = 1;
    imageCreateInfo.arrayLayers = 1;
    imageCreateInfo.format = creation.format;
    imageCreateInfo.tiling = vk::ImageTiling::eOptimal;
    imageCreateInfo.initialLayout = vk::ImageLayout::eUndefined;
    imageCreateInfo.sharingMode = vk::SharingMode::eExclusive;
    imageCreateInfo.samples = vk::SampleCountFlagBits::e1;
    imageCreateInfo.usage = creation.usage;

    if (!creation.data.empty())
    {
        imageCreateInfo.usage |= vk::ImageUsageFlagBits::eTransferDst;
    }

    VmaAllocationCreateInfo allocCreateInfo {};
    allocCreateInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    vmaCreateImage(_vulkanContext->MemoryAllocator(), reinterpret_cast<VkImageCreateInfo*>(&imageCreateInfo), &allocCreateInfo, reinterpret_cast<VkImage*>(&image), &allocation, nullptr);
    std::string allocName = creation.name + " texture allocation";
    vmaSetAllocationName(_vulkanContext->MemoryAllocator(), allocation, allocName.c_str());

    vk::ImageViewCreateInfo viewCreateInfo {};
    viewCreateInfo.image = image;
    viewCreateInfo.viewType = vk::ImageViewType::e2D;
    viewCreateInfo.format = creation.format;
    viewCreateInfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    viewCreateInfo.subresourceRange.baseMipLevel = 0;
    viewCreateInfo.subresourceRange.levelCount = 1;
    viewCreateInfo.subresourceRange.baseArrayLayer = 0;
    viewCreateInfo.subresourceRange.layerCount = 1;
    view = _vulkanContext->Device().createImageView(viewCreateInfo);

    if (!creation.data.empty())
    {
        vk::DeviceSize imageSize = creation.width * creation.height * 4;

        if (VkIsFloatingPoint(format))
        {
            imageSize *= sizeof(float);
        }

        BufferCreation stagingBufferCreation {};
        stagingBufferCreation.SetName("Image staging buffer")
            .SetSize(imageSize)
            .SetMemoryUsage(VMA_MEMORY_USAGE_CPU_ONLY)
            .SetIsMappable(true)
            .SetUsageFlags(vk::BufferUsageFlagBits::eTransferSrc);
        Buffer stagingBuffer(stagingBufferCreation, _vulkanContext);
        memcpy(stagingBuffer.mappedPtr, creation.data.data(), imageSize);

        SingleTimeCommands commands(_vulkanContext);
        commands.Record([&](vk::CommandBuffer commandBuffer)
            {
            VkTransitionImageLayout(commandBuffer, image, format, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);
            VkCopyBufferToImage(commandBuffer, stagingBuffer.buffer, image, creation.width, creation.height);
            VkTransitionImageLayout(commandBuffer, image, format, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal); });
        commands.SubmitAndWait();
    }

    VkNameObject(image, creation.name, _vulkanContext);
}

Image::~Image()
{
    if (!_vulkanContext)
    {
        return;
    }

    _vulkanContext->Device().destroy(view);
    vmaDestroyImage(_vulkanContext->MemoryAllocator(), image, allocation);
}

Image::Image(Image&& other) noexcept
    : image(other.image)
    , view(other.view)
    , allocation(other.allocation)
    , format(other.format)
    , _vulkanContext(other._vulkanContext)
{
    other.image = nullptr;
    other.view = nullptr;
    other.allocation = nullptr;
    other._vulkanContext = nullptr;
}

Image& Image::operator=(Image&& other) noexcept
{
    if (this == &other)
    {
        return *this;
    }

    image = other.image;
    view = other.view;
    allocation = other.allocation;
    format = other.format;
    _vulkanContext = other._vulkanContext;

    other.image = nullptr;
    other.view = nullptr;
    other.allocation = nullptr;
    other._vulkanContext = nullptr;

    return *this;
}

MaterialCreation& MaterialCreation::SetAlbedoMap(ResourceHandle<Image> albedoMap)
{
    this->albedoMap = albedoMap;
    return *this;
}

MaterialCreation& MaterialCreation::SetAlbedoFactor(const glm::vec4& albedoFactor)
{
    this->albedoFactor = albedoFactor;
    return *this;
}

MaterialCreation& MaterialCreation::SetAlbedoUVChannel(uint32_t albedoUVChannel)
{
    this->albedoUVChannel = albedoUVChannel;
    return *this;
}

MaterialCreation& MaterialCreation::SetMetallicRoughnessMap(ResourceHandle<Image> metallicRoughnessMap)
{
    this->metallicRoughnessMap = metallicRoughnessMap;
    return *this;
}

MaterialCreation& MaterialCreation::SetMetallicFactor(float metallicFactor)
{
    this->metallicFactor = metallicFactor;
    return *this;
}

MaterialCreation& MaterialCreation::SetRoughnessFactor(float roughnessFactor)
{
    this->roughnessFactor = roughnessFactor;
    return *this;
}

MaterialCreation& MaterialCreation::SetMetallicRoughnessUVChannel(uint32_t metallicRoughnessUVChannel)
{
    this->metallicRoughnessUVChannel = metallicRoughnessUVChannel;
    return *this;
}

MaterialCreation& MaterialCreation::SetNormalMap(ResourceHandle<Image> normalMap)
{
    this->normalMap = normalMap;
    return *this;
}

MaterialCreation& MaterialCreation::SetNormalScale(float normalScale)
{
    this->normalScale = normalScale;
    return *this;
}

MaterialCreation& MaterialCreation::SetNormalUVChannel(uint32_t normalUVChannel)
{
    this->normalUVChannel = normalUVChannel;
    return *this;
}

MaterialCreation& MaterialCreation::SetOcclusionMap(ResourceHandle<Image> occlusionMap)
{
    this->occlusionMap = occlusionMap;
    return *this;
}

MaterialCreation& MaterialCreation::SetOcclusionStrength(float occlusionStrength)
{
    this->occlusionStrength = occlusionStrength;
    return *this;
}

MaterialCreation& MaterialCreation::SetOcclusionUVChannel(uint32_t occlusionUVChannel)
{
    this->occlusionUVChannel = occlusionUVChannel;
    return *this;
}

MaterialCreation& MaterialCreation::SetEmissiveMap(ResourceHandle<Image> emissiveMap)
{
    this->emissiveMap = emissiveMap;
    return *this;
}

MaterialCreation& MaterialCreation::SetEmissiveFactor(const glm::vec3& emissiveFactor)
{
    this->emissiveFactor = emissiveFactor;
    return *this;
}

MaterialCreation& MaterialCreation::SetEmissiveUVChannel(uint32_t emissiveUVChannel)
{
    this->emissiveUVChannel = emissiveUVChannel;
    return *this;
}

Material::Material(const MaterialCreation& creation)
{
    useAlbedoMap = !creation.albedoMap.IsNull();
    useMetallicRoughnessMap = !creation.metallicRoughnessMap.IsNull();
    useNormalMap = !creation.normalMap.IsNull();
    useOcclusionMap = !creation.occlusionMap.IsNull();
    useEmissiveMap = !creation.emissiveMap.IsNull();

    albedoMapIndex = creation.albedoMap.handle;
    metallicRoughnessMapIndex = creation.metallicRoughnessMap.handle;
    normalMapIndex = creation.normalMap.handle;
    occlusionMapIndex = creation.occlusionMap.handle;
    emissiveMapIndex = creation.emissiveMap.handle;

    albedoFactor = creation.albedoFactor;
    metallicFactor = creation.metallicFactor;
    roughnessFactor = creation.roughnessFactor;
    normalScale = creation.normalScale;
    occlusionStrength = creation.occlusionStrength;
    emissiveFactor = creation.emissiveFactor;

    transparency = creation.transparency;
    ior = creation.ior;
}

GeometryNode::GeometryNode(const GeometryNodeCreation& creation)
{
    primitiveBufferDeviceAddress = creation.primitiveBufferDeviceAddress;
    indexBufferDeviceAddress = creation.indexBufferDeviceAddress;
    materialIndex = creation.material.handle;
}
