#include "MaterialDescriptors.hpp"
#include "vk_enum_string_helper.h"
#include <format>
#include <stdexcept>

namespace {
void check(VkResult res, const char* what) {
    if (res != VK_SUCCESS) {
        throw std::runtime_error(
            std::format("{} failed. VkError: {}", what, string_VkResult(res)));
    }
}
} // namespace

MaterialDescriptors::MaterialDescriptors(const GraphicsContext& pContext,
                                         const Texture&         pTexture,
                                         const Sampler&         pSampler)
    : device(pContext.deviceHandle()) {
    try {
        // 1. Layout: the SHAPE. Binding 0 is an image + sampler pair, read by the fragment shader
        VkDescriptorSetLayoutBinding binding{};
        binding.binding         = 0;
        binding.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        binding.descriptorCount = 1;
        binding.stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutCreateInfo layoutCreateInfo{};
        layoutCreateInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutCreateInfo.bindingCount = 1;
        layoutCreateInfo.pBindings    = &binding;
        check(vkCreateDescriptorSetLayout(device, &layoutCreateInfo, nullptr, &layout),
              "vkCreateDescriptorSetLayout (material)");

        // 2. Pool: room for exactly one set holding one image+sampler descriptor.
        //    Not per frame: the texture never changes, so both frames in flight share it.
        VkDescriptorPoolSize poolSize{};
        poolSize.type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSize.descriptorCount = 1;

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.maxSets       = 1;
        poolInfo.poolSizeCount = 1;
        poolInfo.pPoolSizes    = &poolSize;
        check(vkCreateDescriptorPool(device, &poolInfo, nullptr, &pool),
              "vkCreateDescriptorPool (material)");

        // 3. Set: the INSTANCE of the layout
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool     = pool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts        = &layout;
        check(vkAllocateDescriptorSets(device, &allocInfo, &set),
              "vkAllocateDescriptorSets (material)");

        // 4. Point binding 0 at the texture's view + the sampler.
        //    imageLayout must be the layout the image is in when sampled (barrier B's newLayout).
        VkDescriptorImageInfo imageInfo{};
        imageInfo.sampler     = pSampler.handle();
        imageInfo.imageView   = pTexture.viewHandle();
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkWriteDescriptorSet write{};
        write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet          = set;
        write.dstBinding      = 0;
        write.dstArrayElement = 0;
        write.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        write.descriptorCount = 1;
        write.pImageInfo      = &imageInfo; // pImageInfo, not pBufferInfo
        vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
    } catch (...) {
        destroy();
        throw;
    }
}

MaterialDescriptors::~MaterialDescriptors() {
    destroy();
}

void MaterialDescriptors::destroy() {
    if (pool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device, pool, nullptr); // also frees `set`
        pool = VK_NULL_HANDLE;
        set  = VK_NULL_HANDLE;
    }
    if (layout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device, layout, nullptr);
        layout = VK_NULL_HANDLE;
    }
}

VkDescriptorSetLayout MaterialDescriptors::layoutHandle() const {
    return layout;
}

VkDescriptorSet MaterialDescriptors::setHandle() const {
    return set;
}
