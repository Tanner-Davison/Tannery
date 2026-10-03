#include "Buffer.hpp"
#include "vk_enum_string_helper.h"
#include <cstring>
#include <format>
#include <stdexcept>
#include <vk_mem_alloc.h>

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

    VmaAllocationCreateInfo allocCreateInfo{};
    allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
    allocCreateInfo.flags = pFlags;

    VmaAllocationInfo allocInfo{};

    VkResult createBufferResult = vmaCreateBuffer(this->allocator,
                                                  &bufferInfo,
                                                  &allocCreateInfo,
                                                  &this->buffer,
                                                  &this->allocation,
                                                  &allocInfo);
    if (createBufferResult != VK_SUCCESS) {
        throw std::runtime_error(std::format("Failed to create buffer. VkError:{}",
                                             string_VkResult(createBufferResult)));
    }

    mapped = allocInfo.pMappedData;
};

void* Buffer::mappedData() const {
    return this->mapped;
}

void Buffer::write(const void* data, VkDeviceSize bytes) {
    if (mapped == nullptr) {
        throw std::runtime_error("Buffer::write: buffer is not host-mapped");
    }
    if (bytes > this->size) {
        throw std::runtime_error("Buffer::write: data larger than the buffer");
    }
    std::memcpy(mapped, data, bytes);
    VkResult res = vmaFlushAllocation(this->allocator, this->allocation, 0, bytes);
    if (res != VK_SUCCESS) {
        throw std::runtime_error(
            std::format("vmaFlushAllocation failed. VkError: {}", string_VkResult(res)));
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
