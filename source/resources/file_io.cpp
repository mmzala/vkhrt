#include "resources/file_io.hpp"
#include <stb_image.h>
#include <spdlog/spdlog.h>

std::vector<std::byte> LoadImageFromFile(const std::string& path, int32_t& width, int32_t& height, int32_t& nrChannels, int32_t desiredChannels)
{
    unsigned char* stbiData = stbi_load(path.c_str(), &width, &height, &nrChannels, desiredChannels);

    if (!stbiData)
    {
        spdlog::error("[IMAGE LOADING] Failed to load data for image from path [{}]", path);
        return {};
    }

    std::vector<std::byte> data = std::vector<std::byte>(width * height * 4);
    std::memcpy(data.data(), std::bit_cast<std::byte*>(stbiData), data.size());
    stbi_image_free(stbiData);

    return data;
}

std::vector<std::byte> LoadFloatImageFromFile(const std::string& path, int32_t& width, int32_t& height, int32_t& nrChannels, int32_t desiredChannels)
{
    float* stbiData = stbi_loadf(path.c_str(), &width, &height, &nrChannels, desiredChannels);

    if (!stbiData)
    {
        spdlog::error("[IMAGE LOADING] Failed to load data for image from path [{}]", path);
        return {};
    }

    std::vector<std::byte> data = std::vector<std::byte>(width * height * 4 * sizeof(float));
    std::memcpy(data.data(), std::bit_cast<std::byte*>(stbiData), data.size());
    stbi_image_free(stbiData);

    return data;
}