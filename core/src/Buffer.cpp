#include "Buffer.hpp"
#include "vk_enum_string_helper.h"
#include <format>
#include <stdexcept>

Buffer::Buffer(VmaAllocator             pAllocator,
               VkDeviceSize             pSize,
               VkBufferUsageFlags       pUsage,
               VmaAllocationCreateFlags pFlags)
    : allocator(pAllocator)
    , size(pSize) {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size        = pSize;
    bufferInfo.usage       = pUsage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
    allocInfo.flags = pFlags;

    VkResult createBufferResult = vmaCreateBuffer(this->allocator,
                                                  &bufferInfo,
                                                  &allocInfo,
                                                  &this->buffer,
                                                  &this->allocation,
                                                  nullptr);
    if (createBufferResult != VK_SUCCESS) {
        throw std::runtime_error(std::format("Failed to create buffer. VkError:{}",
                                             string_VkResult(createBufferResult)));
    }
};

Buffer::~Buffer() {
    vmaDestroyBuffer(this->allocator, this->buffer, this->allocation);
}

VkBuffer Buffer::handle() const {
    return this->buffer;
};

VmaAllocation Buffer::allocationHandle() const {
    return this->allocation;
};

VkDeviceSize Buffer::sizeBytes() const {
    return this->size;
};
