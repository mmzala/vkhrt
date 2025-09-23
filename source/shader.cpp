#include "shader.hpp"
#include "vk_common.hpp"
#include <fstream>
#include <spdlog/spdlog.h>

std::vector<std::byte> Shader::ReadFile(std::string_view filename)
{
    // Open file at the end and interpret data as binary.
    std::ifstream file { filename.data(), std::ios::ate | std::ios::binary };

    // Failed to open file.
    if (!file.is_open())
    {
        spdlog::error("[FILE] Failed to open file {}", filename);
        return {};
    }

    // Deduce file size based on read position (remember we opened the file at the end with the ate flag).
    size_t fileSize = file.tellg();

    // Allocate buffer with required file size.
    std::vector<std::byte> buffer(fileSize);

    // Place read position back to the start.
    file.seekg(0);

    // Read the buffer.
    file.read(reinterpret_cast<char*>(buffer.data()), fileSize);

    file.close();

    return buffer;
}

vk::ShaderModule Shader::CreateShaderModule(const std::vector<std::byte>& byteCode, const vk::Device& device)
{
    vk::ShaderModuleCreateInfo createInfo {};
    createInfo.codeSize = byteCode.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(byteCode.data());

    vk::ShaderModule shaderModule {};
    VkCheckResult(device.createShaderModule(&createInfo, nullptr, &shaderModule), "Failed creating shader module!");

    return shaderModule;
}

vk::ShaderModule Shader::CreateShaderModule(std::string_view filename, const vk::Device& device)
{
    std::vector<std::byte> bytes = ReadFile(filename);

    if (bytes.empty())
    {
        return {};
    }

    return CreateShaderModule(bytes, device);
}
