#include "Sampler.hpp"
#include "vk_enum_string_helper.h"
#include <format>
#include <stdexcept>

Sampler::Sampler(const GraphicsContext& pContext) : device(pContext.deviceHandle()) {
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    // Filtering: blend the 4 nearest pixels when magnified or minified (smooth, not blocky)
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;

    // Wrapping: UVs outside 0..1 tile the image
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW =
        VK_SAMPLER_ADDRESS_MODE_REPEAT; // 3D textures only, set for safety

    // Anisotropy: off until the samplerAnisotropy device feature is enabled
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.maxAnisotropy    = 1.0f;

    // Mips: LINEAR blends between two levels (trilinear). The sampler is shared by many
    // textures with different level counts, so don't cap maxLod: CLAMP_NONE lets the
    // image's own level count be the limit.
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod     = 0.0f;
    samplerInfo.maxLod     = VK_LOD_CLAMP_NONE;

    // Unused for this sampler; values match the {} zero-init, written out to be explicit
    samplerInfo.borderColor   = VK_BORDER_COLOR_INT_OPAQUE_BLACK; // CLAMP_TO_BORDER only
    samplerInfo.compareEnable = VK_FALSE; // for shadow-map depth compares
    samplerInfo.compareOp     = VK_COMPARE_OP_ALWAYS;
    samplerInfo.unnormalizedCoordinates = VK_FALSE; // UVs are 0..1, not pixel indices

    VkResult res = vkCreateSampler(device, &samplerInfo, nullptr, &sampler);
    if (res != VK_SUCCESS) {
        throw std::runtime_error(
            std::format("Failed to create sampler. VkError: {}", string_VkResult(res)));
    }
}

Sampler::~Sampler() {
    vkDestroySampler(device, sampler, nullptr);
}

VkSampler Sampler::handle() const {
    return sampler;
}
