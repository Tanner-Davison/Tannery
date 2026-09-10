#pragma once
#include <filesystem>
#include <vector>
#include <vulkan/vulkan.h>

std::vector<char> readFile(const std::filesystem::path& filePath);

VkShaderModule createShaderModule(VkDevice device, const std::vector<char>& code);
