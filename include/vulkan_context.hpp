#pragma once
#include <functional>
#include <optional>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.hpp>
#include "common.hpp"

struct VulkanInitInfo
{
    uint32_t extensionCount { 0 };
    const char* const* extensions { nullptr };
    uint32_t width {}, height {};

    std::function<vk::SurfaceKHR(vk::Instance)> retrieveSurface;
};

struct QueueFamilyIndices
{
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;

   [[nodiscard]] bool IsComplete() const;

    static QueueFamilyIndices FindQueueFamilies(vk::PhysicalDevice device, vk::SurfaceKHR surface);
};

class VulkanContext
{
public:
    explicit VulkanContext(const VulkanInitInfo& initInfo);
    ~VulkanContext();
    NON_COPYABLE(VulkanContext);
    NON_MOVABLE(VulkanContext);

    [[nodiscard]] vk::detail::DispatchLoaderDynamic Dldi() const { return _dldi; }
    [[nodiscard]] vk::Instance Instance() const { return _instance; }
    [[nodiscard]] vk::PhysicalDevice PhysicalDevice() const { return _physicalDevice; }
    [[nodiscard]] vk::Device Device() const { return _device; }
    [[nodiscard]] vk::Queue GraphicsQueue() const { return _graphicsQueue; }
    [[nodiscard]] vk::Queue PresentQueue() const { return _presentQueue; }
    [[nodiscard]] vk::SurfaceKHR Surface() const { return _surface; }
    [[nodiscard]] vk::CommandPool CommandPool() const { return _commandPool; }
    [[nodiscard]] VmaAllocator MemoryAllocator() const { return _vmaAllocator; }
    [[nodiscard]] const QueueFamilyIndices& QueueFamilies() const { return _queueFamilyIndices; }
    [[nodiscard]] vk::DescriptorPool DescriptorPool() const { return _descriptorPool; }

    [[nodiscard]] vk::PhysicalDeviceRayTracingPipelinePropertiesKHR RayTracingPipelineProperties() const;
    [[nodiscard]] uint64_t GetBufferDeviceAddress(vk::Buffer buffer) const;
    [[nodiscard]] bool IsExtensionSupported(const std::string& extension) const;

private:
    vk::Instance _instance;
    vk::detail::DispatchLoaderDynamic _dldi;
    vk::PhysicalDevice _physicalDevice;
    vk::Device _device;
    vk::Queue _graphicsQueue;
    vk::Queue _presentQueue;
    vk::CommandPool _commandPool;
    QueueFamilyIndices _queueFamilyIndices;
    VmaAllocator _vmaAllocator;
    vk::DescriptorPool _descriptorPool;

    vk::SurfaceKHR _surface;

    vk::DebugUtilsMessengerEXT _debugMessenger;
    bool _validationLayersEnabled = false;

    const std::vector<const char*> _validationLayers = {
        "VK_LAYER_KHRONOS_validation"
    };

    const std::vector<const char*> _deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
        VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
        VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME,
        VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
        VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
        VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
        VK_KHR_PIPELINE_LIBRARY_EXTENSION_NAME,
        VK_KHR_MAINTENANCE3_EXTENSION_NAME,
        VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME,
        VK_EXT_SCALAR_BLOCK_LAYOUT_EXTENSION_NAME,
        VK_KHR_SHADER_CLOCK_EXTENSION_NAME
    };

    void InitializeInstance(const VulkanInitInfo& initInfo);
    void InizializeValidationLayers();
    void InitializePhysicalDevice();
    void InitializeDevice();
    void InitializeCommandPool();
    void InitializeVMA();
    void InitializeDescriptorPool();
    [[nodiscard]] bool AreValidationLayersSupported() const;
    [[nodiscard]] std::vector<const char*> GetRequiredInstanceExtensions(const VulkanInitInfo& initInfo) const;
    [[nodiscard]] uint32_t RateDeviceSuitability(const vk::PhysicalDevice& deviceToRate) const;
    [[nodiscard]] bool AreExtensionsSupported(const vk::PhysicalDevice& deviceToCheckSupport) const;
};
