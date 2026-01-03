#pragma once
#include <string>
#include <vector>
#include <cstdint>

std::vector<std::byte> LoadImageFromFile(const std::string& path, int32_t& width, int32_t& height, int32_t& nrChannels, int32_t desiredChannels = 4);
std::vector<std::byte> LoadFloatImageFromFile(const std::string& path, int32_t& width, int32_t& height, int32_t& nrChannels, int32_t desiredChannels = 4);