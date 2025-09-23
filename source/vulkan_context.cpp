#include "vulkan_context.hpp"
#include "swap_chain.hpp"
#include "vk_common.hpp"
#include <map>
#include <set>
#include <spdlog/spdlog.h>

VKAPI_ATTR VkBool32 VKAPI_CALL ValidationLayerCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    [[maybe_unused]] void* pUserData)
{
    static std::string type {};
    switch (messageType)
    {
    case VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT:
        type = "[GENERAL]";
        break;
    case VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT:
        type = "[VALIDATION]";
        break;
    case VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT:
        type = "[PERFORMANCE]";
        break;
    default:
        break;
    }

    spdlog::level::level_enum logLevel {};
    switch (messageSeverity)
    {
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
        logLevel = spdlog::level::level_enum::trace;
        break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
        logLevel = spdlog::level::level_enum::info;
        break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
        logLevel = spdlog::level::level_enum::warn;
        break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
        logLevel = spdlog::level::level_enum::err;
        break;
    default:
        break;
    }

    if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
    {
        spdlog::log(logLevel, "[VULKAN] {} {}", type, pCallbackData->pMessage);
    }

    return VK_FALSE;
}

bool QueueFamilyIndices::IsComplete() const
{
    return graphicsFamily.has_value() && presentFamily.has_value();
}

QueueFamilyIndices QueueFamilyIndices::FindQueueFamilies(vk::PhysicalDevice device, vk::SurfaceKHR surface)
{
    QueueFamilyIndices indices {};

    uint32_t queueFamilyCount { 0 };
    device.getQueueFamilyProperties(&queueFamilyCount, nullptr);

    std::vector<vk::QueueFamilyProperties> queueFamilies(queueFamilyCount);
    device.getQueueFamilyProperties(&queueFamilyCount, queueFamilies.data());

    for (size_t i = 0; i < queueFamilies.size(); ++i)
    {
        if (queueFamilies[i].queueFlags & vk::QueueFlagBits::eGraphics)
        {
            indices.graphicsFamily = i;
        }

        if (!indices.presentFamily.has_value())
        {
            vk::Bool32 supported;
            VkCheckResult(device.getSurfaceSupportKHR(i, surface, &supported),
                "Failed querying surface support on physical device!");
            if (supported)
            {
                indices.presentFamily = i;
            }
        }

        if (indices.IsComplete())
        {
            break;
        }
    }

    return indices;
}

VulkanContext::VulkanContext(const VulkanInitInfo& initInfo)
{
    _validationLayersEnabled = AreValidationLayersSupported();
    spdlog::info("[VULKAN] Validation layers enabled: {}", _validationLayersEnabled);

    InitializeInstance(initInfo);
    _dldi = vk::detail::DispatchLoaderDynamic { _instance, vkGetInstanceProcAddr, _device, vkGetDeviceProcAddr };
    InizializeValidationLayers();
    _surface = initInfo.retrieveSurface(_instance);

    InitializePhysicalDevice();
    InitializeDevice();
    InitializeCommandPool();
    InitializeVMA();
}

VulkanContext::~VulkanContext()
{
    if (_validationLayersEnabled)
    {
        _instance.destroyDebugUtilsMessengerEXT(_debugMessenger, nullptr, _dldi);
    }

    vmaDestroyAllocator(_vmaAllocator);
    _instance.destroy(_surface);
    _device.destroy();
    _instance.destroy();
}

vk::PhysicalDeviceRayTracingPipelinePropertiesKHR VulkanContext::RayTracingPipelineProperties() const
{
    vk::PhysicalDeviceRayTracingPipelinePropertiesKHR rayTracingPipelineProperties {};
    vk::PhysicalDeviceProperties2KHR physicalDeviceProperties {};
    physicalDeviceProperties.pNext = &rayTracingPipelineProperties;
    _physicalDevice.getProperties2(&physicalDeviceProperties);
    return rayTracingPipelineProperties;
}

uint64_t VulkanContext::GetBufferDeviceAddress(vk::Buffer buffer) const
{
    vk::BufferDeviceAddressInfoKHR bufferDeviceAI {};
    bufferDeviceAI.buffer = buffer;
    return _device.getBufferAddressKHR(&bufferDeviceAI, _dldi);
}

void VulkanContext::InitializeInstance(const VulkanInitInfo& initInfo)
{
    vk::ApplicationInfo appInfo {};
    appInfo.pApplicationName = "Ray Tracing";
    appInfo.applicationVersion = vk::makeApiVersion(0, 0, 0, 0);
    appInfo.pEngineName = "Ray Tracer";
    appInfo.engineVersion = vk::makeApiVersion(0, 1, 0, 0);
    appInfo.apiVersion = vk::makeApiVersion(0, 1, 3, 0);

    auto extensions = GetRequiredInstanceExtensions(initInfo);

    vk::InstanceCreateInfo instanceInfo {};
    instanceInfo.flags = vk::InstanceCreateFlags {};
    instanceInfo.pApplicationInfo = &appInfo;
    instanceInfo.enabledExtensionCount = extensions.size();
    instanceInfo.ppEnabledExtensionNames = extensions.data();

    if (_validationLayersEnabled)
    {
        instanceInfo.enabledLayerCount = _validationLayers.size();
        instanceInfo.ppEnabledLayerNames = _validationLayers.data();
    }
    else
    {
        instanceInfo.enabledLayerCount = 0;
        instanceInfo.ppEnabledLayerNames = nullptr;
    }

    VkCheckResult(vk::createInstance(&instanceInfo, nullptr, &_instance), "Failed to create instance");
}

void VulkanContext::InizializeValidationLayers()
{
    if (!_validationLayersEnabled)
    {
        return;
    }

    vk::DebugUtilsMessengerCreateInfoEXT debugMessengerInfo {};
    debugMessengerInfo.messageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose | vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo | vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError;
    debugMessengerInfo.messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance;
    debugMessengerInfo.pfnUserCallback = reinterpret_cast<vk::PFN_DebugUtilsMessengerCallbackEXT>(ValidationLayerCallback); // Cannot directly set as compiler thinks the function signatures don't match
    debugMessengerInfo.pUserData = nullptr;

    VkCheckResult(_instance.createDebugUtilsMessengerEXT(&debugMessengerInfo, nullptr, &_debugMessenger, _dldi), "Failed to create debug messenger for validation layers");
}

void VulkanContext::InitializePhysicalDevice()
{
    std::vector<vk::PhysicalDevice> devices = _instance.enumeratePhysicalDevices();
    if (devices.empty())
    {
        spdlog::info("[VULKAN] No GPU's with Vulkan support available!");
    }

    std::multimap<int, vk::PhysicalDevice> candidates {};

    for (const auto& device : devices)
    {
        uint32_t score = RateDeviceSuitability(device);
        if (score > 0)
        {
            candidates.emplace(score, device);
        }
    }

    if (candidates.empty())
    {
        spdlog::error("[VULKAN] Failed finding suitable device!");
    }

    _physicalDevice = candidates.rbegin()->second;
}

void VulkanContext::InitializeDevice()
{
    _queueFamilyIndices = QueueFamilyIndices::FindQueueFamilies(_physicalDevice, _surface);
    std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos {};
    std::set<uint32_t> uniqueQueueFamilies = { _queueFamilyIndices.graphicsFamily.value(), _queueFamilyIndices.presentFamily.value() };
    float queuePriority = 1.0f;

    for (uint32_t familyQueueIndex : uniqueQueueFamilies)
    {
        vk::DeviceQueueCreateInfo queueCreateInfo {};
        queueCreateInfo.flags = vk::DeviceQueueCreateFlags {};
        queueCreateInfo.queueFamilyIndex = familyQueueIndex;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    vk::StructureChain<vk::DeviceCreateInfo, vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceSynchronization2Features, vk::PhysicalDeviceDescriptorIndexingFeatures,
        vk::PhysicalDeviceScalarBlockLayoutFeatures, vk::PhysicalDeviceBufferDeviceAddressFeatures, vk::PhysicalDeviceAccelerationStructureFeaturesKHR,
        vk::PhysicalDeviceRayTracingPipelineFeaturesKHR, vk::PhysicalDeviceShaderClockFeaturesKHR>
        structureChain;

    auto& shaderClockFeatures = structureChain.get<vk::PhysicalDeviceShaderClockFeaturesKHR>();
    shaderClockFeatures.shaderSubgroupClock = true;

    auto& rayTracingPipelineFeatures = structureChain.get<vk::PhysicalDeviceRayTracingPipelineFeaturesKHR>();
    rayTracingPipelineFeatures.rayTracingPipeline = true;

    auto& accelerationStructuresFeatures = structureChain.get<vk::PhysicalDeviceAccelerationStructureFeaturesKHR>();
    accelerationStructuresFeatures.accelerationStructure = true;

    auto& deviceAddressFeatures = structureChain.get<vk::PhysicalDeviceBufferDeviceAddressFeatures>();
    deviceAddressFeatures.bufferDeviceAddress = true;

    auto& scalarBlockLayoutFeatures = structureChain.get<vk::PhysicalDeviceScalarBlockLayoutFeatures>();
    scalarBlockLayoutFeatures.scalarBlockLayout = true;

    auto& indexingFeatures = structureChain.get<vk::PhysicalDeviceDescriptorIndexingFeatures>();
    indexingFeatures.descriptorBindingPartiallyBound = true;

    auto& synchronization2Features = structureChain.get<vk::PhysicalDeviceSynchronization2Features>();
    synchronization2Features.synchronization2 = true;

    auto& deviceFeatures = structureChain.get<vk::PhysicalDeviceFeatures2>();
    _physicalDevice.getFeatures2(&deviceFeatures);

    auto& createInfo = structureChain.get<vk::DeviceCreateInfo>();
    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.pEnabledFeatures = nullptr;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(_deviceExtensions.size());
    createInfo.ppEnabledExtensionNames = _deviceExtensions.data();

    VkCheckResult(_physicalDevice.createDevice(&createInfo, nullptr, &_device), "Failed creating a logical device!");

    _device.getQueue(_queueFamilyIndices.graphicsFamily.value(), 0, &_graphicsQueue);
    _device.getQueue(_queueFamilyIndices.presentFamily.value(), 0, &_presentQueue);
}

void VulkanContext::InitializeCommandPool()
{
    vk::CommandPoolCreateInfo commandPoolCreateInfo {};
    commandPoolCreateInfo.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
    commandPoolCreateInfo.queueFamilyIndex = _queueFamilyIndices.graphicsFamily.value();

    VkCheckResult(_device.createCommandPool(&commandPoolCreateInfo, nullptr, &_commandPool), "Failed creating command pool!");
}

void VulkanContext::InitializeVMA()
{
    VmaVulkanFunctions vulkanFunctions = {};
    vulkanFunctions.vkGetInstanceProcAddr = &vkGetInstanceProcAddr;
    vulkanFunctions.vkGetDeviceProcAddr = &vkGetDeviceProcAddr;

    VmaAllocatorCreateInfo vmaAllocatorCreateInfo {};
    vmaAllocatorCreateInfo.physicalDevice = _physicalDevice;
    vmaAllocatorCreateInfo.device = _device;
    vmaAllocatorCreateInfo.instance = _instance;
    vmaAllocatorCreateInfo.vulkanApiVersion = vk::makeApiVersion(0, 1, 3, 0);
    vmaAllocatorCreateInfo.pVulkanFunctions = &vulkanFunctions;
    vmaAllocatorCreateInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;

    VkCheckResult(vmaCreateAllocator(&vmaAllocatorCreateInfo, &_vmaAllocator), "Failed creating VMA allocator!");
}

bool VulkanContext::AreValidationLayersSupported() const
{
    std::vector<vk::LayerProperties> availableLayers = vk::enumerateInstanceLayerProperties();
    bool result = std::all_of(_validationLayers.begin(), _validationLayers.end(), [&availableLayers](const auto& layerName)
        {
        const auto it = std::find_if(availableLayers.begin(), availableLayers.end(), [&layerName](const auto &layer)
        { return strcmp(layerName, layer.layerName) == 0; });

        return it != availableLayers.end(); });

    return result;
}

std::vector<const char*> VulkanContext::GetRequiredInstanceExtensions(const VulkanInitInfo& initInfo) const
{
    std::vector<const char*> extensions(initInfo.extensions, initInfo.extensions + initInfo.extensionCount);
    if (_validationLayersEnabled)
    {
        extensions.emplace_back(vk::EXTDebugUtilsExtensionName);
    }

    return extensions;
}

uint32_t VulkanContext::RateDeviceSuitability(const vk::PhysicalDevice& deviceToRate) const
{
    vk::PhysicalDeviceFeatures2 deviceFeatures;
    deviceToRate.getFeatures2(&deviceFeatures);

    vk::PhysicalDeviceProperties deviceProperties;
    deviceToRate.getProperties(&deviceProperties);

    QueueFamilyIndices familyIndices = QueueFamilyIndices::FindQueueFamilies(deviceToRate, _surface);

    // Failed if graphics family queue is not supported
    if (!familyIndices.IsComplete())
    {
        return 0;
    }

    // Failed if the extensions needed are not supported
    if (!AreExtensionsSupported(deviceToRate))
    {
        return 0;
    }

    // Check support for swap chain
    SwapChain::SupportDetails swapChainSupportDetails = SwapChain::QuerySupport(deviceToRate, _surface);
    bool swapChainUnsupported = swapChainSupportDetails.formats.empty() || swapChainSupportDetails.presentModes.empty();
    if (swapChainUnsupported)
    {
        return 0;
    }

    uint32_t score = 0;

    // Favor discrete GPUs above all else
    if (deviceProperties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu)
    {
        score += 50000;
    }

    // Slightly favor integrated GPUs
    if (deviceProperties.deviceType == vk::PhysicalDeviceType::eIntegratedGpu)
    {
        score += 20000;
    }

    score += deviceProperties.limits.maxImageDimension2D;

    return score;
}

bool VulkanContext::AreExtensionsSupported(const vk::PhysicalDevice& deviceToCheckSupport) const
{
    std::vector<vk::ExtensionProperties> availableExtensions = deviceToCheckSupport.enumerateDeviceExtensionProperties();
    std::set<std::string> requiredExtensions { _deviceExtensions.begin(), _deviceExtensions.end() };
    for (const auto& extension : availableExtensions)
    {
        requiredExtensions.erase(extension.extensionName);
    }

    return requiredExtensions.empty();
}
