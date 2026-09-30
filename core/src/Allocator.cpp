#include "Allocator.hpp"
#include <format>
#include <stdexcept>
#include <vk_enum_string_helper.h>

Allocator::Allocator(VkInstance       pInstance,
                     VkPhysicalDevice pPhysicalDevice,
                     VkDevice         pDevice) {
    VmaAllocatorCreateInfo allocatorInfo{};
    allocatorInfo.physicalDevice   = pPhysicalDevice;
    allocatorInfo.device           = pDevice;
    allocatorInfo.instance         = pInstance;
    allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_3;
    VkResult vmaResult             = vmaCreateAllocator(&allocatorInfo, &allocator);
    if (vmaResult != VK_SUCCESS) {
        throw std::runtime_error(std::format("failed to create vma allocator. VkError: {}",
                                             string_VkResult(vmaResult)));
    }
};

Allocator::~Allocator() {
    vmaDestroyAllocator(this->allocator);
}

VmaAllocator Allocator::handle() const {
    return this->allocator;
};
