#pragma once
#include <vulkan/vulkan_core.h>

void copyBuffer(VkDevice     pDevice,
                VkQueue      pQueue,
                uint32_t     pQueueFamilyIndex,
                VkBuffer     pSrc,
                VkBuffer     pDst,
                VkDeviceSize pSize);
