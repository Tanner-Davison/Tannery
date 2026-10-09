#include "GpuProfiler.hpp"
#include "vk_enum_string_helper.h"
#include <format>
#include <stdexcept>

GpuProfiler::GpuProfiler(const GraphicsContext& pContext) {
#ifdef TRACY_ENABLE
    const VkDevice device = pContext.deviceHandle();

    // Tracy records, submits and waits on this command buffer while it sets up (it resets the
    // query pool and takes a first timestamp to line the GPU clock up with the CPU clock).
    // RESET_COMMAND_BUFFER lets it begin the same buffer several times. The pool is only needed
    // for this setup, so it is thrown away afterwards.
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = pContext.queueFamilies().graphicsFamilyIndex.value();

    VkCommandPool pool = VK_NULL_HANDLE;
    VkResult      res  = vkCreateCommandPool(device, &poolInfo, nullptr, &pool);
    if (res != VK_SUCCESS) {
        throw std::runtime_error(
            std::format("GpuProfiler vkCreateCommandPool failed. VkError: {}",
                        string_VkResult(res)));
    }

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool        = pool;
    allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer cmd = VK_NULL_HANDLE;
    res                 = vkAllocateCommandBuffers(device, &allocInfo, &cmd);
    if (res != VK_SUCCESS) {
        vkDestroyCommandPool(device, pool, nullptr);
        throw std::runtime_error(
            std::format("GpuProfiler vkAllocateCommandBuffers failed. VkError: {}",
                        string_VkResult(res)));
    }

    // "Uncalibrated" form: uses the GPU's own timestamp clock (VK_TIME_DOMAIN_DEVICE_EXT)
    context = TracyVkContext(pContext.physicalDeviceHandle(),
                             device,
                             pContext.graphicsQueue(),
                             cmd);
    TracyVkContextName(context, "Graphics", 8);

    vkDestroyCommandPool(device, pool, nullptr); // also frees `cmd`
#else
    (void)pContext;
#endif
}

GpuProfiler::~GpuProfiler() {
    TracyVkDestroy(context);
}

TracyVkCtx GpuProfiler::handle() const {
    return context;
}
