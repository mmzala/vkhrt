#pragma once
#include <vector>
#include <string_view>
#include <vulkan/vulkan.hpp>

class Shader
{
public:
    static std::vector<std::byte> ReadFile(std::string_view filename);
    static vk::ShaderModule CreateShaderModule(const std::vector<std::byte>& byteCode, const vk::Device& device);
    static vk::ShaderModule CreateShaderModule(std::string_view filename, const vk::Device& device);
};