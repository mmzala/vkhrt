#pragma once
#include "vk_common.hpp"
#include "common.hpp"
#include "bottom_level_acceleration_structure.hpp"
#include <memory>
#include <vulkan/vulkan.hpp>

struct VulkanInitInfo;
class FlyCamera;
class CameraResource;
struct Image;
class VulkanContext;
class SwapChain;
class ModelLoader;
class BottomLevelAccelerationStructure;
class TopLevelAccelerationStructure;
class BindlessResources;

class Renderer
{
public:
    Renderer(const VulkanInitInfo& initInfo, const std::shared_ptr<VulkanContext>& vulkanContext, const std::shared_ptr<FlyCamera>& flyCamera);
    ~Renderer();
    NON_COPYABLE(Renderer);
    NON_MOVABLE(Renderer);

    void Render();

private:
    void RecordCommands(const vk::CommandBuffer& commandBuffer, uint32_t swapChainImageIndex, uint32_t currentResourceFrame);
    void UpdateCameraResource(uint32_t currentResourceFrame);

    void InitializeCommandBuffers();
    void InitializeSynchronizationObjects();
    void InitializeRenderTarget();

    void InitializeDescriptorSets();
    void InitializePipeline();
    void InitializeShaderBindingTable(const vk::RayTracingPipelineCreateInfoKHR& pipelineInfo);

    void InitializeBLAS();

    std::shared_ptr<VulkanContext> _vulkanContext;
    std::unique_ptr<SwapChain> _swapChain;
    std::array<vk::CommandBuffer, MAX_FRAMES_IN_FLIGHT> _commandBuffers;
    std::array<vk::Semaphore, MAX_FRAMES_IN_FLIGHT> _imageAvailableSemaphores;
    std::array<vk::Semaphore, MAX_FRAMES_IN_FLIGHT> _renderFinishedSemaphores;
    std::array<vk::Fence, MAX_FRAMES_IN_FLIGHT> _inFlightFences;
    std::unique_ptr<Image> _renderTarget;

    uint32_t _renderedFrames = 0;

    std::unique_ptr<ModelLoader> _modelLoader;
    std::shared_ptr<BindlessResources> _bindlessResources;

    std::vector<std::shared_ptr<Model>> _models {};
    std::vector<BottomLevelAccelerationStructure> _blases {};
    std::unique_ptr<TopLevelAccelerationStructure> _tlas;

    vk::DescriptorPool _descriptorPool;
    vk::DescriptorSetLayout _descriptorSetLayout;
    vk::DescriptorSet _descriptorSet;

    std::shared_ptr<FlyCamera> _flyCamera;
    std::unique_ptr<CameraResource> _cameraResource;

    std::unique_ptr<Buffer> _raygenSBT;
    std::unique_ptr<Buffer> _missSBT;
    std::unique_ptr<Buffer> _hitSBT;
    vk::StridedDeviceAddressRegionKHR _raygenAddressRegion {};
    vk::StridedDeviceAddressRegionKHR _missAddressRegion {};
    vk::StridedDeviceAddressRegionKHR _hitAddressRegion {};

    vk::PipelineLayout _pipelineLayout;
    vk::Pipeline _pipeline;

    uint32_t _windowWidth = 0;
    uint32_t _windowHeight = 0;
};
