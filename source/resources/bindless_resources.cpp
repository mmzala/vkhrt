#include "resources/bindless_resources.hpp"
#include "single_time_commands.hpp"
#include "vk_common.hpp"
#include "vulkan_context.hpp"
#include <spdlog/spdlog.h>

ImageResources::ImageResources(const std::shared_ptr<VulkanContext>& vulkanContext)
    : _vulkanContext(vulkanContext)
{
}

ResourceHandle<Image> ImageResources::Create(const ImageCreation& creation)
{
    return ResourceManager::Create(Image(creation, _vulkanContext));
}

MaterialResources::MaterialResources(const std::shared_ptr<VulkanContext>& vulkanContext)
    : _vulkanContext(vulkanContext)
{
}

ResourceHandle<Material> MaterialResources::Create(const MaterialCreation& creation)
{
    return ResourceManager::Create(Material(creation));
}

ResourceHandle<GeometryNode> GeometryNodeResources::Create(const GeometryNodeCreation& creation)
{
    // TODO: Fallback material
    return ResourceManager::Create(GeometryNode(creation));
}

ResourceHandle<BLASInstance> BLASInstanceResources::Create(const BLASInstanceCreation& creation)
{
    return ResourceManager::Create(BLASInstance(creation));
}

BindlessResources::BindlessResources(const std::shared_ptr<VulkanContext>& vulkanContext)
    : _vulkanContext(vulkanContext)
    , _imageResources(vulkanContext)
    , _materialResources(vulkanContext)
{
    InitializeSet();
    InitializeMaterialBuffer();
    InitializeGeometryNodeBuffer();
    InitializeBLASInstanceBuffer();

    SamplerCreation fallbackSamplerCreation {};
    fallbackSamplerCreation.name = "Fallback sampler";
    _fallbackSampler = std::make_unique<Sampler>(fallbackSamplerCreation, _vulkanContext);

    constexpr uint32_t size = 2;
    std::vector<std::byte> data {};
    data.assign(size * size * 4, std::byte {});
    ImageCreation fallbackImageCreation {};
    fallbackImageCreation.SetName("Fallback texture")
        .SetSize(size, size)
        .SetUsageFlags(vk::ImageUsageFlagBits::eSampled)
        .SetFormat(vk::Format::eR8G8B8A8Unorm)
        .SetData(data);
    _fallbackImage = _imageResources.Create(fallbackImageCreation);
}

BindlessResources::~BindlessResources()
{
    _vulkanContext->Device().destroy(_bindlessLayout);
}

void BindlessResources::UpdateDescriptorSet()
{
    UploadImages();
    UploadMaterials();
    UploadGeometryNodes();
    UploadBLASInstances();
}

void BindlessResources::UploadImages()
{
    if (_imageResources.GetAll().empty())
    {
        return;
    }

    if (_imageResources.GetAll().size() > MAX_RESOURCES)
    {
        spdlog::error("[RESOURCES] Too many images to fit into the bindless set");
        return;
    }

    std::array<vk::DescriptorImageInfo, MAX_RESOURCES> imageInfos {};
    std::array<vk::WriteDescriptorSet, MAX_RESOURCES> descriptorWrites {};

    for (uint32_t i = 0; i < MAX_RESOURCES; ++i)
    {
        const Image& image = _imageResources.GetAll().size() > i ? _imageResources.GetAll()[i] : _imageResources.Get(_fallbackImage);

        vk::DescriptorImageInfo& imageInfo = imageInfos.at(i);
        imageInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        imageInfo.imageView = image.view;
        imageInfo.sampler = _fallbackSampler->sampler;

        vk::WriteDescriptorSet& descriptorWrite = descriptorWrites.at(i);
        descriptorWrite.dstSet = _bindlessSet;
        descriptorWrite.dstBinding = static_cast<uint32_t>(BindlessBinding::eImages);
        descriptorWrite.dstArrayElement = i;
        descriptorWrite.descriptorType = vk::DescriptorType::eCombinedImageSampler;
        descriptorWrite.descriptorCount = 1;
        descriptorWrite.pImageInfo = &imageInfo;
    }

    _vulkanContext->Device().updateDescriptorSets(MAX_RESOURCES, descriptorWrites.data(), 0, nullptr);
}

void BindlessResources::UploadMaterials()
{
    if (_materialResources.GetAll().empty())
    {
        return;
    }

    if (_materialResources.GetAll().size() > MAX_RESOURCES)
    {
        spdlog::error("[RESOURCES] Material buffer is too small to fit all of the available materials");
        return;
    }

    // TODO: Transfer to host memory
    std::memcpy(_materialBuffer->mappedPtr, _materialResources.GetAll().data(), _materialResources.GetAll().size() * sizeof(Material));

    vk::DescriptorBufferInfo bufferInfo {};
    bufferInfo.buffer = _materialBuffer->buffer;
    bufferInfo.offset = 0;
    bufferInfo.range = sizeof(Material) * _materialResources.GetAll().size();

    vk::WriteDescriptorSet descriptorWrite {};
    descriptorWrite.dstSet = _bindlessSet;
    descriptorWrite.dstBinding = static_cast<uint32_t>(BindlessBinding::eMaterials);
    descriptorWrite.dstArrayElement = 0;
    descriptorWrite.descriptorType = vk::DescriptorType::eUniformBuffer;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pBufferInfo = &bufferInfo;

    _vulkanContext->Device().updateDescriptorSets(1, &descriptorWrite, 0, nullptr);
}

void BindlessResources::UploadGeometryNodes()
{
    if (_geometryNodeResources.GetAll().empty())
    {
        return;
    }

    if (_geometryNodeResources.GetAll().size() > MAX_RESOURCES)
    {
        spdlog::error("[RESOURCES] Geometry node buffer is too small to fit all of the available nodes");
        return;
    }

    vk::DeviceSize bufferSize = _geometryNodeResources.GetAll().size() * sizeof(GeometryNode);
    BufferCreation stagingBufferCreation {};
    stagingBufferCreation.SetSize(bufferSize)
        .SetUsageFlags(vk::BufferUsageFlagBits::eTransferSrc)
        .SetMemoryUsage(VMA_MEMORY_USAGE_CPU_ONLY)
        .SetIsMappable(true)
        .SetName("GeometryNode staging buffer");
    Buffer stagingBuffer(stagingBufferCreation, _vulkanContext);
    std::memcpy(stagingBuffer.mappedPtr, _geometryNodeResources.GetAll().data(), bufferSize);

    SingleTimeCommands commands(_vulkanContext);
    commands.Record([&](vk::CommandBuffer commandBuffer)
        { VkCopyBufferToBuffer(commandBuffer, stagingBuffer.buffer, _geometryNodeBuffer->buffer, bufferSize); });
    commands.SubmitAndWait();

    vk::DescriptorBufferInfo bufferInfo {};
    bufferInfo.buffer = _geometryNodeBuffer->buffer;
    bufferInfo.offset = 0;
    bufferInfo.range = bufferSize;

    vk::WriteDescriptorSet descriptorWrite {};
    descriptorWrite.dstSet = _bindlessSet;
    descriptorWrite.dstBinding = static_cast<uint32_t>(BindlessBinding::eGeometryNodes);
    descriptorWrite.dstArrayElement = 0;
    descriptorWrite.descriptorType = vk::DescriptorType::eStorageBuffer;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pBufferInfo = &bufferInfo;

    _vulkanContext->Device().updateDescriptorSets(1, &descriptorWrite, 0, nullptr);
}

void BindlessResources::UploadBLASInstances()
{
    if (_blasInstanceResources.GetAll().empty())
    {
        return;
    }

    if (_blasInstanceResources.GetAll().size() > MAX_RESOURCES)
    {
        spdlog::error("[RESOURCES] BLAS instance buffer is too small to fit all of the available BLASes");
        return;
    }

    vk::DeviceSize bufferSize = _blasInstanceResources.GetAll().size() * sizeof(BLASInstance);
    BufferCreation stagingBufferCreation {};
    stagingBufferCreation.SetSize(bufferSize)
        .SetUsageFlags(vk::BufferUsageFlagBits::eTransferSrc)
        .SetMemoryUsage(VMA_MEMORY_USAGE_CPU_ONLY)
        .SetIsMappable(true)
        .SetName("BLASInstance staging buffer");
    Buffer stagingBuffer(stagingBufferCreation, _vulkanContext);
    std::memcpy(stagingBuffer.mappedPtr, _blasInstanceResources.GetAll().data(), bufferSize);

    SingleTimeCommands commands(_vulkanContext);
    commands.Record([&](vk::CommandBuffer commandBuffer)
        { VkCopyBufferToBuffer(commandBuffer, stagingBuffer.buffer, _blasInstanceBuffer->buffer, bufferSize); });
    commands.SubmitAndWait();

    vk::DescriptorBufferInfo bufferInfo {};
    bufferInfo.buffer = _blasInstanceBuffer->buffer;
    bufferInfo.offset = 0;
    bufferInfo.range = bufferSize;

    vk::WriteDescriptorSet descriptorWrite {};
    descriptorWrite.dstSet = _bindlessSet;
    descriptorWrite.dstBinding = static_cast<uint32_t>(BindlessBinding::eBLASInstances);
    descriptorWrite.dstArrayElement = 0;
    descriptorWrite.descriptorType = vk::DescriptorType::eStorageBuffer;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pBufferInfo = &bufferInfo;

    _vulkanContext->Device().updateDescriptorSets(1, &descriptorWrite, 0, nullptr);
}

void BindlessResources::InitializeSet()
{
    std::vector<vk::DescriptorSetLayoutBinding> bindings(4);

    vk::DescriptorSetLayoutBinding& combinedImageSampler = bindings[0];
    combinedImageSampler.descriptorType = vk::DescriptorType::eCombinedImageSampler;
    combinedImageSampler.descriptorCount = MAX_RESOURCES;
    combinedImageSampler.binding = static_cast<uint32_t>(BindlessBinding::eImages);
    combinedImageSampler.stageFlags = vk::ShaderStageFlagBits::eClosestHitKHR;

    vk::DescriptorSetLayoutBinding& materialBinding = bindings[1];
    materialBinding.descriptorType = vk::DescriptorType::eUniformBuffer;
    materialBinding.descriptorCount = 1;
    materialBinding.binding = static_cast<uint32_t>(BindlessBinding::eMaterials);
    materialBinding.stageFlags = vk::ShaderStageFlagBits::eClosestHitKHR;

    vk::DescriptorSetLayoutBinding& geometryNodeBinding = bindings[2];
    geometryNodeBinding.descriptorType = vk::DescriptorType::eStorageBuffer;
    geometryNodeBinding.descriptorCount = 1;
    geometryNodeBinding.binding = static_cast<uint32_t>(BindlessBinding::eGeometryNodes);
    geometryNodeBinding.stageFlags = vk::ShaderStageFlagBits::eClosestHitKHR | vk::ShaderStageFlagBits::eIntersectionKHR;

    vk::DescriptorSetLayoutBinding& blasInstanceBinding = bindings[3];
    blasInstanceBinding.descriptorType = vk::DescriptorType::eStorageBuffer;
    blasInstanceBinding.descriptorCount = 1;
    blasInstanceBinding.binding = static_cast<uint32_t>(BindlessBinding::eBLASInstances);
    blasInstanceBinding.stageFlags = vk::ShaderStageFlagBits::eClosestHitKHR | vk::ShaderStageFlagBits::eIntersectionKHR;

    vk::StructureChain<vk::DescriptorSetLayoutCreateInfo, vk::DescriptorSetLayoutBindingFlagsCreateInfo> structureChain;

    auto& layoutCreateInfo = structureChain.get<vk::DescriptorSetLayoutCreateInfo>();
    layoutCreateInfo.bindingCount = bindings.size();
    layoutCreateInfo.pBindings = bindings.data();
    layoutCreateInfo.flags = vk::DescriptorSetLayoutCreateFlagBits::eUpdateAfterBindPool;

    std::array<vk::DescriptorBindingFlagsEXT, 4> bindingFlags = {
        vk::DescriptorBindingFlagBits::ePartiallyBound | vk::DescriptorBindingFlagBits::eUpdateAfterBind,
        vk::DescriptorBindingFlagBits::ePartiallyBound | vk::DescriptorBindingFlagBits::eUpdateAfterBind,
        vk::DescriptorBindingFlagBits::ePartiallyBound | vk::DescriptorBindingFlagBits::eUpdateAfterBind,
        vk::DescriptorBindingFlagBits::ePartiallyBound | vk::DescriptorBindingFlagBits::eUpdateAfterBind,
    };

    auto& extInfo = structureChain.get<vk::DescriptorSetLayoutBindingFlagsCreateInfoEXT>();
    extInfo.bindingCount = bindings.size();
    extInfo.pBindingFlags = bindingFlags.data();

    _bindlessLayout = _vulkanContext->Device().createDescriptorSetLayout(layoutCreateInfo);

    vk::DescriptorSetAllocateInfo allocInfo {};
    allocInfo.descriptorPool = _vulkanContext->DescriptorPool();
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &_bindlessLayout;
    VkCheckResult(_vulkanContext->Device().allocateDescriptorSets(&allocInfo, &_bindlessSet), "Failed creating bindless descriptor set");

    VkNameObject(_bindlessSet, "Bindless Set", _vulkanContext);
}

void BindlessResources::InitializeMaterialBuffer()
{
    BufferCreation creation {};
    creation.SetSize(MAX_RESOURCES * sizeof(Material))
        .SetUsageFlags(vk::BufferUsageFlagBits::eUniformBuffer)
        .SetIsMappable(true)
        .SetName("Material buffer");

    _materialBuffer = std::make_unique<Buffer>(creation, _vulkanContext);
}

void BindlessResources::InitializeGeometryNodeBuffer()
{
    BufferCreation creation {};
    creation.SetSize(MAX_RESOURCES * sizeof(GeometryNode))
        .SetUsageFlags(vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst)
        .SetMemoryUsage(VMA_MEMORY_USAGE_GPU_ONLY)
        .SetIsMappable(false)
        .SetName("GeometryNode buffer");

    _geometryNodeBuffer = std::make_unique<Buffer>(creation, _vulkanContext);
}

void BindlessResources::InitializeBLASInstanceBuffer()
{
    BufferCreation creation {};
    creation.SetSize(MAX_RESOURCES * sizeof(BLASInstance))
        .SetUsageFlags(vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst)
        .SetMemoryUsage(VMA_MEMORY_USAGE_GPU_ONLY)
        .SetIsMappable(false)
        .SetName("BLASInstance buffer");

    _blasInstanceBuffer = std::make_unique<Buffer>(creation, _vulkanContext);
}
