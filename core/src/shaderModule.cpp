#include "shaderModule.hpp"
#include <fstream>
#include <stdexcept>

std::vector<char> readFile(const std::filesystem::path& filename) {
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        throw std::runtime_error(
            "Error: unable to open .SPV file OR .SPV file does not exist");
    }
    size_t            fileSize{static_cast<size_t>(file.tellg())};
    std::vector<char> buffer(fileSize); // init buffer to filesize length
    file.seekg(0);                      // puts the read position to the front of the file
    file.read(buffer.data(), fileSize); // adds the read into our buffer

    if (buffer.empty()) {
        throw std::runtime_error("Warning: Returning empty buffer");
    }
    return buffer;
};

VkShaderModule createShaderModule(VkDevice device, const std::vector<char>& code) {
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode    = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule shaderModule{VK_NULL_HANDLE};
    if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        throw std::runtime_error("Error while attempting to create Shader Module");
    };
    return shaderModule;
};
