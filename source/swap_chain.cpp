#include "swap_chain.hpp"
#include "vk_common.hpp"
#include "vulkan_context.hpp"

SwapChain::SwapChain(std::shared_ptr<VulkanContext> vulkanContext, glm::uvec2 screenSize)
    : _vulkanContext(vulkanContext)
{
    InitializeSwapChain(screenSize);
}

SwapChain::~SwapChain()
{
    CleanUp();
}

SwapChain::SupportDetails SwapChain::QuerySupport(vk::PhysicalDevice device, vk::SurfaceKHR surface)
{
    SupportDetails details {};

    VkCheckResult(device.getSurfaceCapabilitiesKHR(surface, &details.capabilities), "Failed getting surface capabilities from physical device!");

    details.formats = device.getSurfaceFormatsKHR(surface);
    details.presentModes = device.getSurfacePresentModesKHR(surface);

    return details;
}

void SwapChain::InitializeSwapChain(glm::uvec2 screenSize)
{
    SupportDetails swapChainSupport = QuerySupport(_vulkanContext->PhysicalDevice(), _vulkanContext->Surface());

    auto surfaceFormat = ChooseSwapSurfaceFormat(swapChainSupport.formats);
    auto presentMode = ChoosePresentMode(swapChainSupport.presentModes);
    auto extent = ChooseSwapExtent(swapChainSupport.capabilities, screenSize);

    uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
    if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount)
    {
        imageCount = swapChainSupport.capabilities.maxImageCount;
    }

    vk::SwapchainCreateInfoKHR createInfo {};
    createInfo.surface = _vulkanContext->Surface();
    createInfo.minImageCount = imageCount + 1;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = vk::ImageUsageFlagBits::eColorAttachment; // TODO: Can change this later to a memory transfer operation, when doing post-processing.
    if (swapChainSupport.capabilities.supportedUsageFlags & vk::ImageUsageFlagBits::eTransferSrc)
    {
        createInfo.imageUsage |= vk::ImageUsageFlagBits::eTransferSrc;
    }
    if (swapChainSupport.capabilities.supportedUsageFlags & vk::ImageUsageFlagBits::eTransferDst)
    {
        createInfo.imageUsage |= vk::ImageUsageFlagBits::eTransferDst;
    }

    uint32_t queueFamilyIndices[] = { _vulkanContext->QueueFamilies().graphicsFamily.value(), _vulkanContext->QueueFamilies().presentFamily.value() };
    if (_vulkanContext->QueueFamilies().graphicsFamily != _vulkanContext->QueueFamilies().presentFamily)
    {
        createInfo.imageSharingMode = vk::SharingMode::eConcurrent;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    }
    else
    {
        createInfo.imageSharingMode = vk::SharingMode::eExclusive;
        createInfo.queueFamilyIndexCount = 0;
        createInfo.pQueueFamilyIndices = nullptr;
    }

    createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
    createInfo.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
    createInfo.presentMode = presentMode;
    createInfo.clipped = vk::True;
    createInfo.oldSwapchain = nullptr;

    VkCheckResult(_vulkanContext->Device().createSwapchainKHR(&createInfo, nullptr, &_swapChain), "Failed creating swap chain!");
    VkNameObject(_swapChain, "Swapchain", _vulkanContext);

    _images = _vulkanContext->Device().getSwapchainImagesKHR(_swapChain);
    _format = surfaceFormat.format;
    _extent = extent;

    InitializeImageViews();
}

void SwapChain::CleanUp()
{
    for (auto imageView : _imageViews)
    {
        _vulkanContext->Device().destroy(imageView);
    }

    _vulkanContext->Device().destroy(_swapChain);
}

void SwapChain::InitializeImageViews()
{
    _imageViews.resize(_images.size());

    for (size_t i = 0; i < _imageViews.size(); ++i)
    {
        vk::ImageViewCreateInfo createInfo {};
        createInfo.flags = vk::ImageViewCreateFlags {},
        createInfo.image = _images[i],
        createInfo.viewType = vk::ImageViewType::e2D,
        createInfo.format = _format,
        createInfo.components = vk::ComponentMapping { vk::ComponentSwizzle::eIdentity, vk::ComponentSwizzle::eIdentity, vk::ComponentSwizzle::eIdentity, vk::ComponentSwizzle::eIdentity },
        createInfo.subresourceRange = vk::ImageSubresourceRange {
            vk::ImageAspectFlagBits::eColor, // aspect mask
            0, // base mip level
            1, // level count
            0, // base array level
            1 // layer count
        };
        VkCheckResult(_vulkanContext->Device().createImageView(&createInfo, nullptr, &_imageViews[i]), "Failed creating image view for swap chain!");
        VkNameObject(_imageViews[i], "Swapchain Image View", _vulkanContext);
        VkNameObject(_images[i], "Swapchain Image", _vulkanContext);
    }
}

vk::SurfaceFormatKHR SwapChain::ChooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& availableFormats) const
{
    for (const auto& format : availableFormats)
    {
        if (format.format == vk::Format::eB8G8R8A8Unorm && format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear)
        {
            return format;
        }
    }

    return availableFormats[0];
}

vk::PresentModeKHR SwapChain::ChoosePresentMode(const std::vector<vk::PresentModeKHR>& availablePresentModes) const
{
    auto it = std::find_if(availablePresentModes.begin(), availablePresentModes.end(),
        [](const auto& mode)
        { return mode == vk::PresentModeKHR::eMailbox; });
    if (it != availablePresentModes.end())
    {
        return *it;
    }

    it = std::find_if(availablePresentModes.begin(), availablePresentModes.end(),
        [](const auto& mode)
        { return mode == vk::PresentModeKHR::eFifo; });
    if (it != availablePresentModes.end())
    {
        return *it;
    }

    return availablePresentModes[0];
}

vk::Extent2D SwapChain::ChooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities, glm::uvec2 screenSize) const
{
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
    {
        return capabilities.currentExtent;
    }

    vk::Extent2D extent = { screenSize.x, screenSize.y };
    extent.width = std::clamp(extent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
    extent.height = std::clamp(extent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

    return extent;
}
