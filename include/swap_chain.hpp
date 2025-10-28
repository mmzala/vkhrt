#pragma once
#include "common.hpp"
#include <vulkan/vulkan.hpp>
#include <glm/glm.hpp>

class VulkanContext;

class SwapChain
{
public:
    struct SupportDetails
    {
        vk::SurfaceCapabilitiesKHR capabilities;
        std::vector<vk::SurfaceFormatKHR> formats;
        std::vector<vk::PresentModeKHR> presentModes;
    };

    SwapChain(std::shared_ptr<VulkanContext> vulkanContext, glm::uvec2 screenSize);
    ~SwapChain();
    NON_COPYABLE(SwapChain);
    NON_MOVABLE(SwapChain);

    [[nodiscard]] vk::SwapchainKHR GetSwapChain() const { return _swapChain; }
    [[nodiscard]] vk::Image GetImage(uint32_t index) const { return _images[index]; }
    [[nodiscard]] vk::ImageView GetImageView(uint32_t index) const { return _imageViews[index]; }
    [[nodiscard]] vk::Format GetFormat() const { return _format; }
    [[nodiscard]] uint32_t GetImageCount() const { return _images.size(); }
    [[nodiscard]] vk::Extent2D GetExtent() const { return _extent; }

    static SupportDetails QuerySupport(vk::PhysicalDevice device, vk::SurfaceKHR surface);

private:
    void InitializeSwapChain(glm::uvec2 screenSize);
    void CleanUp();
    void InitializeImageViews();
    [[nodiscard]] vk::SurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& availableFormats) const;
    [[nodiscard]] vk::PresentModeKHR ChoosePresentMode(const std::vector<vk::PresentModeKHR>& availablePresentModes) const;
    [[nodiscard]] vk::Extent2D ChooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities, glm::uvec2 screenSize) const;

    std::shared_ptr<VulkanContext> _vulkanContext;
    vk::SwapchainKHR _swapChain;
    std::vector<vk::Image> _images;
    std::vector<vk::ImageView> _imageViews;
    vk::Format _format;
    vk::Extent2D _extent;
};
