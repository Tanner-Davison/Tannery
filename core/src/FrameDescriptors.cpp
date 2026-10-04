#include "FrameDescriptors.hpp"
#include "vk_enum_string_helper.h"
#include <format>

// function only exists in this implementation file
namespace {
void check(VkResult res, const char* what) {
    if (res != VK_SUCCESS) {
        throw std::runtime_error(
            std::format("{} failed. VkError: {}", what, string_VkResult(res)));
    }
}
} // namespace

FrameDescriptors::FrameDescriptors(const GraphicsContext& pContext, uint32_t pFrameCount)
    : device(pContext.deviceHandle()) {
    try {
        // Layout: the SHAPE. Binding 0 is a uniform buffer, visibile to the vertex shader
        VkDescriptorSetLayoutBinding bindingDescriptorLayout{};
        bindingDescriptorLayout.binding         = 0;
        bindingDescriptorLayout.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        bindingDescriptorLayout.descriptorCount = 1;
        bindingDescriptorLayout.stageFlags      = VK_SHADER_STAGE_VERTEX_BIT;

        VkDescriptorSetLayoutCreateInfo layoutCreateInfo{};
        layoutCreateInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutCreateInfo.bindingCount = 1;
        layoutCreateInfo.pBindings    = &bindingDescriptorLayout;

        check(vkCreateDescriptorSetLayout(this->device, &layoutCreateInfo, nullptr, &layout),
              "vkCreateDescriptorSetLayout");

        // 2. Pool: where sets are allocated from (room for one UBO descriptor per frame slot)
        VkDescriptorPoolSize poolSize{};
        poolSize.type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        poolSize.descriptorCount = pFrameCount;

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.maxSets       = pFrameCount;
        poolInfo.poolSizeCount = 1;
        poolInfo.pPoolSizes    = &poolSize;
        check(vkCreateDescriptorPool(this->device, &poolInfo, nullptr, &pool),
              "vkCreateDescriptorPool");

        // 3. Sets: the INSTANCES, one per frame slot, all with the same layout
        std::vector<VkDescriptorSetLayout> layouts(pFrameCount, layout);
        VkDescriptorSetAllocateInfo        allocDescriptorInfo{};
        allocDescriptorInfo.sType          = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocDescriptorInfo.descriptorPool = pool;
        allocDescriptorInfo.descriptorSetCount = pFrameCount;
        allocDescriptorInfo.pSetLayouts        = layouts.data();
        sets.resize(pFrameCount);
        check(vkAllocateDescriptorSets(this->device, &allocDescriptorInfo, sets.data()),
              "vkAllocateDescriptorSets");

        // 4. One persistently-mapped uniform buffer per slot; point set i at buffer i
        for (uint32_t i = 0; i < pFrameCount; ++i) {
            buffers.push_back(std::make_unique<Buffer>(
                pContext.allocatorHandle(),
                sizeof(CameraUBO),
                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                    VMA_ALLOCATION_CREATE_MAPPED_BIT));
            VkDescriptorBufferInfo bufferInfo{};
            bufferInfo.buffer = buffers[i]->handle();
            bufferInfo.offset = 0;
            bufferInfo.range  = sizeof(CameraUBO);

            VkWriteDescriptorSet write{};
            write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write.dstSet          = sets[i];
            write.dstBinding      = 0;
            write.dstArrayElement = 0;
            write.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            write.descriptorCount = 1;
            write.pBufferInfo     = &bufferInfo;
            vkUpdateDescriptorSets(this->device, 1, &write, 0, nullptr);
        }
    } catch (...) {
        destroy();
        throw;
    }
};

FrameDescriptors::~FrameDescriptors() {
    destroy();
};

void FrameDescriptors::destroy() {
    buffers.clear();
    if (pool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(this->device, this->pool, nullptr);
        pool = VK_NULL_HANDLE;
    }
    if (layout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(this->device, this->layout, nullptr);
        layout = VK_NULL_HANDLE;
    }
}

void FrameDescriptors::update(uint32_t pFrameIndex, const CameraUBO& pData) {
    buffers.at(pFrameIndex)->write(&pData, sizeof(CameraUBO));
}

VkDescriptorSetLayout FrameDescriptors::layoutHandle() const {
    return this->layout;
}

VkDescriptorSet FrameDescriptors::setHandle(uint32_t frameIndex) const {
    return sets.at(frameIndex);
}
